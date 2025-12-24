/*
 * MeshSwarm Library - Serial Module
 * 
 * Serial command interface for debugging and control.
 * Only compiled when MESHSWARM_ENABLE_SERIAL is enabled.
 */

#include "../MeshSwarm.h"

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
  else if (input.startsWith("cmd ")) {
    // Remote command: cmd <target> <command> [key=value ...]
    // Examples:
    //   cmd N1234 status
    //   cmd N5678 set key=led value=on
    //   cmd * ping
    String rest = input.substring(4);
    rest.trim();

    int firstSpace = rest.indexOf(' ');
    if (firstSpace < 0) {
      Serial.println("Usage: cmd <target> <command> [key=value ...]");
      Serial.println("Examples:");
      Serial.println("  cmd N1234 status");
      Serial.println("  cmd N5678 set key=led value=on");
      Serial.println("  cmd * ping");
    } else {
      String target = rest.substring(0, firstSpace);
      String cmdRest = rest.substring(firstSpace + 1);
      cmdRest.trim();

      int secondSpace = cmdRest.indexOf(' ');
      String command;
      String argsStr;

      if (secondSpace < 0) {
        command = cmdRest;
        argsStr = "";
      } else {
        command = cmdRest.substring(0, secondSpace);
        argsStr = cmdRest.substring(secondSpace + 1);
      }

      // Parse key=value pairs into args
      JsonDocument argsDoc;
      JsonObject args = argsDoc.to<JsonObject>();

      if (argsStr.length() > 0) {
        // Split by spaces and parse key=value
        int pos = 0;
        while (pos < (int)argsStr.length()) {
          int nextSpace = argsStr.indexOf(' ', pos);
          String pair;
          if (nextSpace < 0) {
            pair = argsStr.substring(pos);
            pos = argsStr.length();
          } else {
            pair = argsStr.substring(pos, nextSpace);
            pos = nextSpace + 1;
          }

          int eqPos = pair.indexOf('=');
          if (eqPos > 0) {
            String key = pair.substring(0, eqPos);
            String value = pair.substring(eqPos + 1);
            args[key] = value;
          }
        }
      }

      Serial.printf("[CMD] Sending '%s' to %s\n", command.c_str(), target.c_str());

      // Send command with callback to print response
      sendCommand(target, command, args, [](bool success, const String& node, JsonObject& result) {
        if (success) {
          Serial.printf("\n[CMD] Response from %s:\n", node.c_str());
          serializeJsonPretty(result, Serial);
          Serial.println("\n");
        } else {
          Serial.printf("\n[CMD] Failed or timeout for %s\n", node.c_str());
        }
      }, 5000);
    }
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
    Serial.println("Commands: status, peers, state, set <k> <v>, get <k>, sync, scan");
    Serial.println("          cmd <target> <command> [args...]");
#if MESHSWARM_ENABLE_TELEMETRY
    Serial.println("          telem, push");
#endif
    Serial.println("          reboot");
  }
}

#endif // MESHSWARM_ENABLE_SERIAL
