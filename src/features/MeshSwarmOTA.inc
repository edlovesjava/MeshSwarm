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
  OTA_LOG("Distribution %s", enable ? "enabled" : "disabled");
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
    OTA_LOG_D("WiFi not connected, skipping check");
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
    OTA_LOG_D("Transfer in progress, skipping poll");
    return false;
  }

  String url = telemetryUrl + "/api/v1/ota/updates/pending";
  String response;
  int httpCode = httpGet(url, &response, 10000);

  if (httpCode != 200) {
    OTA_LOG("Poll failed: %d", httpCode);
    return false;
  }

  // Parse JSON array of pending updates
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    OTA_LOG("JSON parse error: %s", err.c_str());
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

  OTA_LOG("Found update: id=%d, type=%s, version=%s, parts=%d",
          currentOTAUpdate.updateId,
          currentOTAUpdate.nodeType.c_str(),
          currentOTAUpdate.version.c_str(),
          currentOTAUpdate.numParts);

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

  OTA_LOG("Firmware %d ready for distribution (%d bytes, %d parts)",
          firmwareId, otaFirmwareSize, currentOTAUpdate.numParts);

  return true;
}

void MeshSwarm::startOTADistribution() {
  if (otaFirmwareSize == 0) {
    OTA_LOG("No firmware size set");
    currentOTAUpdate.active = false;
    return;
  }

  OTA_LOG("Starting distribution for %s v%s",
          currentOTAUpdate.nodeType.c_str(),
          currentOTAUpdate.version.c_str());

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
    String url = telemetryUrl + "/api/v1/firmware/" + String(firmwareId) + "/download";
    int result = httpGetRange(url, (uint8_t*)buffer, chunkSize,
                              offset, offset + chunkSize - 1, 10000);

    if (result < 0) {
      OTA_LOG("Chunk fetch failed: %d (part %d)", result, pkg.partNo);
      return (size_t)0;
    }

    if ((size_t)result != chunkSize) {
      OTA_LOG("Incomplete chunk: %d/%d bytes (part %d)", result, chunkSize, pkg.partNo);
      return (size_t)0;
    }

    // Track transfer progress
    if (!otaTransferStarted) {
      otaTransferStarted = true;
      OTA_LOG("Transfer started - node is receiving firmware");
    }
    otaLastPartSent = pkg.partNo;

    OTA_LOG_D("Sent part %d/%d", pkg.partNo + 1, currentOTAUpdate.numParts);

    // Check if this was the last part
    if (pkg.partNo + 1 >= currentOTAUpdate.numParts) {
      OTA_LOG("All parts sent - transfer complete!");
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
    OTA_LOG("Offered to nodes with role=%s", currentOTAUpdate.nodeType.c_str());
    OTA_LOG("Waiting for nodes to request firmware...");
    // Note: painlessMesh will handle distribution automatically
    // The update remains active - chunks will be sent via the callback
    // We do NOT mark complete here - that happens when transfer actually occurs
    // or when a new update supersedes this one
  } else {
    OTA_LOG("Failed to offer update");
    reportOTAFail(currentOTAUpdate.updateId, "Failed to offer update via mesh");
    currentOTAUpdate.active = false;
  }
}

void MeshSwarm::reportOTAStart(int updateId) {
  if (telemetryUrl.length() == 0 || !isWiFiConnected()) {
    return;
  }

  String url = telemetryUrl + "/api/v1/ota/updates/" + String(updateId) + "/start";
  int httpCode = httpPost(url, "");

  if (httpCode == 200) {
    OTA_LOG_D("Reported start for update %d", updateId);
  } else {
    OTA_LOG("Failed to report start: %d", httpCode);
  }
}

void MeshSwarm::reportOTAProgress(const String& nodeId, int currentPart, int totalParts, const String& status, const String& error) {
  if (telemetryUrl.length() == 0 || !isWiFiConnected() || currentOTAUpdate.updateId == 0) {
    return;
  }

  JsonDocument doc;
  doc["current_part"] = currentPart;
  doc["total_parts"] = totalParts;
  doc["status"] = status;
  if (error.length() > 0) {
    doc["error_message"] = error;
  }

  String payload;
  serializeJson(doc, payload);

  String url = telemetryUrl + "/api/v1/ota/updates/" + String(currentOTAUpdate.updateId) +
               "/node/" + nodeId + "/progress";
  int httpCode = httpPost(url, payload);

  if (httpCode != 200) {
    OTA_LOG("Failed to report progress: %d", httpCode);
  }
}

void MeshSwarm::reportOTAComplete(int updateId) {
  if (telemetryUrl.length() == 0 || !isWiFiConnected()) {
    return;
  }

  String url = telemetryUrl + "/api/v1/ota/updates/" + String(updateId) + "/complete";
  int httpCode = httpPost(url, "");

  if (httpCode == 200) {
    OTA_LOG_D("Reported complete for update %d", updateId);
  } else {
    OTA_LOG("Failed to report complete: %d", httpCode);
  }
}

void MeshSwarm::reportOTAFail(int updateId, const String& error) {
  if (telemetryUrl.length() == 0 || !isWiFiConnected()) {
    return;
  }

  String url = telemetryUrl + "/api/v1/ota/updates/" + String(updateId) + "/fail?error_message=" + error;
  int httpCode = httpPost(url, "");

  if (httpCode == 200) {
    OTA_LOG_D("Reported failure for update %d", updateId);
  } else {
    OTA_LOG("Failed to report failure: %d", httpCode);
  }
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
  OTA_LOG("Receiver enabled for role: %s", role.c_str());
}

#endif // MESHSWARM_ENABLE_OTA
