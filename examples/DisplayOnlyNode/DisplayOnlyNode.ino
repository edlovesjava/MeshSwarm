/**
 * DisplayOnlyNode - Core + Display MeshSwarm example
 *
 * This example demonstrates MeshSwarm with ONLY the display feature enabled.
 * Serial debugging and network features (telemetry, OTA) are disabled.
 *
 * Flash usage: ~30-40KB (vs ~58-84KB for full build)
 * Savings: ~30-45KB
 *
 * Features enabled:
 * - Core mesh networking
 * - State management and synchronization
 * - Peer discovery and tracking
 * - Coordinator election
 * - OLED display support
 * - Custom display callbacks
 * 
 * Features disabled (to save flash):
 * - Serial command interface
 * - HTTP telemetry
 * - OTA firmware updates
 *
 * Hardware:
 * - ESP32
 * - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 *
 * Use case:
 * - Production nodes with OLED display
 * - Battery-powered nodes with visual feedback
 * - Standalone sensor/actuator nodes
 */

// Enable only display feature
#define MESHSWARM_ENABLE_SERIAL 0
#define MESHSWARM_ENABLE_TELEMETRY 0
#define MESHSWARM_ENABLE_OTA 0
// MESHSWARM_ENABLE_DISPLAY defaults to 1 (enabled)
// MESHSWARM_ENABLE_CALLBACKS defaults to 1 (enabled - needed for display callbacks)

#include <MeshSwarm.h>

MeshSwarm swarm;

// Simulated temperature sensor
float temperature = 20.0;

void setup() {
  // Note: No Serial.begin() since SERIAL is disabled
  
  // Initialize with default network settings
  swarm.begin("DisplayNode");
  
  // Set initial temperature state
  swarm.setState("temp", String(temperature, 1));
  
  // Watch for temperature changes from other nodes
  swarm.watchState("temp", [](const String& key, const String& value, const String& oldValue) {
    // State changed, display will auto-update
  });
  
  // Add custom display handler to show temperature
  swarm.onDisplayUpdate([](Adafruit_SSD1306& display, int startLine) {
    // This is called during display refresh
    // The first 3 lines are used by default status (node name, peers, separator)
    // We can add custom content starting from line 3
    
    display.println("---------------------");
    
    // Get current temperature from shared state
    String tempStr = swarm.getState("temp", "N/A");
    display.print("Temp: ");
    display.print(tempStr);
    display.println(" C");
    
    // Show coordinator status
    if (swarm.isCoordinator()) {
      display.println("Role: COORDINATOR");
    } else {
      display.println("Role: Node");
    }
    
    // Show uptime
    unsigned long uptimeSeconds = millis() / 1000;
    display.print("Up: ");
    display.print(uptimeSeconds / 60);
    display.print("m ");
    display.print(uptimeSeconds % 60);
    display.println("s");
  });
  
  // Set a custom status line
  swarm.setStatusLine("Temp Monitor");
}

void loop() {
  // Must be called frequently to maintain mesh and update display
  swarm.update();
  
  // Simulate reading temperature sensor every 5 seconds
  static unsigned long lastRead = 0;
  unsigned long now = millis();
  
  if (now - lastRead >= 5000) {
    lastRead = now;
    
    // Simulate temperature reading with slight variation
    temperature += random(-10, 11) / 10.0;
    temperature = constrain(temperature, 15.0, 30.0);
    
    // Update shared state (broadcasts to all nodes and updates display)
    swarm.setState("temp", String(temperature, 1));
  }
}
