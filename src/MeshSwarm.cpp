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
#if MESHSWARM_ENABLE_SERIAL
  Serial.printf("[BOOT] Startup delay: %dms\n", startDelay);
#endif
  delay(startDelay);

  // Initialize mesh
  initMesh(prefix, password, port);

  myId = mesh.getNodeId();
  myName = nodeName ? String(nodeName) : nodeIdToName(myId);
  bootTime = millis();

#if MESHSWARM_ENABLE_SERIAL
  Serial.printf("[MESH] Node ID: %u\n", myId);
  Serial.printf("[MESH] Name: %s\n", myName.c_str());
  Serial.println();
  Serial.println("Commands: status, peers, state, set <k> <v>, get <k>, sync, reboot");
  Serial.println("----------------------------------------");
  Serial.println();
#endif
}

#if MESHSWARM_ENABLE_DISPLAY
void MeshSwarm::initDisplay() {
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
#if MESHSWARM_ENABLE_SERIAL
    Serial.println("[OLED] Init failed!");
#endif
  } else {
#if MESHSWARM_ENABLE_SERIAL
    Serial.println("[OLED] Initialized");
#endif
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Mesh Swarm");
    display.println("Starting...");
    display.display();
  }
}
#endif

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
#if MESHSWARM_ENABLE_SERIAL
      Serial.printf("[TELEM] State change push for %s=%s\n", key.c_str(), value.c_str());
#endif
      if (gatewayMode) {
        pushTelemetry();
      } else {
        sendTelemetryToGateway();
      }
      lastTelemetryPush = now;
      lastStateTelemetryPush = now;
    } else {
#if MESHSWARM_ENABLE_SERIAL
      Serial.printf("[TELEM] Debounced %s=%s (wait %lums)\n", key.c_str(), value.c_str(),
                    STATE_TELEMETRY_MIN_INTERVAL - (now - lastStateTelemetryPush));
#endif
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
#if MESHSWARM_ENABLE_SERIAL
      Serial.println("[TELEM] State change push (batch)");
#endif
      if (gatewayMode) {
        pushTelemetry();
      } else {
        sendTelemetryToGateway();
      }
      lastTelemetryPush = now;
      lastStateTelemetryPush = now;
    } else {
#if MESHSWARM_ENABLE_SERIAL
      Serial.printf("[TELEM] Debounced batch (wait %lums)\n",
                    STATE_TELEMETRY_MIN_INTERVAL - (now - lastStateTelemetryPush));
#endif
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

#if MESHSWARM_ENABLE_SERIAL
    Serial.printf("[STATE] %s = %s (v%u from %s)\n",
                  key.c_str(), value.c_str(), version, nodeIdToName(origin).c_str());
#endif
  }
}

void MeshSwarm::handleStateSync(uint32_t from, JsonObject& data) {
  JsonArray arr = data["s"].as<JsonArray>();

  for (JsonObject entry : arr) {
    handleStateSet(from, entry);
  }

#if MESHSWARM_ENABLE_SERIAL
  Serial.printf("[SYNC] Received %d state entries from %s\n",
                arr.size(), nodeIdToName(from).c_str());
#endif
}

// ============== MESH CALLBACKS ==============
void MeshSwarm::onReceive(uint32_t from, String &msg) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, msg);

  if (err) {
#if MESHSWARM_ENABLE_SERIAL
    Serial.printf("[RX] JSON error from %u\n", from);
#endif
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
      break;
  }
}

void MeshSwarm::onNewConnection(uint32_t nodeId) {
  Serial.printf("[MESH] + Connected: %s\n", nodeIdToName(nodeId).c_str());
  sendHeartbeat();
  broadcastFullState();
}

void MeshSwarm::onDroppedConnection(uint32_t nodeId) {
  Serial.printf("[MESH] - Dropped: %s\n", nodeIdToName(nodeId).c_str());
  if (peers.count(nodeId)) {
    peers[nodeId].alive = false;
  }
  electCoordinator();
}

