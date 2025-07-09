#ifndef CONFIG_FIXED_H
#define CONFIG_FIXED_H

#include <EEPROM.h>
#include <WiFiManager.h>

// Security: Remove hardcoded credentials - use WiFiManager for provisioning
// WiFi Configuration will be handled by WiFiManager
// Credentials stored securely in EEPROM with encryption

// Network Configuration
#define MONITOR_PORT 80
#define WEBSOCKET_PORT 81

// CCTV/DVR Configuration  
#define DVR_IP_EEPROM_ADDR 100      // Store DVR IP in EEPROM
#define DVR_PORT 554                // RTSP port
#define CCTV_STREAM_PORT 8000       // Video stream port

// RTSP Authentication (stored securely in EEPROM)
#define RTSP_USER_EEPROM_ADDR 120
#define RTSP_PASS_EEPROM_ADDR 150
#define MAX_CREDENTIAL_LENGTH 32

// Dynamic Buffer Configuration (prevents buffer overflows)
#define MIN_PACKET_BUFFER_SIZE 2048
#define MAX_PACKET_BUFFER_SIZE 8192
#define INITIAL_PACKET_BUFFER_SIZE 4096
#define BUFFER_GROWTH_FACTOR 1.5

// Circular Buffer Sizes (prevents memory leaks)
#define PACKET_HISTORY_BUFFER_SIZE 100
#define JITTER_SAMPLES_BUFFER_SIZE 50
#define RTP_TIMESTAMP_BUFFER_SIZE 50
#define ARRIVAL_TIME_BUFFER_SIZE 50

// Metrics Update Configuration
#define METRICS_UPDATE_INTERVAL 1000  // ms
#define METRICS_HISTORY_SIZE 100

// Video Quality Thresholds
#define JITTER_THRESHOLD 50.0    // ms
#define DELAY_THRESHOLD 200.0    // ms  
#define LATENCY_THRESHOLD 100.0  // ms
#define PACKET_LOSS_THRESHOLD 1.0 // percentage

// Web Server Configuration
#define WEB_SERVER_PORT 80
#define MAX_CLIENTS 5
#define CLIENT_TIMEOUT_MS 30000      // 30 seconds
#define WEBSOCKET_PING_INTERVAL 25000 // 25 seconds

// Security Configuration
#define ENABLE_AUTHENTICATION true
#define SESSION_TIMEOUT_MS 3600000   // 1 hour
#define MAX_LOGIN_ATTEMPTS 3
#define LOCKOUT_DURATION_MS 300000   // 5 minutes

// Debug Configuration
#define DEBUG_SERIAL true
#define DEBUG_NETWORK true  
#define DEBUG_PACKETS true
#define DEBUG_MEMORY true

// Memory Management
#define HEAP_WARNING_THRESHOLD 10240  // 10KB
#define HEAP_CRITICAL_THRESHOLD 5120  // 5KB
#define MEMORY_CHECK_INTERVAL 5000    // 5 seconds

// Task Configuration (for FreeRTOS)
#define PACKET_ANALYSIS_TASK_PRIORITY 2
#define NETWORK_MONITOR_TASK_PRIORITY 2  
#define WEB_SERVER_TASK_PRIORITY 1
#define METRICS_UPDATE_TASK_PRIORITY 1

#define PACKET_ANALYSIS_STACK_SIZE 8192
#define NETWORK_MONITOR_STACK_SIZE 4096
#define WEB_SERVER_STACK_SIZE 8192
#define METRICS_UPDATE_STACK_SIZE 4096

// HikVision Specific Configuration
#define HIKVISION_DEFAULT_RTSP_PORT 554
#define HIKVISION_DEFAULT_HTTP_PORT 80
#define HIKVISION_KEEP_ALIVE_INTERVAL 30000  // 30 seconds
#define HIKVISION_RECONNECT_DELAY 5000       // 5 seconds

// Supported HikVision Payload Types
#define HIKVISION_H264_PT 96
#define HIKVISION_H265_PT 97  
#define HIKVISION_SMART_PT 98
#define HIKVISION_MJPEG_PT 26

