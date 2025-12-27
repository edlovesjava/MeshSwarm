/*
 * MeshSwarm Library Implementation
 */

#include "MeshSwarm.h"

// ============== CONSTRUCTOR ==============
MeshSwarm::MeshSwarm()
  : 
#if MESHSWARM_ENABLE_DISPLAY
    display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET),
#endif
    myId(0),
    myName(""),
    myRole("PEER"),
    coordinatorId(0),
    lastHeartbeat(0),
    lastStateSync(0)
#if MESHSWARM_ENABLE_DISPLAY
    ,lastDisplayUpdate(0)
#endif
#if MESHSWARM_ENABLE_TELEMETRY
    ,lastTelemetryPush(0)
    ,lastStateTelemetryPush(0)
#endif
    ,bootTime(0)
#if MESHSWARM_ENABLE_TELEMETRY
    ,telemetryUrl("")
    ,telemetryApiKey("")
    ,telemetryInterval(TELEMETRY_INTERVAL)
    ,telemetryEnabled(false)
    ,gatewayMode(false)
#endif
#if MESHSWARM_ENABLE_OTA
    ,otaDistributionEnabled(false)
    ,lastOTACheck(0)
    ,otaFirmwareBuffer(nullptr)
    ,otaFirmwareSize(0)
    ,otaLastPartSent(-1)
    ,otaTransferStarted(false)
#endif
#if MESHSWARM_ENABLE_DISPLAY
    ,lastStateChange("")
    ,customStatus("")
#endif
{
#if MESHSWARM_ENABLE_OTA
  // Initialize OTA update info
  currentOTAUpdate.active = false;
  currentOTAUpdate.updateId = 0;
#endif
}

// ============== INITIALIZATION ==============
void MeshSwarm::begin(const char* nodeName) {
  begin(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, nodeName);
}

void MeshSwarm::begin(const char* prefix, const char* password, uint16_t port, const char* nodeName) {
#if MESHSWARM_ENABLE_SERIAL
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n");
  Serial.println("========================================");
  Serial.println("       ESP32 MESH SWARM NODE");
  Serial.println("========================================");
  Serial.println();
#endif

#if MESHSWARM_ENABLE_DISPLAY
  // Initialize display
  initDisplay();
#endif

  // Stagger startup to reduce collisions
  uint32_t chipId = ESP.getEfuseMac() & 0xFFFF;
  uint32_t startDelay = (chipId % 3) * 500;
  MESH_LOG_D("Startup delay: %dms", startDelay);
  delay(startDelay);

  // Initialize mesh
  initMesh(prefix, password, port);

  myId = mesh.getNodeId();
  myName = nodeName ? String(nodeName) : nodeIdToName(myId);
  bootTime = millis();

  MESH_LOG("Node ID: %u", myId);
  MESH_LOG("Name: %s", myName.c_str());
#if MESHSWARM_ENABLE_SERIAL
  Serial.println();
  Serial.println("Commands: status, peers, state, set <k> <v>, get <k>, sync, reboot");
  Serial.println("----------------------------------------");
  Serial.println();
#endif
}

// initDisplay() is defined in features/MeshSwarmDisplay.inc

void MeshSwarm::initMesh(const char* prefix, const char* password, uint16_t port) {
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(prefix, password, port);

  // Use lambdas to capture 'this' for callbacks
  mesh.onReceive([this](uint32_t from, String &msg) {
    this->onReceive(from, msg);
  });

  mesh.onNewConnection([this](uint32_t nodeId) {
    this->onNewConnection(nodeId);
  });

  mesh.onDroppedConnection([this](uint32_t nodeId) {
    this->onDroppedConnection(nodeId);
  });

  mesh.onChangedConnections([this]() {
    this->onChangedConnections();
  });
}

// ============== MAIN LOOP ==============
void MeshSwarm::update() {
  mesh.update();

  unsigned long now = millis();

  // Heartbeat
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    pruneDeadPeers();
    lastHeartbeat = now;
  }

  // Periodic full state sync
  if (now - lastStateSync >= STATE_SYNC_INTERVAL) {
    broadcastFullState();
    lastStateSync = now;
  }

