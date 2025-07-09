#ifndef PACKET_ANALYZER_H
#define PACKET_ANALYZER_H

#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include "config_fixed.h"

class PacketAnalyzer {
public:
    struct PacketInfo {
        uint32_t timestamp;
        uint16_t sequenceNumber;
        uint32_t sourceIP;
        uint32_t destIP;
        uint16_t sourcePort;
        uint16_t destPort;
        uint16_t length;
        uint8_t protocol;
        bool isRTP;
        uint8_t payloadType;
        uint32_t rtpTimestamp;  // Added missing RTP timestamp field
    };
    
    struct Metrics {
        float jitter;           // Network jitter in ms
        float delay;            // Network delay in ms
        float latency;          // Round-trip latency in ms
        float bitrate;          // Current bitrate in Mbps
        float packetLoss;       // Packet loss percentage
        uint32_t totalPackets;  // Total packets analyzed
        uint32_t lostPackets;   // Total lost packets
        uint32_t timestamp;     // Last update timestamp
    };
    
private:
    // Data storage vectors (replacing circular buffers for simplicity)
    std::vector<uint32_t> rtpTimestamps;
    std::vector<uint32_t> arrivalTimes;
    std::vector<uint16_t> sequenceNumbers;
    
    // Current metrics
    Metrics currentMetrics;
    
    // Timing and state variables
    uint32_t lastUpdateTime;
    uint32_t totalBytesReceived;
    uint32_t lastBytesReceived;
    uint32_t lastBitrateUpdate;
    uint8_t lastProcessedPayloadType;
    bool analysisEnabled;
    bool newDataAvailable;
    
    // Private methods
    void analyzeRTPPacket(const PacketInfo& packet);
    void calculateJitter();
    void calculateDelay();
    void calculateLatency();
    void calculateBitrate();
    void calculatePacketLoss();
    void updateMetrics();
    bool isVideoPacket(const PacketInfo& packet);
    uint32_t getCurrentTime();
    uint32_t getClockRateForPayloadType(uint8_t payloadType);
    
public:
    PacketAnalyzer();
    ~PacketAnalyzer();
    
    // Initialization
    bool begin();
    void stop();
    void reset();
    
    // Packet processing
    void processPacket(const uint8_t* data, size_t length, uint32_t timestamp);
    void addPacket(const PacketInfo& packet);
    
    // Metrics access
    Metrics getMetrics() const;
    bool hasNewData() const;
    void clearNewDataFlag();
    
    // Analysis controls
    void setAnalysisEnabled(bool enabled);
    bool isAnalysisEnabled() const;
    
    // Statistics
    uint32_t getTotalPacketsAnalyzed() const { return currentMetrics.totalPackets; }
    uint32_t getLostPacketsCount() const { return currentMetrics.lostPackets; }
    float getCurrentBitrate() const { return currentMetrics.bitrate; }
    float getPacketLossRate() const { return currentMetrics.packetLoss; }
    
    // Debug and diagnostics
    void printDiagnostics() const;
    String getAnalysisReport() const;
};

#endif
