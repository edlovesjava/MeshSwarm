/*
 * MeshSwarm Library - Remote Command Protocol Module
 *
 * Enables nodes to send commands to other nodes through the mesh network.
 * Supports request/response correlation and custom command handlers.
 */

#include "../MeshSwarm.h"

// ============== REQUEST ID GENERATION ==============
String MeshSwarm::generateRequestId() {
  commandIdCounter++;
  // Format: nodeId_counter_millis for uniqueness
  return String(myId, HEX) + "_" + String(commandIdCounter) + "_" + String(millis() & 0xFFFF, HEX);
}

// ============== TARGET MATCHING ==============
bool MeshSwarm::isCommandTargetMatch(const String& target) {
  if (target == "*") return true;  // Broadcast
  if (target == myName) return true;  // Match by name
  if (target == String(myId)) return true;  // Match by ID (decimal)
  if (target.equalsIgnoreCase(nodeIdToName(myId))) return true;  // Match by short name
  return false;
}

// ============== SEND COMMAND ==============
void MeshSwarm::sendCommand(const String& target, const String& command) {
  JsonDocument emptyArgs;
  JsonObject args = emptyArgs.to<JsonObject>();
  sendCommand(target, command, args, nullptr, 5000);
}

void MeshSwarm::sendCommand(const String& target, const String& command, JsonObject& args) {
  sendCommand(target, command, args, nullptr, 5000);
}

void MeshSwarm::sendCommand(const String& target, const String& command, JsonObject& args,
                            CommandCallback callback, unsigned long timeout) {
  String requestId = generateRequestId();

  // Build command message
  JsonDocument data;
  data["cmd"] = command;
  data["target"] = target;
  data["rid"] = requestId;
  if (!args.isNull() && args.size() > 0) {
    data["args"] = args;
  }

  String msg = createMsg(MSG_COMMAND, data);

  // Store pending request if callback provided
  if (callback) {
    PendingCommand pending;
    pending.requestId = requestId;
    pending.target = target;
    pending.command = command;
    pending.callback = callback;
    pending.timestamp = millis();
    pending.timeout = timeout;
    pending.isBroadcast = (target == "*");
    pendingCommands.push_back(pending);
  }

  // Send via mesh
  if (target == "*") {
    // Broadcast to all nodes
    mesh.sendBroadcast(msg);
    CMD_LOG("CMD broadcast: %s (rid=%s)", command.c_str(), requestId.c_str());
  } else {
    // Try to find target node ID
    uint32_t targetId = 0;

    // Check if target is a numeric ID
    if (target.toInt() > 0) {
      targetId = target.toInt();
    } else {
      // Search peers by name
      for (auto& p : peers) {
        if (p.second.name == target || nodeIdToName(p.first) == target) {
          targetId = p.first;
          break;
        }
      }
    }

    if (targetId > 0) {
      mesh.sendSingle(targetId, msg);
      CMD_LOG("CMD -> %s: %s (rid=%s)", target.c_str(), command.c_str(), requestId.c_str());
    } else {
      // Target not found, broadcast and let target filter
      mesh.sendBroadcast(msg);
      CMD_LOG("CMD -> %s (broadcast, target unknown): %s", target.c_str(), command.c_str());
    }
  }
}

// ============== HANDLE INCOMING COMMAND ==============
void MeshSwarm::handleCommand(uint32_t from, JsonObject& data) {
  String command = data["cmd"] | "";
  String target = data["target"] | "";
  String requestId = data["rid"] | "";
  String senderName = nodeIdToName(from);

  // Check if we're the target
  if (!isCommandTargetMatch(target)) {
    return;  // Not for us
  }

  CMD_LOG("CMD from %s: %s (rid=%s)", senderName.c_str(), command.c_str(), requestId.c_str());

  JsonObject args = data["args"].as<JsonObject>();

  // Look up handler
  JsonDocument resultDoc;
  bool success = false;
  String error = "";

  if (commandHandlers.count(command)) {
    // Execute custom handler
    resultDoc = commandHandlers[command](senderName, args);
    success = true;
  } else {
    // Unknown command
    error = "Unknown command: " + command;
    CMD_LOG("Unknown command: %s", command.c_str());
  }

  // Send response back to sender
  sendCommandResponse(senderName, requestId, success, resultDoc, error);
}

