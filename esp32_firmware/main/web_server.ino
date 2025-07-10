#include "web_server.h"
#include "config_fixed.h"  // Use fixed config
#include <ArduinoJson.h>

// External variables from main.cpp
extern VideoMetrics currentMetrics;
extern SemaphoreHandle_t metricsMutex;

CustomWebServer::CustomWebServer() : server(nullptr), webSocket(nullptr) {
}

CustomWebServer::~CustomWebServer() {
    stop();
}

bool CustomWebServer::begin(WebServer* webServer, WebSocketsServer* webSocketServer) {
    server = webServer;
    webSocket = webSocketServer;
    
    if (!server || !webSocket) {
        Serial.println("Invalid server or websocket pointer");
        return false;
    }
    
    setupRoutes();
    Serial.println("Web server routes configured");
    return true;
}

void CustomWebServer::setupRoutes() {
    if (!server) return;
    
    // Main routes
    server->on("/", [this]() { handleRoot(); });
    server->on("/dashboard", [this]() { handleDashboard(); });
    server->on("/api/metrics", [this]() { handleMetrics(); });
    server->on("/api/status", [this]() { handleAPI(); });
    
    // Static file routes
    server->on("/style.css", [this]() { handleCSS(); });
    server->on("/script.js", [this]() { handleJS(); });
    
    // Catch-all for file requests
    server->onNotFound([this]() {
        if (!handleFileRead(server->uri())) {
            handleNotFound();
        }
    });
}

void CustomWebServer::handleRoot() {
    String html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-S3 CCTV Monitor</title>
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <div class="container">
        <header>
            <h1>ESP32-S3 CCTV Monitor</h1>
            <p>Real-time video quality analysis and monitoring</p>
        </header>
        
        <main>
            <div class="status-card">
                <h2>System Status</h2>
                <div class="status-grid">
                    <div class="status-item">
                        <span class="label">WiFi Status:</span>
                        <span class="value" id="wifiStatus">Connected</span>
                    </div>
                    <div class="status-item">
                        <span class="label">IP Address:</span>
                        <span class="value" id="ipAddress">)" + WiFi.localIP().toString() + R"(</span>
                    </div>
                    <div class="status-item">
                        <span class="label">Monitoring:</span>
                        <span class="value active" id="monitoringStatus">Active</span>
                    </div>
                </div>
            </div>
            
            <div class="navigation">
                <a href="/dashboard" class="btn btn-primary">Open Dashboard</a>
                <button onclick="refreshStatus()" class="btn btn-secondary">Refresh Status</button>
            </div>
        </main>
    </div>
    
    <script>
        function refreshStatus() {
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('wifiStatus').textContent = data.wifi ? 'Connected' : 'Disconnected';
                    document.getElementById('ipAddress').textContent = data.ip;
                    document.getElementById('monitoringStatus').textContent = data.monitoring ? 'Active' : 'Inactive';
                })
                .catch(error => console.error('Error:', error));
        }
        
        // Auto-refresh every 5 seconds
        setInterval(refreshStatus, 5000);
    </script>
</body>
</html>
)";
    
    server->send(200, "text/html", html);
}

void CustomWebServer::handleDashboard() {
    String html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>CCTV Monitor Dashboard</title>
    <link rel="stylesheet" href="style.css">
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
    <div class="dashboard">
        <header class="dashboard-header">
            <h1>CCTV Monitor Dashboard</h1>
            <div class="connection-status">
                <span id="connectionStatus">Connecting...</span>
            </div>
        </header>
        
        <div class="dashboard-content">
            <div class="video-section">
                <div class="video-container">
                    <h2>Live Video Feed</h2>
                    <div class="video-placeholder">
                        <p>Video Stream</p>
                        <p>Connect to your DVR/CCTV system</p>
                    </div>
                </div>
            </div>
            
            <div class="metrics-section">
                <div class="metrics-grid">
                    <div class="metric-card">
                        <h3>Jitter</h3>
                        <div class="metric-value">
                            <span id="jitterValue">0.0</span>
                            <span class="unit">ms</span>
                        </div>
                        <div class="metric-status" id="jitterStatus">Good</div>
                    </div>
                    
                    <div class="metric-card">
                        <h3>Delay</h3>
                        <div class="metric-value">
                            <span id="delayValue">0.0</span>
                            <span class="unit">ms</span>
                        </div>
                        <div class="metric-status" id="delayStatus">Good</div>
                    </div>
                    
                    <div class="metric-card">
                        <h3>Latency</h3>
                        <div class="metric-value">
                            <span id="latencyValue">0.0</span>
                            <span class="unit">ms</span>
                        </div>
                        <div class="metric-status" id="latencyStatus">Good</div>
                    </div>
                    
                    <div class="metric-card">
                        <h3>Bitrate</h3>
                        <div class="metric-value">
                            <span id="bitrateValue">0.0</span>
                            <span class="unit">Mbps</span>
                        </div>
                        <div class="metric-status" id="bitrateStatus">Good</div>
                    </div>
                    
                    <div class="metric-card">
                        <h3>Packet Loss</h3>
                        <div class="metric-value">
                            <span id="packetLossValue">0.0</span>
                            <span class="unit">%</span>
                        </div>
                        <div class="metric-status" id="packetLossStatus">Good</div>
                    </div>
                </div>
                
                <div class="chart-container">
                    <canvas id="metricsChart"></canvas>
                </div>
            </div>
        </div>
    </div>
    
    <script src="script.js"></script>
</body>
</html>
)";
    
    server->send(200, "text/html", html);
}

