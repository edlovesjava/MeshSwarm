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

// ============== TELEMETRY PUSHING ==============
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
