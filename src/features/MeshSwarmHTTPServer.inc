/*
 * MeshSwarm Library - HTTP Server Module
 *
 * REST API endpoints for remote command protocol.
 * Only compiled when MESHSWARM_ENABLE_HTTP_SERVER is enabled.
 *
 * Endpoints:
 *   POST /api/command - Send command to node(s)
 *   GET /api/nodes - List all known nodes
 *   GET /api/state - Get all shared state
 */

#include "../MeshSwarm.h"

#if MESHSWARM_ENABLE_HTTP_SERVER

// Global server instance (needed for async callbacks)
static AsyncWebServer* _httpServer = nullptr;
static MeshSwarm* _swarmInstance = nullptr;

// Pending HTTP requests waiting for command responses
struct PendingHTTPRequest {
  AsyncWebServerRequest* request;
  String requestId;
  String target;
  unsigned long timestamp;
  unsigned long timeout;
  bool isBroadcast;
  std::vector<JsonDocument> responses;
  int expectedResponses;
};

static std::vector<PendingHTTPRequest> _pendingHTTPRequests;

// ============== HTTP SERVER CONTROL ==============
void MeshSwarm::startHTTPServer(uint16_t port) {
  if (httpServerRunning) {
    HTTP_LOG("Server already running");
    return;
  }

  _swarmInstance = this;

  if (_httpServer == nullptr) {
    _httpServer = new AsyncWebServer(port);
  }

  // CORS headers for all responses
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

  // OPTIONS handler for CORS preflight
  _httpServer->on("/api/command", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    request->send(204);
  });

  // POST /api/command - Send command to node(s)
  _httpServer->on("/api/command", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      // Body handler processes the request
    },
    nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (_swarmInstance == nullptr) {
        request->send(500, "application/json", "{\"error\":\"Server not initialized\"}");
        return;
      }

      // Parse JSON body
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, data, len);
      if (err) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }

      String target = doc["target"] | "";
      String command = doc["command"] | "";
      unsigned long timeout = doc["timeout"] | 5000;

      if (target.length() == 0 || command.length() == 0) {
        request->send(400, "application/json", "{\"error\":\"Missing target or command\"}");
        return;
      }

      HTTP_LOG("API command: %s -> %s", command.c_str(), target.c_str());

      // Build args object (avoid deprecated containsKey)
      JsonDocument argsDoc;
      JsonObject args = argsDoc.to<JsonObject>();
      if (doc["args"].is<JsonObject>()) {
        args = doc["args"].as<JsonObject>();
      }

      // Check if target is self
      if (_swarmInstance->isCommandTargetMatch(target)) {
        // Execute locally
        JsonDocument resultDoc;
        bool success = false;
        String error = "";

        if (_swarmInstance->commandHandlers.count(command)) {
          String selfName = _swarmInstance->getNodeName();
          resultDoc = _swarmInstance->commandHandlers[command](selfName, args);
          success = true;
        } else {
          error = "Unknown command: " + command;
        }

        // Build response
        JsonDocument responseDoc;
        responseDoc["target"] = target;
        responseDoc["command"] = command;
        JsonArray responses = responseDoc["responses"].to<JsonArray>();
        JsonObject resp = responses.add<JsonObject>();
        resp["node"] = _swarmInstance->getNodeName();
        resp["success"] = success;
        if (success) {
          resp["result"] = resultDoc;
        } else {
          resp["error"] = error;
        }
        responseDoc["timeout_nodes"] = JsonArray();

        String responseStr;
        serializeJson(responseDoc, responseStr);
        request->send(200, "application/json", responseStr);
        return;
      }

      // Generate request ID
      String requestId = _swarmInstance->generateRequestId();

      // For remote targets, send command and wait for response
      _swarmInstance->sendCommand(target, command, args,
        [request, requestId, target](bool success, const String& node, JsonObject& result) {
          // Find matching pending request
          for (auto it = _pendingHTTPRequests.begin(); it != _pendingHTTPRequests.end(); ++it) {
            if (it->requestId == requestId) {
              // Store response
              JsonDocument respDoc;
              respDoc["node"] = node;
              respDoc["success"] = success;
              if (success && !result.isNull()) {
                respDoc["result"] = result;
              }
              it->responses.push_back(std::move(respDoc));

              // For non-broadcast, respond immediately
              if (!it->isBroadcast) {
                JsonDocument responseDoc;
                responseDoc["request_id"] = requestId;
                responseDoc["target"] = target;
                responseDoc["command"] = it->request ? "completed" : "";
                JsonArray responses = responseDoc["responses"].to<JsonArray>();
                for (auto& r : it->responses) {
                  responses.add(r);
                }
                responseDoc["timeout_nodes"] = JsonArray();

                String responseStr;
                serializeJson(responseDoc, responseStr);

                if (it->request) {
                  it->request->send(200, "application/json", responseStr);
                }
                _pendingHTTPRequests.erase(it);
              }
              return;
            }
          }
        }, timeout);

      // Store pending request
      PendingHTTPRequest pending;
      pending.request = request;
      pending.requestId = requestId;
      pending.target = target;
      pending.timestamp = millis();
      pending.timeout = timeout;
      pending.isBroadcast = (target == "*");
      pending.expectedResponses = (target == "*") ? _swarmInstance->getPeerCount() : 1;
      _pendingHTTPRequests.push_back(pending);

      // For broadcast, we need to wait and collect responses
      // The response will be sent when timeout occurs or all responses received
      // For now, we'll handle timeout in the main loop
    });

  // GET /api/nodes - List all known nodes
  _httpServer->on("/api/nodes", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (_swarmInstance == nullptr) {
      request->send(500, "application/json", "{\"error\":\"Server not initialized\"}");
      return;
    }

    JsonDocument doc;
    JsonArray nodes = doc["nodes"].to<JsonArray>();

    // Add self
    JsonObject self = nodes.add<JsonObject>();
    self["id"] = _swarmInstance->getNodeId();
    self["name"] = _swarmInstance->getNodeName();
    self["role"] = _swarmInstance->getRole();
    self["alive"] = true;
    self["self"] = true;

    // Add peers
    for (auto& p : _swarmInstance->getPeers()) {
      JsonObject peer = nodes.add<JsonObject>();
      peer["id"] = p.second.id;
      peer["name"] = p.second.name;
      peer["role"] = p.second.role;
      peer["alive"] = p.second.alive;
      peer["self"] = false;
    }

    doc["count"] = nodes.size();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // GET /api/state - Get all shared state
  _httpServer->on("/api/state", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (_swarmInstance == nullptr) {
      request->send(500, "application/json", "{\"error\":\"Server not initialized\"}");
      return;
    }

    JsonDocument doc;
    JsonObject state = doc["state"].to<JsonObject>();

    for (auto& kv : _swarmInstance->sharedState) {
      JsonObject entry = state[kv.first].to<JsonObject>();
      entry["value"] = kv.second.value;
      entry["version"] = kv.second.version;
      entry["origin"] = _swarmInstance->nodeIdToName(kv.second.origin);
    }

    doc["count"] = _swarmInstance->sharedState.size();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // GET /api/info - Get gateway info
  _httpServer->on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (_swarmInstance == nullptr) {
      request->send(500, "application/json", "{\"error\":\"Server not initialized\"}");
      return;
    }

    JsonDocument doc;
    doc["name"] = _swarmInstance->getNodeName();
    doc["id"] = _swarmInstance->getNodeId();
    doc["role"] = _swarmInstance->getRole();
    doc["version"] = FIRMWARE_VERSION;
    doc["uptime"] = millis() / 1000;
    doc["heap"] = ESP.getFreeHeap();
    doc["peers"] = _swarmInstance->getPeerCount();

    // List available commands
    JsonArray cmds = doc["commands"].to<JsonArray>();
    for (auto& kv : _swarmInstance->commandHandlers) {
      cmds.add(kv.first);
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // 404 handler
  _httpServer->onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "application/json", "{\"error\":\"Not found\"}");
  });

  _httpServer->begin();
  httpServerRunning = true;
  HTTP_LOG("Server started on port %d", port);
}

void MeshSwarm::stopHTTPServer() {
  if (!httpServerRunning) {
    return;
  }

  if (_httpServer != nullptr) {
    _httpServer->end();
    delete _httpServer;
    _httpServer = nullptr;
  }

  _swarmInstance = nullptr;
  httpServerRunning = false;
  HTTP_LOG("Server stopped");
}

#endif // MESHSWARM_ENABLE_HTTP_SERVER
