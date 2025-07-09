// ESP32-S3 CCTV Monitor - Dashboard JavaScript

// Global variables
let socket;
let metricsChart;
let connectionStatus = 'disconnected';
let metricsData = {
    jitter: [],
    delay: [],
    latency: [],
    bitrate: [],
    packetLoss: [],
    timestamps: []
};

// Configuration
const CONFIG = {
    WEBSOCKET_PORT: 81,
    RECONNECT_INTERVAL: 3000,
    CHART_MAX_POINTS: 20,
    UPDATE_INTERVAL: 1000,
    METRICS_HISTORY_SIZE: 100
};

// Thresholds for quality assessment
const THRESHOLDS = {
    jitter: { good: 20, warning: 50 },
    delay: { good: 100, warning: 200 },
    latency: { good: 50, warning: 100 },
    packetLoss: { good: 0.5, warning: 2.0 },
    bitrate: { good: 1.0, warning: 0.5 }
};

// Initialize dashboard when DOM is loaded
document.addEventListener('DOMContentLoaded', function() {
    console.log('ESP32-S3 CCTV Monitor Dashboard initializing...');
    
    initializeDashboard();
    initializeWebSocket();
    initializeChart();
    startPeriodicUpdates();
});

// Initialize dashboard components
function initializeDashboard() {
    updateConnectionStatus('Initializing...');
    
    // Add event listeners for interactive elements
    setupEventListeners();
    
    // Initialize metrics display
    initializeMetricsDisplay();
    
    console.log('Dashboard initialized');
}

// Setup event listeners
function setupEventListeners() {
    // Add refresh button if it exists
    const refreshButton = document.getElementById('refreshButton');
    if (refreshButton) {
        refreshButton.addEventListener('click', function() {
            refreshMetrics();
        });
    }
    
    // Add fullscreen toggle for video
    const videoContainer = document.querySelector('.video-placeholder');
    if (videoContainer) {
        videoContainer.addEventListener('click', function() {
            toggleFullscreen(videoContainer);
        });
    }
    
    // Handle window resize for chart responsiveness
    window.addEventListener('resize', function() {
        if (metricsChart) {
            metricsChart.resize();
        }
    });
    
    // Handle visibility change to pause updates when tab is hidden
    document.addEventListener('visibilitychange', function() {
        if (document.hidden) {
            pauseUpdates();
        } else {
            resumeUpdates();
        }
    });
}

// Initialize metrics display with default values
function initializeMetricsDisplay() {
    const defaultMetrics = {
        jitter: 0.0,
        delay: 0.0,
        latency: 0.0,
        bitrate: 0.0,
        packetLoss: 0.0
    };
    
    updateMetricsDisplay(defaultMetrics);
}

// Initialize WebSocket connection
function initializeWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const host = window.location.hostname;
    const port = window.location.port ? ':' + window.location.port : '';
    const wsUrl = `${protocol}//${host}:${CONFIG.WEBSOCKET_PORT}`;
    
    console.log('Connecting to WebSocket:', wsUrl);
    
    try {
        socket = new WebSocket(wsUrl);
        
        socket.onopen = function(event) {
            console.log('WebSocket connected successfully');
            updateConnectionStatus('Connected');
            connectionStatus = 'connected';
            
            // Request initial metrics
            requestMetrics();
        };
        
        socket.onmessage = function(event) {
            try {
                const data = JSON.parse(event.data);
                handleWebSocketMessage(data);
            } catch (error) {
                console.error('Error parsing WebSocket message:', error, event.data);
            }
        };
        
        socket.onclose = function(event) {
            console.log('WebSocket disconnected:', event.code, event.reason);
            updateConnectionStatus('Disconnected');
            connectionStatus = 'disconnected';
            
            // Attempt to reconnect
            setTimeout(function() {
                if (connectionStatus === 'disconnected') {
                    updateConnectionStatus('Reconnecting...');
                    initializeWebSocket();
                }
            }, CONFIG.RECONNECT_INTERVAL);
        };
        
        socket.onerror = function(error) {
            console.error('WebSocket error:', error);
            updateConnectionStatus('Connection Error');
            connectionStatus = 'error';
        };
        
    } catch (error) {
        console.error('Failed to create WebSocket connection:', error);
        updateConnectionStatus('Connection Failed');
        connectionStatus = 'error';
        
        // Fallback to HTTP polling
        startHttpPolling();
    }
}

