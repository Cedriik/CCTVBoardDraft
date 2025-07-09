#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "config.h"
#include "network_monitor.h"
#include "web_server.h"
#include "packet_analyzer.h"

// Global objects
WebServer server(80);
WebSocketsServer webSocket(81);
NetworkMonitor netMonitor;
PacketAnalyzer packetAnalyzer;
CustomWebServer webServerHandler;

// Global variables for metrics
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
    
    // Initialize SPIFFS for web files
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    
    // Initialize WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("Connected to WiFi. IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Initialize network monitor
    netMonitor.begin();
    
    // Initialize packet analyzer
    packetAnalyzer.begin();
    
    // Initialize web server
    webServerHandler.begin(&server, &webSocket);
    
    // Setup WebSocket event handler
    webSocket.onEvent(webSocketEvent);
    webSocket.begin();
    
    // Start HTTP server
    server.begin();
    
    Serial.println("CCTV Monitor System Ready");
    Serial.println("Web Interface: http://" + WiFi.localIP().toString());
}

void loop() {
    // Handle web server requests
    server.handleClient();
    webSocket.loop();
    
    // Update network monitoring
    netMonitor.update();
    
    // Analyze packets and calculate metrics
    if (packetAnalyzer.hasNewData()) {
        PacketAnalyzer::Metrics metrics = packetAnalyzer.getMetrics();
        
        // Update current metrics
        currentMetrics.jitter = metrics.jitter;
        currentMetrics.delay = metrics.delay;
        currentMetrics.latency = metrics.latency;
        currentMetrics.bitrate = metrics.bitrate;
        currentMetrics.packetLoss = metrics.packetLoss;
        currentMetrics.timestamp = millis();
        
        // Send metrics to connected WebSocket clients
        sendMetricsToClients();
        
        // Print metrics to serial for debugging
        Serial.printf("Jitter: %.2f ms, Delay: %.2f ms, Latency: %.2f ms, Bitrate: %.2f Mbps, Packet Loss: %.2f%%\n",
                     currentMetrics.jitter, currentMetrics.delay, currentMetrics.latency, 
                     currentMetrics.bitrate, currentMetrics.packetLoss);
    }
    
    delay(100); // Small delay to prevent excessive CPU usage
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
            
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
                
                // Send initial metrics to newly connected client
                sendMetricsToClient(num);
            }
            break;
            
        case WStype_TEXT:
            Serial.printf("[%u] Received Text: %s\n", num, payload);
            
            // Handle client requests
            if (strcmp((char*)payload, "get_metrics") == 0) {
                sendMetricsToClient(num);
            }
            break;
            
        default:
            break;
    }
}

void sendMetricsToClients() {
    DynamicJsonDocument doc(1024);
    doc["jitter"] = currentMetrics.jitter;
    doc["delay"] = currentMetrics.delay;
    doc["latency"] = currentMetrics.latency;
    doc["bitrate"] = currentMetrics.bitrate;
    doc["packetLoss"] = currentMetrics.packetLoss;
    doc["timestamp"] = currentMetrics.timestamp;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    webSocket.broadcastTXT(jsonString);
}

void sendMetricsToClient(uint8_t clientNum) {
    DynamicJsonDocument doc(1024);
    doc["jitter"] = currentMetrics.jitter;
    doc["delay"] = currentMetrics.delay;
    doc["latency"] = currentMetrics.latency;
    doc["bitrate"] = currentMetrics.bitrate;
    doc["packetLoss"] = currentMetrics.packetLoss;
    doc["timestamp"] = currentMetrics.timestamp;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    webSocket.sendTXT(clientNum, jsonString);
}
