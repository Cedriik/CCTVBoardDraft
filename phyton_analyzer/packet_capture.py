#!/usr/bin/env python3
"""
ESP32-S3 CCTV Monitor - Python Packet Capture Module
Advanced packet analysis using Wireshark/tshark integration
"""

import os
import sys
import time
import json
import subprocess
import threading
import queue
import signal
from datetime import datetime
from typing import Dict, List, Optional, Tuple
import logging

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

class PacketCapture:
    """
    Advanced packet capture and analysis for CCTV monitoring
    """
    
    def __init__(self, interface: str = "wlan0", capture_filter: str = ""):
        self.interface = interface
        self.capture_filter = capture_filter
        self.capture_process = None
        self.analysis_thread = None
        self.packet_queue = queue.Queue()
        self.running = False
        self.statistics = {
            'total_packets': 0,
            'video_packets': 0,
            'rtp_packets': 0,
            'lost_packets': 0,
            'jitter_samples': [],
            'delay_samples': [],
            'bitrate_samples': [],
            'start_time': None
        }
        
        # Video analysis parameters
        self.video_ports = [554, 8000, 8080, 1935]  # Common video streaming ports
        self.rtp_payload_types = [96, 97, 98, 99, 26]  # Common video payload types
        
        # Metrics thresholds
        self.jitter_threshold = 50.0  # ms
        self.delay_threshold = 200.0  # ms
        self.packet_loss_threshold = 1.0  # %
        
    def start_capture(self) -> bool:
        """Start packet capture using tshark"""
        try:
            # Build tshark command
            cmd = [
                'tshark',
                '-i', self.interface,
                '-l',  # Line buffered output
                '-T', 'json',  # JSON output format
                '-e', 'frame.number',
                '-e', 'frame.time_relative',
                '-e', 'frame.len',
                '-e', 'ip.src',
                '-e', 'ip.dst',
                '-e', 'udp.srcport',
                '-e', 'udp.dstport',
                '-e', 'rtp.seq',
                '-e', 'rtp.timestamp',
                '-e', 'rtp.p_type',
                '-e', 'rtp.marker',
                '-Y', 'udp'  # Only capture UDP packets
            ]
            
            if self.capture_filter:
                cmd.extend(['-f', self.capture_filter])
            
            logger.info(f"Starting packet capture on interface {self.interface}")
            logger.info(f"Command: {' '.join(cmd)}")
            
            # Start tshark process
            self.capture_process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                bufsize=1
            )
            
            # Start analysis thread
            self.running = True
            self.statistics['start_time'] = time.time()
            self.analysis_thread = threading.Thread(target=self._analyze_packets)
            self.analysis_thread.daemon = True
            self.analysis_thread.start()
            
            logger.info("Packet capture started successfully")
            return True
            
        except Exception as e:
            logger.error(f"Failed to start packet capture: {e}")
            return False
    
    def stop_capture(self):
        """Stop packet capture"""
        logger.info("Stopping packet capture...")
        
        self.running = False
        
        if self.capture_process:
            self.capture_process.terminate()
            try:
                self.capture_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.capture_process.kill()
            self.capture_process = None
        
        if self.analysis_thread:
            self.analysis_thread.join(timeout=5)
            self.analysis_thread = None
        
        logger.info("Packet capture stopped")
    
    def _analyze_packets(self):
        """Analyze packets from tshark output"""
        logger.info("Starting packet analysis thread")
        
        packet_buffer = ""
        
        while self.running and self.capture_process:
            try:
                # Read output from tshark
                if self.capture_process.poll() is None:
                    line = self.capture_process.stdout.readline()
                    if line:
                        packet_buffer += line
                        
                        # Try to parse complete JSON objects
                        if packet_buffer.strip().endswith('}'):
                            try:
                                packets = json.loads(packet_buffer.strip())
                                if isinstance(packets, list):
                                    for packet in packets:
                                        self._process_packet(packet)
                                else:
                                    self._process_packet(packets)
                                packet_buffer = ""
                            except json.JSONDecodeError:
                                # Continue accumulating buffer
                                pass
                    else:
                        time.sleep(0.1)
                else:
                    break
                    
            except Exception as e:
                logger.error(f"Error in packet analysis: {e}")
                break
        
        logger.info("Packet analysis thread stopped")
    
    def _process_packet(self, packet: Dict):
        """Process individual packet"""
        try:
            layers = packet.get('_source', {}).get('layers', {})
            
            # Extract frame information
            frame_info = layers.get('frame', {})
            frame_number = frame_info.get('frame.number', ['0'])[0]
            frame_time = float(frame_info.get('frame.time_relative', ['0'])[0])
            frame_len = int(frame_info.get('frame.len', ['0'])[0])
            
            # Extract IP information
            ip_info = layers.get('ip', {})
            src_ip = ip_info.get('ip.src', [''])[0]
            dst_ip = ip_info.get('ip.dst', [''])[0]
            
            # Extract UDP information
            udp_info = layers.get('udp', {})
            src_port = int(udp_info.get('udp.srcport', ['0'])[0])
            dst_port = int(udp_info.get('udp.dstport', ['0'])[0])
            
            # Update statistics
            self.statistics['total_packets'] += 1
            
            # Check if this is a video packet
            if self._is_video_packet(src_port, dst_port):
                self.statistics['video_packets'] += 1
                
                # Check for RTP
                rtp_info = layers.get('rtp', {})
                if rtp_info:
                    self.statistics['rtp_packets'] += 1
                    self._analyze_rtp_packet(rtp_info, frame_time, frame_len)
            
            # Add to queue for further processing
            packet_data = {
                'frame_number': frame_number,
                'timestamp': frame_time,
                'length': frame_len,
                'src_ip': src_ip,
                'dst_ip': dst_ip,
                'src_port': src_port,
                'dst_port': dst_port,
                'is_video': self._is_video_packet(src_port, dst_port),
                'rtp_info': rtp_info
            }
            
            self.packet_queue.put(packet_data)
            
        except Exception as e:
            logger.error(f"Error processing packet: {e}")
    
    def _is_video_packet(self, src_port: int, dst_port: int) -> bool:
        """Check if packet is likely a video packet"""
        return (src_port in self.video_ports or 
                dst_port in self.video_ports or
                src_port >= 16384 or  # Common RTP port range
                dst_port >= 16384)
    
    def _analyze_rtp_packet(self, rtp_info: Dict, timestamp: float, length: int):
        """Analyze RTP packet for video metrics"""
        try:
            seq_num = int(rtp_info.get('rtp.seq', ['0'])[0])
            rtp_timestamp = int(rtp_info.get('rtp.timestamp', ['0'])[0])
            payload_type = int(rtp_info.get('rtp.p_type', ['0'])[0])
            
            # Check if this is a video payload type
            if payload_type in self.rtp_payload_types:
                # Calculate jitter (simplified)
                if len(self.statistics['jitter_samples']) > 0:
                    last_time = self.statistics['jitter_samples'][-1]['timestamp']
                    time_diff = (timestamp - last_time) * 1000  # Convert to ms
                    
                    if time_diff > 0:
                        self.statistics['jitter_samples'].append({
                            'timestamp': timestamp,
                            'jitter': time_diff,
                            'seq_num': seq_num
                        })
                        
                        # Keep only recent samples
                        if len(self.statistics['jitter_samples']) > 100:
                            self.statistics['jitter_samples'].pop(0)
                else:
                    self.statistics['jitter_samples'].append({
                        'timestamp': timestamp,
                        'jitter': 0,
                        'seq_num': seq_num
                    })
                
                # Calculate bitrate
                self.statistics['bitrate_samples'].append({
                    'timestamp': timestamp,
                    'bytes': length
                })
                
                # Keep only recent samples (last 10 seconds)
                current_time = time.time()
                self.statistics['bitrate_samples'] = [
                    s for s in self.statistics['bitrate_samples']
                    if current_time - s['timestamp'] <= 10
                ]
                
        except Exception as e:
            logger.error(f"Error analyzing RTP packet: {e}")
    
    def get_metrics(self) -> Dict:
        """Get current network metrics"""
        try:
            metrics = {
                'total_packets': self.statistics['total_packets'],
                'video_packets': self.statistics['video_packets'],
                'rtp_packets': self.statistics['rtp_packets'],
                'lost_packets': self.statistics['lost_packets'],
                'jitter': self._calculate_jitter(),
                'delay': self._calculate_delay(),
                'latency': self._calculate_latency(),
                'bitrate': self._calculate_bitrate(),
                'packet_loss': self._calculate_packet_loss(),
                'timestamp': time.time()
            }
            
            return metrics
            
        except Exception as e:
            logger.error(f"Error calculating metrics: {e}")
            return {}
    
    def _calculate_jitter(self) -> float:
        """Calculate network jitter"""
        if len(self.statistics['jitter_samples']) < 2:
            return 0.0
        
        jitter_values = [s['jitter'] for s in self.statistics['jitter_samples']]
        return sum(jitter_values) / len(jitter_values)
    
    def _calculate_delay(self) -> float:
        """Calculate network delay"""
        if not self.statistics['jitter_samples']:
            return 0.0
        
        # Simple delay estimation based on packet timing
        recent_samples = self.statistics['jitter_samples'][-10:]
        if len(recent_samples) >= 2:
            time_diff = recent_samples[-1]['timestamp'] - recent_samples[0]['timestamp']
            return (time_diff * 1000) / len(recent_samples)
        
        return 0.0
    
    def _calculate_latency(self) -> float:
        """Calculate network latency"""
        jitter = self._calculate_jitter()
        delay = self._calculate_delay()
        return (jitter + delay) / 2
    
    def _calculate_bitrate(self) -> float:
        """Calculate current bitrate in Mbps"""
        if len(self.statistics['bitrate_samples']) < 2:
            return 0.0
        
        # Calculate bitrate over last 5 seconds
        current_time = time.time()
        recent_samples = [
            s for s in self.statistics['bitrate_samples']
            if current_time - s['timestamp'] <= 5
        ]
        
        if len(recent_samples) >= 2:
            total_bytes = sum(s['bytes'] for s in recent_samples)
            time_span = recent_samples[-1]['timestamp'] - recent_samples[0]['timestamp']
            
            if time_span > 0:
                bitrate = (total_bytes * 8) / time_span  # bits per second
                return bitrate / 1_000_000  # Convert to Mbps
        
        return 0.0
    
    def _calculate_packet_loss(self) -> float:
        """Calculate packet loss percentage"""
        if len(self.statistics['jitter_samples']) < 2:
            return 0.0
        
        # Analyze sequence numbers for gaps
        seq_numbers = [s['seq_num'] for s in self.statistics['jitter_samples']]
        seq_numbers.sort()
        
        expected_packets = seq_numbers[-1] - seq_numbers[0] + 1
        received_packets = len(seq_numbers)
        
        if expected_packets > received_packets:
            lost_packets = expected_packets - received_packets
            self.statistics['lost_packets'] = lost_packets
            return (lost_packets / expected_packets) * 100
        
        return 0.0
    
    def get_status(self) -> Dict:
        """Get capture status"""
        return {
            'running': self.running,
            'interface': self.interface,
            'capture_filter': self.capture_filter,
            'process_alive': self.capture_process is not None and self.capture_process.poll() is None,
            'uptime': time.time() - self.statistics['start_time'] if self.statistics['start_time'] else 0
        }