void CustomWebServer::handleMetrics() {
    String json = generateMetricsJSON();
    server->send(200, "application/json", json);
}

void CustomWebServer::handleAPI() {
    JsonDocument doc;
    
    doc["wifi"] = WiFi.status() == WL_CONNECTED;
    doc["ip"] = WiFi.localIP().toString();
    doc["monitoring"] = true;
    doc["uptime"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["timestamp"] = millis();
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    server->send(200, "application/json", jsonString);
}

void CustomWebServer::handleCSS() {
    String css = R"(
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    min-height: 100vh;
    color: #333;
}

.container {
    max-width: 1200px;
    margin: 0 auto;
    padding: 20px;
}

header {
    text-align: center;
    margin-bottom: 30px;
    color: white;
}

header h1 {
    font-size: 2.5rem;
    margin-bottom: 10px;
}

header p {
    font-size: 1.2rem;
    opacity: 0.9;
}

.status-card {
    background: white;
    border-radius: 15px;
    padding: 30px;
    margin-bottom: 30px;
    box-shadow: 0 10px 30px rgba(0,0,0,0.1);
}

.status-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
    gap: 20px;
    margin-top: 20px;
}

.status-item {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 15px;
    background: #f8f9fa;
    border-radius: 10px;
}

.label {
    font-weight: 600;
    color: #666;
}

.value {
    font-weight: 700;
    color: #333;
}

.value.active {
    color: #28a745;
}

.navigation {
    text-align: center;
}

.btn {
    display: inline-block;
    padding: 15px 30px;
    margin: 0 10px;
    border: none;
    border-radius: 25px;
    font-size: 1rem;
    font-weight: 600;
    text-decoration: none;
    cursor: pointer;
    transition: all 0.3s ease;
}

.btn-primary {
    background: #007bff;
    color: white;
}

.btn-primary:hover {
    background: #0056b3;
    transform: translateY(-2px);
}

.btn-secondary {
    background: #6c757d;
    color: white;
}

.btn-secondary:hover {
    background: #545b62;
    transform: translateY(-2px);
}

/* Dashboard Styles */
.dashboard {
    min-height: 100vh;
    background: #f8f9fa;
}

.dashboard-header {
    background: white;
    padding: 20px;
    box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    display: flex;
    justify-content: space-between;
    align-items: center;
}

.connection-status {
    padding: 8px 16px;
    border-radius: 20px;
    font-weight: 600;
    background: #28a745;
    color: white;
}

.dashboard-content {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 30px;
    padding: 30px;
    max-width: 1400px;
    margin: 0 auto;
}

.video-section {
    background: white;
    border-radius: 15px;
    padding: 20px;
    box-shadow: 0 5px 20px rgba(0,0,0,0.1);
}

.video-container h2 {
    margin-bottom: 20px;
    color: #333;
}

.video-placeholder {
    aspect-ratio: 16/9;
    background: #000;
    border-radius: 10px;
    display: flex;
    flex-direction: column;
    justify-content: center;
    align-items: center;
    color: white;
    font-size: 1.2rem;
}

.metrics-section {
    display: flex;
    flex-direction: column;
    gap: 20px;
}

.metrics-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 20px;
}

