#include "network_monitor.h"
#include "config.h"
#include <esp_wifi.h>
#include <esp_netif.h>

NetworkMonitor::NetworkMonitor() : 
    lastUpdateTime(0),
    isInitialized(false),
    totalPackets(0),
    droppedPackets(0),
    totalBytes(0),
    currentBandwidth(0.0) {
    
    packetBuffer.reserve(PACKET_CAPTURE_BUFFER);
}

NetworkMonitor::~NetworkMonitor() {
    stop();
}

bool NetworkMonitor::begin() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected, cannot start network monitor");
        return false;
    }
    
    // Initialize UDP for packet capture
    if (!udp.begin(MONITOR_PORT)) {
        Serial.println("Failed to start UDP for network monitoring");
        return false;
    }
    
    // Enable promiscuous mode for packet capture
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb([](void* buf, wifi_promiscuous_pkt_type_t type) {
        // This callback will be called for each packet
        // Implementation depends on specific packet capture requirements
    });
    
    resetStats();
    isInitialized = true;
    lastUpdateTime = millis();
    
    Serial.println("Network Monitor initialized successfully");
    return true;
}

void NetworkMonitor::stop() {
    if (isInitialized) {
        esp_wifi_set_promiscuous(false);
        udp.stop();
        isInitialized = false;
        Serial.println("Network Monitor stopped");
    }
}

void NetworkMonitor::update() {
    if (!isInitialized) return;
    
    unsigned long currentTime = millis();
    
    // Update bandwidth calculation every second
    if (currentTime - lastUpdateTime >= 1000) {
        updateBandwidth();
        calculateNetworkStats();
        lastUpdateTime = currentTime;
    }
    
    // Check for new packets
    if (capturePacket()) {
        totalPackets++;
    }
}

bool NetworkMonitor::capturePacket() {
    if (!isInitialized) return false;
    
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
        packetBuffer.resize(packetSize);
        int bytesRead = udp.read(packetBuffer.data(), packetSize);
        
        if (bytesRead > 0) {
            totalBytes += bytesRead;
            analyzePacket(packetBuffer.data(), bytesRead);
            return true;
        }
    }
    
    return false;
}

void NetworkMonitor::analyzePacket(uint8_t* packet, size_t length) {
    if (!packet || length == 0) return;
    
    // Basic packet analysis
    // This is a simplified implementation - in practice, you'd need
    // more sophisticated packet parsing for different protocols
    
    // Check for video stream packets (typically RTP over UDP)
    if (length > 12) { // Minimum RTP header size
        // RTP header analysis
        uint8_t version = (packet[0] >> 6) & 0x03;
        uint8_t payloadType = packet[1] & 0x7F;
        uint16_t sequenceNumber = (packet[2] << 8) | packet[3];
        
        if (version == 2) { // RTP version 2
            // This looks like an RTP packet
            // Additional analysis can be done here
            
            #if DEBUG_PACKETS
            Serial.printf("RTP Packet: PT=%d, Seq=%d, Size=%d\n", 
                         payloadType, sequenceNumber, length);
            #endif
        }
    }
}

void NetworkMonitor::updateBandwidth() {
    static unsigned long lastBytes = 0;
    static unsigned long lastTime = 0;
    
    unsigned long currentTime = millis();
    unsigned long timeDiff = currentTime - lastTime;
    
    if (timeDiff > 0) {
        unsigned long bytesDiff = totalBytes - lastBytes;
        currentBandwidth = (bytesDiff * 8.0) / (timeDiff / 1000.0); // bits per second
        currentBandwidth /= 1000000.0; // Convert to Mbps
        
        lastBytes = totalBytes;
        lastTime = currentTime;
    }
}

void NetworkMonitor::calculateNetworkStats() {
    // Calculate packet loss based on expected vs received packets
    // This is a simplified calculation
    static unsigned long lastTotalPackets = 0;
    static unsigned long expectedPackets = 0;
    
    unsigned long newPackets = totalPackets - lastTotalPackets;
    expectedPackets += newPackets;
    
    // Simulate some packet loss detection logic
    // In practice, this would be based on sequence number analysis
    if (newPackets > 0) {
        // Simple heuristic: if we receive significantly fewer packets
        // than expected, consider some as dropped
        if (newPackets < (expectedPackets * 0.95)) {
            droppedPackets += (expectedPackets - newPackets);
        }
    }
    
    lastTotalPackets = totalPackets;
}

float NetworkMonitor::getPacketLossRate() const {
    if (totalPackets == 0) return 0.0;
    return (float)droppedPackets / (float)(totalPackets + droppedPackets) * 100.0;
}

bool NetworkMonitor::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String NetworkMonitor::getLocalIP() const {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "Not Connected";
}

String NetworkMonitor::getNetworkInfo() const {
    String info = "Network Info:\n";
    info += "Status: " + String(isConnected() ? "Connected" : "Disconnected") + "\n";
    info += "IP: " + getLocalIP() + "\n";
    info += "SSID: " + WiFi.SSID() + "\n";
    info += "RSSI: " + String(WiFi.RSSI()) + " dBm\n";
    info += "Bandwidth: " + String(currentBandwidth, 2) + " Mbps\n";
    info += "Total Packets: " + String(totalPackets) + "\n";
    info += "Dropped Packets: " + String(droppedPackets) + "\n";
    info += "Packet Loss: " + String(getPacketLossRate(), 2) + "%\n";
    return info;
}

void NetworkMonitor::resetStats() {
    totalPackets = 0;
    droppedPackets = 0;
    totalBytes = 0;
    currentBandwidth = 0.0;
    lastUpdateTime = millis();
}

void NetworkMonitor::setMonitoringEnabled(bool enabled) {
    if (enabled && !isInitialized) {
        begin();
    } else if (!enabled && isInitialized) {
        stop();
    }
}