// ============== SEND COMMAND RESPONSE ==============
void MeshSwarm::sendCommandResponse(const String& target, const String& requestId,
                                    bool success, JsonDocument& result, const String& error) {
  JsonDocument data;
  data["rid"] = requestId;
  data["success"] = success;
  if (error.length() > 0) {
    data["error"] = error;
  }
  if (!result.isNull() && result.size() > 0) {
    data["result"] = result;
  }

  String msg = createMsg(MSG_COMMAND_RESPONSE, data);

  // Find target node ID
  uint32_t targetId = 0;
  for (auto& p : peers) {
    if (p.second.name == target || nodeIdToName(p.first) == target) {
      targetId = p.first;
      break;
    }
  }

  if (targetId > 0) {
    mesh.sendSingle(targetId, msg);
  } else {
    // Target not found, broadcast response
    mesh.sendBroadcast(msg);
  }

  CMD_LOG("CMD response -> %s (rid=%s, success=%d)", target.c_str(), requestId.c_str(), success);
}

// ============== HANDLE COMMAND RESPONSE ==============
void MeshSwarm::handleCommandResponse(uint32_t from, JsonObject& data) {
  String requestId = data["rid"] | "";
  bool success = data["success"] | false;
  String error = data["error"] | "";
  String senderName = nodeIdToName(from);

  // Find matching pending command
  for (auto it = pendingCommands.begin(); it != pendingCommands.end(); ++it) {
    if (it->requestId == requestId) {
      CMD_LOG("CMD response from %s (rid=%s, success=%d)",
              senderName.c_str(), requestId.c_str(), success);

      if (it->callback) {
        JsonObject result = data["result"].as<JsonObject>();
        it->callback(success, senderName, result);
      }

      // For non-broadcast, remove immediately
      // For broadcast, we might want to collect multiple responses (future enhancement)
      if (!it->isBroadcast) {
        pendingCommands.erase(it);
      } else {
        // Track that this node responded
        it->respondedNodes.push_back(senderName);
      }
      return;
    }
  }

  CMD_LOG_D("Received response for unknown request: %s", requestId.c_str());
}

// ============== PROCESS COMMAND TIMEOUTS ==============
void MeshSwarm::processCommandTimeouts() {
  unsigned long now = millis();

  for (auto it = pendingCommands.begin(); it != pendingCommands.end(); ) {
    if (now - it->timestamp >= it->timeout) {
      CMD_LOG("CMD timeout: %s -> %s (rid=%s)",
              it->command.c_str(), it->target.c_str(), it->requestId.c_str());

      if (it->callback) {
        // Create empty result for timeout callback
        JsonDocument emptyDoc;
        JsonObject emptyResult = emptyDoc.to<JsonObject>();
        it->callback(false, it->target, emptyResult);
      }

      it = pendingCommands.erase(it);
    } else {
      ++it;
    }
  }
}

// ============== REGISTER CUSTOM COMMAND HANDLER ==============
void MeshSwarm::onCommand(const String& command, CommandHandler handler) {
  commandHandlers[command] = handler;
  CMD_LOG_D("Registered command handler: %s", command.c_str());
}

