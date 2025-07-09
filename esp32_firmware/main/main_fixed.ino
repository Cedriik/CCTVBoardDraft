#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <WiFiManager.h>
#include "config_fixed.h"
#include "network_monitor.h"
#include "web_server.h"
#include "packet_analyzer.h"

// Global objects
WebServer server(WEB_SERVER_PORT);
WebSocketsServer webSocket(WEBSOCKET_PORT);
NetworkMonitor netMonitor;
PacketAnalyzer packetAnalyzer;
CustomWebServer webServerHandler;
WiFiManager wifiManager;

// Thread-safe metrics structure
struct VideoMetrics {
    float jitter;
    float delay;
    float latency;
    float bitrate;
    float packetLoss;
    unsigned long timestamp;
} __attribute__((packed));

// Thread-safe global metrics with mutex protection
SemaphoreHandle_t metricsMutex = nullptr;
volatile VideoMetrics currentMetrics = {0, 0, 0, 0, 0, 0};

// Task handles for FreeRTOS
TaskHandle_t networkMonitorTaskHandle = nullptr;
TaskHandle_t packetAnalysisTaskHandle = nullptr;
TaskHandle_t webServerTaskHandle = nullptr;
TaskHandle_t metricsUpdateTaskHandle = nullptr;

// System status flags
volatile bool systemInitialized = false;
volatile bool wifiConnected = false;
volatile unsigned long lastHeartbeat = 0;

// Forward declarations
void networkMonitorTask(void* parameter);
void packetAnalysisTask(void* parameter);
void webServerTask(void* parameter);
void metricsUpdateTask(void* parameter);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
void sendMetricsToClients();
void sendMetricsToClient(uint8_t clientNum);
bool initializeSystem();
void handleSystemError(const char* error);
void checkSystemHealth();

void setup() {
    Serial.begin(115200);
    delay(2000); // Allow serial to stabilize
    
    Serial.println("ESP32-S3 Enhanced CCTV Monitor Starting...");
    Serial.printf("Free heap at startup: %u bytes\n", ESP.getFreeHeap());
    
    // Create mutex for thread-safe metrics access
    metricsMutex = xSemaphoreCreateMutex();
    if (!metricsMutex) {
        handleSystemError("Failed to create metrics mutex");
        return;
    }
    
    // Initialize SPIFFS for web files
    if (!SPIFFS.begin(true)) {
        handleSystemError("SPIFFS Mount Failed");
        return;
    }
    Serial.println("SPIFFS initialized successfully");
    
    // Initialize WiFi with WiFiManager for secure credential handling
    wifiManager.setAPCallback([](WiFiManager* manager) {
        Serial.println("Entered WiFi config mode");
        Serial.printf("Connect to AP: %s\n", manager->getConfigPortalSSID().c_str());
    });
    
    wifiManager.setSaveConfigCallback([]() {
        Serial.println("WiFi configuration saved");
    });
    
    // Set timeout for configuration portal
    wifiManager.setConfigPortalTimeout(300); // 5 minutes
    
    // Auto-connect or start configuration portal
    if (!wifiManager.autoConnect("ESP32-CCTV-Setup")) {
        handleSystemError("Failed to connect to WiFi");
        return;
    }
    
    wifiConnected = true;
    Serial.println("WiFi connected successfully");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Signal strength: %d dBm\n", WiFi.RSSI());
    
    // Initialize system components
    if (!initializeSystem()) {
        handleSystemError("System initialization failed");
        return;
    }
    
    // Create FreeRTOS tasks for parallel processing
    if (!createSystemTasks()) {
        handleSystemError("Failed to create system tasks");
        return;
    }
    
    systemInitialized = true;
    lastHeartbeat = millis();
    
    Serial.println("Enhanced CCTV Monitor System Ready");
    Serial.printf("Web Interface: http://%s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Free heap after initialization: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
    // Lightweight main loop - heavy lifting done by tasks
    checkSystemHealth();
    
    // Update heartbeat
    lastHeartbeat = millis();
    
    // Small delay to prevent watchdog issues
    delay(100);
}

bool initializeSystem() {
    Serial.println("Initializing system components...");
    
    // Initialize network monitor
    if (!netMonitor.begin()) {
        Serial.println("ERROR: Failed to initialize network monitor");
        return false;
    }
    Serial.println("Network monitor initialized");
    
    // Initialize packet analyzer
    if (!packetAnalyzer.begin()) {
        Serial.println("ERROR: Failed to initialize packet analyzer");
        return false;
    }
    Serial.println("Packet analyzer initialized");
    
    // Initialize web server with error handling
    if (!webServerHandler.begin(&server, &webSocket)) {
        Serial.println("ERROR: Failed to initialize web server");
        return false;
    }
    Serial.println("Web server initialized");
    
    // Setup WebSocket event handler
    webSocket.onEvent(webSocketEvent);
    webSocket.begin();
    Serial.println("WebSocket server started");
    
    // Start HTTP server
    server.begin();
    Serial.println("HTTP server started");
    
    return true;
}

bool createSystemTasks() {
    Serial.println("Creating FreeRTOS tasks...");
    
    // Create network monitoring task
    if (xTaskCreatePinnedToCore(
        networkMonitorTask,
        "NetworkMonitor",
        NETWORK_MONITOR_STACK_SIZE,
        nullptr,
        NETWORK_MONITOR_TASK_PRIORITY,
        &networkMonitorTaskHandle,
        0) != pdPASS) {
        Serial.println("ERROR: Failed to create network monitor task");
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
        1) != pdPASS) {
        Serial.println("ERROR: Failed to create packet analysis task");
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
        0) != pdPASS) {
        Serial.println("ERROR: Failed to create web server task");
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
        1) != pdPASS) {
        Serial.println("ERROR: Failed to create metrics update task");
        return false;
    }
    
    Serial.println("All tasks created successfully");
    return true;
}