// Handle WebSocket messages
function handleWebSocketMessage(data) {
    if (data.jitter !== undefined) {
        // This is a metrics update
        updateMetricsDisplay(data);
        updateChart(data);
        
        // Store metrics for history
        storeMetricsHistory(data);
    } else if (data.status) {
        // This is a status update
        handleStatusUpdate(data);
    } else {
        console.log('Unknown WebSocket message:', data);
    }
}

// Request metrics from ESP32
function requestMetrics() {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send('get_metrics');
    }
}

// Update connection status display
function updateConnectionStatus(status) {
    const statusElement = document.getElementById('connectionStatus');
    if (statusElement) {
        statusElement.textContent = status;
        
        // Update status indicator class
        const statusClasses = {
            'Connected': 'connected',
            'Disconnected': 'disconnected',
            'Reconnecting...': 'connecting',
            'Connection Error': 'error',
            'Connection Failed': 'error',
            'Initializing...': 'connecting'
        };
        
        statusElement.className = 'connection-status ' + (statusClasses[status] || 'default');
    }
}

// Update metrics display
function updateMetricsDisplay(data) {
    // Update individual metric values
    updateElement('jitterValue', data.jitter, 1, 'ms');
    updateElement('delayValue', data.delay, 1, 'ms');
    updateElement('latencyValue', data.latency, 1, 'ms');
    updateElement('bitrateValue', data.bitrate, 2, 'Mbps');
    updateElement('packetLossValue', data.packetLoss, 2, '%');
    
    // Update status indicators
    updateMetricStatus('jitter', data.jitter, THRESHOLDS.jitter);
    updateMetricStatus('delay', data.delay, THRESHOLDS.delay);
    updateMetricStatus('latency', data.latency, THRESHOLDS.latency);
    updateMetricStatus('bitrate', data.bitrate, THRESHOLDS.bitrate, true);
    updateMetricStatus('packetLoss', data.packetLoss, THRESHOLDS.packetLoss);
    
    // Update last update time
    const now = new Date();
    const lastUpdateElement = document.getElementById('lastUpdate');
    if (lastUpdateElement) {
        lastUpdateElement.textContent = now.toLocaleTimeString();
    }
}

// Update individual metric element
function updateElement(elementId, value, decimals, unit) {
    const element = document.getElementById(elementId);
    if (element) {
        element.textContent = value.toFixed(decimals);
        
        // Add unit if specified
        if (unit) {
            const unitElement = element.nextElementSibling;
            if (unitElement && unitElement.classList.contains('unit')) {
                unitElement.textContent = unit;
            }
        }
    }
}

// Update metric status indicator
function updateMetricStatus(metric, value, thresholds, higherIsBetter = false) {
    const statusElement = document.getElementById(metric + 'Status');
    if (!statusElement) return;
    
    let status = 'Good';
    let className = '';
    
    if (higherIsBetter) {
        if (value < thresholds.warning) {
            status = 'Poor';
            className = 'error';
        } else if (value < thresholds.good) {
            status = 'Fair';
            className = 'warning';
        }
    } else {
        if (value > thresholds.warning) {
            status = 'Poor';
            className = 'error';
        } else if (value > thresholds.good) {
            status = 'Warning';
            className = 'warning';
        }
    }
    
    statusElement.textContent = status;
    statusElement.className = 'metric-status ' + className;
}

