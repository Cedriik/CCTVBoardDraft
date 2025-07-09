#ifndef NETWORK_MONITOR_H
#define NETWORK_MONITOR_H

#include <WiFi.h>
#include <WiFiUdp.h>
#include <vector>

class NetworkMonitor {
private:
    WiFiUDP udp;
    std::vector<uint8_t> packetBuffer;
    unsigned long lastUpdateTime;
    bool isInitialized;
    
    // Network statistics
    unsigned long totalPackets;
    unsigned long droppedPackets;
    unsigned long totalBytes;
    float currentBandwidth;
    
    // Private methods
    void analyzePacket(uint8_t* packet, size_t length);
    void updateBandwidth();
    void calculateNetworkStats();
    
public:
    NetworkMonitor();
    ~NetworkMonitor();
    
    // Initialization
    bool begin();
    void stop();
    
    // Update methods
    void update();
    bool capturePacket();
    
    // Statistics getters
    unsigned long getTotalPackets() const { return totalPackets; }
    unsigned long getDroppedPackets() const { return droppedPackets; }
    unsigned long getTotalBytes() const { return totalBytes; }
    float getCurrentBandwidth() const { return currentBandwidth; }
    float getPacketLossRate() const;
    
    // Network information
    bool isConnected() const;
    String getLocalIP() const;
    String getNetworkInfo() const;
    
    // Monitoring controls
    void resetStats();
    void setMonitoringEnabled(bool enabled);
    bool isMonitoringEnabled() const { return isInitialized; }
};

#endif