void networkMonitorTask(void* parameter) {
    Serial.println("Network Monitor Task started");
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    while (true) {
        if (systemInitialized && wifiConnected) {
            // Update network monitoring (non-blocking)
            netMonitor.update();
            
            // Check WiFi connection status
            if (WiFi.status() != WL_CONNECTED) {
                wifiConnected = false;
                Serial.println("WiFi connection lost");
            }
        }
        
        // Task runs every 100ms for responsive network monitoring
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(100));
    }
}

void packetAnalysisTask(void* parameter) {
    Serial.println("Packet Analysis Task started");
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    while (true) {
        if (systemInitialized && wifiConnected) {
            // Packet analysis is handled internally by the analyzer
            // This task just ensures it's running and handles memory management
            
            // Check memory health
            if (!MemoryManager::checkHeapHealth()) {
                Serial.println("WARNING: Low memory in packet analysis task");
                // Trigger garbage collection or reduce buffer sizes
                packetAnalyzer.reset();
            }
            
            // Check for new analysis data
            if (packetAnalyzer.hasNewData()) {
                // Data is available, metrics will be updated by metrics task
            }
        }
        
        // Task runs every 50ms for real-time packet processing
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(50));
    }
}

void webServerTask(void* parameter) {
    Serial.println("Web Server Task started");
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    while (true) {
        if (systemInitialized && wifiConnected) {
            // Handle web server requests (non-blocking)
            server.handleClient();
            webSocket.loop();
            
            // Handle WebSocket maintenance
            static unsigned long lastPing = 0;
            unsigned long currentTime = millis();
            if (currentTime - lastPing > WEBSOCKET_PING_INTERVAL) {
                webSocket.ping();
                lastPing = currentTime;
            }
        }
        
        // Task runs every 10ms for responsive web interface
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(10));
    }
}

void metricsUpdateTask(void* parameter) {
    Serial.println("Metrics Update Task started");
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    while (true) {
        if (systemInitialized && wifiConnected) {
            // Get metrics from packet analyzer (thread-safe)
            if (packetAnalyzer.hasNewData()) {
                PacketAnalyzer::Metrics metrics = packetAnalyzer.getMetrics();
                
                // Update global metrics with mutex protection
                if (metricsMutex && xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100))) {
                    currentMetrics.jitter = metrics.jitter;
                    currentMetrics.delay = metrics.delay;
                    currentMetrics.latency = metrics.latency;
                    currentMetrics.bitrate = metrics.bitrate;
                    currentMetrics.packetLoss = metrics.packetLoss;
                    currentMetrics.timestamp = millis();
                    
                    xSemaphoreGive(metricsMutex);
                    
                    // Send metrics to connected WebSocket clients
                    sendMetricsToClients();
                    
                    // Debug output with rate limiting
                    #if DEBUG_PACKETS
                    static unsigned long lastDebugOutput = 0;
                    unsigned long currentTime = millis();
                    if (currentTime - lastDebugOutput > 5000) { // Every 5 seconds
                        Serial.printf("Metrics - Jitter: %.2f ms, Delay: %.2f ms, Latency: %.2f ms, Bitrate: %.2f Mbps, Loss: %.2f%%\n",
                                     currentMetrics.jitter, currentMetrics.delay, currentMetrics.latency, 
                                     currentMetrics.bitrate, currentMetrics.packetLoss);
                        lastDebugOutput = currentTime;
                    }
                    #endif
                }
            }
        }
        
        // Task runs every 1000ms for metrics updates
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(METRICS_UPDATE_INTERVAL));
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] WebSocket Disconnected\n", num);
            break;
            
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] WebSocket Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
                
                // Send initial metrics to newly connected client
                sendMetricsToClient(num);
            }
            break;
            
        case WStype_TEXT:
            // Enhanced input validation
            if (length > 0 && length < 256) { // Reasonable message length
                char* message = (char*)payload;
                message[length] = '\0'; // Ensure null termination
                
                Serial.printf("[%u] Received: %s\n", num, message);
                
                // Handle client requests with validation
                if (strncmp(message, "get_metrics", 11) == 0) {
                    sendMetricsToClient(num);
                } else if (strncmp(message, "get_status", 10) == 0) {
                    // Send system status
                    DynamicJsonDocument doc(512);
                    doc["system_initialized"] = systemInitialized;
                    doc["wifi_connected"] = wifiConnected;
                    doc["free_heap"] = ESP.getFreeHeap();
                    doc["uptime"] = millis() / 1000;
                    doc["rssi"] = WiFi.RSSI();
                    
                    String jsonString;
                    serializeJson(doc, jsonString);
                    webSocket.sendTXT(num, jsonString);
                } else {
                    Serial.printf("[%u] Unknown command: %s\n", num, message);
                }
            } else {
                Serial.printf("[%u] Invalid message length: %zu\n", num, length);
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
    if (!metricsMutex) return;
    
    // Thread-safe metrics access
    if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100))) {
        DynamicJsonDocument doc(JSON_BUFFER_SIZE);
        doc["jitter"] = currentMetrics.jitter;
        doc["delay"] = currentMetrics.delay;
        doc["latency"] = currentMetrics.latency;
        doc["bitrate"] = currentMetrics.bitrate;
        doc["packetLoss"] = currentMetrics.packetLoss;
        doc["timestamp"] = currentMetrics.timestamp;
        doc["free_heap"] = ESP.getFreeHeap();
        doc["wifi_rssi"] = WiFi.RSSI();
        
        xSemaphoreGive(metricsMutex);
        
        String jsonString;
        if (serializeJson(doc, jsonString) > 0) {
            webSocket.broadcastTXT(jsonString);
        }
    }
}

