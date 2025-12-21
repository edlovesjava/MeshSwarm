/*
 * MeshSwarm Library
 *
 * Self-organizing ESP32 mesh network with distributed shared state.
 *
 * Features:
 *   - Auto peer discovery and coordinator election
 *   - Distributed key-value state with conflict resolution
 *   - State watcher callbacks
 *   - OLED display support (optional)
 *   - Serial command interface (optional)
 *   - HTTP telemetry and gateway mode (optional)
 *   - OTA firmware distribution (optional)
 */

#ifndef MESH_SWARM_H
#define MESH_SWARM_H

// Include configuration first to get feature flags
#include "MeshSwarmConfig.h"

#include <Arduino.h>
#include <painlessMesh.h>
#include <ArduinoJson.h>
#include <map>
#include <vector>
#include <functional>

// Conditional includes based on feature flags
#if MESHSWARM_ENABLE_DISPLAY
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif

#if MESHSWARM_ENABLE_TELEMETRY || MESHSWARM_ENABLE_OTA
#include <HTTPClient.h>
#endif

// ============== DEFAULT CONFIGURATION ==============
// Override these before including MeshSwarm.h if needed

#ifndef MESH_PREFIX
#define MESH_PREFIX     "swarm"
#endif

#ifndef MESH_PASSWORD
#define MESH_PASSWORD   "swarmnet123"
#endif

#ifndef MESH_PORT
#define MESH_PORT       5555
#endif

// OLED Configuration (only if display is enabled)
#if MESHSWARM_ENABLE_DISPLAY
#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH    128
#endif

#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT   64
#endif

#ifndef OLED_RESET
#define OLED_RESET      -1
#endif

#ifndef OLED_ADDR
#define OLED_ADDR       0x3C
#endif

#ifndef I2C_SDA
#define I2C_SDA         21
#endif

#ifndef I2C_SCL
#define I2C_SCL         22
#endif
#endif // MESHSWARM_ENABLE_DISPLAY

// Timing
#ifndef HEARTBEAT_INTERVAL
#define HEARTBEAT_INTERVAL   5000
#endif

#ifndef STATE_SYNC_INTERVAL
#define STATE_SYNC_INTERVAL  10000
#endif

#ifndef DISPLAY_INTERVAL
#define DISPLAY_INTERVAL     500
#endif

// Telemetry Configuration (only if telemetry is enabled)
#if MESHSWARM_ENABLE_TELEMETRY
#ifndef TELEMETRY_INTERVAL
#define TELEMETRY_INTERVAL   30000
#endif

#ifndef STATE_TELEMETRY_MIN_INTERVAL
#define STATE_TELEMETRY_MIN_INTERVAL  2000  // Min ms between state-triggered pushes
#endif
#endif // MESHSWARM_ENABLE_TELEMETRY

// OTA Configuration (only if OTA is enabled)
#if MESHSWARM_ENABLE_OTA
#ifndef OTA_POLL_INTERVAL
#define OTA_POLL_INTERVAL    60000   // Poll for updates every 60 seconds
#endif

#ifndef OTA_PART_SIZE
#define OTA_PART_SIZE        1024    // Bytes per OTA chunk
#endif
#endif // MESHSWARM_ENABLE_OTA

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION     "1.0.0"
#endif

// ============== MESSAGE TYPES ==============
enum MsgType {
  MSG_HEARTBEAT  = 1,
  MSG_STATE_SET  = 2,
  MSG_STATE_SYNC = 3,
  MSG_STATE_REQ  = 4,
  MSG_COMMAND    = 5,
  MSG_TELEMETRY  = 6   // Node telemetry to gateway
};

// ============== DATA STRUCTURES ==============
struct StateEntry {
  String value;
  uint32_t version;
  uint32_t origin;
  unsigned long timestamp;
};

struct Peer {
  uint32_t id;
  String name;
  String role;
  unsigned long lastSeen;
  bool alive;
};

