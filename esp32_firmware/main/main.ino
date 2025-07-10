#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "config_fixed.h"  // Use the fixed config instead of config.h
#include "network_monitor.h"
#include "web_server.h"
#include "packet_analyzer.h"

// Global objects
WebServer server(80);
WebSocketsServer webSocket(81);
NetworkMonitor netMonitor;
PacketAnalyzer packetAnalyzer;
CustomWebServer webServerHandler;

// Task handles for FreeRTOS
TaskHandle_t networkMonitorTaskHandle = nullptr;
TaskHandle_t packetAnalysisTaskHandle = nullptr;
TaskHandle_t webServerTaskHandle = nullptr;
TaskHandle_t metricsUpdateTaskHandle = nullptr;

// Synchronization primitives
SemaphoreHandle_t metricsMutex = nullptr;

// Global variables for metrics (non-volatile to avoid ArduinoJson binding issues)
struct VideoMetrics {
    float jitter;
    float delay;
    float latency;
    float bitrate;
    float packetLoss;
    unsigned long timestamp;
};

VideoMetrics currentMetrics = {0, 0, 0, 0, 0, 0};

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("ESP32-S3 CCTV Monitor Starting...");
    Serial.printf("Free heap at startup: %lu bytes\n", ESP.getFreeHeap());
    
    // Initialize SPIFFS for web files
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    
    // Create synchronization primitives
    metricsMutex = xSemaphoreCreateMutex();
    if (metricsMutex == nullptr) {
        Serial.println("Failed to create metrics mutex");
        return;
    }
    
    // Initialize WiFi with WiFiManager for secure provisioning
    WiFiManager wifiManager;
    
    // Uncomment to reset WiFi settings for testing
    // wifiManager.resetSettings();
    
    // Auto-connect or start config portal
    if (!wifiManager.autoConnect("ESP32-CCTV-Setup")) {
        Serial.println("Failed to connect to WiFi");
        delay(3000);
        ESP.restart();
    }
    
    Serial.println("WiFi connected successfully");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Initialize network monitor
    if (!netMonitor.begin()) {
        Serial.println("Failed to initialize network monitor");
    }
    
    // Initialize packet analyzer
    if (!packetAnalyzer.begin()) {
        Serial.println("Failed to initialize packet analyzer");
    }
    
    // Initialize web server
    if (!webServerHandler.begin(&server, &webSocket)) {
        Serial.println("Failed to initialize web server");
    }
    
    // Setup WebSocket event handler
    webSocket.onEvent(webSocketEvent);
    webSocket.begin();
    
    // Start HTTP server
    server.begin();
    
    // Create system tasks
    if (!createSystemTasks()) {
        Serial.println("Failed to create system tasks");
        return;
    }
    
    Serial.println("CCTV Monitor System Ready");
    Serial.println("Web Interface: http://" + WiFi.localIP().toString());
    Serial.printf("Free heap after initialization: %lu bytes\n", ESP.getFreeHeap());
}

void loop() {
    // Main loop handles basic web server functionality
    server.handleClient();
    webSocket.loop();
    
    // Check system health
    checkSystemHealth();
    
    // Small delay to prevent watchdog issues
    delay(50);
}

bool createSystemTasks() {
    // Create network monitoring task
    if (xTaskCreatePinnedToCore(
        networkMonitorTask,
        "NetworkMonitor",
        NETWORK_MONITOR_STACK_SIZE,
        nullptr,
        NETWORK_MONITOR_TASK_PRIORITY,
        &networkMonitorTaskHandle,
        0  // Core 0
    ) != pdPASS) {
        Serial.println("Failed to create network monitor task");
        return false;
    }
    
    // Create packet analysis task
    if (xTaskCreatePinnedToCore(
        packetAnalysisTask,
        "PacketAnalysis", 
        PACKET_ANALYSIS_STACK_SIZE,
        nullptr,
        PACKET_ANALYSIS_TASK_PRIORITY,
        &packetAnalysisTaskHandle,
        1  // Core 1
    ) != pdPASS) {
        Serial.println("Failed to create packet analysis task");
        return false;
    }
    
    // Create web server task
    if (xTaskCreatePinnedToCore(
        webServerTask,
        "WebServer",
        WEB_SERVER_STACK_SIZE,
        nullptr,
        WEB_SERVER_TASK_PRIORITY,
        &webServerTaskHandle,
        0  // Core 0
    ) != pdPASS) {
        Serial.println("Failed to create web server task");
        return false;
    }
    
    // Create metrics update task
    if (xTaskCreatePinnedToCore(
        metricsUpdateTask,
        "MetricsUpdate",
        METRICS_UPDATE_STACK_SIZE,
        nullptr,
        METRICS_UPDATE_TASK_PRIORITY,
        &metricsUpdateTaskHandle,
        1  // Core 1
    ) != pdPASS) {
        Serial.println("Failed to create metrics update task");
        return false;
    }
    
    Serial.println("All system tasks created successfully");
    return true;
}