void sendMetricsToClient(uint8_t clientNum) {
    if (!metricsMutex) return;
    
    // Thread-safe metrics access
    if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100))) {
        DynamicJsonDocument doc(JSON_BUFFER_SIZE);
        doc["jitter"] = currentMetrics.jitter;
        doc["delay"] = currentMetrics.delay;
        doc["latency"] = currentMetrics.latency;
        doc["bitrate"] = currentMetrics.bitrate;
        doc["packetLoss"] = currentMetrics.packetLoss;
        doc["timestamp"] = currentMetrics.timestamp;
        doc["free_heap"] = ESP.getFreeHeap();
        doc["wifi_rssi"] = WiFi.RSSI();
        
        xSemaphoreGive(metricsMutex);
        
        String jsonString;
        if (serializeJson(doc, jsonString) > 0) {
            webSocket.sendTXT(clientNum, jsonString);
        }
    }
}

void handleSystemError(const char* error) {
    Serial.printf("SYSTEM ERROR: %s\n", error);
    Serial.println("System will restart in 5 seconds...");
    
    // Cleanup resources
    if (metricsMutex) {
        vSemaphoreDelete(metricsMutex);
        metricsMutex = nullptr;
    }
    
    // Stop all tasks
    if (networkMonitorTaskHandle) {
        vTaskDelete(networkMonitorTaskHandle);
    }
    if (packetAnalysisTaskHandle) {
        vTaskDelete(packetAnalysisTaskHandle);
    }
    if (webServerTaskHandle) {
        vTaskDelete(webServerTaskHandle);
    }
    if (metricsUpdateTaskHandle) {
        vTaskDelete(metricsUpdateTaskHandle);
    }
    
    delay(5000);
    ESP.restart();
}

void checkSystemHealth() {
    static unsigned long lastHealthCheck = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastHealthCheck > MEMORY_CHECK_INTERVAL) {
        // Check memory health
        size_t freeHeap = ESP.getFreeHeap();
        
        if (freeHeap < HEAP_CRITICAL_THRESHOLD) {
            handleSystemError("Critical memory shortage");
        } else if (freeHeap < HEAP_WARNING_THRESHOLD) {
            Serial.printf("WARNING: Low memory - %u bytes free\n", freeHeap);
        }
        
        // Check WiFi connection
        if (wifiConnected && WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi connection lost, attempting reconnection...");
            wifiConnected = false;
            
            // Attempt to reconnect
            WiFi.reconnect();
            delay(1000);
            
            if (WiFi.status() == WL_CONNECTED) {
                wifiConnected = true;
                Serial.println("WiFi reconnected successfully");
            }
        }
        
        // Check task health
        if (systemInitialized) {
            UBaseType_t stackHighWaterMark;
            
            if (networkMonitorTaskHandle) {
                stackHighWaterMark = uxTaskGetStackHighWaterMark(networkMonitorTaskHandle);
                if (stackHighWaterMark < 512) {
                    Serial.printf("WARNING: Network monitor task low stack: %u\n", stackHighWaterMark);
                }
            }
            
            if (packetAnalysisTaskHandle) {
                stackHighWaterMark = uxTaskGetStackHighWaterMark(packetAnalysisTaskHandle);
                if (stackHighWaterMark < 512) {
                    Serial.printf("WARNING: Packet analysis task low stack: %u\n", stackHighWaterMark);
                }
            }
        }
        
        lastHealthCheck = currentTime;
        
        #if DEBUG_MEMORY
        Serial.printf("System Health - Free Heap: %u, WiFi: %s, Tasks: %s\n",
                     freeHeap, 
                     wifiConnected ? "Connected" : "Disconnected",
                     systemInitialized ? "Running" : "Initializing");
        #endif
    }
}