#if MESHSWARM_ENABLE_DISPLAY
  // Display update
  if (now - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = now;
  }
#endif

#if MESHSWARM_ENABLE_TELEMETRY
  // Telemetry push
  if (telemetryEnabled && (now - lastTelemetryPush >= telemetryInterval)) {
    if (gatewayMode) {
      // Gateway pushes its own telemetry directly
      pushTelemetry();
    } else {
      // Regular node sends telemetry via mesh to gateway
      sendTelemetryToGateway();
    }
    lastTelemetryPush = now;
  }
#endif

#if MESHSWARM_ENABLE_SERIAL
  // Serial commands
  if (Serial.available()) {
    processSerial();
  }
#endif

#if MESHSWARM_ENABLE_CALLBACKS
  // Custom loop callbacks
  for (auto& cb : loopCallbacks) {
    cb();
  }
#endif
}

// ============== STATE MANAGEMENT ==============
bool MeshSwarm::setState(const String& key, const String& value) {
  String oldValue = "";
  uint32_t newVersion = 1;

  if (sharedState.count(key)) {
    oldValue = sharedState[key].value;
    newVersion = sharedState[key].version + 1;

    if (oldValue == value) {
      return false;
    }
  }

  StateEntry entry;
  entry.value = value;
  entry.version = newVersion;
  entry.origin = myId;
  entry.timestamp = millis();
  sharedState[key] = entry;

  triggerWatchers(key, value, oldValue);
  broadcastState(key);
#if MESHSWARM_ENABLE_DISPLAY
  lastStateChange = key + "=" + value;
#endif

#if MESHSWARM_ENABLE_TELEMETRY
  // Push telemetry on state change (with debouncing)
  unsigned long now = millis();
  if (telemetryEnabled) {
    if (now - lastStateTelemetryPush >= STATE_TELEMETRY_MIN_INTERVAL) {
      TELEM_LOG("State change push for %s=%s", key.c_str(), value.c_str());
      if (gatewayMode) {
        pushTelemetry();
      } else {
        sendTelemetryToGateway();
      }
      lastTelemetryPush = now;
      lastStateTelemetryPush = now;
    } else {
      TELEM_LOG_D("Debounced %s=%s (wait %lums)", key.c_str(), value.c_str(),
                  STATE_TELEMETRY_MIN_INTERVAL - (now - lastStateTelemetryPush));
    }
  }
#endif

  return true;
}

bool MeshSwarm::setStates(std::initializer_list<std::pair<String, String>> states) {
  bool anyChanged = false;

  for (const auto& kv : states) {
    const String& key = kv.first;
    const String& value = kv.second;

    String oldValue = "";
    uint32_t newVersion = 1;

    if (sharedState.count(key)) {
      oldValue = sharedState[key].value;
      newVersion = sharedState[key].version + 1;

      if (oldValue == value) {
        continue;  // Skip unchanged values
      }
    }

    StateEntry entry;
    entry.value = value;
    entry.version = newVersion;
    entry.origin = myId;
    entry.timestamp = millis();
    sharedState[key] = entry;

    triggerWatchers(key, value, oldValue);
    broadcastState(key);
#if MESHSWARM_ENABLE_DISPLAY
    lastStateChange = key + "=" + value;
#endif
    anyChanged = true;
  }

#if MESHSWARM_ENABLE_TELEMETRY
  // Push telemetry once for all changes (with debouncing)
  if (anyChanged && telemetryEnabled) {
    unsigned long now = millis();
    if (now - lastStateTelemetryPush >= STATE_TELEMETRY_MIN_INTERVAL) {
      TELEM_LOG("State change push (batch)");
      if (gatewayMode) {
        pushTelemetry();
      } else {
        sendTelemetryToGateway();
      }
      lastTelemetryPush = now;
      lastStateTelemetryPush = now;
    } else {
      TELEM_LOG_D("Debounced batch (wait %lums)",
                  STATE_TELEMETRY_MIN_INTERVAL - (now - lastStateTelemetryPush));
    }
  }
#endif

  return anyChanged;
}

String MeshSwarm::getState(const String& key, const String& defaultVal) {
  if (sharedState.count(key)) {
    return sharedState[key].value;
  }
  return defaultVal;
}

