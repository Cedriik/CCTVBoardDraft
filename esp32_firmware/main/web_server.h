#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WebServer.h>
#include <WebSocketsServer.h>
#include <SPIFFS.h>

class CustomWebServer {
private:
    WebServer* server;
    WebSocketsServer* webSocket;
    
    // Route handlers
    void handleRoot();
    void handleDashboard();
    void handleAPI();
    void handleMetrics();
    void handleCSS();
    void handleJS();
    void handleNotFound();
    
    // Utility methods
    String getContentType(String filename);
    bool handleFileRead(String path);
    String generateMetricsJSON();
    
public:
    CustomWebServer();
    ~CustomWebServer();
    
    // Initialization
    bool begin(WebServer* webServer, WebSocketsServer* webSocketServer);
    void stop();
    
    // Server management
    void handleClient();
    void update();
    
    // API endpoints
    void setupRoutes();
    void sendMetrics(int clientId = -1);
    
    // Status
    bool isRunning() const;
    int getConnectedClients() const;
};

#endif
