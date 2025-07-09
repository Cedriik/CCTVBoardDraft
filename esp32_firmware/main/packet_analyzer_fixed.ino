#include "packet_analyzer.h"
#include "config_fixed.h"
#include <algorithm>
#include <cmath>

// Thread-safe mutex for metrics access
SemaphoreHandle_t metricsMutex = nullptr;

PacketAnalyzer::PacketAnalyzer() : 
    lastUpdateTime(0),
    totalBytesReceived(0),
    lastBytesReceived(0),
    lastBitrateUpdate(0) {
    
    // Initialize metrics with thread safety
    currentMetrics = {0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0};
    
    // Create mutex for thread-safe access
    metricsMutex = xSemaphoreCreateMutex();
    
    // Initialize circular buffers instead of vectors (prevents memory leaks)
    rtpTimestampBuffer = new CircularBuffer<uint32_t, RTP_TIMESTAMP_BUFFER_SIZE>();
    arrivalTimeBuffer = new CircularBuffer<uint32_t, ARRIVAL_TIME_BUFFER_SIZE>();
    sequenceNumberBuffer = new CircularBuffer<uint16_t, PACKET_HISTORY_BUFFER_SIZE>();
    
    if (!rtpTimestampBuffer || !arrivalTimeBuffer || !sequenceNumberBuffer) {
        Serial.println("ERROR: Failed to allocate circular buffers");
    }
}

PacketAnalyzer::~PacketAnalyzer() {
    stop();
    
    // Clean up circular buffers
    if (rtpTimestampBuffer) {
        delete rtpTimestampBuffer;
        rtpTimestampBuffer = nullptr;
    }
    
    if (arrivalTimeBuffer) {
        delete arrivalTimeBuffer;
        arrivalTimeBuffer = nullptr;
    }
    
    if (sequenceNumberBuffer) {
        delete sequenceNumberBuffer;
        sequenceNumberBuffer = nullptr;
    }
    
    // Clean up mutex
    if (metricsMutex) {
        vSemaphoreDelete(metricsMutex);
        metricsMutex = nullptr;
    }
}

bool PacketAnalyzer::begin() {
    if (!metricsMutex) {
        Serial.println("ERROR: Metrics mutex not initialized");
        return false;
    }
    
    if (!rtpTimestampBuffer || !arrivalTimeBuffer || !sequenceNumberBuffer) {
        Serial.println("ERROR: Circular buffers not initialized");
        return false;
    }
    
    reset();
    lastUpdateTime = getCurrentTime();
    lastBitrateUpdate = lastUpdateTime;
    
    Serial.println("Packet Analyzer initialized with thread safety");
    return true;
}

void PacketAnalyzer::stop() {
    // Thread-safe cleanup
    if (metricsMutex && xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(1000))) {
        // Clear all buffers
        if (rtpTimestampBuffer) rtpTimestampBuffer->clear();
        if (arrivalTimeBuffer) arrivalTimeBuffer->clear();
        if (sequenceNumberBuffer) sequenceNumberBuffer->clear();
        
        xSemaphoreGive(metricsMutex);
    }
    
    Serial.println("Packet Analyzer stopped");
}

void PacketAnalyzer::reset() {
    if (metricsMutex && xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(1000))) {
        currentMetrics = {0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0};
        totalBytesReceived = 0;
        lastBytesReceived = 0;
        
        // Clear circular buffers safely
        if (rtpTimestampBuffer) rtpTimestampBuffer->clear();
        if (arrivalTimeBuffer) arrivalTimeBuffer->clear();
        if (sequenceNumberBuffer) sequenceNumberBuffer->clear();
        
        xSemaphoreGive(metricsMutex);
    }
}