void MeshSwarm::watchState(const String& key, StateCallback callback) {
  stateWatchers[key].push_back(callback);
}

void MeshSwarm::triggerWatchers(const String& key, const String& value, const String& oldValue) {
  if (stateWatchers.count(key)) {
    for (auto& cb : stateWatchers[key]) {
      cb(key, value, oldValue);
    }
  }

  // Wildcard watchers
  if (stateWatchers.count("*")) {
    for (auto& cb : stateWatchers["*"]) {
      cb(key, value, oldValue);
    }
  }
}

void MeshSwarm::broadcastState(const String& key) {
  if (!sharedState.count(key)) return;

  StateEntry& entry = sharedState[key];

  JsonDocument data;
  data["k"] = key;
  data["v"] = entry.value;
  data["ver"] = entry.version;
  data["org"] = entry.origin;

  String msg = createMsg(MSG_STATE_SET, data);
  mesh.sendBroadcast(msg);
}

void MeshSwarm::broadcastFullState() {
  if (sharedState.empty()) return;

  JsonDocument data;
  JsonArray arr = data["s"].to<JsonArray>();

  for (auto& kv : sharedState) {
    JsonObject entry = arr.add<JsonObject>();
    entry["k"] = kv.first;
    entry["v"] = kv.second.value;
    entry["ver"] = kv.second.version;
    entry["org"] = kv.second.origin;
  }

  String msg = createMsg(MSG_STATE_SYNC, data);
  mesh.sendBroadcast(msg);
}

void MeshSwarm::requestStateSync() {
  JsonDocument data;
  data["req"] = 1;
  String msg = createMsg(MSG_STATE_REQ, data);
  mesh.sendBroadcast(msg);
}

void MeshSwarm::handleStateSet(uint32_t from, JsonObject& data) {
  String key = data["k"] | "";
  String value = data["v"] | "";
  uint32_t version = data["ver"] | 0;
  uint32_t origin = data["org"] | from;

  if (key.length() == 0) return;

  bool shouldUpdate = false;
  String oldValue = "";

  if (!sharedState.count(key)) {
    shouldUpdate = true;
  } else {
    StateEntry& existing = sharedState[key];
    oldValue = existing.value;

    if (version > existing.version) {
      shouldUpdate = true;
    } else if (version == existing.version && origin < existing.origin) {
      shouldUpdate = true;
    }
  }

  if (shouldUpdate && oldValue != value) {
    StateEntry entry;
    entry.value = value;
    entry.version = version;
    entry.origin = origin;
    entry.timestamp = millis();
    sharedState[key] = entry;

    triggerWatchers(key, value, oldValue);
#if MESHSWARM_ENABLE_DISPLAY
    lastStateChange = key + "=" + value;
#endif

    STATE_LOG("%s = %s (v%u from %s)",
              key.c_str(), value.c_str(), version, nodeIdToName(origin).c_str());
  }
}

void MeshSwarm::handleStateSync(uint32_t from, JsonObject& data) {
  JsonArray arr = data["s"].as<JsonArray>();

  for (JsonObject entry : arr) {
    handleStateSet(from, entry);
  }

  STATE_LOG_D("Received %d state entries from %s",
              arr.size(), nodeIdToName(from).c_str());
}

// ============== MESH CALLBACKS ==============
void MeshSwarm::onReceive(uint32_t from, String &msg) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, msg);

  if (err) {
    MESH_LOG_ERROR("JSON error from %u", from);
    return;
  }

  MsgType type = (MsgType)doc["t"].as<int>();
  String senderName = doc["n"] | "???";
  JsonObject data = doc["d"].as<JsonObject>();

  switch (type) {
    case MSG_HEARTBEAT: {
      Peer &p = peers[from];
      p.id = from;
      p.name = senderName;
      p.role = data["role"] | "PEER";
      p.lastSeen = millis();
      p.alive = true;
      electCoordinator();
      break;
    }

    case MSG_STATE_SET:
      handleStateSet(from, data);
      break;

    case MSG_STATE_SYNC:
      handleStateSync(from, data);
      break;

    case MSG_STATE_REQ:
      broadcastFullState();
      break;

    case MSG_COMMAND:
      break;

#if MESHSWARM_ENABLE_TELEMETRY
    case MSG_TELEMETRY:
      // Only gateway handles telemetry messages
      if (gatewayMode) {
        handleTelemetry(from, data);
      }
      break;
#endif

    default:
      break;
  }
}

