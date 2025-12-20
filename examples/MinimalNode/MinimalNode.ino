/**
 * MinimalNode - Core-only MeshSwarm example
 *
 * This example demonstrates the absolute minimum MeshSwarm configuration
 * with ALL optional features disabled for maximum flash memory savings.
 */

// ESP32 doesn't define LED_BUILTIN, use GPIO2
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

/*
 *
 * Flash usage: ~15-20KB (vs ~58-84KB for full build)
 * Savings: ~40-65KB
 *
 * Features enabled:
 * - Core mesh networking
 * - State management and synchronization
 * - Peer discovery and tracking
 * - Coordinator election
 * 
 * Features disabled (to save flash):
 * - OLED display support
 * - Serial command interface
 * - HTTP telemetry
 * - OTA firmware updates
 * - Custom callbacks
 *
 * Hardware:
 * - ESP32 only (no display, no serial debugging)
 *
 * Use case:
 * - Headless sensor nodes
 * - Ultra-low-memory deployments
 * - Production nodes without debugging
 * - Maximum number of nodes on constrained devices
 */

// Disable all optional features BEFORE including MeshSwarm.h
#define MESHSWARM_ENABLE_DISPLAY 0
#define MESHSWARM_ENABLE_SERIAL 0
#define MESHSWARM_ENABLE_TELEMETRY 0
#define MESHSWARM_ENABLE_OTA 0

#include <MeshSwarm.h>

MeshSwarm swarm;

// Simulated sensor value
int sensorValue = 0;

void setup() {
  // Note: Serial.begin() is not called since SERIAL is disabled
  // No serial output will be generated
  
  // Initialize with default network settings
  swarm.begin("MinimalNode");
  
  // Set initial sensor state
  swarm.setState("sensor", String(sensorValue));
  
  // Watch for state changes from other nodes
  // Note: This uses the core state watcher, not custom callbacks
  swarm.watchState("command", [](const String& key, const String& value, const String& oldValue) {
    // React to commands from other nodes
    // For example, toggle an LED based on value
    if (value == "on") {
      digitalWrite(LED_BUILTIN, HIGH);
    } else if (value == "off") {
      digitalWrite(LED_BUILTIN, LOW);
    }
  });
  
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  // Must be called frequently to maintain mesh
  swarm.update();
  
  // Simulate reading a sensor every 10 seconds
  static unsigned long lastRead = 0;
  unsigned long now = millis();
  
  if (now - lastRead >= 10000) {
    lastRead = now;
    
    // Simulate sensor reading
    sensorValue = random(0, 100);
    
    // Update shared state (broadcasts to all nodes)
    swarm.setState("sensor", String(sensorValue));
    
    // Toggle LED to indicate activity
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}