// ============== BUILT-IN COMMANDS ==============
void MeshSwarm::registerBuiltinCommands() {
  // status - Get node status
  onCommand("status", [this](const String& from, JsonObject& args) {
    JsonDocument doc;
    doc["id"] = myId;
    doc["name"] = myName;
    doc["role"] = myRole;
    doc["heap"] = ESP.getFreeHeap();
    doc["uptime"] = (millis() - bootTime) / 1000;
    doc["peers"] = getPeerCount();
    doc["states"] = sharedState.size();
    doc["version"] = FIRMWARE_VERSION;
    return doc;
  });

  // peers - List known peers
  onCommand("peers", [this](const String& from, JsonObject& args) {
    JsonDocument doc;
    JsonArray arr = doc["peers"].to<JsonArray>();
    for (auto& p : peers) {
      JsonObject peer = arr.add<JsonObject>();
      peer["id"] = p.second.id;
      peer["name"] = p.second.name;
      peer["role"] = p.second.role;
      peer["alive"] = p.second.alive;
    }
    return doc;
  });

  // state - Get all shared state
  onCommand("state", [this](const String& from, JsonObject& args) {
    JsonDocument doc;
    JsonObject state = doc["state"].to<JsonObject>();
    for (auto& kv : sharedState) {
      JsonObject entry = state[kv.first].to<JsonObject>();
      entry["value"] = kv.second.value;
      entry["version"] = kv.second.version;
      entry["origin"] = nodeIdToName(kv.second.origin);
    }
    return doc;
  });

  // get - Get specific state key
  onCommand("get", [this](const String& from, JsonObject& args) {
    JsonDocument doc;
    String key = args["key"] | "";
    if (key.length() > 0 && sharedState.count(key)) {
      StateEntry& entry = sharedState[key];
      doc["key"] = key;
      doc["value"] = entry.value;
      doc["version"] = entry.version;
      doc["origin"] = nodeIdToName(entry.origin);
      doc["found"] = true;
    } else {
      doc["key"] = key;
      doc["found"] = false;
    }
    return doc;
  });

  // set - Set state key
  onCommand("set", [this](const String& from, JsonObject& args) {
    JsonDocument doc;
    String key = args["key"] | "";
    String value = args["value"] | "";
    if (key.length() > 0) {
      bool changed = setState(key, value);
      doc["key"] = key;
      doc["value"] = value;
      doc["changed"] = changed;
    } else {
      doc["error"] = "Missing key";
    }
    return doc;
  });

  // sync - Force broadcast full state
  onCommand("sync", [this](const String& from, JsonObject& args) {
    broadcastFullState();
    JsonDocument doc;
    doc["ack"] = true;
    return doc;
  });

  // ping - Simple connectivity test
  onCommand("ping", [this](const String& from, JsonObject& args) {
    JsonDocument doc;
    doc["pong"] = true;
    doc["time"] = millis();
    return doc;
  });

  // info - Get node capabilities
  onCommand("info", [this](const String& from, JsonObject& args) {
    JsonDocument doc;
    doc["version"] = FIRMWARE_VERSION;
    doc["hardware"] = "ESP32";

    JsonObject features = doc["features"].to<JsonObject>();
#if MESHSWARM_ENABLE_DISPLAY
    features["display"] = true;
#else
    features["display"] = false;
#endif
#if MESHSWARM_ENABLE_SERIAL
    features["serial"] = true;
#else
    features["serial"] = false;
#endif
#if MESHSWARM_ENABLE_TELEMETRY
    features["telemetry"] = true;
#else
    features["telemetry"] = false;
#endif
#if MESHSWARM_ENABLE_OTA
    features["ota"] = true;
#else
    features["ota"] = false;
#endif
#if MESHSWARM_ENABLE_CALLBACKS
    features["callbacks"] = true;
#else
    features["callbacks"] = false;
#endif

    JsonObject meshInfo = doc["mesh"].to<JsonObject>();
    meshInfo["prefix"] = MESH_PREFIX;
    meshInfo["port"] = MESH_PORT;

    // List available commands
    JsonArray cmds = doc["commands"].to<JsonArray>();
    for (auto& kv : commandHandlers) {
      cmds.add(kv.first);
    }

    return doc;
  });

  // reboot - Restart the node
  onCommand("reboot", [this](const String& from, JsonObject& args) {
    JsonDocument doc;
    doc["ack"] = true;
    doc["rebooting"] = true;

    // Schedule reboot after response is sent
    // Using a simple delay approach - in production you might use a flag checked in loop()
    delay(100);  // Give time for response to be sent
    ESP.restart();

    return doc;  // Won't reach here
  });
}
