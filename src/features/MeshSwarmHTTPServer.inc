/*
 * MeshSwarm Library - HTTP Server Module
 *
 * REST API endpoints for remote command protocol.
 * Only compiled when MESHSWARM_ENABLE_HTTP_SERVER is enabled.
 *
 * Endpoints:
 *   POST /api/command - Send command to node(s)
 *   GET /api/result/{id} - Poll for command result
 *   GET /api/nodes - List all known nodes
 *   GET /api/state - Get all shared state
 *   GET /api/info - Get gateway info
 */

#include "../MeshSwarm.h"

#if MESHSWARM_ENABLE_HTTP_SERVER

// Global server instance (needed for async callbacks)
static AsyncWebServer* _httpServer = nullptr;
static MeshSwarm* _swarmInstance = nullptr;

// Store pending command results for polling
struct CommandResult {
  String requestId;
  String target;
  String command;
  String status;  // "pending", "complete", "timeout"
  unsigned long timestamp;
  unsigned long timeout;
  std::vector<JsonDocument> responses;
  int expectedResponses;
  bool isBroadcast;
};

static std::map<String, CommandResult> _commandResults;
static const size_t MAX_STORED_RESULTS = 20;  // Limit memory usage
static const unsigned long RESULT_EXPIRY_MS = 60000;  // Clean up after 1 minute

// Clean up old results
static void cleanupOldResults() {
  unsigned long now = millis();
  for (auto it = _commandResults.begin(); it != _commandResults.end(); ) {
    if (now - it->second.timestamp > RESULT_EXPIRY_MS) {
      it = _commandResults.erase(it);
    } else {
      ++it;
    }
  }
  // Also limit total count
  while (_commandResults.size() > MAX_STORED_RESULTS) {
    // Remove oldest
    auto oldest = _commandResults.begin();
    for (auto it = _commandResults.begin(); it != _commandResults.end(); ++it) {
      if (it->second.timestamp < oldest->second.timestamp) {
        oldest = it;
      }
    }
    _commandResults.erase(oldest);
  }
}

// Forward declaration (friend of MeshSwarm class)
void handleCommandRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len);

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
      // This handler is called after body is received
      // The body handler below does the actual work
    },
    nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      // Only process complete body
      if (index + len == total) {
        handleCommandRequest(request, data, len);
      }
    });

  // GET /api/result?id=xxx - Poll for command result by request_id
  _httpServer->on("/api/result", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (_swarmInstance == nullptr) {
      request->send(500, "application/json", "{\"error\":\"Server not initialized\"}");
      return;
    }

    if (!request->hasParam("id")) {
      request->send(400, "application/json", "{\"error\":\"Missing id parameter\"}");
      return;
    }

    String requestId = request->getParam("id")->value();

    auto it = _commandResults.find(requestId);
    if (it == _commandResults.end()) {
      request->send(404, "application/json", "{\"error\":\"Request ID not found\"}");
      return;
    }

    CommandResult& result = it->second;

    JsonDocument doc;
    doc["request_id"] = result.requestId;
    doc["target"] = result.target;
    doc["command"] = result.command;
    doc["status"] = result.status;

    JsonArray responses = doc["responses"].to<JsonArray>();
    for (auto& r : result.responses) {
      responses.add(r);
    }

    // For broadcast, check if we've received expected responses or timed out
    if (result.isBroadcast) {
      doc["expected"] = result.expectedResponses;
      doc["received"] = result.responses.size();
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);

    // If complete, remove from storage
    if (result.status == "complete" || result.status == "timeout") {
      _commandResults.erase(it);
    }
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

// ============== COMMAND REQUEST HANDLER ==============
void handleCommandRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
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

  // Build args object
  JsonDocument argsDoc;
  JsonObject args = argsDoc.to<JsonObject>();
  if (doc["args"].is<JsonObject>()) {
    args = doc["args"].as<JsonObject>();
  }

  // Check if target is self (gateway)
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
    responseDoc["request_id"] = "local";
    responseDoc["target"] = target;
    responseDoc["command"] = command;
    responseDoc["status"] = "complete";
    JsonArray responses = responseDoc["responses"].to<JsonArray>();
    JsonObject resp = responses.add<JsonObject>();
    resp["node"] = _swarmInstance->getNodeName();
    resp["success"] = success;
    if (success) {
      resp["result"] = resultDoc;
    } else {
      resp["error"] = error;
    }

    String responseStr;
    serializeJson(responseDoc, responseStr);
    request->send(200, "application/json", responseStr);
    return;
  }

  // Clean up old results before adding new one
  cleanupOldResults();

  // Generate request ID for correlation
  String requestId = _swarmInstance->generateRequestId();
  bool isBroadcast = (target == "*");

  // Create result entry for polling
  CommandResult& result = _commandResults[requestId];
  result.requestId = requestId;
  result.target = target;
  result.command = command;
  result.status = "pending";
  result.timestamp = millis();
  result.timeout = timeout;
  result.isBroadcast = isBroadcast;
  result.expectedResponses = isBroadcast ? _swarmInstance->getPeerCount() : 1;

  // Capture values for lambda
  String ridCopy = requestId;

  // Send command to mesh
  _swarmInstance->sendCommand(target, command, args,
    [ridCopy](bool success, const String& node, JsonObject& cmdResult) {
      // Store the response
      auto it = _commandResults.find(ridCopy);
      if (it != _commandResults.end()) {
        JsonDocument respDoc;
        respDoc["node"] = node;
        respDoc["success"] = success;
        if (success && !cmdResult.isNull()) {
          respDoc["result"] = cmdResult;
        }
        it->second.responses.push_back(std::move(respDoc));

        // Mark complete for unicast, or when all broadcast responses received
        if (!it->second.isBroadcast) {
          it->second.status = "complete";
        } else if ((int)it->second.responses.size() >= it->second.expectedResponses) {
          it->second.status = "complete";
        }

        HTTP_LOG("CMD response stored: rid=%s node=%s (status=%s)",
                 ridCopy.c_str(), node.c_str(), it->second.status.c_str());
      }
    }, timeout);

  // Send immediate response with request_id for polling
  JsonDocument responseDoc;
  responseDoc["request_id"] = requestId;
  responseDoc["target"] = target;
  responseDoc["command"] = command;
  responseDoc["status"] = "pending";
  responseDoc["poll_url"] = "/api/result?id=" + requestId;

  String responseStr;
  serializeJson(responseDoc, responseStr);
  request->send(202, "application/json", responseStr);
}

#endif // MESHSWARM_ENABLE_HTTP_SERVER
