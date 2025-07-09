#include "packet_analyzer.h"
#include "config.h"
#include <algorithm>
#include <cmath>

PacketAnalyzer::PacketAnalyzer() : 
    lastUpdateTime(0),
    totalBytesReceived(0),
    lastBytesReceived(0),
    lastBitrateUpdate(0) {
    
    // Initialize metrics
    currentMetrics = {0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0};
    
    // Reserve space for vectors
    rtpTimestamps.reserve(MAX_JITTER_SAMPLES);
    arrivalTimes.reserve(MAX_JITTER_SAMPLES);
    sequenceNumbers.reserve(MAX_PACKET_HISTORY);
}

PacketAnalyzer::~PacketAnalyzer() {
    stop();
}

bool PacketAnalyzer::begin() {
    reset();
    lastUpdateTime = getCurrentTime();
    lastBitrateUpdate = lastUpdateTime;
    
    Serial.println("Packet Analyzer initialized");
    return true;
}

void PacketAnalyzer::stop() {
    // Clear all data structures
    while (!packetQueue.empty()) {
        packetQueue.pop();
    }
    rtpTimestamps.clear();
    arrivalTimes.clear();
    sequenceNumbers.clear();
    
    Serial.println("Packet Analyzer stopped");
}

void PacketAnalyzer::reset() {
    currentMetrics = {0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0};
    totalBytesReceived = 0;
    lastBytesReceived = 0;
    
    // Clear history but keep reserved space
    rtpTimestamps.clear();
    arrivalTimes.clear();
    sequenceNumbers.clear();
    
    while (!packetQueue.empty()) {
        packetQueue.pop();
    }
}

void PacketAnalyzer::processPacket(const uint8_t* data, size_t length, uint32_t timestamp) {
    if (!data || length < 20) return; // Minimum IP header size
    
    PacketInfo packet;
    packet.timestamp = timestamp;
    packet.length = length;
    
    // Parse IP header (simplified)
    if (length >= 20) {
        packet.protocol = data[9];
        packet.sourceIP = (data[12] << 24) | (data[13] << 16) | (data[14] << 8) | data[15];
        packet.destIP = (data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19];
        
        // Parse UDP header if it's UDP
        if (packet.protocol == 17 && length >= 28) { // UDP
            packet.sourcePort = (data[20] << 8) | data[21];
            packet.destPort = (data[22] << 8) | data[23];
            
            // Check if this might be RTP (UDP payload)
            if (length >= 32) {
                uint8_t rtpVersion = (data[28] >> 6) & 0x03;
                if (rtpVersion == 2) {
                    packet.isRTP = true;
                    packet.payloadType = data[29] & 0x7F;
                    packet.sequenceNumber = (data[30] << 8) | data[31];
                } else {
                    packet.isRTP = false;
                }
            }
        }
    }
    
    addPacket(packet);
}

void PacketAnalyzer::addPacket(const PacketInfo& packet) {
    // Add to queue
    packetQueue.push(packet);
    
    // Keep queue size manageable
    if (packetQueue.size() > MAX_PACKET_HISTORY) {
        packetQueue.pop();
    }
    
    // Update total bytes
    totalBytesReceived += packet.length;
    currentMetrics.totalPackets++;
    
    // Process RTP packets for video analysis
    if (packet.isRTP && isVideoPacket(packet)) {
        analyzeRTPPacket(packet);
    }
    
    // Update metrics periodically
    uint32_t currentTime = getCurrentTime();
    if (currentTime - lastUpdateTime >= METRICS_UPDATE_INTERVAL) {
        updateMetrics();
        lastUpdateTime = currentTime;
    }
}

void PacketAnalyzer::analyzeRTPPacket(const PacketInfo& packet) {
    // Store sequence number for packet loss analysis
    sequenceNumbers.push_back(packet.sequenceNumber);
    if (sequenceNumbers.size() > MAX_PACKET_HISTORY) {
        sequenceNumbers.erase(sequenceNumbers.begin());
    }
    
    // Store timestamps for jitter calculation
    rtpTimestamps.push_back(packet.timestamp);
    arrivalTimes.push_back(getCurrentTime());
    
    if (rtpTimestamps.size() > MAX_JITTER_SAMPLES) {
        rtpTimestamps.erase(rtpTimestamps.begin());
        arrivalTimes.erase(arrivalTimes.begin());
    }
}

void PacketAnalyzer::calculateJitter() {
    if (rtpTimestamps.size() < 2) {
        currentMetrics.jitter = 0.0;
        return;
    }
    
    std::vector<float> jitterSamples;
    jitterSamples.reserve(rtpTimestamps.size() - 1);
    
    for (size_t i = 1; i < rtpTimestamps.size(); i++) {
        // Calculate inter-arrival time difference
        uint32_t rtpDiff = rtpTimestamps[i] - rtpTimestamps[i-1];
        uint32_t arrivalDiff = arrivalTimes[i] - arrivalTimes[i-1];
        
        // Convert to milliseconds (assuming 90kHz RTP clock for video)
        float rtpDiffMs = rtpDiff / 90.0;
        float arrivalDiffMs = arrivalDiff;
        
        float jitter = std::abs(arrivalDiffMs - rtpDiffMs);
        jitterSamples.push_back(jitter);
    }
    
    if (!jitterSamples.empty()) {
        float sum = 0.0;
        for (float j : jitterSamples) {
            sum += j;
        }
        currentMetrics.jitter = sum / jitterSamples.size();
    }
}

