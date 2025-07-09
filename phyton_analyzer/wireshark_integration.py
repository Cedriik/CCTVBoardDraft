#!/usr/bin/env python3
"""
ESP32-S3 CCTV Monitor - Wireshark Integration Module
Advanced packet analysis using Wireshark/tshark for deep inspection
"""

import os
import sys
import json
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any
import logging
import xml.etree.ElementTree as ET

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

class WiresharkAnalyzer:
    """
    Advanced Wireshark integration for CCTV packet analysis
    """
    
    def __init__(self, interface: str = "wlan0"):
        self.interface = interface
        self.temp_dir = tempfile.mkdtemp()
        self.capture_file = None
        self.analysis_profiles = {
            'video_streaming': {
                'protocols': ['rtp', 'rtsp', 'udp'],
                'ports': [554, 8000, 8080, 1935],
                'analysis_fields': [
                    'rtp.seq',
                    'rtp.timestamp',
                    'rtp.p_type',
                    'rtp.marker',
                    'frame.time_relative',
                    'frame.len'
                ]
            },
            'network_quality': {
                'protocols': ['ip', 'udp', 'tcp'],
                'analysis_fields': [
                    'ip.src',
                    'ip.dst',
                    'frame.time_relative',
                    'frame.len',
                    'udp.srcport',
                    'udp.dstport'
                ]
            }
        }
        
        self.quality_metrics = {
            'jitter': [],
            'delay': [],
            'packet_loss': 0.0,
            'bitrate': 0.0,
            'quality_score': 0.0
        }
    
    def check_wireshark_availability(self) -> bool:
        """Check if Wireshark/tshark is available"""
        try:
            result = subprocess.run(['tshark', '-v'], 
                                  capture_output=True, 
                                  text=True, 
                                  timeout=10)
            if result.returncode == 0:
                logger.info("Wireshark/tshark is available")
                return True
            else:
                logger.error("tshark command failed")
                return False
        except Exception as e:
            logger.error(f"Wireshark/tshark not available: {e}")
            return False
    
    def start_live_capture(self, duration: int = 60, 
                          capture_filter: str = "") -> Optional[str]:
        """Start live packet capture"""
        try:
            capture_file = os.path.join(self.temp_dir, f"capture_{int(time.time())}.pcap")
            
            # Build tshark command for live capture
            cmd = [
                'tshark',
                '-i', self.interface,
                '-a', f'duration:{duration}',
                '-w', capture_file
            ]
            
            if capture_filter:
                cmd.extend(['-f', capture_filter])
            
            logger.info(f"Starting live capture: {' '.join(cmd)}")
            
            # Start capture process
            process = subprocess.Popen(cmd, 
                                     stdout=subprocess.PIPE, 
                                     stderr=subprocess.PIPE,
                                     text=True)
            
            # Wait for capture to complete
            stdout, stderr = process.communicate()
            
            if process.returncode == 0:
                logger.info(f"Capture completed successfully: {capture_file}")
                self.capture_file = capture_file
                return capture_file
            else:
                logger.error(f"Capture failed: {stderr}")
                return None
                
        except Exception as e:
            logger.error(f"Error starting live capture: {e}")
            return None
    
    def analyze_capture_file(self, pcap_file: str) -> Dict[str, Any]:
        """Analyze existing capture file"""
        try:
            if not os.path.exists(pcap_file):
                logger.error(f"Capture file not found: {pcap_file}")
                return {}
            
            logger.info(f"Analyzing capture file: {pcap_file}")
            
            # Perform different types of analysis
            results = {
                'video_analysis': self._analyze_video_streams(pcap_file),
                'network_analysis': self._analyze_network_quality(pcap_file),
                'protocol_analysis': self._analyze_protocols(pcap_file),
                'statistics': self._get_capture_statistics(pcap_file)
            }
            
            # Calculate overall quality metrics
            results['quality_metrics'] = self._calculate_quality_metrics(results)
            
            return results
            
        except Exception as e:
            logger.error(f"Error analyzing capture file: {e}")
            return {}
    
    def _analyze_video_streams(self, pcap_file: str) -> Dict[str, Any]:
        """Analyze video streams in capture"""
        try:
            # Use tshark to extract RTP stream information
            cmd = [
                'tshark',
                '-r', pcap_file,
                '-Y', 'rtp',
                '-T', 'json',
                '-e', 'rtp.seq',
                '-e', 'rtp.timestamp',
                '-e', 'rtp.p_type',
                '-e', 'rtp.marker',
                '-e', 'frame.time_relative',
                '-e', 'frame.len',
                '-e', 'ip.src',
                '-e', 'ip.dst',
                '-e', 'udp.srcport',
                '-e', 'udp.dstport'
            ]
            
            logger.info(f"Analyzing video streams: {' '.join(cmd)}")
            
            result = subprocess.run(cmd, 
                                  capture_output=True, 
                                  text=True, 
                                  timeout=300)
            
            if result.returncode == 0:
                packets = json.loads(result.stdout)
                return self._process_video_packets(packets)
            else:
                logger.error(f"Video analysis failed: {result.stderr}")
                return {}
                
        except Exception as e:
            logger.error(f"Error analyzing video streams: {e}")
            return {}
    
    def _process_video_packets(self, packets: List[Dict]) -> Dict[str, Any]:
        """Process video packets for quality analysis"""
        try:
            streams = {}
            
            for packet in packets:
                layers = packet.get('_source', {}).get('layers', {})
                
                # Extract packet information
                frame_time = float(layers.get('frame.time_relative', ['0'])[0])
                frame_len = int(layers.get('frame.len', ['0'])[0])
                
                rtp_seq = layers.get('rtp.seq', ['0'])[0]
                rtp_timestamp = layers.get('rtp.timestamp', ['0'])[0]
                rtp_payload_type = layers.get('rtp.p_type', ['0'])[0]
                
                src_ip = layers.get('ip.src', [''])[0]
                dst_ip = layers.get('ip.dst', [''])[0]
                src_port = layers.get('udp.srcport', ['0'])[0]
                dst_port = layers.get('udp.dstport', ['0'])[0]
                
                # Create stream identifier
                stream_id = f"{src_ip}:{src_port}->{dst_ip}:{dst_port}"
                
                if stream_id not in streams:
                    streams[stream_id] = {
                        'packets': [],
                        'payload_type': rtp_payload_type,
                        'total_bytes': 0,
                        'packet_count': 0
                    }
                
                # Add packet to stream
                streams[stream_id]['packets'].append({
                    'seq': int(rtp_seq),
                    'timestamp': int(rtp_timestamp),
                    'time': frame_time,
                    'length': frame_len
                })
                
                streams[stream_id]['total_bytes'] += frame_len
                streams[stream_id]['packet_count'] += 1
            
            # Analyze each stream
            for stream_id, stream_data in streams.items():
                stream_data['analysis'] = self._analyze_single_stream(stream_data)
            
            return streams
            
        except Exception as e:
            logger.error(f"Error processing video packets: {e}")
            return {}
    
    def _analyze_single_stream(self, stream_data: Dict) -> Dict[str, Any]:
        """Analyze a single video stream"""
        try:
            packets = stream_data['packets']
            if len(packets) < 2:
                return {}
            
            # Sort packets by sequence number
            packets.sort(key=lambda x: x['seq'])
            
            # Calculate jitter
            jitter_samples = []
            for i in range(1, len(packets)):
                time_diff = (packets[i]['time'] - packets[i-1]['time']) * 1000  # ms
                jitter_samples.append(time_diff)
            
            avg_jitter = sum(jitter_samples) / len(jitter_samples) if jitter_samples else 0
            
            # Calculate packet loss
            expected_packets = packets[-1]['seq'] - packets[0]['seq'] + 1
            received_packets = len(packets)
            packet_loss = ((expected_packets - received_packets) / expected_packets) * 100 if expected_packets > 0 else 0
            
            # Calculate bitrate
            duration = packets[-1]['time'] - packets[0]['time']
            bitrate = (stream_data['total_bytes'] * 8) / duration / 1_000_000 if duration > 0 else 0  # Mbps
            
            # Calculate delay (simplified)
            delay = avg_jitter / 2
            
            return {
                'jitter_ms': avg_jitter,
                'packet_loss_percent': packet_loss,
                'bitrate_mbps': bitrate,
                'delay_ms': delay,
                'total_packets': received_packets,
                'expected_packets': expected_packets,
                'duration_seconds': duration
            }
            
        except Exception as e:
            logger.error(f"Error analyzing single stream: {e}")
            return {}
    
    def _analyze_network_quality(self, pcap_file: str) -> Dict[str, Any]:
        """Analyze overall network quality"""
        try:
            # Get network statistics using tshark
            cmd = [
                'tshark',
                '-r', pcap_file,
                '-q',  # Quiet mode
                '-z', 'conv,udp',  # UDP conversations
                '-z', 'io,stat,1'  # I/O statistics per second
            ]
            
            result = subprocess.run(cmd, 
                                  capture_output=True, 
                                  text=True, 
                                  timeout=120)
            
            if result.returncode == 0:
                return self._parse_network_statistics(result.stdout)
            else:
                logger.error(f"Network analysis failed: {result.stderr}")
                return {}
                
        except Exception as e:
            logger.error(f"Error analyzing network quality: {e}")
            return {}
    
    def _parse_network_statistics(self, stats_output: str) -> Dict[str, Any]:
        """Parse network statistics from tshark output"""
        try:
            lines = stats_output.split('\n')
            
            udp_conversations = []
            io_stats = []
            
            parsing_udp = False
            parsing_io = False
            
            for line in lines:
                line = line.strip()
                
                if 'UDP Conversations' in line:
                    parsing_udp = True
                    parsing_io = False
                    continue
                elif 'IO Statistics' in line:
                    parsing_udp = False
                    parsing_io = True
                    continue
                elif line.startswith('='):
                    parsing_udp = False
                    parsing_io = False
                    continue
                
                if parsing_udp and '<->' in line:
                    parts = line.split()
                    if len(parts) >= 6:
                        udp_conversations.append({
                            'endpoints': parts[0],
                            'frames': int(parts[1]),
                            'bytes': int(parts[2]),
                            'frames_ab': int(parts[3]),
                            'bytes_ab': int(parts[4])
                        })
                
                if parsing_io and '|' in line:
                    parts = [p.strip() for p in line.split('|')]
                    if len(parts) >= 3:
                        try:
                            io_stats.append({
                                'interval': parts[0],
                                'frames': int(parts[1]),
                                'bytes': int(parts[2])
                            })
                        except (ValueError, IndexError):
                            continue
            
            return {
                'udp_conversations': udp_conversations,
                'io_statistics': io_stats,
                'total_udp_conversations': len(udp_conversations),
                'total_udp_frames': sum(conv['frames'] for conv in udp_conversations),
                'total_udp_bytes': sum(conv['bytes'] for conv in udp_conversations)
            }
            
        except Exception as e:
            logger.error(f"Error parsing network statistics: {e}")
            return {}
    
    def _analyze_protocols(self, pcap_file: str) -> Dict[str, Any]:
        """Analyze protocol distribution"""
        try:
            cmd = [
                'tshark',
                '-r', pcap_file,
                '-q',
                '-z', 'ptype,tree'
            ]
            
            result = subprocess.run(cmd, 
                                  capture_output=True, 
                                  text=True, 
                                  timeout=120)
            
            if result.returncode == 0:
                return self._parse_protocol_tree(result.stdout)
            else:
                logger.error(f"Protocol analysis failed: {result.stderr}")
                return {}
                
        except Exception as e:
            logger.error(f"Error analyzing protocols: {e}")
            return {}
    
    def _parse_protocol_tree(self, tree_output: str) -> Dict[str, Any]:
        """Parse protocol tree output"""
        try:
            lines = tree_output.split('\n')
            protocols = {}
            
            for line in lines:
                line = line.strip()
                if line and not line.startswith('=') and 'frames' in line:
                    parts = line.split()
                    if len(parts) >= 2:
                        protocol = parts[0]
                        frames = int(parts[1])
                        protocols[protocol] = frames
            
            return protocols
            
        except Exception as e:
            logger.error(f"Error parsing protocol tree: {e}")
            return {}
    
    def _get_capture_statistics(self, pcap_file: str) -> Dict[str, Any]:
        """Get basic capture statistics"""
        try:
            cmd = [
                'capinfos',
                '-T',  # Tab-separated output
                pcap_file
            ]
            
            result = subprocess.run(cmd, 
                                  capture_output=True, 
                                  text=True, 
                                  timeout=60)
            
            if result.returncode == 0:
                return self._parse_capture_info(result.stdout)
            else:
                logger.error(f"Capture statistics failed: {result.stderr}")
                return {}
                
        except Exception as e:
            logger.error(f"Error getting capture statistics: {e}")
            return {}
    
    def _parse_capture_info(self, info_output: str) -> Dict[str, Any]:
        """Parse capture info output"""
        try:
            lines = info_output.split('\n')
            stats = {}
            
            for line in lines:
                if '\t' in line:
                    key, value = line.split('\t', 1)
                    stats[key.strip()] = value.strip()
            
            return stats
            
        except Exception as e:
            logger.error(f"Error parsing capture info: {e}")
            return {}
    
    def _calculate_quality_metrics(self, analysis_results: Dict) -> Dict[str, float]:
        """Calculate overall quality metrics"""
        try:
            quality_metrics = {
                'overall_score': 0.0,
                'video_quality': 0.0,
                'network_quality': 0.0,
                'avg_jitter': 0.0,
                'avg_packet_loss': 0.0,
                'avg_bitrate': 0.0
            }
            
            # Analyze video streams
            video_analysis = analysis_results.get('video_analysis', {})
            if video_analysis:
                jitter_values = []
                packet_loss_values = []
                bitrate_values = []
                
                for stream_id, stream_data in video_analysis.items():
                    analysis = stream_data.get('analysis', {})
                    if analysis:
                        jitter_values.append(analysis.get('jitter_ms', 0))
                        packet_loss_values.append(analysis.get('packet_loss_percent', 0))
                        bitrate_values.append(analysis.get('bitrate_mbps', 0))
                
                if jitter_values:
                    quality_metrics['avg_jitter'] = sum(jitter_values) / len(jitter_values)
                if packet_loss_values:
                    quality_metrics['avg_packet_loss'] = sum(packet_loss_values) / len(packet_loss_values)
                if bitrate_values:
                    quality_metrics['avg_bitrate'] = sum(bitrate_values) / len(bitrate_values)
                
                # Calculate video quality score (0-100)
                jitter_score = max(0, 100 - quality_metrics['avg_jitter'])
                loss_score = max(0, 100 - quality_metrics['avg_packet_loss'] * 10)
                bitrate_score = min(100, quality_metrics['avg_bitrate'] * 10)
                
                quality_metrics['video_quality'] = (jitter_score + loss_score + bitrate_score) / 3
            
            # Calculate overall score
            quality_metrics['overall_score'] = quality_metrics['video_quality']
            
            return quality_metrics
            
        except Exception as e:
            logger.error(f"Error calculating quality metrics: {e}")
            return {}
    
    def generate_report(self, analysis_results: Dict) -> str:
        """Generate analysis report"""
        try:
            report = []
            report.append("=" * 60)
            report.append("ESP32-S3 CCTV Monitor - Wireshark Analysis Report")
            report.append("=" * 60)
            report.append(f"Generated: {time.strftime('%Y-%m-%d %H:%M:%S')}")
            report.append("")
            
            # Quality metrics
            quality_metrics = analysis_results.get('quality_metrics', {})
            if quality_metrics:
                report.append("QUALITY METRICS:")
                report.append(f"  Overall Score: {quality_metrics.get('overall_score', 0):.1f}/100")
                report.append(f"  Video Quality: {quality_metrics.get('video_quality', 0):.1f}/100")
                report.append(f"  Average Jitter: {quality_metrics.get('avg_jitter', 0):.2f} ms")
                report.append(f"  Average Packet Loss: {quality_metrics.get('avg_packet_loss', 0):.2f}%")
                report.append(f"  Average Bitrate: {quality_metrics.get('avg_bitrate', 0):.2f} Mbps")
                report.append("")
            
            # Video streams
            video_analysis = analysis_results.get('video_analysis', {})
            if video_analysis:
                report.append("VIDEO STREAMS:")
                for stream_id, stream_data in video_analysis.items():
                    report.append(f"  Stream: {stream_id}")
                    analysis = stream_data.get('analysis', {})
                    if analysis:
                        report.append(f"    Jitter: {analysis.get('jitter_ms', 0):.2f} ms")
                        report.append(f"    Packet Loss: {analysis.get('packet_loss_percent', 0):.2f}%")
                        report.append(f"    Bitrate: {analysis.get('bitrate_mbps', 0):.2f} Mbps")
                        report.append(f"    Packets: {analysis.get('total_packets', 0)}")
                    report.append("")
            
            # Network statistics
            network_analysis = analysis_results.get('network_analysis', {})
            if network_analysis:
                report.append("NETWORK STATISTICS:")
                report.append(f"  UDP Conversations: {network_analysis.get('total_udp_conversations', 0)}")
                report.append(f"  Total UDP Frames: {network_analysis.get('total_udp_frames', 0)}")
                report.append(f"  Total UDP Bytes: {network_analysis.get('total_udp_bytes', 0)}")
                report.append("")
            
            # Protocol distribution
            protocol_analysis = analysis_results.get('protocol_analysis', {})
            if protocol_analysis:
                report.append("PROTOCOL DISTRIBUTION:")
                for protocol, frames in protocol_analysis.items():
                    report.append(f"  {protocol}: {frames} frames")
                report.append("")
            
            return "\n".join(report)
            
        except Exception as e:
            logger.error(f"Error generating report: {e}")
            return "Error generating report"
    
    def cleanup(self):
        """Clean up temporary files"""
        try:
            if os.path.exists(self.temp_dir):
                import shutil
                shutil.rmtree(self.temp_dir)
                logger.info(f"Cleaned up temporary directory: {self.temp_dir}")
        except Exception as e:
            logger.error(f"Error cleaning up: {e}")

