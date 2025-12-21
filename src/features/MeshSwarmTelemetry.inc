/*
 * MeshSwarm Library - Telemetry Module
 *
 * HTTP telemetry pushing and gateway mode functionality.
 * Only compiled when MESHSWARM_ENABLE_TELEMETRY is enabled.
 */

#include "../MeshSwarm.h"

#if MESHSWARM_ENABLE_TELEMETRY

// ============== TELEMETRY CONFIGURATION ==============
void MeshSwarm::setTelemetryServer(const char* url, const char* apiKey) {
  telemetryUrl = String(url);
  if (apiKey != nullptr) {
    telemetryApiKey = String(apiKey);
  }
  TELEM_LOG("Server: %s", telemetryUrl.c_str());
}

void MeshSwarm::setTelemetryInterval(unsigned long ms) {
  telemetryInterval = ms;
  TELEM_LOG("Interval: %lu ms", telemetryInterval);
}

void MeshSwarm::enableTelemetry(bool enable) {
  telemetryEnabled = enable;
  TELEM_LOG("%s", enable ? "Enabled" : "Disabled");
}

void MeshSwarm::connectToWiFi(const char* ssid, const char* password) {
  // painlessMesh supports station mode alongside mesh
  mesh.stationManual(ssid, password);
  MESH_LOG("WiFi connecting to %s...", ssid);
}

bool MeshSwarm::isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

// ============== TELEMETRY PUSHING ==============
void MeshSwarm::pushTelemetry() {
  if (!telemetryEnabled || telemetryUrl.length() == 0) {
    return;
  }

  if (!isWiFiConnected()) {
    TELEM_LOG_D("WiFi not connected, skipping push");
    return;
  }

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

  String url = telemetryUrl + "/api/v1/nodes/" + String(myId, HEX) + "/telemetry";
  int httpCode = httpPost(url, payload);

  if (httpCode == 200 || httpCode == 201) {
    TELEM_LOG_D("Push OK");
  } else {
    TELEM_LOG("Push failed: %d", httpCode);
  }
}

// ============== GATEWAY MODE ==============
void MeshSwarm::setGatewayMode(bool enable) {
  gatewayMode = enable;
  GATEWAY_LOG("%s", enable ? "Enabled" : "Disabled");
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

  TELEM_LOG_D("Sent to gateway via mesh");
}

void MeshSwarm::handleTelemetry(uint32_t from, JsonObject& data) {
  // Gateway received telemetry from another node - push to server
  GATEWAY_LOG("Received telemetry from %s", nodeIdToName(from).c_str());

#if MESHSWARM_ENABLE_SERIAL && MESHSWARM_LOG_LEVEL >= MESHSWARM_LOG_DEBUG
  // Debug: dump the telemetry payload
  String debugPayload;
  serializeJson(data, debugPayload);
  Serial.printf("[GATEWAY] Payload: %s\n", debugPayload.c_str());
#endif

  pushTelemetryForNode(from, data);
}

void MeshSwarm::pushTelemetryForNode(uint32_t nodeId, JsonObject& data) {
  if (!isWiFiConnected() || telemetryUrl.length() == 0) {
    GATEWAY_LOG("Cannot push - WiFi not connected or no server URL");
    return;
  }

  String payload;
  serializeJson(data, payload);

  String url = telemetryUrl + "/api/v1/nodes/" + String(nodeId, HEX) + "/telemetry";
  int httpCode = httpPost(url, payload);

  if (httpCode == 200 || httpCode == 201) {
    GATEWAY_LOG("Push OK for %s", nodeIdToName(nodeId).c_str());
  } else {
    GATEWAY_LOG("Push failed for %s: %d", nodeIdToName(nodeId).c_str(), httpCode);
  }
}

#endif // MESHSWARM_ENABLE_TELEMETRY