// RTP Clock Rates for different codecs
#define H264_CLOCK_RATE 90000
#define H265_CLOCK_RATE 90000
#define MJPEG_CLOCK_RATE 90000
#define AUDIO_CLOCK_RATE 8000

// Network Monitoring Configuration
#define PROMISCUOUS_MODE_ENABLED false  // Disabled by default for security
#define CAPTURE_FILTER_ENABLED true
#define DEFAULT_CAPTURE_FILTER "udp and (port 554 or portrange 16384-32767)"

// Quality Scoring Weights
#define JITTER_WEIGHT 0.3
#define DELAY_WEIGHT 0.2
#define LATENCY_WEIGHT 0.2
#define BITRATE_WEIGHT 0.15
#define PACKET_LOSS_WEIGHT 0.15

// Buffer sizes for JSON operations
#define JSON_BUFFER_SIZE 2048
#define PACKET_CAPTURE_BUFFER 4096

// Utility Functions
class SecureConfig {
public:
    static bool saveCredentials(const char* ssid, const char* password);
    static bool loadCredentials(char* ssid, char* password, size_t maxLen);
    static bool saveDVRConfig(const char* ip, const char* user, const char* pass);
    static bool loadDVRConfig(char* ip, char* user, char* pass, size_t maxLen);
    static void clearAllCredentials();
    static bool isConfigValid();
private:
    static void encryptString(const char* input, char* output, size_t maxLen);
    static void decryptString(const char* input, char* output, size_t maxLen);
};

// Memory Management Functions
class MemoryManager {
public:
    static void* safeMalloc(size_t size) {
        if (ESP.getFreeHeap() < size + HEAP_CRITICAL_THRESHOLD) {
            Serial.printf("Memory allocation failed: requested %d bytes, free heap: %lu\n", 
                         size, ESP.getFreeHeap());
            return nullptr;
        }
        return malloc(size);
    }
    
    static void* safeRealloc(void* ptr, size_t newSize) {
        if (ESP.getFreeHeap() < newSize + HEAP_CRITICAL_THRESHOLD) {
            Serial.printf("Memory reallocation failed: requested %d bytes, free heap: %lu\n", 
                         newSize, ESP.getFreeHeap());
            return nullptr;
        }
        return realloc(ptr, newSize);
    }
    
    static void safeFree(void* ptr) {
        if (ptr != nullptr) {
            free(ptr);
        }
    }
    
    static bool checkHeapHealth() {
        uint32_t freeHeap = ESP.getFreeHeap();
        return freeHeap >= HEAP_CRITICAL_THRESHOLD;
    }
    
    static size_t getFreeHeap() {
        return ESP.getFreeHeap();
    }
    
    static void printMemoryStats() {
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t largestBlock = ESP.getMaxAllocHeap();
        
        Serial.printf("Memory Stats:\n");
        Serial.printf("  Free Heap: %lu bytes\n", freeHeap);
        Serial.printf("  Largest Block: %lu bytes\n", largestBlock);
        Serial.printf("  Health Status: %s\n", 
                     checkHeapHealth() ? "GOOD" : "CRITICAL");
    }
};

// Circular Buffer Template for preventing memory leaks
template<typename T, size_t SIZE>
class CircularBuffer {
private:
    T buffer[SIZE];
    size_t head;
    size_t tail;
    size_t count;
    
public:
    CircularBuffer() : head(0), tail(0), count(0) {}
    
    bool push(const T& item) {
        if (count >= SIZE) {
            // Overwrite oldest item
            tail = (tail + 1) % SIZE;
        } else {
            count++;
        }
        
        buffer[head] = item;
        head = (head + 1) % SIZE;
        return true;
    }
    
    bool pop(T& item) {
        if (count == 0) return false;
        
        item = buffer[tail];
        tail = (tail + 1) % SIZE;
        count--;
        return true;
    }
    
    size_t size() const { return count; }
    bool empty() const { return count == 0; }
    bool full() const { return count >= SIZE; }
    void clear() { head = tail = count = 0; }
    
    const T& operator[](size_t index) const {
        return buffer[(tail + index) % SIZE];
    }
    
    T& operator[](size_t index) {
        return buffer[(tail + index) % SIZE];
    }
};

#endif