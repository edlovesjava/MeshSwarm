/**
 * BasicNode - Minimal MeshSwarm example
 *
 * Demonstrates:
 * - Mesh network auto-connection
 * - Coordinator election
 * - OLED display
 * - Serial commands
 *
 * This example uses the DEFAULT configuration with ALL features enabled.
 * To reduce flash memory usage, you can disable features before including MeshSwarm.h:
 *
 *   #define MESHSWARM_ENABLE_DISPLAY 0    // Disable OLED display support
 *   #define MESHSWARM_ENABLE_SERIAL 0     // Disable serial commands
 *   #define MESHSWARM_ENABLE_TELEMETRY 0  // Disable HTTP telemetry
 *   #define MESHSWARM_ENABLE_OTA 0        // Disable OTA updates
 *   #include <MeshSwarm.h>
 *
 * See docs/MODULAR_BUILD.md for more information on modular builds.
 *
 * Hardware:
 * - ESP32
 * - SSD1306 OLED (I2C: SDA=21, SCL=22)
 */

#include <MeshSwarm.h>

MeshSwarm swarm;

void setup() {
  Serial.begin(115200);

  // Initialize with default network settings
  // Uses: MESH_PREFIX="meshswarm", MESH_PASSWORD="meshpassword", MESH_PORT=5555
  swarm.begin();

  // Or use custom network settings:
  // swarm.begin("mynetwork", "password123", 5555);

  Serial.println("BasicNode started");
}

void loop() {
  // Must be called frequently to maintain mesh
  swarm.update();
}
