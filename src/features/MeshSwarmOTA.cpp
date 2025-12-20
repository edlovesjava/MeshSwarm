/*
 * MeshSwarm Library - OTA Module
 * 
 * OTA (Over-The-Air) firmware distribution functionality.
 * Only compiled when MESHSWARM_ENABLE_OTA is enabled.
 */

#include "../MeshSwarm.h"

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
