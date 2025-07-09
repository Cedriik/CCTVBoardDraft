#!/usr/bin/env python3
"""
ESP32-S3 CCTV Monitor - Enhanced Python Packet Capture Module
Advanced packet analysis using Wireshark/tshark integration with comprehensive error handling
"""

import os
import sys
import time
import json
import subprocess
import threading
import queue
import signal
import tempfile
import shutil
from datetime import datetime
from typing import Dict, List, Optional, Tuple, Union
import logging
from pathlib import Path
import psutil
import hashlib
import fcntl

# Configure enhanced logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('/tmp/packet_capture.log'),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)

class DependencyChecker:
    """Check and validate required dependencies"""
    
    @staticmethod
    def check_tshark() -> bool:
        """Check if tshark is available and accessible"""
        try:
            result = subprocess.run(['tshark', '-v'], 
                                  capture_output=True, 
                                  text=True, 
                                  timeout=10)
            if result.returncode == 0:
                logger.info("tshark is available")
                return True
            else:
                logger.error(f"tshark command failed: {result.stderr}")
                return False
        except subprocess.TimeoutExpired:
            logger.error("tshark command timed out")
            return False
        except FileNotFoundError:
            logger.error("tshark not found - please install Wireshark")
            return False
        except Exception as e:
            logger.error(f"Error checking tshark: {e}")
            return False
    
    @staticmethod
    def check_permissions() -> bool:
        """Check if we have necessary permissions for packet capture"""
        try:
            # Check if running as root or with CAP_NET_RAW capability
            if os.geteuid() == 0:
                logger.info("Running as root - packet capture permissions OK")
                return True
            
            # Check for dumpcap permissions (alternative to root)
            try:
                result = subprocess.run(['dumpcap', '-D'], 
                                      capture_output=True, 
                                      text=True, 
                                      timeout=5)
                if result.returncode == 0:
                    logger.info("dumpcap permissions OK")
                    return True
            except:
                pass
            
            logger.warning("Insufficient permissions for packet capture")
            return False
        except Exception as e:
            logger.error(f"Error checking permissions: {e}")
            return False
    
    @staticmethod
    def check_system_resources() -> bool:
        """Check if system has sufficient resources"""
        try:
            memory = psutil.virtual_memory()
            disk = psutil.disk_usage('/')
            
            # Check available memory (need at least 100MB)
            if memory.available < 100 * 1024 * 1024:
                logger.error(f"Insufficient memory: {memory.available // 1024 // 1024}MB available")
                return False
            
            # Check available disk space (need at least 1GB)
            if disk.free < 1024 * 1024 * 1024:
                logger.error(f"Insufficient disk space: {disk.free // 1024 // 1024 // 1024}GB available")
                return False
            
            logger.info("System resources OK")
            return True
        except Exception as e:
            logger.error(f"Error checking system resources: {e}")
            return False

class SecureFileManager:
    """Secure file operations for temporary files"""
    
    def __init__(self):
        self.temp_dir = tempfile.mkdtemp(prefix='esp32_cctv_')
        os.chmod(self.temp_dir, 0o700)  # Only owner can access
        self.created_files = set()
    
    def create_temp_file(self, suffix: str = '') -> str:
        """Create a secure temporary file"""
        try:
            fd, filepath = tempfile.mkstemp(suffix=suffix, dir=self.temp_dir)
            os.close(fd)
            os.chmod(filepath, 0o600)  # Only owner can read/write
            self.created_files.add(filepath)
            return filepath
        except Exception as e:
            logger.error(f"Failed to create temp file: {e}")
            raise
    
    def cleanup(self):
        """Clean up all temporary files"""
        try:
            for filepath in self.created_files:
                if os.path.exists(filepath):
                    os.remove(filepath)
            shutil.rmtree(self.temp_dir, ignore_errors=True)
            logger.info("Temporary files cleaned up")
        except Exception as e:
            logger.error(f"Error during cleanup: {e}")