// Initialize Chart.js chart
function initializeChart() {
    const ctx = document.getElementById('metricsChart');
    if (!ctx) {
        console.warn('Chart canvas not found');
        return;
    }
    
    const chartCtx = ctx.getContext('2d');
    
    metricsChart = new Chart(chartCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                {
                    label: 'Jitter (ms)',
                    data: [],
                    borderColor: '#ef4444',
                    backgroundColor: 'rgba(239, 68, 68, 0.1)',
                    tension: 0.4,
                    pointRadius: 3,
                    pointHoverRadius: 6,
                    borderWidth: 2
                },
                {
                    label: 'Delay (ms)',
                    data: [],
                    borderColor: '#3b82f6',
                    backgroundColor: 'rgba(59, 130, 246, 0.1)',
                    tension: 0.4,
                    pointRadius: 3,
                    pointHoverRadius: 6,
                    borderWidth: 2
                },
                {
                    label: 'Latency (ms)',
                    data: [],
                    borderColor: '#f59e0b',
                    backgroundColor: 'rgba(245, 158, 11, 0.1)',
                    tension: 0.4,
                    pointRadius: 3,
                    pointHoverRadius: 6,
                    borderWidth: 2
                },
                {
                    label: 'Packet Loss (%)',
                    data: [],
                    borderColor: '#10b981',
                    backgroundColor: 'rgba(16, 185, 129, 0.1)',
                    tension: 0.4,
                    pointRadius: 3,
                    pointHoverRadius: 6,
                    borderWidth: 2,
                    yAxisID: 'y1'
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                mode: 'index',
                intersect: false,
            },
            scales: {
                x: {
                    display: true,
                    title: {
                        display: true,
                        text: 'Time',
                        font: {
                            size: 12,
                            weight: 'bold'
                        }
                    },
                    grid: {
                        display: true,
                        color: 'rgba(0, 0, 0, 0.1)'
                    }
                },
                y: {
                    type: 'linear',
                    display: true,
                    position: 'left',
                    title: {
                        display: true,
                        text: 'Time (ms)',
                        font: {
                            size: 12,
                            weight: 'bold'
                        }
                    },
                    grid: {
                        display: true,
                        color: 'rgba(0, 0, 0, 0.1)'
                    },
                    beginAtZero: true
                },
                y1: {
                    type: 'linear',
                    display: true,
                    position: 'right',
                    title: {
                        display: true,
                        text: 'Packet Loss (%)',
                        font: {
                            size: 12,
                            weight: 'bold'
                        }
                    },
                    grid: {
                        drawOnChartArea: false,
                    },
                    beginAtZero: true
                }
            },
            plugins: {
                title: {
                    display: true,
                    text: 'Network Quality Metrics - Real-time',
                    font: {
                        size: 16,
                        weight: 'bold'
                    }
                },
                legend: {
                    display: true,
                    position: 'top'
                },
                tooltip: {
                    backgroundColor: 'rgba(0, 0, 0, 0.8)',
                    titleColor: 'white',
                    bodyColor: 'white',
                    borderColor: '#2563eb',
                    borderWidth: 1
                }
            },
            animation: {
                duration: 300
            }
        }
    });
    
    console.log('Chart initialized');
}

// Update chart with new data
function updateChart(data) {
    if (!metricsChart) return;
    
    const now = new Date();
    const timeLabel = now.toLocaleTimeString();
    
    // Add new data point
    metricsChart.data.labels.push(timeLabel);
    metricsChart.data.datasets[0].data.push(data.jitter);
    metricsChart.data.datasets[1].data.push(data.delay);
    metricsChart.data.datasets[2].data.push(data.latency);
    metricsChart.data.datasets[3].data.push(data.packetLoss);
    
    // Keep only the last N data points
    if (metricsChart.data.labels.length > CONFIG.CHART_MAX_POINTS) {
        metricsChart.data.labels.shift();
        metricsChart.data.datasets.forEach(dataset => {
            dataset.data.shift();
        });
    }
    
    // Update chart
    metricsChart.update('none');
}

// Store metrics history
function storeMetricsHistory(data) {
    const timestamp = Date.now();
    
    // Add to history
    metricsData.timestamps.push(timestamp);
    metricsData.jitter.push(data.jitter);
    metricsData.delay.push(data.delay);
    metricsData.latency.push(data.latency);
    metricsData.bitrate.push(data.bitrate);
    metricsData.packetLoss.push(data.packetLoss);
    
    // Keep only recent history
    if (metricsData.timestamps.length > CONFIG.METRICS_HISTORY_SIZE) {
        metricsData.timestamps.shift();
        metricsData.jitter.shift();
        metricsData.delay.shift();
        metricsData.latency.shift();
        metricsData.bitrate.shift();
        metricsData.packetLoss.shift();
    }
}

// Start periodic updates
function startPeriodicUpdates() {
    // Request metrics every second if WebSocket is connected
    setInterval(function() {
        if (socket && socket.readyState === WebSocket.OPEN) {
            requestMetrics();
        }
    }, CONFIG.UPDATE_INTERVAL);
}

// Fallback HTTP polling when WebSocket is not available
function startHttpPolling() {
    console.log('Starting HTTP polling fallback');
    
    function pollMetrics() {
        fetch('/api/metrics')
            .then(response => response.json())
            .then(data => {
                updateMetricsDisplay(data);
                updateChart(data);
                storeMetricsHistory(data);
                
                if (connectionStatus === 'error') {
                    updateConnectionStatus('HTTP Polling');
                }
            })
            .catch(error => {
                console.error('HTTP polling error:', error);
                updateConnectionStatus('Connection Error');
            });
    }
    
    // Poll every 2 seconds
    setInterval(pollMetrics, 2000);
    
    // Initial poll
    pollMetrics();
}