#if MESHSWARM_ENABLE_OTA
// OTA update info from server
struct OTAUpdateInfo {
  int updateId;
  int firmwareId;
  String nodeType;
  String version;
  String hardware;
  String md5;
  int numParts;
  int sizeBytes;
  String targetNodeId;  // Empty = all nodes of type
  bool force;
  bool active;
};
#endif // MESHSWARM_ENABLE_OTA

// State change callback type
typedef std::function<void(const String& key, const String& value, const String& oldValue)> StateCallback;

#if MESHSWARM_ENABLE_CALLBACKS
// Custom loop callback type (for node-specific logic)
typedef std::function<void()> LoopCallback;

#if MESHSWARM_ENABLE_SERIAL
// Custom serial command handler
typedef std::function<bool(const String& input)> SerialHandler;
#endif

#if MESHSWARM_ENABLE_DISPLAY
// Custom display handler
typedef std::function<void(Adafruit_SSD1306& display, int startLine)> DisplayHandler;
#endif
#endif // MESHSWARM_ENABLE_CALLBACKS

// ============== MESHSWARM CLASS ==============
class MeshSwarm {
public:
  MeshSwarm();

  // Initialization
  void begin(const char* nodeName = nullptr);
  void begin(const char* prefix, const char* password, uint16_t port, const char* nodeName = nullptr);

  // Main loop - call from Arduino loop()
  void update();

  // State management
  bool setState(const String& key, const String& value);
  bool setStates(std::initializer_list<std::pair<String, String>> states);  // Batch update
  String getState(const String& key, const String& defaultVal = "");
  void watchState(const String& key, StateCallback callback);
  void broadcastFullState();
  void requestStateSync();

  // Node info
  uint32_t getNodeId() { return myId; }
  String getNodeName() { return myName; }
  String getRole() { return myRole; }
  bool isCoordinator() { return myRole == "COORD"; }
  int getPeerCount();

  // Peer access
  std::map<uint32_t, Peer>& getPeers() { return peers; }

#if MESHSWARM_ENABLE_DISPLAY
  // Display access
  Adafruit_SSD1306& getDisplay() { return display; }
#endif

  // Mesh access (for advanced usage)
  painlessMesh& getMesh() { return mesh; }

#if MESHSWARM_ENABLE_CALLBACKS
  // Customization hooks
  void onLoop(LoopCallback callback);
#if MESHSWARM_ENABLE_SERIAL
  void onSerialCommand(SerialHandler handler);
#endif
#if MESHSWARM_ENABLE_DISPLAY
  void onDisplayUpdate(DisplayHandler handler);
#endif
#endif // MESHSWARM_ENABLE_CALLBACKS

#if MESHSWARM_ENABLE_DISPLAY
  // Set custom status line for display
  void setStatusLine(const String& status);
#endif

  // Heartbeat data customization
  void setHeartbeatData(const String& key, int value);

#if MESHSWARM_ENABLE_TELEMETRY
  // Telemetry to server
  void setTelemetryServer(const char* url, const char* apiKey = nullptr);
  void setTelemetryInterval(unsigned long ms);
  void enableTelemetry(bool enable);
  bool isTelemetryEnabled() { return telemetryEnabled; }
  void pushTelemetry();

  // WiFi station mode for telemetry
  void connectToWiFi(const char* ssid, const char* password);
  bool isWiFiConnected();

  // Gateway mode - receives telemetry from other nodes and pushes to server
  void setGatewayMode(bool enable);
  bool isGateway() { return gatewayMode; }
#endif // MESHSWARM_ENABLE_TELEMETRY

#if MESHSWARM_ENABLE_OTA
  // OTA distribution (gateway mode)
  void enableOTADistribution(bool enable);
  bool isOTADistributionEnabled() { return otaDistributionEnabled; }
  void checkForOTAUpdates();  // Call from loop() to poll for updates

  // OTA reception (node mode)
  void enableOTAReceive(const String& role);
#endif // MESHSWARM_ENABLE_OTA

private:
  // Core objects
  painlessMesh mesh;

#if MESHSWARM_ENABLE_DISPLAY
  Adafruit_SSD1306 display;
#endif

  // State
  std::map<String, StateEntry> sharedState;
  std::map<String, std::vector<StateCallback>> stateWatchers;
  std::map<uint32_t, Peer> peers;