void PacketAnalyzer::processPacket(const uint8_t* data, size_t length, uint32_t timestamp) {
    // Critical Fix: Comprehensive bounds checking
    if (!data || length < 20) {
        #if DEBUG_PACKETS
        Serial.printf("Invalid packet: data=%p, length=%zu\n", data, length);
        #endif
        return;
    }
    
    // Check for buffer overflow conditions
    if (length > MAX_PACKET_BUFFER_SIZE) {
        Serial.printf("WARNING: Packet too large (%zu bytes), truncating\n", length);
        length = MAX_PACKET_BUFFER_SIZE;
    }
    
    PacketInfo packet;
    packet.timestamp = timestamp;
    packet.length = length;
    packet.isRTP = false;
    packet.protocol = 0;
    
    // Parse IP header with proper bounds checking
    if (length >= 20) {
        // Verify IP version (should be 4)
        uint8_t version = (data[0] >> 4) & 0x0F;
        if (version != 4) {
            #if DEBUG_PACKETS
            Serial.printf("Non-IPv4 packet (version %d), skipping\n", version);
            #endif
            return;
        }
        
        // Extract header length and verify bounds
        uint8_t headerLength = (data[0] & 0x0F) * 4;
        if (headerLength < 20 || headerLength > length) {
            #if DEBUG_PACKETS
            Serial.printf("Invalid IP header length: %d\n", headerLength);
            #endif
            return;
        }
        
        packet.protocol = data[9];
        
        // Safe extraction of IP addresses
        if (length >= headerLength) {
            packet.sourceIP = (data[12] << 24) | (data[13] << 16) | (data[14] << 8) | data[15];
            packet.destIP = (data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19];
        }
        
        // Parse UDP header if it's UDP and we have enough data
        if (packet.protocol == 17 && length >= headerLength + 8) { // UDP
            size_t udpOffset = headerLength;
            
            // Verify we have complete UDP header
            if (length >= udpOffset + 8) {
                packet.sourcePort = (data[udpOffset] << 8) | data[udpOffset + 1];
                packet.destPort = (data[udpOffset + 2] << 8) | data[udpOffset + 3];
                uint16_t udpLength = (data[udpOffset + 4] << 8) | data[udpOffset + 5];
                
                // Verify UDP length is reasonable
                if (udpLength <= length - udpOffset && udpLength >= 8) {
                    // Check if this might be RTP (UDP payload)
                    size_t rtpOffset = udpOffset + 8;
                    if (length >= rtpOffset + 12) { // Minimum RTP header size
                        uint8_t rtpVersion = (data[rtpOffset] >> 6) & 0x03;
                        if (rtpVersion == 2) {
                            packet.isRTP = true;
                            packet.payloadType = data[rtpOffset + 1] & 0x7F;
                            packet.sequenceNumber = (data[rtpOffset + 2] << 8) | data[rtpOffset + 3];
                            
                            // Extract RTP timestamp safely
                            if (length >= rtpOffset + 8) {
                                packet.rtpTimestamp = (data[rtpOffset + 4] << 24) | 
                                                    (data[rtpOffset + 5] << 16) | 
                                                    (data[rtpOffset + 6] << 8) | 
                                                    data[rtpOffset + 7];
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Add packet to analysis queue
    addPacket(packet);
}

void PacketAnalyzer::addPacket(const PacketInfo& packet) {
    // Memory health check before processing
    if (!MemoryManager::checkHeapHealth()) {
        Serial.println("WARNING: Low memory, skipping packet analysis");
        return;
    }
    
    // Thread-safe access to metrics
    if (metricsMutex && xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100))) {
        // Update total bytes and packet count
        totalBytesReceived += packet.length;
        currentMetrics.totalPackets++;
        
        // Process RTP packets for video analysis
        if (packet.isRTP && isVideoPacket(packet)) {
            analyzeRTPPacket(packet);
        }
        
        xSemaphoreGive(metricsMutex);
    }
    
    // Update metrics periodically
    uint32_t currentTime = getCurrentTime();
    if (currentTime - lastUpdateTime >= METRICS_UPDATE_INTERVAL) {
        updateMetrics();
        lastUpdateTime = currentTime;
    }
}

void PacketAnalyzer::analyzeRTPPacket(const PacketInfo& packet) {
    // Store sequence number for packet loss analysis using circular buffer
    if (sequenceNumberBuffer) {
        sequenceNumberBuffer->push(packet.sequenceNumber);
    }
    
    // Store timestamps for jitter calculation using circular buffers
    if (rtpTimestampBuffer && arrivalTimeBuffer) {
        rtpTimestampBuffer->push(packet.rtpTimestamp);
        arrivalTimeBuffer->push(getCurrentTime());
    }
}

void PacketAnalyzer::calculateJitter() {
    if (!rtpTimestampBuffer || !arrivalTimeBuffer) {
        currentMetrics.jitter = 0.0;
        return;
    }
    
    size_t bufferSize = rtpTimestampBuffer->size();
    if (bufferSize < 2) {
        currentMetrics.jitter = 0.0;
        return;
    }
    
    // Get the appropriate clock rate based on payload type
    uint32_t clockRate = getClockRateForPayloadType(lastProcessedPayloadType);
    
    std::vector<float> jitterSamples;
    jitterSamples.reserve(bufferSize - 1);
    
    for (size_t i = 1; i < bufferSize; i++) {
        // Calculate inter-arrival time difference
        uint32_t rtpDiff = (*rtpTimestampBuffer)[i] - (*rtpTimestampBuffer)[i-1];
        uint32_t arrivalDiff = (*arrivalTimeBuffer)[i] - (*arrivalTimeBuffer)[i-1];
        
        // Convert to milliseconds using correct clock rate
        float rtpDiffMs = (float)rtpDiff * 1000.0f / clockRate;
        float arrivalDiffMs = arrivalDiff;
        
        // Handle timestamp wraparound
        if (rtpDiff > (UINT32_MAX / 2)) {
            rtpDiff = UINT32_MAX - rtpDiff;
            rtpDiffMs = (float)rtpDiff * 1000.0f / clockRate;
        }
        
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

uint32_t PacketAnalyzer::getClockRateForPayloadType(uint8_t payloadType) {
    // Fixed: Use correct clock rates for different payload types
    switch (payloadType) {
        case HIKVISION_H264_PT:
            return H264_CLOCK_RATE;
        case HIKVISION_H265_PT:
            return H265_CLOCK_RATE;
        case HIKVISION_MJPEG_PT:
            return MJPEG_CLOCK_RATE;
        case HIKVISION_SMART_PT:
            return H264_CLOCK_RATE; // Assume H264 base for SMART codec
        default:
            if (payloadType >= 96 && payloadType <= 127) {
                return H264_CLOCK_RATE; // Dynamic payload type, assume H264
            }
            return MJPEG_CLOCK_RATE; // Default fallback
    }
}

void PacketAnalyzer::calculateDelay() {
    if (!arrivalTimeBuffer || arrivalTimeBuffer->empty()) {
        currentMetrics.delay = 0.0;
        return;
    }
    
    uint32_t currentTime = getCurrentTime();
    size_t bufferSize = arrivalTimeBuffer->size();
    
    // Calculate average processing delay
    uint32_t totalDelay = 0;
    size_t validSamples = 0;
    
    for (size_t i = 0; i < bufferSize; i++) {
        uint32_t packetTime = (*arrivalTimeBuffer)[i];
        if (currentTime >= packetTime) {
            totalDelay += (currentTime - packetTime);
            validSamples++;
        }
    }
    
    if (validSamples > 0) {
        currentMetrics.delay = (float)totalDelay / validSamples;
    } else {
        currentMetrics.delay = 0.0;
    }
}

void PacketAnalyzer::calculateLatency() {
    // Improved latency calculation
    currentMetrics.latency = (currentMetrics.jitter * 0.6f) + (currentMetrics.delay * 0.4f);
}

void PacketAnalyzer::calculateBitrate() {
    uint32_t currentTime = getCurrentTime();
    uint32_t timeDiff = currentTime - lastBitrateUpdate;
    
    if (timeDiff >= 1000) { // Update every second
        uint32_t bytesDiff = totalBytesReceived - lastBytesReceived;
        
        // Prevent division by zero
        if (timeDiff > 0) {
            currentMetrics.bitrate = (bytesDiff * 8.0f) / (timeDiff / 1000.0f) / 1000000.0f; // Mbps
        } else {
            currentMetrics.bitrate = 0.0f;
        }
        
        lastBytesReceived = totalBytesReceived;
        lastBitrateUpdate = currentTime;
    }
}

void PacketAnalyzer::calculatePacketLoss() {
    if (!sequenceNumberBuffer || sequenceNumberBuffer->size() < 2) {
        currentMetrics.packetLoss = 0.0;
        return;
    }
    
    // Improved packet loss calculation with proper sequence number analysis
    std::vector<uint16_t> seqNumbers;
    seqNumbers.reserve(sequenceNumberBuffer->size());
    
    // Copy sequence numbers from circular buffer
    for (size_t i = 0; i < sequenceNumberBuffer->size(); i++) {
        seqNumbers.push_back((*sequenceNumberBuffer)[i]);
    }
    
    if (seqNumbers.size() < 2) {
        currentMetrics.packetLoss = 0.0;
        return;
    }
    
    // Handle sequence number wraparound
    std::sort(seqNumbers.begin(), seqNumbers.end());
    
    uint32_t expectedPackets = 0;
    uint32_t lostPackets = 0;
    
    for (size_t i = 1; i < seqNumbers.size(); i++) {
        uint16_t diff = seqNumbers[i] - seqNumbers[i-1];
        
        // Handle wraparound case
        if (diff > 32768) {
            diff = (65536 - seqNumbers[i-1]) + seqNumbers[i];
        }
        
        expectedPackets += diff;
        if (diff > 1) {
            lostPackets += (diff - 1);
        }
    }
    
    if (expectedPackets > 0) {
        currentMetrics.lostPackets = lostPackets;
        currentMetrics.packetLoss = ((float)lostPackets / expectedPackets) * 100.0f;
        
        // Sanity check - packet loss shouldn't exceed 100%
        if (currentMetrics.packetLoss > 100.0f) {
            currentMetrics.packetLoss = 100.0f;
        }
    } else {
        currentMetrics.packetLoss = 0.0f;
    }
}

void PacketAnalyzer::updateMetrics() {
    if (metricsMutex && xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(500))) {
        calculateJitter();
        calculateDelay();
        calculateLatency();
        calculateBitrate();
        calculatePacketLoss();
        
        currentMetrics.timestamp = getCurrentTime();
        
        xSemaphoreGive(metricsMutex);
        
        #if DEBUG_PACKETS
        printDiagnostics();
        #endif
        
        // Memory health check
        #if DEBUG_MEMORY
        MemoryManager::printMemoryStats();
        #endif
    }
}

bool PacketAnalyzer::isVideoPacket(const PacketInfo& packet) {
    if (!packet.isRTP) return false;
    
    // Enhanced video packet detection for HikVision cameras
    switch (packet.payloadType) {
        case HIKVISION_H264_PT:
        case HIKVISION_H265_PT:
        case HIKVISION_SMART_PT:
        case HIKVISION_MJPEG_PT:
            lastProcessedPayloadType = packet.payloadType;
            return true;
        default:
            // Dynamic payload types (96-127) are likely video
            if (packet.payloadType >= 96 && packet.payloadType <= 127) {
                lastProcessedPayloadType = packet.payloadType;
                return true;
            }
            return false;
    }
}

uint32_t PacketAnalyzer::getCurrentTime() {
    return millis();
}

bool PacketAnalyzer::hasNewData() const {
    if (metricsMutex && xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100))) {
        bool hasNew = (getCurrentTime() - currentMetrics.timestamp) < (METRICS_UPDATE_INTERVAL * 2);
        xSemaphoreGive(metricsMutex);
        return hasNew;
    }
    return false;
}

PacketAnalyzer::Metrics PacketAnalyzer::getMetrics() const {
    Metrics metrics = {0};
    
    if (metricsMutex && xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100))) {
        metrics = currentMetrics;
        xSemaphoreGive(metricsMutex);
    }
    
    return metrics;
}

void PacketAnalyzer::printDiagnostics() const {
    Serial.println("=== Enhanced Packet Analyzer Diagnostics ===");
    Serial.printf("Total Packets: %u\n", currentMetrics.totalPackets);
    Serial.printf("Lost Packets: %u\n", currentMetrics.lostPackets);
    Serial.printf("Jitter: %.2f ms\n", currentMetrics.jitter);
    Serial.printf("Delay: %.2f ms\n", currentMetrics.delay);
    Serial.printf("Latency: %.2f ms\n", currentMetrics.latency);
    Serial.printf("Bitrate: %.2f Mbps\n", currentMetrics.bitrate);
    Serial.printf("Packet Loss: %.2f%%\n", currentMetrics.packetLoss);
    
    if (rtpTimestampBuffer && arrivalTimeBuffer && sequenceNumberBuffer) {
        Serial.printf("RTP Buffer: %u/%u\n", rtpTimestampBuffer->size(), RTP_TIMESTAMP_BUFFER_SIZE);
        Serial.printf("Arrival Buffer: %u/%u\n", arrivalTimeBuffer->size(), ARRIVAL_TIME_BUFFER_SIZE);
        Serial.printf("Sequence Buffer: %u/%u\n", sequenceNumberBuffer->size(), PACKET_HISTORY_BUFFER_SIZE);
    }
    
    Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.println("============================================");
}

String PacketAnalyzer::getAnalysisReport() const {
    Metrics metrics = getMetrics();
    
    String report = "Enhanced Packet Analysis Report:\n";
    report += "Total Packets: " + String(metrics.totalPackets) + "\n";
    report += "Lost Packets: " + String(metrics.lostPackets) + "\n";
    report += "Jitter: " + String(metrics.jitter, 2) + " ms\n";
    report += "Delay: " + String(metrics.delay, 2) + " ms\n";
    report += "Latency: " + String(metrics.latency, 2) + " ms\n";
    report += "Bitrate: " + String(metrics.bitrate, 2) + " Mbps\n";
    report += "Packet Loss: " + String(metrics.packetLoss, 2) + "%\n";
    report += "Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";
    return report;
}