void MeshSwarm::onChangedConnections() {
  Serial.printf("[MESH] Topology changed. Nodes: %d\n", mesh.getNodeList().size());
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
    Serial.printf("[ROLE] %s -> %s\n", oldRole.c_str(), myRole.c_str());
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
#if MESHSWARM_ENABLE_CALLBACKS
void MeshSwarm::onLoop(LoopCallback callback) {
  loopCallbacks.push_back(callback);
}

#if MESHSWARM_ENABLE_SERIAL
void MeshSwarm::onSerialCommand(SerialHandler handler) {
  serialHandlers.push_back(handler);
}
#endif

#if MESHSWARM_ENABLE_DISPLAY
void MeshSwarm::onDisplayUpdate(DisplayHandler handler) {
  displayHandlers.push_back(handler);
}
#endif
#endif // MESHSWARM_ENABLE_CALLBACKS

#if MESHSWARM_ENABLE_DISPLAY
void MeshSwarm::setStatusLine(const String& status) {
  customStatus = status;
}
#endif

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

#if MESHSWARM_ENABLE_DISPLAY
// ============== DISPLAY ==============
void MeshSwarm::updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  // Line 1: Identity
  uint32_t uptime = (millis() - bootTime) / 1000;
  display.printf("%s [%s] %d:%02d\n", myName.c_str(), myRole.c_str(), uptime/60, uptime%60);

  // Line 2: Network
  display.printf("Peers:%d States:%d\n", getPeerCount(), sharedState.size());

  // Line 3: Custom status or separator
  if (customStatus.length() > 0) {
    display.println(customStatus.substring(0, 21));
  } else {
    display.println("---------------------");
  }

#if MESHSWARM_ENABLE_CALLBACKS
  // Call custom display handlers (lines 4+)
  int startLine = 3;
  for (auto& handler : displayHandlers) {
    handler(display, startLine);
  }

  // If no custom handlers, show state values
  if (displayHandlers.empty()) {
#endif
    // Lines 4-7: State values (up to 4)
    int shown = 0;
    for (auto& kv : sharedState) {
      if (shown >= 4) break;
      String line = kv.first + "=" + kv.second.value;
      if (line.length() > 21) line = line.substring(0, 21);
      display.println(line);
      shown++;
    }

    while (shown < 4) {
      display.println();
      shown++;
    }

    // Line 8: Last change
    if (lastStateChange.length() > 0) {
      display.printf("Last:%s\n", lastStateChange.substring(0, 16).c_str());
    }
#if MESHSWARM_ENABLE_CALLBACKS
  }
#endif

  display.display();
}
#endif // MESHSWARM_ENABLE_DISPLAY

#if MESHSWARM_ENABLE_SERIAL
// ============== SERIAL COMMANDS ==============
void MeshSwarm::processSerial() {
  String input = Serial.readStringUntil('\n');
  input.trim();

  if (input.length() == 0) return;

#if MESHSWARM_ENABLE_CALLBACKS
  // Try custom handlers first
  for (auto& handler : serialHandlers) {
    if (handler(input)) {
      return;  // Handler consumed the command
    }
  }
#endif

  // Built-in commands
  if (input == "status") {
    Serial.println("\n--- NODE STATUS ---");
    Serial.printf("ID: %u (%s)\n", myId, myName.c_str());
    Serial.printf("Role: %s\n", myRole.c_str());
    Serial.printf("Peers: %d\n", getPeerCount());
    Serial.printf("States: %d\n", sharedState.size());
    Serial.printf("Heap: %u\n", ESP.getFreeHeap());
    Serial.println();
  }
  else if (input == "peers") {
    Serial.println("\n--- PEERS ---");
    for (auto& p : peers) {
      Serial.printf("  %s [%s] %s\n", p.second.name.c_str(),
                    p.second.role.c_str(), p.second.alive ? "OK" : "DEAD");
    }
    Serial.println();
  }
  else if (input == "state") {
    Serial.println("\n--- SHARED STATE ---");
    for (auto& kv : sharedState) {
      Serial.printf("  %s = %s (v%u from %s)\n",
                    kv.first.c_str(),
                    kv.second.value.c_str(),
                    kv.second.version,
                    nodeIdToName(kv.second.origin).c_str());
    }
    Serial.println();
  }
  else if (input.startsWith("set ")) {
    int firstSpace = input.indexOf(' ', 4);
    if (firstSpace > 0) {
      String key = input.substring(4, firstSpace);
      String value = input.substring(firstSpace + 1);
      setState(key, value);
      Serial.printf("[SET] %s = %s\n", key.c_str(), value.c_str());
    } else {
      Serial.println("Usage: set <key> <value>");
    }
  }
  else if (input.startsWith("get ")) {
    String key = input.substring(4);
    String value = getState(key, "(not set)");
    Serial.printf("[GET] %s = %s\n", key.c_str(), value.c_str());
  }
  else if (input == "sync") {
    broadcastFullState();
    Serial.println("[SYNC] Broadcast full state");
  }
#if MESHSWARM_ENABLE_DISPLAY
  else if (input == "scan") {
    Serial.println("\n--- I2C SCAN ---");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.printf("  Found device at 0x%02X\n", addr);
        found++;
      }
    }
    Serial.printf("Found %d device(s)\n\n", found);
  }