// Refresh metrics manually
function refreshMetrics() {
    if (socket && socket.readyState === WebSocket.OPEN) {
        requestMetrics();
    } else {
        // Fallback to HTTP
        fetch('/api/metrics')
            .then(response => response.json())
            .then(data => {
                updateMetricsDisplay(data);
                updateChart(data);
                storeMetricsHistory(data);
            })
            .catch(error => {
                console.error('Error refreshing metrics:', error);
            });
    }
}

// Toggle fullscreen for video container
function toggleFullscreen(element) {
    if (!document.fullscreenElement) {
        element.requestFullscreen().catch(err => {
            console.error('Error attempting to enable fullscreen:', err);
        });
    } else {
        document.exitFullscreen();
    }
}

// Pause updates when tab is hidden
function pauseUpdates() {
    console.log('Pausing updates - tab hidden');
    // Could implement update pausing logic here
}

// Resume updates when tab is visible
function resumeUpdates() {
    console.log('Resuming updates - tab visible');
    // Request fresh metrics
    refreshMetrics();
}

// Handle status updates
function handleStatusUpdate(data) {
    console.log('Status update received:', data);
    
    // Update system status if provided
    if (data.system) {
        updateSystemStatus(data.system);
    }
    
    // Update network status if provided
    if (data.network) {
        updateNetworkStatus(data.network);
    }
}

// Update system status
function updateSystemStatus(systemData) {
    const uptimeElement = document.getElementById('systemUptime');
    if (uptimeElement && systemData.uptime) {
        uptimeElement.textContent = formatUptime(systemData.uptime);
    }
    
    const memoryElement = document.getElementById('systemMemory');
    if (memoryElement && systemData.freeHeap) {
        memoryElement.textContent = formatBytes(systemData.freeHeap);
    }
}

// Update network status
function updateNetworkStatus(networkData) {
    const ipElement = document.getElementById('networkIP');
    if (ipElement && networkData.ip) {
        ipElement.textContent = networkData.ip;
    }
    
    const rssiElement = document.getElementById('networkRSSI');
    if (rssiElement && networkData.rssi) {
        rssiElement.textContent = networkData.rssi + ' dBm';
    }
}

// Utility function to format uptime
function formatUptime(seconds) {
    if (seconds < 60) return seconds + 's';
    if (seconds < 3600) return Math.floor(seconds / 60) + 'm ' + (seconds % 60) + 's';
    
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    return hours + 'h ' + minutes + 'm';
}

// Utility function to format bytes
function formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

// Export metrics data
function exportMetricsData() {
    const data = {
        timestamp: new Date().toISOString(),
        metrics: metricsData,
        summary: calculateMetricsSummary()
    };
    
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    
    const a = document.createElement('a');
    a.href = url;
    a.download = `cctv_metrics_${new Date().toISOString().slice(0, 19).replace(/:/g, '-')}.json`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
}

// Calculate metrics summary
function calculateMetricsSummary() {
    if (metricsData.jitter.length === 0) return {};
    
    const summary = {
        jitter: {
            avg: average(metricsData.jitter),
            min: Math.min(...metricsData.jitter),
            max: Math.max(...metricsData.jitter)
        },
        delay: {
            avg: average(metricsData.delay),
            min: Math.min(...metricsData.delay),
            max: Math.max(...metricsData.delay)
        },
        latency: {
            avg: average(metricsData.latency),
            min: Math.min(...metricsData.latency),
            max: Math.max(...metricsData.latency)
        },
        bitrate: {
            avg: average(metricsData.bitrate),
            min: Math.min(...metricsData.bitrate),
            max: Math.max(...metricsData.bitrate)
        },
        packetLoss: {
            avg: average(metricsData.packetLoss),
            min: Math.min(...metricsData.packetLoss),
            max: Math.max(...metricsData.packetLoss)
        }
    };
    
    return summary;
}

// Calculate average
function average(array) {
    return array.reduce((a, b) => a + b, 0) / array.length;
}

// Cleanup on page unload
window.addEventListener('beforeunload', function() {
    if (socket) {
        socket.close();
    }
});

// Error handling
window.addEventListener('error', function(event) {
    console.error('JavaScript error:', event.error);
});

// Console logging for debugging
console.log('ESP32-S3 CCTV Monitor Dashboard script loaded');