.metric-card {
    background: white;
    border-radius: 15px;
    padding: 20px;
    text-align: center;
    box-shadow: 0 5px 20px rgba(0,0,0,0.1);
    transition: transform 0.3s ease;
}

.metric-card:hover {
    transform: translateY(-5px);
}

.metric-card h3 {
    margin-bottom: 15px;
    color: #666;
    font-size: 1rem;
}

.metric-value {
    font-size: 2rem;
    font-weight: 700;
    color: #333;
    margin-bottom: 10px;
}

.unit {
    font-size: 1rem;
    color: #666;
    font-weight: 400;
}

.metric-status {
    padding: 5px 10px;
    border-radius: 15px;
    font-size: 0.9rem;
    font-weight: 600;
    background: #28a745;
    color: white;
    display: inline-block;
}

.metric-status.warning {
    background: #ffc107;
    color: #333;
}

.metric-status.error {
    background: #dc3545;
    color: white;
}

.chart-container {
    background: white;
    border-radius: 15px;
    padding: 20px;
    box-shadow: 0 5px 20px rgba(0,0,0,0.1);
    height: 300px;
}

@media (max-width: 768px) {
    .dashboard-content {
        grid-template-columns: 1fr;
        padding: 20px;
    }
    
    .metrics-grid {
        grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
    }
}
)";
    
    server->send(200, "text/css", css);
}

void CustomWebServer::handleJS() {
    String js = R"(
// WebSocket connection
let socket;
let metricsChart;
let metricsData = {
    jitter: [],
    delay: [],
    latency: [],
    bitrate: [],
    packetLoss: []
};

// Initialize dashboard
document.addEventListener('DOMContentLoaded', function() {
    initWebSocket();
    initChart();
    updateConnectionStatus('Connecting...');
});

function initWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const host = window.location.hostname;
    const wsUrl = `${protocol}//${host}:81`;
    
    socket = new WebSocket(wsUrl);
    
    socket.onopen = function(event) {
        console.log('WebSocket connected');
        updateConnectionStatus('Connected');
        socket.send('get_metrics');
    };
    
    socket.onmessage = function(event) {
        try {
            const data = JSON.parse(event.data);
            updateMetrics(data);
        } catch (error) {
            console.error('Error parsing WebSocket message:', error);
        }
    };
    
    socket.onclose = function(event) {
        console.log('WebSocket disconnected');
        updateConnectionStatus('Disconnected');
        
        // Attempt to reconnect after 3 seconds
        setTimeout(function() {
            updateConnectionStatus('Reconnecting...');
            initWebSocket();
        }, 3000);
    };
    
    socket.onerror = function(error) {
        console.error('WebSocket error:', error);
        updateConnectionStatus('Error');
    };
}

function updateConnectionStatus(status) {
    const statusElement = document.getElementById('connectionStatus');
    if (statusElement) {
        statusElement.textContent = status;
        statusElement.className = 'connection-status ' + status.toLowerCase();
    }
}

function updateMetrics(data) {
    // Update metric values
    document.getElementById('jitterValue').textContent = data.jitter.toFixed(1);
    document.getElementById('delayValue').textContent = data.delay.toFixed(1);
    document.getElementById('latencyValue').textContent = data.latency.toFixed(1);
    document.getElementById('bitrateValue').textContent = data.bitrate.toFixed(2);
    document.getElementById('packetLossValue').textContent = data.packetLoss.toFixed(2);
    
    // Update status indicators
    updateStatus('jitter', data.jitter, 50);
    updateStatus('delay', data.delay, 200);
    updateStatus('latency', data.latency, 100);
    updateStatus('bitrate', data.bitrate, 1, 'higher');
    updateStatus('packetLoss', data.packetLoss, 1);
    
    // Update chart data
    updateChartData(data);
}

function updateStatus(metric, value, threshold, higherIsBetter = false) {
    const statusElement = document.getElementById(metric + 'Status');
    if (!statusElement) return;
    
    let status = 'Good';
    let className = '';
    
    if (higherIsBetter) {
        if (value < threshold * 0.5) {
            status = 'Poor';
            className = 'error';
        } else if (value < threshold) {
            status = 'Fair';
            className = 'warning';
        }
    } else {
        if (value > threshold * 2) {
            status = 'Poor';
            className = 'error';
        } else if (value > threshold) {
            status = 'Warning';
            className = 'warning';
        }
    }
    
    statusElement.textContent = status;
    statusElement.className = 'metric-status ' + className;
}