  // Node identity
  uint32_t myId;
  String myName;
  String myRole;
  uint32_t coordinatorId;

  // Timing
  unsigned long lastHeartbeat;
  unsigned long lastStateSync;
#if MESHSWARM_ENABLE_DISPLAY
  unsigned long lastDisplayUpdate;
#endif
#if MESHSWARM_ENABLE_TELEMETRY
  unsigned long lastTelemetryPush;
  unsigned long lastStateTelemetryPush;  // For debouncing state-triggered pushes
#endif
  unsigned long bootTime;

#if MESHSWARM_ENABLE_TELEMETRY
  // Telemetry config
  String telemetryUrl;
  String telemetryApiKey;
  unsigned long telemetryInterval;
  bool telemetryEnabled;
  bool gatewayMode;
#endif

#if MESHSWARM_ENABLE_OTA
  // OTA distribution state (gateway)
  bool otaDistributionEnabled;
  unsigned long lastOTACheck;
  OTAUpdateInfo currentOTAUpdate;
  uint8_t* otaFirmwareBuffer;
  size_t otaFirmwareSize;
  int otaLastPartSent;        // Track highest part number sent
  bool otaTransferStarted;    // True once first chunk is sent
#endif

#if MESHSWARM_ENABLE_CALLBACKS
  // Custom hooks
  std::vector<LoopCallback> loopCallbacks;
#if MESHSWARM_ENABLE_SERIAL
  std::vector<SerialHandler> serialHandlers;
#endif
#if MESHSWARM_ENABLE_DISPLAY
  std::vector<DisplayHandler> displayHandlers;
#endif
#endif

#if MESHSWARM_ENABLE_DISPLAY
  // Display state
  String lastStateChange;
  String customStatus;
#endif

  // Custom heartbeat data
  std::map<String, int> heartbeatExtras;

  // Internal methods
  void initMesh(const char* prefix, const char* password, uint16_t port);
#if MESHSWARM_ENABLE_DISPLAY
  void initDisplay();
#endif

  void onReceive(uint32_t from, String &msg);
  void onNewConnection(uint32_t nodeId);
  void onDroppedConnection(uint32_t nodeId);
  void onChangedConnections();

  void electCoordinator();
  void sendHeartbeat();
  void pruneDeadPeers();
#if MESHSWARM_ENABLE_DISPLAY
  void updateDisplay();
#endif
#if MESHSWARM_ENABLE_SERIAL
  void processSerial();
#endif

  void triggerWatchers(const String& key, const String& value, const String& oldValue);
  void broadcastState(const String& key);
  void handleStateSet(uint32_t from, JsonObject& data);
  void handleStateSync(uint32_t from, JsonObject& data);
#if MESHSWARM_ENABLE_TELEMETRY
  void handleTelemetry(uint32_t from, JsonObject& data);
  void sendTelemetryToGateway();
  void pushTelemetryForNode(uint32_t nodeId, JsonObject& data);
#endif

  String createMsg(MsgType type, JsonDocument& data);
  String nodeIdToName(uint32_t id);

#if MESHSWARM_ENABLE_OTA
  // OTA distribution methods (gateway)
  bool pollPendingOTAUpdates();
  bool downloadOTAFirmware(int firmwareId);
  void startOTADistribution();
  void reportOTAProgress(const String& nodeId, int currentPart, int totalParts, const String& status, const String& error = "");
  void reportOTAStart(int updateId);
  void reportOTAComplete(int updateId);
  void reportOTAFail(int updateId, const String& error);
  void cleanupOTABuffer();
#endif

#if MESHSWARM_ENABLE_TELEMETRY || MESHSWARM_ENABLE_OTA
  // HTTP helpers (shared by telemetry and OTA)
  int httpPost(const String& url, const String& payload, String* response = nullptr, int timeout = 5000);
  int httpGet(const String& url, String* response = nullptr, int timeout = 5000);
  int httpGetRange(const String& url, uint8_t* buffer, size_t bufferSize, int rangeStart, int rangeEnd, int timeout = 10000);
#endif
};

#endif // MESH_SWARM_H