class StreamingJSONParser:
    """Memory-efficient streaming JSON parser for large tshark outputs"""
    
    def __init__(self, max_buffer_size: int = 1024 * 1024):  # 1MB default
        self.max_buffer_size = max_buffer_size
        self.buffer = ""
        self.brace_count = 0
        self.in_string = False
        self.escape_next = False
    
    def feed(self, data: str) -> List[Dict]:
        """Feed data and return complete JSON objects"""
        self.buffer += data
        objects = []
        
        # Prevent buffer from growing too large
        if len(self.buffer) > self.max_buffer_size:
            logger.warning("JSON buffer too large, truncating")
            self.buffer = self.buffer[-self.max_buffer_size//2:]
            self.brace_count = 0
            self.in_string = False
            self.escape_next = False
        
        i = 0
        start_pos = 0
        
        while i < len(self.buffer):
            char = self.buffer[i]
            
            if self.escape_next:
                self.escape_next = False
            elif char == '\\':
                self.escape_next = True
            elif char == '"' and not self.escape_next:
                self.in_string = not self.in_string
            elif not self.in_string:
                if char == '{':
                    if self.brace_count == 0:
                        start_pos = i
                    self.brace_count += 1
                elif char == '}':
                    self.brace_count -= 1
                    if self.brace_count == 0:
                        # Complete JSON object found
                        json_str = self.buffer[start_pos:i+1]
                        try:
                            obj = json.loads(json_str)
                            objects.append(obj)
                        except json.JSONDecodeError as e:
                            logger.debug(f"JSON parse error: {e}")
                        
                        # Remove processed data from buffer
                        self.buffer = self.buffer[i+1:]
                        i = -1  # Reset counter
            
            i += 1
        
        return objects

class EnhancedPacketCapture:
    """
    Enhanced packet capture and analysis with comprehensive error handling
    """
    
    def __init__(self, interface: str = "wlan0", capture_filter: str = ""):
        self.interface = interface
        self.capture_filter = capture_filter
        self.capture_process = None
        self.analysis_thread = None
        self.packet_queue = queue.Queue(maxsize=10000)  # Limit queue size
        self.running = False
        self.statistics = {
            'total_packets': 0,
            'video_packets': 0,
            'rtp_packets': 0,
            'lost_packets': 0,
            'jitter_samples': [],
            'delay_samples': [],
            'bitrate_samples': [],
            'start_time': None,
            'errors': 0
        }
        
        # Enhanced security and resource management
        self.file_manager = SecureFileManager()
        self.json_parser = StreamingJSONParser()
        self.dependency_checker = DependencyChecker()
        
        # Video analysis parameters
        self.video_ports = [554, 8000, 8080, 1935]  # Common video streaming ports
        self.rtp_payload_types = [96, 97, 98, 99, 26]  # Common video payload types
        
        # Enhanced metrics thresholds
        self.jitter_threshold = 50.0  # ms
        self.delay_threshold = 200.0  # ms
        self.packet_loss_threshold = 1.0  # %
        
        # Performance monitoring
        self.performance_stats = {
            'packets_per_second': 0,
            'memory_usage': 0,
            'cpu_usage': 0
        }
    
    def validate_dependencies(self) -> bool:
        """Validate all dependencies before starting"""
        logger.info("Validating dependencies...")
        
        checks = [
            ("tshark availability", self.dependency_checker.check_tshark),
            ("system permissions", self.dependency_checker.check_permissions),
            ("system resources", self.dependency_checker.check_system_resources)
        ]
        
        for check_name, check_func in checks:
            if not check_func():
                logger.error(f"Dependency check failed: {check_name}")
                return False
        
        logger.info("All dependencies validated successfully")
        return True
    
    def start_capture(self) -> bool:
        """Start packet capture with comprehensive error handling"""
        try:
            # Validate dependencies first
            if not self.validate_dependencies():
                return False
            
            # Build tshark command with security considerations
            cmd = self._build_tshark_command()
            
            logger.info(f"Starting packet capture on interface {self.interface}")
            logger.info(f"Command: {' '.join(cmd)}")
            
            # Start tshark process with security settings
            self.capture_process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                bufsize=1,
                preexec_fn=self._setup_process_security  # Security hardening
            )
            
            # Set non-blocking mode for stdout
            fd = self.capture_process.stdout.fileno()
            flags = fcntl.fcntl(fd, fcntl.F_GETFL)
            fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)
            
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
            self.statistics['errors'] += 1
            return False
    
    def _build_tshark_command(self) -> List[str]:
        """Build secure tshark command"""
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
            '-Y', 'udp',  # Only capture UDP packets
            '-a', 'duration:3600',  # Auto-stop after 1 hour for safety
            '-b', 'filesize:100000'  # Limit file size to 100MB
        ]
        
        if self.capture_filter:
            # Sanitize capture filter to prevent injection
            sanitized_filter = self._sanitize_capture_filter(self.capture_filter)
            cmd.extend(['-f', sanitized_filter])
        
        return cmd
    
    def _sanitize_capture_filter(self, filter_str: str) -> str:
        """Sanitize capture filter to prevent command injection"""
        # Allow only alphanumeric, spaces, and common filter operators
        allowed_chars = set('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .-()[]{}!&|=<>')
        sanitized = ''.join(c for c in filter_str if c in allowed_chars)
        
        if sanitized != filter_str:
            logger.warning(f"Capture filter sanitized: '{filter_str}' -> '{sanitized}'")
        
        return sanitized
    
    def _setup_process_security(self):
        """Setup security restrictions for child process"""
        try:
            # Set process group to enable clean termination
            os.setpgrp()
            
            # Limit resources
            import resource
            resource.setrlimit(resource.RLIMIT_AS, (512 * 1024 * 1024, 512 * 1024 * 1024))  # 512MB memory limit
            resource.setrlimit(resource.RLIMIT_CPU, (3600, 3600))  # 1 hour CPU limit
        except Exception as e:
            logger.warning(f"Failed to set process security limits: {e}")
    
    def stop_capture(self):
        """Stop packet capture with proper cleanup"""
        logger.info("Stopping packet capture...")
        
        self.running = False
        
        # Terminate capture process
        if self.capture_process:
            try:
                self.capture_process.terminate()
                self.capture_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                logger.warning("Process didn't terminate gracefully, killing...")
                self.capture_process.kill()
                self.capture_process.wait()
            except Exception as e:
                logger.error(f"Error stopping capture process: {e}")
            finally:
                self.capture_process = None
        
        # Wait for analysis thread
        if self.analysis_thread:
            self.analysis_thread.join(timeout=5)
            if self.analysis_thread.is_alive():
                logger.warning("Analysis thread didn't stop gracefully")
            self.analysis_thread = None
        
        # Clean up temporary files
        self.file_manager.cleanup()
        
        logger.info("Packet capture stopped")
    
    def _analyze_packets(self):
        """Analyze packets with enhanced error handling"""
        logger.info("Starting packet analysis thread")
        
        buffer = ""
        last_performance_check = time.time()
        packet_count_since_check = 0
        
        while self.running and self.capture_process:
            try:
                # Check if process is still running
                if self.capture_process.poll() is not None:
                    logger.warning("Capture process terminated unexpectedly")
                    break
                
                # Read output with timeout
                try:
                    line = self.capture_process.stdout.readline()
                    if line:
                        buffer += line
                        
                        # Process complete JSON objects
                        objects = self.json_parser.feed(buffer)
                        buffer = ""  # Clear buffer after processing
                        
                        for packet in objects:
                            try:
                                self._process_packet(packet)
                                packet_count_since_check += 1
                            except Exception as e:
                                logger.debug(f"Error processing packet: {e}")
                                self.statistics['errors'] += 1
                    else:
                        time.sleep(0.01)  # Short sleep to prevent busy waiting
                except OSError:
                    # Non-blocking read, no data available
                    time.sleep(0.01)
                
                # Performance monitoring
                current_time = time.time()
                if current_time - last_performance_check >= 1.0:
                    self._update_performance_stats(packet_count_since_check, current_time - last_performance_check)
                    packet_count_since_check = 0
                    last_performance_check = current_time
                    
            except Exception as e:
                logger.error(f"Error in packet analysis: {e}")
                self.statistics['errors'] += 1
                time.sleep(0.1)  # Brief pause before retrying
        
        logger.info("Packet analysis thread stopped")
    
    def _update_performance_stats(self, packet_count: int, time_elapsed: float):
        """Update performance statistics"""
        try:
            self.performance_stats['packets_per_second'] = packet_count / time_elapsed
            
            # Get current process stats
            process = psutil.Process()
            self.performance_stats['memory_usage'] = process.memory_info().rss / 1024 / 1024  # MB
            self.performance_stats['cpu_usage'] = process.cpu_percent()
            
            # Log performance warnings
            if self.performance_stats['memory_usage'] > 500:  # 500MB
                logger.warning(f"High memory usage: {self.performance_stats['memory_usage']:.1f}MB")
            
            if self.performance_stats['cpu_usage'] > 80:  # 80%
                logger.warning(f"High CPU usage: {self.performance_stats['cpu_usage']:.1f}%")
                
        except Exception as e:
            logger.debug(f"Error updating performance stats: {e}")
    
    def _process_packet(self, packet: Dict):
        """Process individual packet with enhanced validation"""
        try:
            layers = packet.get('_source', {}).get('layers', {})
            
            # Validate packet structure
            if not layers:
                return
            
            # Extract frame information with validation
            frame_info = layers.get('frame', {})
            try:
                frame_number = int(frame_info.get('frame.number', ['0'])[0])
                frame_time = float(frame_info.get('frame.time_relative', ['0'])[0])
                frame_len = int(frame_info.get('frame.len', ['0'])[0])
            except (ValueError, IndexError, TypeError):
                logger.debug("Invalid frame information, skipping packet")
                return
            
            # Validate frame data
            if frame_len <= 0 or frame_len > 65535:  # Invalid frame size
                return
            
            # Extract IP information with validation
            ip_info = layers.get('ip', {})
            src_ip = ip_info.get('ip.src', [''])[0] if ip_info else ''
            dst_ip = ip_info.get('ip.dst', [''])[0] if ip_info else ''
            
            # Extract UDP information with validation
            udp_info = layers.get('udp', {})
            try:
                src_port = int(udp_info.get('udp.srcport', ['0'])[0]) if udp_info else 0
                dst_port = int(udp_info.get('udp.dstport', ['0'])[0]) if udp_info else 0
            except (ValueError, IndexError, TypeError):
                src_port = dst_port = 0
            
            # Update statistics
            self.statistics['total_packets'] += 1
            
            # Check if this is a video packet
            if self._is_video_packet(src_port, dst_port):
                self.statistics['video_packets'] += 1
                
                # Check for RTP with validation
                rtp_info = layers.get('rtp', {})
                if rtp_info:
                    self.statistics['rtp_packets'] += 1
                    self._analyze_rtp_packet(rtp_info, frame_time, frame_len)
            
            # Add to queue with size limit
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
            
            try:
                self.packet_queue.put_nowait(packet_data)
            except queue.Full:
                # Queue is full, remove oldest item and add new one
                try:
                    self.packet_queue.get_nowait()
                    self.packet_queue.put_nowait(packet_data)
                except queue.Empty:
                    pass
            
        except Exception as e:
            logger.debug(f"Error processing packet: {e}")
            self.statistics['errors'] += 1
    
    def _is_video_packet(self, src_port: int, dst_port: int) -> bool:
        """Check if packet is likely a video packet with validation"""
        try:
            return (src_port in self.video_ports or 
                    dst_port in self.video_ports or
                    (16384 <= src_port <= 32767) or  # Common RTP port range
                    (16384 <= dst_port <= 32767))
        except:
            return False
    
    def _analyze_rtp_packet(self, rtp_info: Dict, timestamp: float, length: int):
        """Analyze RTP packet with enhanced validation"""
        try:
            # Validate and extract RTP information
            seq_num = int(rtp_info.get('rtp.seq', ['0'])[0])
            rtp_timestamp = int(rtp_info.get('rtp.timestamp', ['0'])[0])
            payload_type = int(rtp_info.get('rtp.p_type', ['0'])[0])
            
            # Validate values
            if not (0 <= seq_num <= 65535):
                return
            if not (0 <= payload_type <= 127):
                return
            
            # Check if this is a video payload type
            if payload_type in self.rtp_payload_types:
                # Calculate jitter with validation
                self._calculate_jitter(timestamp, seq_num)
                
                # Calculate bitrate with validation
                self._calculate_bitrate(timestamp, length)
                
        except Exception as e:
            logger.debug(f"Error analyzing RTP packet: {e}")
            self.statistics['errors'] += 1
    
    def _calculate_jitter(self, timestamp: float, seq_num: int):
        """Calculate jitter with enhanced validation"""
        try:
            if len(self.statistics['jitter_samples']) > 0:
                last_sample = self.statistics['jitter_samples'][-1]
                time_diff = (timestamp - last_sample['timestamp']) * 1000  # Convert to ms
                
                # Validate time difference
                if 0 < time_diff < 1000:  # Reasonable time difference (0-1000ms)
                    jitter_sample = {
                        'timestamp': timestamp,
                        'jitter': time_diff,
                        'seq_num': seq_num
                    }
                    
                    self.statistics['jitter_samples'].append(jitter_sample)
                    
                    # Keep only recent samples (limit memory usage)
                    if len(self.statistics['jitter_samples']) > 1000:
                        self.statistics['jitter_samples'] = self.statistics['jitter_samples'][-500:]
            else:
                # First sample
                self.statistics['jitter_samples'].append({
                    'timestamp': timestamp,
                    'jitter': 0,
                    'seq_num': seq_num
                })
                
        except Exception as e:
            logger.debug(f"Error calculating jitter: {e}")
    
    def _calculate_bitrate(self, timestamp: float, length: int):
        """Calculate bitrate with enhanced validation"""
        try:
            current_time = time.time()
            
            bitrate_sample = {
                'timestamp': timestamp,
                'bytes': length,
                'real_time': current_time
            }
            
            self.statistics['bitrate_samples'].append(bitrate_sample)
            
            # Keep only recent samples (last 30 seconds)
            cutoff_time = current_time - 30
            self.statistics['bitrate_samples'] = [
                s for s in self.statistics['bitrate_samples']
                if s['real_time'] >= cutoff_time
            ]
            
        except Exception as e:
            logger.debug(f"Error calculating bitrate: {e}")
    
    def get_metrics(self) -> Dict:
        """Get current network metrics with enhanced error handling"""
        try:
            metrics = {
                'total_packets': self.statistics['total_packets'],
                'video_packets': self.statistics['video_packets'],
                'rtp_packets': self.statistics['rtp_packets'],
                'lost_packets': self.statistics['lost_packets'],
                'jitter': self._calculate_jitter_metric(),
                'delay': self._calculate_delay_metric(),
                'latency': self._calculate_latency_metric(),
                'bitrate': self._calculate_bitrate_metric(),
                'packet_loss': self._calculate_packet_loss_metric(),
                'timestamp': time.time(),
                'errors': self.statistics['errors'],
                'performance': self.performance_stats.copy()
            }
            
            return metrics
            
        except Exception as e:
            logger.error(f"Error calculating metrics: {e}")
            return {
                'error': str(e),
                'timestamp': time.time()
            }
    
    def _calculate_jitter_metric(self) -> float:
        """Calculate network jitter metric with validation"""
        try:
            if len(self.statistics['jitter_samples']) < 2:
                return 0.0
            
            jitter_values = [s['jitter'] for s in self.statistics['jitter_samples'][-100:]]  # Last 100 samples
            return sum(jitter_values) / len(jitter_values) if jitter_values else 0.0
        except:
            return 0.0
    
    def _calculate_delay_metric(self) -> float:
        """Calculate network delay metric with validation"""
        try:
            if not self.statistics['jitter_samples']:
                return 0.0
            
            # Simple delay estimation based on packet timing
            recent_samples = self.statistics['jitter_samples'][-10:]
            if len(recent_samples) >= 2:
                time_diff = recent_samples[-1]['timestamp'] - recent_samples[0]['timestamp']
                return (time_diff * 1000) / len(recent_samples) if time_diff > 0 else 0.0
            
            return 0.0
        except:
            return 0.0
    
    def _calculate_latency_metric(self) -> float:
        """Calculate network latency metric"""
        try:
            jitter = self._calculate_jitter_metric()
            delay = self._calculate_delay_metric()
            return (jitter + delay) / 2
        except:
            return 0.0
    
    def _calculate_bitrate_metric(self) -> float:
        """Calculate current bitrate in Mbps with validation"""
        try:
            if len(self.statistics['bitrate_samples']) < 2:
                return 0.0
            
            # Calculate bitrate over last 10 seconds
            current_time = time.time()
            recent_samples = [
                s for s in self.statistics['bitrate_samples']
                if current_time - s['real_time'] <= 10
            ]
            
            if len(recent_samples) >= 2:
                total_bytes = sum(s['bytes'] for s in recent_samples)
                time_span = recent_samples[-1]['real_time'] - recent_samples[0]['real_time']
                
                if time_span > 0:
                    bitrate = (total_bytes * 8) / time_span  # bits per second
                    return bitrate / 1_000_000  # Convert to Mbps
            
            return 0.0
        except:
            return 0.0
    
    def _calculate_packet_loss_metric(self) -> float:
        """Calculate packet loss percentage with enhanced validation"""
        try:
            if len(self.statistics['jitter_samples']) < 10:
                return 0.0
            
            # Analyze sequence numbers for gaps
            seq_numbers = [s['seq_num'] for s in self.statistics['jitter_samples'][-100:]]
            if len(seq_numbers) < 2:
                return 0.0
            
            seq_numbers.sort()
            
            # Handle sequence number wraparound
            gaps = 0
            total_expected = 0
            
            for i in range(1, len(seq_numbers)):
                diff = seq_numbers[i] - seq_numbers[i-1]
                if diff > 32768:  # Wraparound case
                    diff = 65536 - seq_numbers[i-1] + seq_numbers[i]
                
                total_expected += diff
                if diff > 1:
                    gaps += (diff - 1)
            
            if total_expected > 0:
                packet_loss = (gaps / total_expected) * 100
                self.statistics['lost_packets'] = gaps
                return min(packet_loss, 100.0)  # Cap at 100%
            
            return 0.0
        except:
            return 0.0
    
    def get_status(self) -> Dict:
        """Get capture status with enhanced information"""
        try:
            return {
                'running': self.running,
                'interface': self.interface,
                'capture_filter': self.capture_filter,
                'process_alive': self.capture_process is not None and self.capture_process.poll() is None,
                'uptime': time.time() - self.statistics['start_time'] if self.statistics['start_time'] else 0,
                'queue_size': self.packet_queue.qsize(),
                'errors': self.statistics['errors'],
                'performance': self.performance_stats.copy()
            }
        except Exception as e:
            logger.error(f"Error getting status: {e}")
            return {'error': str(e)}

