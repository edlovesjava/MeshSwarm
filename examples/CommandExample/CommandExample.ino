/**
 * CommandExample - Remote Command Protocol (RCP) Demo
 *
 * Demonstrates how to use MeshSwarm's Remote Command Protocol:
 * - Send commands to other nodes
 * - Receive and respond to commands
 * - Register custom command handlers
 * - Use serial interface for testing
 *
 * Built-in commands available on all nodes:
 *   status  - Node info (ID, role, heap, uptime)
 *   peers   - List connected peers
 *   state   - Get all shared state
 *   get     - Get specific state key
 *   set     - Set state key/value
 *   sync    - Force state broadcast
 *   ping    - Connectivity test
 *   info    - Node capabilities
 *   reboot  - Restart node
 *
 * Serial Commands:
 *   cmd <target> <command> [key=value ...]
 *
 * Examples:
 *   cmd * ping                    - Ping all nodes (broadcast)
 *   cmd Node1 status              - Get status from Node1
 *   cmd 12345678 get key=temp     - Get temp state from node by ID
 *   cmd * set key=led value=1     - Set LED state on all nodes
 *
 * Hardware:
 *   - ESP32 (any variant)
 *   - Optional: SSD1306 OLED display
 */

#include <MeshSwarm.h>

// ESP32 built-in LED pin (GPIO2 on most boards)
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

MeshSwarm swarm;

// Custom sensor value for demonstration
float sensorValue = 0.0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n========================================");
  Serial.println("   MeshSwarm Command Example");
  Serial.println("========================================\n");

  // Initialize mesh with a name
  swarm.begin("CmdNode");

  // ============================================
  // REGISTER CUSTOM COMMAND HANDLERS
  // ============================================

  // Custom "sensor" command - returns simulated sensor reading
  swarm.onCommand("sensor", [](const String& sender, JsonObject& args) {
    JsonDocument response;
    response["value"] = sensorValue;
    response["unit"] = "celsius";
    response["timestamp"] = millis();
    Serial.printf("[CMD] Sensor request from %s\n", sender.c_str());
    return response;
  });

  // Custom "led" command - control LED with response
  swarm.onCommand("led", [](const String& sender, JsonObject& args) {
    JsonDocument response;

    if (args["state"].is<const char*>() || args["state"].is<String>()) {
      String state = args["state"].as<String>();
      bool on = (state == "1" || state == "on" || state == "true");
      digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
      response["success"] = true;
      response["led"] = on ? "on" : "off";
      Serial.printf("[CMD] LED set to %s by %s\n", on ? "ON" : "OFF", sender.c_str());
    } else {
      response["success"] = false;
      response["error"] = "Missing 'state' parameter";
    }

    return response;
  });

  // Custom "echo" command - returns whatever was sent
  swarm.onCommand("echo", [](const String& sender, JsonObject& args) {
    JsonDocument response;
    response["echo"] = true;
    response["sender"] = sender;

    // Copy all args to response
    for (JsonPair p : args) {
      response["args"][p.key()] = p.value();
    }

    return response;
  });

  // ============================================
  // PERIODIC SENSOR SIMULATION
  // ============================================

  swarm.onLoop([]() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 5000) {
      lastUpdate = millis();
      // Simulate changing sensor value
      sensorValue = 20.0 + random(-50, 50) / 10.0;
    }
  });

  // ============================================
  // CUSTOM SERIAL COMMANDS FOR DEMO
  // ============================================

  swarm.onSerialCommand([](const String& input) -> bool {
    // "demo" command - demonstrate sending commands
    if (input == "demo") {
      Serial.println("\n--- Command Demo ---");
      Serial.println("Sending ping to all nodes...\n");

      // Create empty args for the ping command
      static JsonDocument pingDoc;
      JsonObject pingArgs = pingDoc.to<JsonObject>();

      // Send broadcast ping with callback (3 params: success, node, response)
      swarm.sendCommand("*", "ping", pingArgs, [](bool success, const String& node, JsonObject& response) {
        if (success) {
          Serial.printf("  Pong from: %s\n", node.c_str());
        } else {
          Serial.println("  (timeout or no response)");
        }
      }, 3000);

      return true;
    }

    // "test" command - send commands to self for testing
    if (input == "test") {
      Serial.println("\n--- Testing Custom Commands ---");
      Serial.println("Sending commands to self...\n");

      // Get our own node name
      String myName = swarm.getNodeName();

      // Create args document
      static JsonDocument doc;
      JsonObject args = doc.to<JsonObject>();

      // Test sensor command
      Serial.println("1. Sending 'sensor' command to self:");
      args.clear();
      swarm.sendCommand(myName, "sensor", args, [](bool success, const String& node, JsonObject& response) {
        if (success) {
          serializeJsonPretty(response, Serial);
          Serial.println();
        } else {
          Serial.println("  Failed!");
        }
      }, 2000);

      return true;
    }

    return false;  // Not our command
  });

  // Setup complete
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println("\nReady! Try these commands:");
  Serial.println("  status       - Show node status");
  Serial.println("  peers        - List mesh peers");
  Serial.println("  demo         - Demo broadcast ping");
  Serial.println("  test         - Test custom commands locally");
  Serial.println("  cmd * ping   - Ping all nodes");
  Serial.println("  cmd <name> sensor - Get sensor from node");
  Serial.println();
}

void loop() {
  swarm.update();
}
