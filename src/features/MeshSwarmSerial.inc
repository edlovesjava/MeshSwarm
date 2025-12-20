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
