/*
 * MeshSwarm Library - HTTP Helper Module
 *
 * Consolidated HTTP request handling for telemetry and OTA.
 * Only compiled when MESHSWARM_ENABLE_TELEMETRY or MESHSWARM_ENABLE_OTA is enabled.
 */

#include "../MeshSwarm.h"

#if MESHSWARM_ENABLE_TELEMETRY || MESHSWARM_ENABLE_OTA

// ============== HTTP POST ==============
int MeshSwarm::httpPost(const String& url, const String& payload, String* response, int timeout) {
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
#if MESHSWARM_ENABLE_TELEMETRY
  if (telemetryApiKey.length() > 0) {
    http.addHeader("X-API-Key", telemetryApiKey);
  }
#endif
  http.setTimeout(timeout);

  int httpCode = http.POST(payload);
  if (response && httpCode > 0) {
    *response = http.getString();
  }
  http.end();
  return httpCode;
}

// ============== HTTP GET ==============
int MeshSwarm::httpGet(const String& url, String* response, int timeout) {
  HTTPClient http;
  http.begin(url);
#if MESHSWARM_ENABLE_TELEMETRY
  if (telemetryApiKey.length() > 0) {
    http.addHeader("X-API-Key", telemetryApiKey);
  }
#endif
  http.setTimeout(timeout);

  int httpCode = http.GET();
  if (response && httpCode > 0) {
    *response = http.getString();
  }
  http.end();
  return httpCode;
}

// ============== HTTP GET WITH RANGE (for OTA chunks) ==============
int MeshSwarm::httpGetRange(const String& url, uint8_t* buffer, size_t bufferSize,
                            int rangeStart, int rangeEnd, int timeout) {
  HTTPClient http;
  http.begin(url);
#if MESHSWARM_ENABLE_TELEMETRY
  if (telemetryApiKey.length() > 0) {
    http.addHeader("X-API-Key", telemetryApiKey);
  }
#endif

  // Add Range header for partial content
  String rangeHeader = "bytes=" + String(rangeStart) + "-" + String(rangeEnd);
  http.addHeader("Range", rangeHeader);
  http.setTimeout(timeout);

  int httpCode = http.GET();
  if (httpCode == 206 || httpCode == 200) {
    // Read binary data into buffer
    WiFiClient* stream = http.getStreamPtr();
    size_t bytesRead = 0;
    size_t expectedSize = rangeEnd - rangeStart + 1;
    if (expectedSize > bufferSize) {
      expectedSize = bufferSize;
    }

    while (bytesRead < expectedSize && http.connected()) {
      size_t available = stream->available();
      if (available > 0) {
        size_t toRead = min(available, expectedSize - bytesRead);
        size_t read = stream->readBytes(buffer + bytesRead, toRead);
        bytesRead += read;
      }
      delay(1);
    }
    http.end();

    // Return bytes read on success, or negative on incomplete read
    return (bytesRead == expectedSize) ? (int)bytesRead : -1;
  }

  http.end();
  return httpCode;  // Return HTTP error code
}

#endif // MESHSWARM_ENABLE_TELEMETRY || MESHSWARM_ENABLE_OTA