def main():
    """Main function for standalone execution"""
    logger.info("Starting ESP32-S3 CCTV Monitor - Packet Capture")
    
    # Configuration
    interface = os.getenv('CAPTURE_INTERFACE', 'wlan0')
    capture_filter = os.getenv('CAPTURE_FILTER', '')
    output_file = os.getenv('OUTPUT_FILE', 'metrics.json')
    
    # Create packet capture instance
    capture = PacketCapture(interface, capture_filter)
    
    # Signal handlers
    def signal_handler(signum, frame):
        logger.info("Received signal, stopping capture...")
        capture.stop_capture()
        sys.exit(0)
    
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    # Start capture
    if not capture.start_capture():
        logger.error("Failed to start packet capture")
        sys.exit(1)
    
    try:
        # Main monitoring loop
        while capture.running:
            time.sleep(5)  # Update every 5 seconds
            
            # Get current metrics
            metrics = capture.get_metrics()
            status = capture.get_status()
            
            # Log metrics
            logger.info(f"Metrics: {metrics}")
            logger.info(f"Status: {status}")
            
            # Save metrics to file
            with open(output_file, 'w') as f:
                json.dump({
                    'metrics': metrics,
                    'status': status,
                    'timestamp': datetime.now().isoformat()
                }, f, indent=2)
    
    except KeyboardInterrupt:
        logger.info("Received keyboard interrupt")
    finally:
        capture.stop_capture()
        logger.info("Packet capture stopped")

if __name__ == "__main__":
    main()
