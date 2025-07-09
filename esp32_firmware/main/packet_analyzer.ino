#include "packet_analyzer.h"
#include "config_fixed.h"
#include <esp_wifi.h>

// External mutex for thread safety
extern SemaphoreHandle_t metricsMutex;

PacketAnalyzer::PacketAnalyzer() {
    currentMetrics = {0, 0, 0, 0, 0, 0, 0, 0};
    lastUpdateTime = 0;
    totalBytesReceived = 0;
    lastBytesReceived = 0;
    lastBitrateUpdate = 0;
    
    // Initialize circular buffers with proper sizes from config
    rtpTimestamps.reserve(RTP_TIMESTAMP_BUFFER_SIZE);
    arrivalTimes.reserve(ARRIVAL_TIME_BUFFER_SIZE);
    sequenceNumbers.reserve(PACKET_HISTORY_BUFFER_SIZE);
    
    lastProcessedPayloadType = 0;
    analysisEnabled = false;
    newDataAvailable = false;
}

PacketAnalyzer::~PacketAnalyzer() {
    stop();
}

bool PacketAnalyzer::begin() {
    if (!MemoryManager::checkHeapHealth()) {
        Serial.println("Warning: Low memory when starting packet analyzer");
    }
    
    // Clear any existing data
    reset();
    
    // Enable packet analysis
    analysisEnabled = true;
    
    Serial.println("Packet analyzer initialized successfully");
    return true;
}

void PacketAnalyzer::stop() {
    analysisEnabled = false;
    
    // Thread-safe cleanup
    if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        rtpTimestamps.clear();
        arrivalTimes.clear();
        sequenceNumbers.clear();
        xSemaphoreGive(metricsMutex);
    }
    
    Serial.println("Packet analyzer stopped");
}

void PacketAnalyzer::reset() {
    if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // Clear buffers
        rtpTimestamps.clear();
        arrivalTimes.clear();
        sequenceNumbers.clear();
        
        // Reset metrics
        currentMetrics = {0, 0, 0, 0, 0, 0, 0, 0};
        lastUpdateTime = 0;
        totalBytesReceived = 0;
        lastBytesReceived = 0;
        lastBitrateUpdate = 0;
        newDataAvailable = false;
        
        xSemaphoreGive(metricsMutex);
    }
    
    Serial.println("Packet analyzer reset");
}

void PacketAnalyzer::processPacket(const uint8_t* data, size_t length, uint32_t timestamp) {
    if (!analysisEnabled || !data || length == 0) {
        return;
    }
    
    // Check for sufficient memory before processing
    if (!MemoryManager::checkHeapHealth()) {
        Serial.println("Skipping packet analysis due to low memory");
        return;
    }
    
    PacketInfo packet;
    packet.timestamp = timestamp;
    packet.length = length;
    
    // Basic packet parsing - simplified for demonstration
    if (length < 20) { // Minimum for IP header
        return;
    }
    
    // Parse IP header (simplified)
    packet.sourceIP = (data[12] << 24) | (data[13] << 16) | (data[14] << 8) | data[15];
    packet.destIP = (data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19];
    packet.protocol = data[9];
    
    // Check for UDP (protocol 17)
    if (packet.protocol == 17 && length > 28) { // IP + UDP headers
        // Parse UDP header
        size_t udpOffset = 20; // Assuming no IP options
        packet.sourcePort = (data[udpOffset] << 8) | data[udpOffset + 1];
        packet.destPort = (data[udpOffset + 2] << 8) | data[udpOffset + 3];
        
        // Check for RTP (common video streaming ports)
        if (packet.destPort >= 16384 && packet.destPort <= 32767) {
            size_t rtpOffset = udpOffset + 8; // UDP header size
            
            if (length > rtpOffset + 12) { // RTP header minimum size
                // Parse RTP header
                uint8_t version = (data[rtpOffset] >> 6) & 0x03;
                if (version == 2) { // RTP version 2
                    packet.isRTP = true;
                    packet.payloadType = data[rtpOffset + 1] & 0x7F;
                    packet.sequenceNumber = (data[rtpOffset + 2] << 8) | data[rtpOffset + 3];
                    
                    // Extract RTP timestamp
                    uint32_t rtpTimestamp = (data[rtpOffset + 4] << 24) |
                                          (data[rtpOffset + 5] << 16) |
                                          (data[rtpOffset + 6] << 8) |
                                          data[rtpOffset + 7];
                    
                    // Store in packet info (adding missing member)
                    packet.rtpTimestamp = rtpTimestamp;
                    
                    // Process RTP packet
                    analyzeRTPPacket(packet);
                }
            }
        }
    }
    
    // Update total bytes
    totalBytesReceived += length;
    
    // Update metrics periodically
    uint32_t currentTime = getCurrentTime();
    if (currentTime - lastUpdateTime >= METRICS_UPDATE_INTERVAL) {
        updateMetrics();
        lastUpdateTime = currentTime;
        newDataAvailable = true;
    }
}