#endif
  else if (input == "reboot") {
    ESP.restart();
  }
#if MESHSWARM_ENABLE_TELEMETRY
  else if (input == "telem") {
    Serial.println("\n--- TELEMETRY STATUS ---");
    Serial.printf("Enabled: %s\n", telemetryEnabled ? "YES" : "NO");
    Serial.printf("Gateway: %s\n", gatewayMode ? "YES" : "NO");
    if (gatewayMode) {
      Serial.printf("URL: %s\n", telemetryUrl.length() > 0 ? telemetryUrl.c_str() : "(not set)");
      Serial.printf("WiFi: %s\n", isWiFiConnected() ? "Connected" : "Not connected");
      if (isWiFiConnected()) {
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
      }
    } else {
      Serial.println("Mode: Sending via mesh to gateway");
    }
    Serial.printf("Interval: %lu ms\n", telemetryInterval);
    Serial.println();
  }
  else if (input == "push") {
    if (telemetryEnabled) {
      pushTelemetry();
      Serial.println("[TELEM] Manual push triggered");
    } else {
      Serial.println("[TELEM] Telemetry not enabled");
    }
  }
#endif
  else {
    Serial.println("Commands: status, peers, state, set <k> <v>, get <k>, sync, scan"
#if MESHSWARM_ENABLE_TELEMETRY
      ", telem, push"
#endif
      ", reboot");
  }
}
#endif // MESHSWARM_ENABLE_SERIAL

#if MESHSWARM_ENABLE_TELEMETRY
// ============== TELEMETRY ==============
void MeshSwarm::setTelemetryServer(const char* url, const char* apiKey) {
  telemetryUrl = String(url);
  if (apiKey != nullptr) {
    telemetryApiKey = String(apiKey);
  }
#if MESHSWARM_ENABLE_SERIAL
  Serial.printf("[TELEM] Server: %s\n", telemetryUrl.c_str());
#endif
}

void MeshSwarm::setTelemetryInterval(unsigned long ms) {
  telemetryInterval = ms;
#if MESHSWARM_ENABLE_SERIAL
  Serial.printf("[TELEM] Interval: %lu ms\n", telemetryInterval);
#endif
}

void MeshSwarm::enableTelemetry(bool enable) {
  telemetryEnabled = enable;
#if MESHSWARM_ENABLE_SERIAL
  Serial.printf("[TELEM] %s\n", enable ? "Enabled" : "Disabled");
#endif
}

void MeshSwarm::connectToWiFi(const char* ssid, const char* password) {
  // painlessMesh supports station mode alongside mesh
  mesh.stationManual(ssid, password);
#if MESHSWARM_ENABLE_SERIAL
  Serial.printf("[WIFI] Connecting to %s...\n", ssid);
#endif
}