void networkMonitorTask(void* parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 100ms
    
    while (true) {
        netMonitor.update();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void packetAnalysisTask(void* parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(50); // 50ms
    
    while (true) {
        // Perform packet analysis
        if (packetAnalyzer.hasNewData()) {
            // Check memory health before intensive operations
            if (!MemoryManager::checkHeapHealth()) {
                Serial.println("Warning: Low memory in packet analysis task");
                vTaskDelay(pdMS_TO_TICKS(1000)); // Wait a bit for memory to free up
                continue;
            }
            
            PacketAnalyzer::Metrics metrics = packetAnalyzer.getMetrics();
            
            // Update global metrics with mutex protection
            if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                currentMetrics.jitter = metrics.jitter;
                currentMetrics.delay = metrics.delay;
                currentMetrics.latency = metrics.latency;
                currentMetrics.bitrate = metrics.bitrate;
                currentMetrics.packetLoss = metrics.packetLoss;
                currentMetrics.timestamp = millis();
                xSemaphoreGive(metricsMutex);
            }
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void webServerTask(void* parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 100ms
    
    uint32_t lastPing = 0;
    
    while (true) {
        // Handle WebSocket events
        webSocket.loop();
        
        // Send periodic ping to keep connections alive
        uint32_t currentTime = millis();
        if (currentTime - lastPing > WEBSOCKET_PING_INTERVAL) {
            // Send ping to all connected clients
            webSocket.pingAll();
            lastPing = currentTime;
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void metricsUpdateTask(void* parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(METRICS_UPDATE_INTERVAL);
    
    while (true) {
        // Send metrics to all connected WebSocket clients
        sendMetricsToClients();
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
            
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", 
                             num, ip[0], ip[1], ip[2], ip[3], payload);
                
                // Send initial metrics to newly connected client
                sendMetricsToClient(num);
            }
            break;
            
        case WStype_TEXT:
            Serial.printf("[%u] Received Text: %s\n", num, payload);
            
            // Handle client requests
            if (strcmp((char*)payload, "get_metrics") == 0) {
                sendMetricsToClient(num);
            } else if (strncmp((char*)payload, "{", 1) == 0) {
                // Handle JSON commands
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, payload);
                
                if (!error) {
                    const char* command = doc["command"];
                    if (command && strcmp(command, "reset_metrics") == 0) {
                        packetAnalyzer.reset();
                        Serial.printf("[%u] Metrics reset requested\n", num);
                    }
                }
            }
            break;
            
        case WStype_ERROR:
            Serial.printf("[%u] WebSocket Error\n", num);
            break;
            
        default:
            break;
    }
}

void sendMetricsToClients() {
    if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        JsonDocument doc;
        doc["jitter"] = currentMetrics.jitter;
        doc["delay"] = currentMetrics.delay;
        doc["latency"] = currentMetrics.latency;
        doc["bitrate"] = currentMetrics.bitrate;
        doc["packetLoss"] = currentMetrics.packetLoss;
        doc["timestamp"] = currentMetrics.timestamp;
        
        String jsonString;
        serializeJson(doc, jsonString);
        
        webSocket.broadcastTXT(jsonString);
        xSemaphoreGive(metricsMutex);
    }
}

void sendMetricsToClient(uint8_t clientNum) {
    if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        JsonDocument doc;
        doc["jitter"] = currentMetrics.jitter;
        doc["delay"] = currentMetrics.delay;
        doc["latency"] = currentMetrics.latency;
        doc["bitrate"] = currentMetrics.bitrate;
        doc["packetLoss"] = currentMetrics.packetLoss;
        doc["timestamp"] = currentMetrics.timestamp;
        
        String jsonString;
        serializeJson(doc, jsonString);
        
        webSocket.sendTXT(clientNum, jsonString);
        xSemaphoreGive(metricsMutex);
    }
}

void checkSystemHealth() {
    static uint32_t lastHealthCheck = 0;
    uint32_t currentTime = millis();
    
    if (currentTime - lastHealthCheck > MEMORY_CHECK_INTERVAL) {
        uint32_t freeHeap = ESP.getFreeHeap();
        
        if (freeHeap < HEAP_CRITICAL_THRESHOLD) {
            Serial.printf("CRITICAL: Low memory! Free heap: %lu bytes\n", freeHeap);
        } else if (freeHeap < HEAP_WARNING_THRESHOLD) {
            Serial.printf("WARNING: Low memory! Free heap: %lu bytes\n", freeHeap);
        }
        
        lastHealthCheck = currentTime;
    }
}
