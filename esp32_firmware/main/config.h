#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
const char* WIFI_SSID = "#TeamD";
const char* WIFI_PASSWORD = "@!?PayatotCArjaye1025";

// Network Configuration
#define MONITOR_PORT 80
#define WEBSOCKET_PORT 81

// CCTV/DVR Configuration
#define DVR_IP "192.168.1.100"  // Default DVR IP - adjust as needed
#define DVR_PORT 554            // RTSP port
#define CCTV_STREAM_PORT 8000   // Video stream port

// Packet Analysis Configuration
#define PACKET_BUFFER_SIZE 1024
#define METRICS_UPDATE_INTERVAL 1000  // ms
#define MAX_PACKET_HISTORY 100

// Video Quality Thresholds
#define JITTER_THRESHOLD 50.0    // ms
#define DELAY_THRESHOLD 200.0    // ms
#define LATENCY_THRESHOLD 100.0  // ms
#define PACKET_LOSS_THRESHOLD 1.0 // percentage

// Web Server Configuration
#define WEB_SERVER_PORT 80
#define MAX_CLIENTS 5

// Debug Configuration
#define DEBUG_SERIAL true
#define DEBUG_NETWORK true
#define DEBUG_PACKETS true

// Buffer sizes
#define JSON_BUFFER_SIZE 2048
#define PACKET_CAPTURE_BUFFER 4096

// WebSocket Configuration
#define WEBSOCKET_PING_INTERVAL 30000  // 30 seconds

// FreeRTOS Task Configuration
#define NETWORK_MONITOR_STACK_SIZE 4096
#define PACKET_ANALYSIS_STACK_SIZE 8192
#define WEB_SERVER_STACK_SIZE 4096
#define METRICS_UPDATE_STACK_SIZE 2048

// Task Priorities (0 = lowest, 25 = highest, avoid 24-25)
#define NETWORK_MONITOR_TASK_PRIORITY 2
#define PACKET_ANALYSIS_TASK_PRIORITY 3
#define WEB_SERVER_TASK_PRIORITY 1
#define METRICS_UPDATE_TASK_PRIORITY 1

// Memory Management
#define HEAP_WARNING_THRESHOLD 50000   // bytes
#define HEAP_CRITICAL_THRESHOLD 20000  // bytes
#define MEMORY_CHECK_INTERVAL 10000    // ms

// Packet Analysis Buffer Sizes
#define RTP_TIMESTAMP_BUFFER_SIZE 100
#define ARRIVAL_TIME_BUFFER_SIZE 100
#define PACKET_HISTORY_BUFFER_SIZE 50

// Clock rates for different codecs
#define AUDIO_CLOCK_RATE 8000
#define MJPEG_CLOCK_RATE 90000
#define H264_CLOCK_RATE 90000
#define H265_CLOCK_RATE 90000

// Simple Memory Manager class
class MemoryManager {
public:
    static bool checkHeapHealth() {
        uint32_t freeHeap = ESP.getFreeHeap();
        return freeHeap > HEAP_CRITICAL_THRESHOLD;
    }
    
    static void printMemoryStats() {
        uint32_t freeHeap = ESP.getFreeHeap();
        Serial.printf("Free Heap: %lu bytes\n", freeHeap);
    }
};

#endif