bool MeshSwarm::isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void MeshSwarm::pushTelemetry() {
  if (!telemetryEnabled || telemetryUrl.length() == 0) {
    return;
  }

  if (!isWiFiConnected()) {
#if MESHSWARM_ENABLE_SERIAL
    Serial.println("[TELEM] WiFi not connected, skipping push");
#endif
    return;
  }

  HTTPClient http;
  String url = telemetryUrl + "/api/v1/nodes/" + String(myId, HEX) + "/telemetry";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  if (telemetryApiKey.length() > 0) {
    http.addHeader("X-API-Key", telemetryApiKey);
  }
  http.setTimeout(5000);  // 5 second timeout

  // Build JSON payload
  JsonDocument doc;
  doc["name"] = myName;
  doc["uptime"] = (millis() - bootTime) / 1000;
  doc["heap_free"] = ESP.getFreeHeap();
  doc["peer_count"] = getPeerCount();
  doc["role"] = myRole;
  doc["firmware"] = FIRMWARE_VERSION;

  // Include all shared state
  JsonObject state = doc["state"].to<JsonObject>();
  for (auto& entry : sharedState) {
    state[entry.first] = entry.second.value;
  }

  String payload;
  serializeJson(doc, payload);

  int httpCode = http.POST(payload);
#if MESHSWARM_ENABLE_SERIAL
  if (httpCode == 200 || httpCode == 201) {
    Serial.println("[TELEM] Push OK");
  } else {
    Serial.printf("[TELEM] Push failed: %d\n", httpCode);
  }
#endif
  http.end();
}

// ============== GATEWAY MODE ==============
void MeshSwarm::setGatewayMode(bool enable) {
  gatewayMode = enable;
#if MESHSWARM_ENABLE_SERIAL
  Serial.printf("[GATEWAY] %s\n", enable ? "Enabled" : "Disabled");
#endif
}

void MeshSwarm::sendTelemetryToGateway() {
  // Build telemetry data
  JsonDocument data;
  data["name"] = myName;
  data["uptime"] = (millis() - bootTime) / 1000;
  data["heap_free"] = ESP.getFreeHeap();
  data["peer_count"] = getPeerCount();
  data["role"] = myRole;
  data["firmware"] = FIRMWARE_VERSION;

  // Include all shared state
  JsonObject state = data["state"].to<JsonObject>();
  for (auto& entry : sharedState) {
    state[entry.first] = entry.second.value;
  }

  // Send via mesh broadcast (gateway will pick it up)
  String msg = createMsg(MSG_TELEMETRY, data);
  mesh.sendBroadcast(msg);

#if MESHSWARM_ENABLE_SERIAL
  Serial.println("[TELEM] Sent to gateway via mesh");
#endif
}

void MeshSwarm::handleTelemetry(uint32_t from, JsonObject& data) {
  // Gateway received telemetry from another node - push to server
#if MESHSWARM_ENABLE_SERIAL
  Serial.printf("[GATEWAY] Received telemetry from %s\n", nodeIdToName(from).c_str());

  // Debug: dump the telemetry payload
  String debugPayload;
  serializeJson(data, debugPayload);
  Serial.printf("[GATEWAY] Payload: %s\n", debugPayload.c_str());
#endif

  pushTelemetryForNode(from, data);
}

void MeshSwarm::pushTelemetryForNode(uint32_t nodeId, JsonObject& data) {
  if (!isWiFiConnected() || telemetryUrl.length() == 0) {
#if MESHSWARM_ENABLE_SERIAL
    Serial.println("[GATEWAY] Cannot push - WiFi not connected or no server URL");
#endif
    return;
  }

  HTTPClient http;
  String url = telemetryUrl + "/api/v1/nodes/" + String(nodeId, HEX) + "/telemetry";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  if (telemetryApiKey.length() > 0) {
    http.addHeader("X-API-Key", telemetryApiKey);
  }
  http.setTimeout(5000);

  String payload;
  serializeJson(data, payload);

  int httpCode = http.POST(payload);
#if MESHSWARM_ENABLE_SERIAL
  if (httpCode == 200 || httpCode == 201) {
    Serial.printf("[GATEWAY] Push OK for %s\n", nodeIdToName(nodeId).c_str());
  } else {
    Serial.printf("[GATEWAY] Push failed for %s: %d\n", nodeIdToName(nodeId).c_str(), httpCode);
  }
#endif
  http.end();
}
#endif // MESHSWARM_ENABLE_TELEMETRY