void PacketAnalyzer::analyzeRTPPacket(const PacketInfo& packet) {
    if (!packet.isRTP) return;
    
    // Store sequence number for packet loss calculation
    if (sequenceNumbers.size() >= PACKET_HISTORY_BUFFER_SIZE) {
        sequenceNumbers.erase(sequenceNumbers.begin());
    }
    sequenceNumbers.push_back(packet.sequenceNumber);
    
    // Store timestamps for jitter calculation
    if (rtpTimestamps.size() >= RTP_TIMESTAMP_BUFFER_SIZE) {
        rtpTimestamps.erase(rtpTimestamps.begin());
    }
    if (arrivalTimes.size() >= ARRIVAL_TIME_BUFFER_SIZE) {
        arrivalTimes.erase(arrivalTimes.begin());
    }
    
    rtpTimestamps.push_back(packet.rtpTimestamp);
    arrivalTimes.push_back(packet.timestamp);
    
    // Track payload type for codec-specific calculations
    lastProcessedPayloadType = packet.payloadType;
    
    // Update packet counters
    currentMetrics.totalPackets++;
    
    // Calculate metrics if we have enough data
    if (rtpTimestamps.size() >= 2) {
        calculateJitter();
        calculateDelay();
        calculatePacketLoss();
    }
}

void PacketAnalyzer::calculateJitter() {
    if (rtpTimestamps.size() < 2 || arrivalTimes.size() < 2) {
        return;
    }
    
    size_t bufferSize = rtpTimestamps.size();
    if (bufferSize < 2) return;
    
    // Get clock rate for the current payload type
    uint32_t clockRate = getClockRateForPayloadType(lastProcessedPayloadType);
    if (clockRate == 0) {
        clockRate = 90000; // Default for video
    }
    
    std::vector<float> jitterSamples;
    jitterSamples.reserve(bufferSize - 1);
    
    // Calculate jitter for each packet pair
    for (size_t i = 1; i < bufferSize; i++) {
        uint32_t rtpDiff = rtpTimestamps[i] - rtpTimestamps[i-1];
        uint32_t arrivalDiff = arrivalTimes[i] - arrivalTimes[i-1];
        
        // Convert RTP timestamp difference to milliseconds
        float rtpDiffMs = (float)rtpDiff * 1000.0f / clockRate;
        float arrivalDiffMs = (float)arrivalDiff;
        
        // Calculate absolute difference
        float jitter = abs(arrivalDiffMs - rtpDiffMs);
        jitterSamples.push_back(jitter);
    }
    
    // Calculate average jitter
    if (!jitterSamples.empty()) {
        float totalJitter = 0;
        for (float j : jitterSamples) {
            totalJitter += j;
        }
        currentMetrics.jitter = totalJitter / jitterSamples.size();
    }
}

uint32_t PacketAnalyzer::getClockRateForPayloadType(uint8_t payloadType) {
    switch (payloadType) {
        case 0:   // PCMU
        case 8:   // PCMA
            return AUDIO_CLOCK_RATE;
        case 26:  // JPEG
            return MJPEG_CLOCK_RATE;
        case 96:  // H.264 (dynamic)
            return H264_CLOCK_RATE;
        case 97:  // H.265 (dynamic)
            return H265_CLOCK_RATE;
        default:
            return H264_CLOCK_RATE; // Default for video
    }
}

void PacketAnalyzer::calculateDelay() {
    if (arrivalTimes.empty()) {
        return;
    }
    
    size_t bufferSize = arrivalTimes.size();
    if (bufferSize < 2) return;
    
    // Calculate average inter-arrival delay
    float totalDelay = 0;
    int validSamples = 0;
    
    for (size_t i = 1; i < bufferSize; i++) {
        uint32_t delay = arrivalTimes[i] - arrivalTimes[i-1];
        if (delay < 1000) { // Filter out unrealistic delays (> 1 second)
            totalDelay += delay;
            validSamples++;
        }
    }
    
    if (validSamples > 0) {
        currentMetrics.delay = totalDelay / validSamples;
    }
}

void PacketAnalyzer::calculateLatency() {
    // For now, use delay as a proxy for latency
    // In a real implementation, this would involve round-trip measurements
    currentMetrics.latency = currentMetrics.delay * 2; // Simplified estimate
}

void PacketAnalyzer::calculateBitrate() {
    uint32_t currentTime = getCurrentTime();
    
    if (currentTime - lastBitrateUpdate >= 1000) { // Update every second
        uint32_t bytesDiff = totalBytesReceived - lastBytesReceived;
        uint32_t timeDiff = currentTime - lastBitrateUpdate;
        
        if (timeDiff > 0) {
            // Calculate bitrate in Mbps
            currentMetrics.bitrate = (float)(bytesDiff * 8) / (timeDiff * 1000.0f);
        }
        
        lastBytesReceived = totalBytesReceived;
        lastBitrateUpdate = currentTime;
    }
}