void MeshSwarm::onNewConnection(uint32_t nodeId) {
  MESH_LOG("+ Connected: %s", nodeIdToName(nodeId).c_str());
  sendHeartbeat();
  broadcastFullState();
}

void MeshSwarm::onDroppedConnection(uint32_t nodeId) {
  MESH_LOG("- Dropped: %s", nodeIdToName(nodeId).c_str());
  if (peers.count(nodeId)) {
    peers[nodeId].alive = false;
  }
  electCoordinator();
}

void MeshSwarm::onChangedConnections() {
  MESH_LOG("Topology changed. Nodes: %d", mesh.getNodeList().size());
  electCoordinator();
}

// ============== COORDINATOR ELECTION ==============
void MeshSwarm::electCoordinator() {
  auto nodeList = mesh.getNodeList();
  uint32_t lowest = myId;

  for (auto &id : nodeList) {
    if (id < lowest) lowest = id;
  }

  String oldRole = myRole;
  coordinatorId = lowest;
  myRole = (lowest == myId) ? "COORD" : "PEER";

  if (oldRole != myRole) {
    MESH_LOG("Role: %s -> %s", oldRole.c_str(), myRole.c_str());
  }
}

// ============== HEARTBEAT ==============
void MeshSwarm::sendHeartbeat() {
  JsonDocument data;
  data["role"] = myRole;
  data["up"] = (millis() - bootTime) / 1000;
  data["heap"] = ESP.getFreeHeap();
  data["states"] = sharedState.size();

  // Add custom heartbeat data
  for (auto& kv : heartbeatExtras) {
    data[kv.first] = kv.second;
  }

  String msg = createMsg(MSG_HEARTBEAT, data);
  mesh.sendBroadcast(msg);
}

void MeshSwarm::pruneDeadPeers() {
  unsigned long now = millis();
  for (auto it = peers.begin(); it != peers.end(); ) {
    if (now - it->second.lastSeen > 15000) {
      it = peers.erase(it);
    } else {
      ++it;
    }
  }
}

int MeshSwarm::getPeerCount() {
  int count = 0;
  for (auto &p : peers) {
    if (p.second.alive) count++;
  }
  return count;
}

// ============== CUSTOMIZATION ==============
// onLoop(), onSerialCommand(), onDisplayUpdate() defined in features/MeshSwarmCallbacks.inc
// setStatusLine() defined in features/MeshSwarmDisplay.inc

void MeshSwarm::setHeartbeatData(const String& key, int value) {
  heartbeatExtras[key] = value;
}

// ============== HELPERS ==============
String MeshSwarm::createMsg(MsgType type, JsonDocument& data) {
  JsonDocument doc;
  doc["t"] = type;
  doc["n"] = myName;
  doc["d"] = data;

  String out;
  serializeJson(doc, out);
  return out;
}

String MeshSwarm::nodeIdToName(uint32_t id) {
  String hex = String(id, HEX);
  hex.toUpperCase();
  if (hex.length() > 4) {
    hex = hex.substring(hex.length() - 4);
  }
  return "N" + hex;
}


// ============== FEATURE MODULE INCLUDES ==============
// Feature implementations are conditionally compiled based on flags
// Using .inc extension to prevent Arduino from compiling them separately
#include "features/MeshSwarmDisplay.inc"
#include "features/MeshSwarmSerial.inc"
#include "features/MeshSwarmCallbacks.inc"
#include "features/MeshSwarmHTTP.inc"
#include "features/MeshSwarmTelemetry.inc"
#include "features/MeshSwarmOTA.inc"

// ============== HTTP SERVER (STUB) ==============
// Placeholder to satisfy gateway builds. Real implementation will
// use an HTTP server (e.g., AsyncWebServer) behind a feature flag.
void MeshSwarm::startHTTPServer(uint16_t port) {
  GATEWAY_LOG("HTTP server stub: requested port %u (not implemented)", port);
}