#if MESHSWARM_ENABLE_OTA
// ============== OTA DISTRIBUTION (GATEWAY) ==============
void MeshSwarm::enableOTADistribution(bool enable) {
  otaDistributionEnabled = enable;
#if MESHSWARM_ENABLE_SERIAL
  Serial.printf("[OTA] Distribution %s\n", enable ? "enabled" : "disabled");
#endif
}

void MeshSwarm::checkForOTAUpdates() {
  if (!otaDistributionEnabled || !gatewayMode) {
    return;
  }

  unsigned long now = millis();
  if (now - lastOTACheck < OTA_POLL_INTERVAL) {
    return;
  }
  lastOTACheck = now;

  // Don't poll if we're already distributing
  if (currentOTAUpdate.active) {
    return;
  }

  if (!isWiFiConnected()) {
#if MESHSWARM_ENABLE_SERIAL
    Serial.println("[OTA] WiFi not connected, skipping check");
#endif
    return;
  }

  // Poll server for pending updates
  if (pollPendingOTAUpdates()) {
    // Found an update, download firmware
    if (downloadOTAFirmware(currentOTAUpdate.firmwareId)) {
      // Start distribution
      startOTADistribution();
    }
  }
}

bool MeshSwarm::pollPendingOTAUpdates() {
  if (telemetryUrl.length() == 0) {
    return false;
  }

  // Don't start a new update if we're actively transferring
  if (currentOTAUpdate.active && otaTransferStarted) {
#if MESHSWARM_ENABLE_SERIAL
    Serial.println("[OTA] Transfer in progress, skipping poll");
#endif
    return false;
  }

  HTTPClient http;
  String url = telemetryUrl + "/api/v1/ota/updates/pending";

  http.begin(url);
  http.setTimeout(10000);

  int httpCode = http.GET();
  if (httpCode != 200) {
#if MESHSWARM_ENABLE_SERIAL
    Serial.printf("[OTA] Poll failed: %d\n", httpCode);
#endif
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  // Parse JSON array of pending updates
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
#if MESHSWARM_ENABLE_SERIAL
    Serial.printf("[OTA] JSON parse error: %s\n", err.c_str());
#endif
    return false;
  }

  JsonArray updates = doc.as<JsonArray>();
  if (updates.size() == 0) {
    return false;
  }

  // Take the first pending update
  JsonObject update = updates[0];

  currentOTAUpdate.updateId = update["update_id"] | 0;
  currentOTAUpdate.firmwareId = update["firmware_id"] | 0;
  currentOTAUpdate.nodeType = update["node_type"] | "";
  currentOTAUpdate.version = update["version"] | "";
  currentOTAUpdate.hardware = update["hardware"] | "ESP32";
  currentOTAUpdate.md5 = update["md5"] | "";
  currentOTAUpdate.numParts = update["num_parts"] | 0;
  currentOTAUpdate.sizeBytes = update["size_bytes"] | 0;
  currentOTAUpdate.targetNodeId = update["target_node_id"] | "";
  currentOTAUpdate.force = update["force"] | false;
  currentOTAUpdate.active = true;

#if MESHSWARM_ENABLE_SERIAL
  Serial.printf("[OTA] Found update: id=%d, type=%s, version=%s, parts=%d\n",
                currentOTAUpdate.updateId,
                currentOTAUpdate.nodeType.c_str(),
                currentOTAUpdate.version.c_str(),
                currentOTAUpdate.numParts);
#endif

  return true;
}

bool MeshSwarm::downloadOTAFirmware(int firmwareId) {
  if (telemetryUrl.length() == 0) {
    return false;
  }

  // For painlessMesh OTA distribution, we only need firmware metadata
  // The actual firmware will be streamed chunk by chunk via the callback
  // Store firmware size for the callback
  otaFirmwareSize = currentOTAUpdate.sizeBytes;

#if MESHSWARM_ENABLE_SERIAL
  Serial.printf("[OTA] Firmware %d ready for distribution (%d bytes, %d parts)\n",
                firmwareId, otaFirmwareSize, currentOTAUpdate.numParts);
#endif

  return true;
}