void PacketAnalyzer::calculateDelay() {
    // Simplified delay calculation based on packet processing time
    if (packetQueue.empty()) {
        currentMetrics.delay = 0.0;
        return;
    }
    
    uint32_t currentTime = getCurrentTime();
    uint32_t oldestPacketTime = packetQueue.front().timestamp;
    
    currentMetrics.delay = (currentTime - oldestPacketTime) / (float)packetQueue.size();
}

void PacketAnalyzer::calculateLatency() {
    // Estimate latency based on jitter and delay
    currentMetrics.latency = currentMetrics.jitter + (currentMetrics.delay * 0.5);
}

void PacketAnalyzer::calculateBitrate() {
    uint32_t currentTime = getCurrentTime();
    uint32_t timeDiff = currentTime - lastBitrateUpdate;
    
    if (timeDiff >= 1000) { // Update every second
        uint32_t bytesDiff = totalBytesReceived - lastBytesReceived;
        currentMetrics.bitrate = (bytesDiff * 8.0) / (timeDiff / 1000.0) / 1000000.0; // Mbps
        
        lastBytesReceived = totalBytesReceived;
        lastBitrateUpdate = currentTime;
    }
}

void PacketAnalyzer::calculatePacketLoss() {
    if (sequenceNumbers.size() < 2) {
        currentMetrics.packetLoss = 0.0;
        return;
    }
    
    // Sort sequence numbers to find gaps
    std::vector<uint16_t> sortedSeq = sequenceNumbers;
    std::sort(sortedSeq.begin(), sortedSeq.end());
    
    uint16_t expectedPackets = sortedSeq.back() - sortedSeq.front() + 1;
    uint16_t receivedPackets = sortedSeq.size();
    
    if (expectedPackets > receivedPackets) {
        currentMetrics.lostPackets = expectedPackets - receivedPackets;
        currentMetrics.packetLoss = ((float)currentMetrics.lostPackets / expectedPackets) * 100.0;
    } else {
        currentMetrics.packetLoss = 0.0;
    }
}

void PacketAnalyzer::updateMetrics() {
    calculateJitter();
    calculateDelay();
    calculateLatency();
    calculateBitrate();
    calculatePacketLoss();
    
    currentMetrics.timestamp = getCurrentTime();
    
    #if DEBUG_PACKETS
    printDiagnostics();
    #endif
}

bool PacketAnalyzer::isVideoPacket(const PacketInfo& packet) {
    // Check if this is likely a video packet based on payload type
    // Common video payload types: H.264 (96-127), MJPEG (26), etc.
    return packet.isRTP && (packet.payloadType >= 96 || packet.payloadType == 26);
}

uint32_t PacketAnalyzer::getCurrentTime() {
    return millis();
}

bool PacketAnalyzer::hasNewData() const {
    return (getCurrentTime() - currentMetrics.timestamp) < (METRICS_UPDATE_INTERVAL * 2);
}

void PacketAnalyzer::clearNewDataFlag() {
    // This is handled automatically by the timestamp comparison
}

void PacketAnalyzer::setAnalysisEnabled(bool enabled) {
    if (!enabled) {
        reset();
    }
}

bool PacketAnalyzer::isAnalysisEnabled() const {
    return true; // Always enabled for now
}

void PacketAnalyzer::printDiagnostics() const {
    Serial.println("=== Packet Analyzer Diagnostics ===");
    Serial.printf("Total Packets: %u\n", currentMetrics.totalPackets);
    Serial.printf("Lost Packets: %u\n", currentMetrics.lostPackets);
    Serial.printf("Jitter: %.2f ms\n", currentMetrics.jitter);
    Serial.printf("Delay: %.2f ms\n", currentMetrics.delay);
    Serial.printf("Latency: %.2f ms\n", currentMetrics.latency);
    Serial.printf("Bitrate: %.2f Mbps\n", currentMetrics.bitrate);
    Serial.printf("Packet Loss: %.2f%%\n", currentMetrics.packetLoss);
    Serial.printf("Queue Size: %u\n", packetQueue.size());
    Serial.printf("RTP Samples: %u\n", rtpTimestamps.size());
    Serial.println("===================================");
}

String PacketAnalyzer::getAnalysisReport() const {
    String report = "Packet Analysis Report:\n";
    report += "Total Packets: " + String(currentMetrics.totalPackets) + "\n";
    report += "Lost Packets: " + String(currentMetrics.lostPackets) + "\n";
    report += "Jitter: " + String(currentMetrics.jitter, 2) + " ms\n";
    report += "Delay: " + String(currentMetrics.delay, 2) + " ms\n";
    report += "Latency: " + String(currentMetrics.latency, 2) + " ms\n";
    report += "Bitrate: " + String(currentMetrics.bitrate, 2) + " Mbps\n";
    report += "Packet Loss: " + String(currentMetrics.packetLoss, 2) + "%\n";
    return report;
}