def main():
    """Main function for standalone execution"""
    logger.info("Starting ESP32-S3 CCTV Monitor - Wireshark Integration")
    
    # Configuration
    interface = os.getenv('CAPTURE_INTERFACE', 'wlan0')
    duration = int(os.getenv('CAPTURE_DURATION', '60'))
    output_dir = os.getenv('OUTPUT_DIR', '.')
    
    # Create analyzer
    analyzer = WiresharkAnalyzer(interface)
    
    try:
        # Check Wireshark availability
        if not analyzer.check_wireshark_availability():
            logger.error("Wireshark/tshark not available")
            sys.exit(1)
        
        # Start live capture
        logger.info(f"Starting {duration} second capture on {interface}")
        capture_file = analyzer.start_live_capture(duration)
        
        if not capture_file:
            logger.error("Failed to capture packets")
            sys.exit(1)
        
        # Analyze capture
        logger.info("Analyzing capture file...")
        results = analyzer.analyze_capture_file(capture_file)
        
        # Generate report
        report = analyzer.generate_report(results)
        
        # Save results
        output_file = os.path.join(output_dir, f"analysis_{int(time.time())}.json")
        with open(output_file, 'w') as f:
            json.dump(results, f, indent=2)
        
        report_file = os.path.join(output_dir, f"report_{int(time.time())}.txt")
        with open(report_file, 'w') as f:
            f.write(report)
        
        logger.info(f"Results saved to {output_file}")
        logger.info(f"Report saved to {report_file}")
        
        # Print report
        print(report)
        
    except KeyboardInterrupt:
        logger.info("Interrupted by user")
    except Exception as e:
        logger.error(f"Error: {e}")
    finally:
        analyzer.cleanup()

if __name__ == "__main__":
    main()