void MeshSwarm::startOTADistribution() {
  if (otaFirmwareSize == 0) {
#if MESHSWARM_ENABLE_SERIAL
    Serial.println("[OTA] No firmware size set");
#endif
    currentOTAUpdate.active = false;
    return;
  }

#if MESHSWARM_ENABLE_SERIAL
  Serial.printf("[OTA] Starting distribution for %s v%s\n",
                currentOTAUpdate.nodeType.c_str(),
                currentOTAUpdate.version.c_str());
#endif

  // Reset tracking variables
  otaLastPartSent = -1;
  otaTransferStarted = false;

  // Report to server that we're starting
  reportOTAStart(currentOTAUpdate.updateId);

  // Store firmware ID for the callback to fetch chunks on demand
  int firmwareId = currentOTAUpdate.firmwareId;

  // Initialize painlessMesh OTA sender
  // The callback fetches firmware data chunk by chunk from the server
  mesh.initOTASend([this, firmwareId](painlessmesh::plugin::ota::DataRequest pkg, char* buffer) {
    // Fetch this specific chunk from the server
    size_t offset = pkg.partNo * OTA_PART_SIZE;
    size_t remaining = otaFirmwareSize - offset;
    size_t chunkSize = min((size_t)OTA_PART_SIZE, remaining);

    if (offset >= otaFirmwareSize) {
      return (size_t)0;
    }

    // Fetch chunk from server using Range header
    HTTPClient http;
    String url = telemetryUrl + "/api/v1/firmware/" + String(firmwareId) + "/download";

    http.begin(url);
    http.setTimeout(10000);
    // Request specific byte range
    String rangeHeader = "bytes=" + String(offset) + "-" + String(offset + chunkSize - 1);
    http.addHeader("Range", rangeHeader);

    int httpCode = http.GET();
    if (httpCode != 206 && httpCode != 200) {
#if MESHSWARM_ENABLE_SERIAL
      Serial.printf("[OTA] Chunk fetch failed: %d (part %d)\n", httpCode, pkg.partNo);
#endif
      http.end();
      return (size_t)0;
    }

    // Read chunk into buffer
    WiFiClient* stream = http.getStreamPtr();
    size_t bytesRead = 0;
    while (bytesRead < chunkSize && http.connected()) {
      size_t available = stream->available();
      if (available > 0) {
        size_t toRead = min(available, chunkSize - bytesRead);
        size_t read = stream->readBytes(buffer + bytesRead, toRead);
        bytesRead += read;
      }
      delay(1);
    }
    http.end();

    if (bytesRead != chunkSize) {
#if MESHSWARM_ENABLE_SERIAL
      Serial.printf("[OTA] Incomplete chunk: %d/%d bytes (part %d)\n", bytesRead, chunkSize, pkg.partNo);
#endif
      return (size_t)0;
    }

    // Track transfer progress
    if (!otaTransferStarted) {
      otaTransferStarted = true;
#if MESHSWARM_ENABLE_SERIAL
      Serial.println("[OTA] Transfer started - node is receiving firmware");
#endif
    }
    otaLastPartSent = pkg.partNo;

#if MESHSWARM_ENABLE_SERIAL
    Serial.printf("[OTA] Sent part %d/%d\n", pkg.partNo + 1, currentOTAUpdate.numParts);
#endif

    // Check if this was the last part
    if (pkg.partNo + 1 >= currentOTAUpdate.numParts) {
#if MESHSWARM_ENABLE_SERIAL
      Serial.println("[OTA] All parts sent - transfer complete!");
#endif
      // Report completion to server
      reportOTAComplete(currentOTAUpdate.updateId);
      currentOTAUpdate.active = false;
    }

    return chunkSize;
  }, OTA_PART_SIZE);

  // Offer firmware to nodes with matching role
  // offerOTA returns a shared_ptr<Task>, not bool - check if it's valid
  auto otaTask = mesh.offerOTA(
    currentOTAUpdate.nodeType,      // role (node type)
    currentOTAUpdate.hardware,      // hardware type
    currentOTAUpdate.md5,           // MD5 hash
    currentOTAUpdate.numParts,      // number of parts
    currentOTAUpdate.force          // force update
  );

  if (otaTask) {
#if MESHSWARM_ENABLE_SERIAL
    Serial.printf("[OTA] Offered to nodes with role=%s\n", currentOTAUpdate.nodeType.c_str());
    Serial.println("[OTA] Waiting for nodes to request firmware...");
#endif
    // Note: painlessMesh will handle distribution automatically
    // The update remains active - chunks will be sent via the callback
    // We do NOT mark complete here - that happens when transfer actually occurs
    // or when a new update supersedes this one
  } else {
#if MESHSWARM_ENABLE_SERIAL
    Serial.println("[OTA] Failed to offer update");
#endif
    reportOTAFail(currentOTAUpdate.updateId, "Failed to offer update via mesh");
    currentOTAUpdate.active = false;
  }
}