def main():
    """Main function for standalone execution with enhanced error handling"""
    logger.info("Starting Enhanced ESP32-S3 CCTV Monitor - Packet Capture")
    
    # Configuration with validation
    interface = os.getenv('CAPTURE_INTERFACE', 'wlan0')
    capture_filter = os.getenv('CAPTURE_FILTER', '')
    output_file = os.getenv('OUTPUT_FILE', '/tmp/metrics.json')
    
    # Validate configuration
    if not interface:
        logger.error("No capture interface specified")
        sys.exit(1)
    
    # Create packet capture instance
    try:
        capture = EnhancedPacketCapture(interface, capture_filter)
    except Exception as e:
        logger.error(f"Failed to create packet capture instance: {e}")
        sys.exit(1)
    
    # Signal handlers for graceful shutdown
    def signal_handler(signum, frame):
        logger.info(f"Received signal {signum}, stopping capture...")
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
            
            # Get current metrics and status
            metrics = capture.get_metrics()
            status = capture.get_status()
            
            # Log metrics with rate limiting
            if metrics.get('total_packets', 0) % 100 == 0:  # Every 100 packets
                logger.info(f"Processed {metrics.get('total_packets', 0)} packets, "
                          f"Jitter: {metrics.get('jitter', 0):.2f}ms, "
                          f"Bitrate: {metrics.get('bitrate', 0):.2f}Mbps")
            
            # Save metrics to file with error handling
            try:
                with open(output_file, 'w') as f:
                    json.dump({
                        'metrics': metrics,
                        'status': status,
                        'timestamp': datetime.now().isoformat()
                    }, f, indent=2)
            except Exception as e:
                logger.error(f"Failed to save metrics: {e}")
    
    except KeyboardInterrupt:
        logger.info("Received keyboard interrupt")
    except Exception as e:
        logger.error(f"Unexpected error in main loop: {e}")
    finally:
        capture.stop_capture()
        logger.info("Enhanced packet capture stopped")

if __name__ == "__main__":
    main()