void PacketAnalyzer::calculatePacketLoss() {
    if (sequenceNumbers.size() < 2) {
        return;
    }
    
    // Create a sorted copy of sequence numbers for analysis
    std::vector<uint16_t> seqNumbers = sequenceNumbers;
    std::sort(seqNumbers.begin(), seqNumbers.end());
    
    // Count missing sequence numbers
    uint32_t expectedPackets = 0;
    uint32_t receivedPackets = seqNumbers.size();
    
    if (receivedPackets >= 2) {
        uint16_t minSeq = seqNumbers[0];
        uint16_t maxSeq = seqNumbers[receivedPackets - 1];
        
        // Handle sequence number wraparound (16-bit)
        if (maxSeq >= minSeq) {
            expectedPackets = maxSeq - minSeq + 1;
        } else {
            expectedPackets = (65536 - minSeq) + maxSeq + 1;
        }
        
        if (expectedPackets > 0) {
            currentMetrics.lostPackets = expectedPackets - receivedPackets;
            currentMetrics.packetLoss = 
                ((float)currentMetrics.lostPackets / expectedPackets) * 100.0f;
            
            // Clamp packet loss to reasonable bounds
            if (currentMetrics.packetLoss > 100.0f) {
                currentMetrics.packetLoss = 100.0f;
            }
            if (currentMetrics.packetLoss < 0.0f) {
                currentMetrics.packetLoss = 0.0f;
            }
        }
    }
}

void PacketAnalyzer::updateMetrics() {
    calculateBitrate();
    calculateLatency();
    
    // Update timestamp
    currentMetrics.timestamp = getCurrentTime();
}

uint32_t PacketAnalyzer::getCurrentTime() {
    return millis();
}

bool PacketAnalyzer::isVideoPacket(const PacketInfo& packet) {
    if (!packet.isRTP) return false;
    
    // Check common video payload types
    switch (packet.payloadType) {
        case 26:  // JPEG
        case 96:  // H.264 (typically)
        case 97:  // H.265 (typically)
        case 98:  // VP8/VP9 (dynamic)
            lastProcessedPayloadType = packet.payloadType;
            return true;
        default:
            return false;
    }
}

void PacketAnalyzer::addPacket(const PacketInfo& packet) {
    processPacket(nullptr, packet.length, packet.timestamp);
}

bool PacketAnalyzer::hasNewData() const {
    if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        bool hasNew = (getCurrentTime() - currentMetrics.timestamp) < (METRICS_UPDATE_INTERVAL * 2);
        xSemaphoreGive(metricsMutex);
        return hasNew && newDataAvailable;
    }
    return false;
}

void PacketAnalyzer::clearNewDataFlag() {
    newDataAvailable = false;
}

PacketAnalyzer::Metrics PacketAnalyzer::getMetrics() const {
    Metrics metrics;
    if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        metrics = currentMetrics;
        xSemaphoreGive(metricsMutex);
    } else {
        // Return zero metrics if mutex timeout
        metrics = {0, 0, 0, 0, 0, 0, 0, 0};
    }
    return metrics;
}

void PacketAnalyzer::setAnalysisEnabled(bool enabled) {
    analysisEnabled = enabled;
}

bool PacketAnalyzer::isAnalysisEnabled() const {
    return analysisEnabled;
}

void PacketAnalyzer::printDiagnostics() const {
    Serial.println("=== Packet Analyzer Diagnostics ===");
    Serial.printf("Total Packets: %lu\n", currentMetrics.totalPackets);
    Serial.printf("Lost Packets: %lu\n", currentMetrics.lostPackets);
    Serial.printf("Jitter: %.2f ms\n", currentMetrics.jitter);
    Serial.printf("Delay: %.2f ms\n", currentMetrics.delay);
    Serial.printf("Latency: %.2f ms\n", currentMetrics.latency);
    Serial.printf("Bitrate: %.2f Mbps\n", currentMetrics.bitrate);
    Serial.printf("Packet Loss: %.2f%%\n", currentMetrics.packetLoss);
    
    Serial.printf("Buffer Status:\n");
    Serial.printf("  RTP Timestamps: %d/%d\n", rtpTimestamps.size(), RTP_TIMESTAMP_BUFFER_SIZE);
    Serial.printf("  Arrival Times: %d/%d\n", arrivalTimes.size(), ARRIVAL_TIME_BUFFER_SIZE);
    Serial.printf("  Sequence Numbers: %d/%d\n", sequenceNumbers.size(), PACKET_HISTORY_BUFFER_SIZE);
    
    Serial.printf("Free Heap: %lu bytes\n", ESP.getFreeHeap());
    Serial.println("===================================");
}

String PacketAnalyzer::getAnalysisReport() const {
    String report = "Packet Analysis Report:\n";
    report += "Packets: " + String(currentMetrics.totalPackets) + " (Lost: " + String(currentMetrics.lostPackets) + ")\n";
    report += "Jitter: " + String(currentMetrics.jitter, 2) + " ms\n";
    report += "Delay: " + String(currentMetrics.delay, 2) + " ms\n";
    report += "Latency: " + String(currentMetrics.latency, 2) + " ms\n";
    report += "Bitrate: " + String(currentMetrics.bitrate, 2) + " Mbps\n";
    report += "Packet Loss: " + String(currentMetrics.packetLoss, 2) + "%\n";
    return report;
}