void MeshSwarm::reportOTAStart(int updateId) {
  if (telemetryUrl.length() == 0 || !isWiFiConnected()) {
    return;
  }

  HTTPClient http;
  String url = telemetryUrl + "/api/v1/ota/updates/" + String(updateId) + "/start";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  int httpCode = http.POST("");
#if MESHSWARM_ENABLE_SERIAL
  if (httpCode == 200) {
    Serial.printf("[OTA] Reported start for update %d\n", updateId);
  } else {
    Serial.printf("[OTA] Failed to report start: %d\n", httpCode);
  }
#endif
  http.end();
}

void MeshSwarm::reportOTAProgress(const String& nodeId, int currentPart, int totalParts, const String& status, const String& error) {
  if (telemetryUrl.length() == 0 || !isWiFiConnected() || currentOTAUpdate.updateId == 0) {
    return;
  }

  HTTPClient http;
  String url = telemetryUrl + "/api/v1/ota/updates/" + String(currentOTAUpdate.updateId) +
               "/node/" + nodeId + "/progress";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  JsonDocument doc;
  doc["current_part"] = currentPart;
  doc["total_parts"] = totalParts;
  doc["status"] = status;
  if (error.length() > 0) {
    doc["error_message"] = error;
  }

  String payload;
  serializeJson(doc, payload);

  int httpCode = http.POST(payload);
#if MESHSWARM_ENABLE_SERIAL
  if (httpCode != 200) {
    Serial.printf("[OTA] Failed to report progress: %d\n", httpCode);
  }
#endif
  http.end();
}

void MeshSwarm::reportOTAComplete(int updateId) {
  if (telemetryUrl.length() == 0 || !isWiFiConnected()) {
    return;
  }

  HTTPClient http;
  String url = telemetryUrl + "/api/v1/ota/updates/" + String(updateId) + "/complete";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  int httpCode = http.POST("");
#if MESHSWARM_ENABLE_SERIAL
  if (httpCode == 200) {
    Serial.printf("[OTA] Reported complete for update %d\n", updateId);
  } else {
    Serial.printf("[OTA] Failed to report complete: %d\n", httpCode);
  }
#endif
  http.end();
}

void MeshSwarm::reportOTAFail(int updateId, const String& error) {
  if (telemetryUrl.length() == 0 || !isWiFiConnected()) {
    return;
  }

  HTTPClient http;
  String url = telemetryUrl + "/api/v1/ota/updates/" + String(updateId) + "/fail?error_message=" + error;

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  int httpCode = http.POST("");
#if MESHSWARM_ENABLE_SERIAL
  if (httpCode == 200) {
    Serial.printf("[OTA] Reported failure for update %d\n", updateId);
  } else {
    Serial.printf("[OTA] Failed to report failure: %d\n", httpCode);
  }
#endif
  http.end();
}

void MeshSwarm::cleanupOTABuffer() {
  if (otaFirmwareBuffer) {
    free(otaFirmwareBuffer);
    otaFirmwareBuffer = nullptr;
  }
  otaFirmwareSize = 0;
}

// ============== OTA RECEPTION (NODE) ==============
void MeshSwarm::enableOTAReceive(const String& role) {
  mesh.initOTAReceive(role.c_str());
#if MESHSWARM_ENABLE_SERIAL
  Serial.printf("[OTA] Receiver enabled for role: %s\n", role.c_str());
#endif
}
#endif // MESHSWARM_ENABLE_OTA
