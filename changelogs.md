# ESP32-S3 CCTV Monitor

## Overview

This is a comprehensive real-time video quality monitoring system for CCTV networks using the ESP32-S3 microcontroller. The system combines hardware-based network monitoring with advanced packet analysis capabilities and provides a web-based dashboard for real-time visualization of video stream quality metrics.

## System Architecture

### Frontend Architecture
- **Web Interface**: HTML5-based dashboard with responsive design
- **Real-time Communication**: WebSocket-based live data updates
- **Visualization**: Chart.js for real-time metrics visualization
- **Mobile-First Design**: Responsive layout supporting desktop, tablet, and mobile

### Backend Architecture
- **Microcontroller**: ESP32-S3 N16R8 for hardware-based monitoring
- **Python Analysis Engine**: Advanced packet capture and analysis using Wireshark integration
- **WebSocket Server**: Real-time data streaming to web clients
- **Packet Processing**: Deep packet inspection with custom analysis algorithms

### Data Processing Pipeline
1. **Packet Capture**: Raw network packets captured at interface level
2. **Protocol Analysis**: Deep packet inspection using Wireshark/tshark
3. **Quality Metrics Extraction**: Jitter, delay, latency, bitrate, and packet loss calculation
4. **Real-time Streaming**: Live data pushed to web dashboard via WebSocket

## Key Components

### Hardware Integration
- **ESP32-S3 N16R8 Microcontroller**: Main processing unit with WiFi capabilities
- **Network Interface**: WiFi-based packet capture and analysis
- **Memory Management**: Optimized for 16MB Flash + 8MB PSRAM configuration

### Packet Analysis Engine
- **Wireshark Integration**: Leverages tshark for professional-grade packet analysis
- **Multi-Protocol Support**: RTP, RTSP, UDP, TCP protocol analysis
- **Quality Metrics**: Advanced algorithms for video stream quality assessment
- **Statistical Analysis**: Real-time calculation of network performance metrics

### Web Dashboard
- **Interactive Interface**: Real-time monitoring with live updates
- **Metrics Visualization**: Charts for jitter, delay, latency, bitrate, and packet loss
- **Video Feed Integration**: Live video display with fullscreen support
- **Export Capabilities**: Data export functionality for reports and analysis

### Communication Layer
- **WebSocket Protocol**: Bi-directional real-time communication
- **JSON Data Format**: Structured data exchange between components
- **Connection Management**: Automatic reconnection and error handling

## Data Flow

1. **Network Monitoring**: ESP32-S3 captures network packets from CCTV streams
2. **Packet Analysis**: Python analyzer processes packets using Wireshark integration
3. **Quality Assessment**: Algorithms calculate video quality metrics
4. **Data Aggregation**: Statistics are collected and processed
5. **Real-time Streaming**: Metrics are pushed to web dashboard via WebSocket
6. **Visualization**: Dashboard displays live charts and video feed
7. **Export/Storage**: Data can be exported for further analysis

## External Dependencies

### Hardware Requirements
- ESP32-S3 N16R8 microcontroller
- WiFi network connectivity
- CCTV system with DVR/NVR access

### Software Dependencies
- **Wireshark/tshark**: Professional packet analysis toolkit
- **Python 3.x**: Analysis engine runtime
- **Chart.js**: Frontend visualization library
- **Font Awesome**: Icon library for UI elements

### Network Requirements
- Access to CCTV video streams
- WiFi network for ESP32-S3 connectivity
- Common video streaming ports (554, 8000, 8080, 1935)

## Deployment Strategy

### Development Environment
- Local development server for web interface testing
- Python virtual environment for analysis engine
- ESP32-S3 development board for hardware testing

### Production Deployment
- ESP32-S3 firmware deployment via Arduino IDE or PlatformIO
- Web interface deployment on ESP32-S3 internal web server
- Python analyzer as background service
- Network configuration for CCTV system integration

### Monitoring and Maintenance
- Real-time system health monitoring
- Automatic error recovery and reconnection
- Performance optimization for continuous operation
- Regular firmware updates and security patches

## Changelog

```
Changelog:
- July 04, 2025. Initial setup
```

## User Preferences

```
Preferred communication style: Simple, everyday language.
```
