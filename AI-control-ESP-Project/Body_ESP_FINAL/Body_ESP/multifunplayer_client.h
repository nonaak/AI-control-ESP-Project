/*
  MultiFunPlayer WebSocket Client - Body ESP ML Integration
  
  Connects to MultiFunPlayer WebSocket server and receives funscript data
  Integrates with ML Autonomy system for intelligent hardware control
*/

#ifndef MULTIFUNPLAYER_CLIENT_H
#define MULTIFUNPLAYER_CLIENT_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "advanced_stress_manager.h"

// ===== MultiFunPlayer Protocol =====
struct FunscriptAction {
  uint32_t timestamp;     // Milliseconds from start
  uint8_t position;       // 0-100 position
  uint8_t speed;          // 0-100 speed (calculated)
  bool valid;             // Data validity flag
  
  FunscriptAction() : timestamp(0), position(50), speed(0), valid(false) {}
  FunscriptAction(uint32_t ts, uint8_t pos, uint8_t spd) 
    : timestamp(ts), position(pos), speed(spd), valid(true) {}
};

struct MLEnhancedAction {
  FunscriptAction original;    // Original funscript data
  uint8_t mlPosition;          // ML adjusted position (0-100)
  uint8_t mlSpeed;             // ML adjusted speed (1-7)
  bool vibeActive;             // ML vibe decision
  bool suctionActive;          // ML suction decision
  float autonomyUsed;          // ML autonomy percentage used
  String reasoning;            // ML decision reasoning
  
  MLEnhancedAction() : mlPosition(50), mlSpeed(1), vibeActive(false), 
                      suctionActive(false), autonomyUsed(0.0f), reasoning("") {}
};

// ===== MultiFunPlayer Client Class =====
class MultiFunPlayerClient {
private:
  WebSocketsClient webSocket;
  bool connected;
  bool enabled;
  
  // Server connection details
  String serverHost;
  uint16_t serverPort;
  String serverPath;
  
  // Funscript playback state
  bool playing;
  uint32_t scriptStartTime;
  uint32_t lastActionTime;
  FunscriptAction lastAction;
  
  // ML integration
  bool mlIntegrationEnabled;
  uint32_t lastMLUpdate;
  
  // Statistics
  uint32_t totalActions;
  uint32_t mlOverrides;
  uint32_t connectionAttempts;
  
  // Private methods
  void onWebSocketEvent(WStype_t type, uint8_t * payload, size_t length);
  void processFunscriptMessage(const String& message);
  void processStatusMessage(const String& message);
  void executeAction(const MLEnhancedAction& action);
  uint8_t calculateSpeed(const FunscriptAction& current, const FunscriptAction& previous);
  
public:
  // Constructor
  MultiFunPlayerClient();
  
  // Connection management
  void begin(const String& host = "127.0.0.1", uint16_t port = 8080, const String& path = "/");
  void connect();
  void disconnect();
  bool isConnected() const { return connected; }
  
  // Client control
  void enable(bool state) { enabled = state; }
  bool isEnabled() const { return enabled; }
  void loop();  // Call in main loop
  
  // ML Integration
  void enableMLIntegration(bool enable) { mlIntegrationEnabled = enable; }
  bool isMLIntegrationEnabled() const { return mlIntegrationEnabled; }
  MLEnhancedAction enhanceWithML(const FunscriptAction& action);
  
  // Playback control
  void startPlayback();
  void stopPlayback();
  void pausePlayback();
  bool isPlaying() const { return playing; }
  
  // Status and debugging
  void printStatus() const;
  String getStatusString() const;
  uint32_t getTotalActions() const { return totalActions; }
  uint32_t getMLOverrides() const { return mlOverrides; }
  float getMLOverridePercentage() const;
  
  // Configuration
  void setServer(const String& host, uint16_t port, const String& path = "/");
  void setReconnectInterval(uint32_t intervalMs);
  
  // Static callback wrapper
  static MultiFunPlayerClient* instance;
  static void webSocketEventStatic(WStype_t type, uint8_t * payload, size_t length);
};

// ===== Global Instance =====
extern MultiFunPlayerClient mfpClient;

// ===== Helper Functions =====

// Convert 0-100 position to motor position
inline uint8_t funscriptToMotorPosition(uint8_t funscriptPos) {
  return constrain(funscriptPos, 0, 100);
}

// Convert 0-100 funscript speed to Body ESP speed (1-7)
inline uint8_t funscriptToBodySpeed(uint8_t funscriptSpeed) {
  if (funscriptSpeed == 0) return 1;
  return constrain(map(funscriptSpeed, 0, 100, 1, 7), 1, 7);
}

// Apply ML autonomy to funscript action
MLEnhancedAction applyMLAutonomy(const FunscriptAction& action, float autonomyLevel);

// ===== GEMAKKELIJKE SETUP FUNCTIE =====
// Roep deze functie aan in je setup() voor automatische configuratie
void setupMultiFunPlayer();

#endif // MULTIFUNPLAYER_CLIENT_H