function initChart() {
    const ctx = document.getElementById('metricsChart');
    if (!ctx) return;
    
    metricsChart = new Chart(ctx.getContext('2d'), {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Jitter (ms)',
                data: [],
                borderColor: 'rgb(255, 99, 132)',
                backgroundColor: 'rgba(255, 99, 132, 0.2)',
                tension: 0.1
            }, {
                label: 'Delay (ms)',
                data: [],
                borderColor: 'rgb(54, 162, 235)',
                backgroundColor: 'rgba(54, 162, 235, 0.2)',
                tension: 0.1
            }, {
                label: 'Packet Loss (%)',
                data: [],
                borderColor: 'rgb(255, 205, 86)',
                backgroundColor: 'rgba(255, 205, 86, 0.2)',
                tension: 0.1
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                y: {
                    beginAtZero: true
                }
            },
            plugins: {
                title: {
                    display: true,
                    text: 'Network Quality Metrics'
                }
            }
        }
    });
}

function updateChartData(data) {
    if (!metricsChart) return;
    
    const now = new Date().toLocaleTimeString();
    const maxPoints = 20;
    
    // Add new data point
    metricsChart.data.labels.push(now);
    metricsChart.data.datasets[0].data.push(data.jitter);
    metricsChart.data.datasets[1].data.push(data.delay);
    metricsChart.data.datasets[2].data.push(data.packetLoss);
    
    // Remove old data points to prevent chart from becoming too crowded
    if (metricsChart.data.labels.length > maxPoints) {
        metricsChart.data.labels.shift();
        metricsChart.data.datasets.forEach(dataset => {
            dataset.data.shift();
        });
    }
    
    metricsChart.update('none');
}
)";
    
    server->send(200, "text/javascript", js);
}

void CustomWebServer::handleNotFound() {
    String message = "File Not Found\n\n";
    message += "URI: " + server->uri() + "\n";
    message += "Method: " + String((server->method() == HTTP_GET) ? "GET" : "POST") + "\n";
    message += "Arguments: " + String(server->args()) + "\n";
    
    for (uint8_t i = 0; i < server->args(); i++) {
        message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
    }
    
    server->send(404, "text/plain", message);
}

String CustomWebServer::generateMetricsJSON() {
    JsonDocument doc;
    
    // Thread-safe access to metrics
    if (xSemaphoreTake(metricsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        doc["jitter"] = currentMetrics.jitter;
        doc["delay"] = currentMetrics.delay;
        doc["latency"] = currentMetrics.latency;
        doc["bitrate"] = currentMetrics.bitrate;
        doc["packetLoss"] = currentMetrics.packetLoss;
        doc["timestamp"] = currentMetrics.timestamp;
        xSemaphoreGive(metricsMutex);
    } else {
        // Return default values if mutex timeout
        doc["jitter"] = 0.0;
        doc["delay"] = 0.0;
        doc["latency"] = 0.0;
        doc["bitrate"] = 0.0;
        doc["packetLoss"] = 0.0;
        doc["timestamp"] = millis();
    }
    
    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

bool CustomWebServer::handleFileRead(String path) {
    Serial.println("handleFileRead: " + path);
    if (path.endsWith("/")) path += "index.html";
    
    String contentType = getContentType(path);
    
    if (SPIFFS.exists(path)) {
        File file = SPIFFS.open(path, "r");
        server->streamFile(file, contentType);
        file.close();
        return true;
    }
    return false;
}

String CustomWebServer::getContentType(String filename) {
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".png")) return "image/png";
    else if (filename.endsWith(".gif")) return "image/gif";
    else if (filename.endsWith(".jpg")) return "image/jpeg";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    else if (filename.endsWith(".xml")) return "text/xml";
    else if (filename.endsWith(".pdf")) return "application/pdf";
    else if (filename.endsWith(".zip")) return "application/zip";
    return "text/plain";
}

void CustomWebServer::handleClient() {
    if (server) {
        server->handleClient();
    }
}

void CustomWebServer::update() {
    handleClient();
}

bool CustomWebServer::isRunning() const {
    return server != nullptr;
}

int CustomWebServer::getConnectedClients() const {
    if (webSocket) {
        return webSocket->connectedClients();
    }
    return 0;
}

void CustomWebServer::stop() {
    if (server) {
        server->close();
    }
}
