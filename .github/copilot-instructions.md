# GitHub Copilot Instructions for MeshSwarm

## Repository Overview

MeshSwarm is an Arduino library for ESP32 that provides self-organizing mesh networks with distributed shared state synchronization. The library is built on top of painlessMesh and provides higher-level abstractions for common IoT mesh networking patterns.

**Key Features:**
- Self-organizing mesh with automatic peer discovery
- Coordinator election (lowest node ID wins)
- Distributed key-value state store with conflict resolution
- State watchers with callback support
- OLED display support (SSD1306)
- Serial command interface for debugging
- OTA firmware updates via mesh
- Telemetry support with gateway mode
- WiFi bridge capabilities

## Technology Stack

- **Language**: C++ (Arduino framework)
- **Target Platform**: ESP32 (dual-core recommended)
- **Build System**: Arduino IDE and PlatformIO
- **Dependencies**:
  - painlessMesh (mesh networking)
  - ArduinoJson (JSON serialization)
  - Adafruit SSD1306 (OLED display)
  - Adafruit GFX Library (graphics primitives)
  - HTTPClient (for gateway WiFi features)

## Project Structure

```
MeshSwarm/
├── src/
│   ├── MeshSwarm.h       # Main header with class definition and configuration
│   └── MeshSwarm.cpp     # Implementation of all library functionality
├── examples/             # Example Arduino sketches
│   ├── BasicNode/        # Minimal mesh node example
│   ├── ButtonNode/       # Button input publishing to mesh
│   ├── LedNode/          # LED controlled by mesh state
│   ├── SensorNode/       # DHT11 temperature/humidity sensor
│   ├── WatcherNode/      # Observer that logs state changes
│   └── GatewayNode/      # WiFi bridge with telemetry and OTA
├── library.properties    # Arduino library metadata
├── keywords.txt          # Arduino IDE syntax highlighting
├── README.md             # Documentation
├── CHANGELOG.md          # Version history
└── LICENSE               # MIT license
```

## Code Organization

### Main Library Files

**MeshSwarm.h** contains:
- Configuration macros (overridable defaults)
- Message type enums
- Data structures (StateEntry, Peer, OTAUpdateInfo)
- Callback type definitions
- MeshSwarm class declaration

**MeshSwarm.cpp** contains:
- Constructor and initialization logic
- Core mesh functionality
- State management and synchronization
- Message handling and routing
- Display management
- Serial command interface
- OTA update logic
- Telemetry functionality

### Examples Structure

All examples follow a consistent pattern:
1. Include MeshSwarm.h
2. Create a global MeshSwarm instance
3. Initialize in setup() with swarm.begin()
4. Call swarm.update() in loop()
5. Use callbacks (onLoop, onDisplayUpdate, onSerialCommand) for custom logic

## Coding Conventions

### Naming Conventions

- **Classes**: PascalCase (e.g., `MeshSwarm`, `StateEntry`)
- **Functions/Methods**: camelCase (e.g., `setState`, `getNodeId`)
- **Constants/Macros**: UPPER_SNAKE_CASE (e.g., `MESH_PREFIX`, `HEARTBEAT_INTERVAL`)
- **Private members**: camelCase without prefix (e.g., `myId`, `coordinatorId`)
- **Local variables**: camelCase (e.g., `lastRead`, `startDelay`)

### Code Style

- **Indentation**: 2 spaces (no tabs)
- **Braces**: Opening brace on same line for functions and control structures
- **Comments**: Use C-style block comments (`/* */`) for file/section headers, C++ style (`//`) for inline
- **Section headers**: Use `// ============== SECTION NAME ==============` format
- **Spacing**: Space after keywords (if, for, while), no space between function name and parenthesis

### Documentation Standards

- **File headers**: Include brief description and feature list
- **Function documentation**: Use concise comments before complex functions
- **Example headers**: Include:
  - Description of what it demonstrates
  - Required hardware
  - Additional library dependencies (if any)

### Memory Management

- **String handling**: Use Arduino String class for simplicity
- **Dynamic allocation**: Minimal use; prefer stack allocation
- **Lambda captures**: Use `[this]` to capture class instance in callbacks
- **Buffer management**: For OTA, use malloc/free with explicit size tracking

## Configuration Patterns

### Overridable Defaults

All configuration macros can be overridden by defining them before including MeshSwarm.h:

```cpp
#define MESH_PREFIX "mynetwork"
#define MESH_PASSWORD "mypassword"
#define I2C_SDA 21
#define I2C_SCL 22
#include <MeshSwarm.h>
```

Common configuration macros:
- Network: `MESH_PREFIX`, `MESH_PASSWORD`, `MESH_PORT`
- Display: `SCREEN_WIDTH`, `SCREEN_HEIGHT`, `OLED_ADDR`
- I2C: `I2C_SDA`, `I2C_SCL`
- Timing: `HEARTBEAT_INTERVAL`, `STATE_SYNC_INTERVAL`, `DISPLAY_INTERVAL`

## State Management Patterns

### Conflict Resolution

When multiple nodes update the same key:
1. Higher version number wins
2. If versions equal, lower origin node ID wins
3. This ensures deterministic convergence without central authority

### State Synchronization

- States are synchronized via broadcast messages
- Heartbeats include state change notifications
- Full state sync can be requested on demand
- Version numbers prevent stale updates

## Message Protocol

Message types (JSON format):
- `MSG_HEARTBEAT (1)`: Periodic peer discovery and state notifications
- `MSG_STATE_SET (2)`: Single key-value update
- `MSG_STATE_SYNC (3)`: Full state dump
- `MSG_STATE_REQ (4)`: Request state from peers
- `MSG_COMMAND (5)`: Control commands
- `MSG_TELEMETRY (6)`: Telemetry data to gateway

## Callback Patterns

### State Watchers

```cpp
swarm.watchState("key", [](const String& key, const String& value, const String& oldValue) {
  // React to state changes
});

// Wildcard watcher (all changes)
swarm.watchState("*", [](const String& key, const String& value, const String& oldValue) {
  // Called for any state change
});
```

### Custom Loop Logic

```cpp
swarm.onLoop([]() {
  // Called every update cycle
  // Use for sensor polling, periodic tasks
});
```

### Serial Command Extensions

```cpp
swarm.onSerialCommand([](const String& input) -> bool {
  if (input == "mycommand") {
    // Handle command
    return true;  // Command handled
  }
  return false;  // Not our command
});
```

### Display Customization

```cpp
swarm.onDisplayUpdate([](Adafruit_SSD1306& display, int startLine) {
  display.println("---------------------");
  display.println("Custom content here");
});
```

## Testing Approach

This library is designed for embedded systems and does not include automated unit tests. Testing is done through:

1. **Example sketches**: Each example serves as an integration test
2. **Serial output**: Verbose logging for debugging
3. **OLED display**: Visual feedback of node state
4. **Multi-node testing**: Deploy to multiple ESP32 devices to test mesh behavior

When adding new features:
- Create or update relevant example sketches
- Test with multiple physical nodes if mesh-related
- Verify serial output matches expected behavior
- Check display updates if UI-related

## Common Development Tasks

### Adding a New Feature

1. Add function declaration to MeshSwarm.h
2. Implement in MeshSwarm.cpp following existing patterns
3. Update relevant example sketches
4. Document in README.md
5. Add entry to CHANGELOG.md

### Adding a New Example

1. Create folder in examples/ with .ino file
2. Follow existing example structure and header format
3. Document hardware requirements and what it demonstrates
4. Keep it simple and focused on one concept

### Modifying Message Protocol

1. Update MsgType enum if adding new type
2. Add handler in onReceive callback
3. Add sender function following existing patterns
4. Ensure JSON structure is documented in comments

## Hardware Considerations

### ESP32 Specifics

- Use dual-core ESP32 for best performance
- painlessMesh uses one core for networking
- Main application runs on other core
- Watchdog timers require frequent updates (hence swarm.update() in loop)

### I2C and Display

- Default pins: SDA=21, SCL=22
- SSD1306 address: 0x3C (default)
- Display updates throttled to avoid blocking mesh
- Wire library initialized by MeshSwarm

### Memory Constraints

- ESP32 has limited heap (~300KB)
- Large OTA updates chunked into 1KB parts
- State store uses std::map (keep entries reasonable)
- ArduinoJson documents sized appropriately

## Dependencies and Version Compatibility

- **painlessMesh**: Core mesh networking, handles WiFi and routing
- **ArduinoJson**: v6 or later for JSON serialization
- **Adafruit libraries**: Latest versions via Arduino Library Manager
- **ESP32 core**: Arduino-ESP32 v2.x or later

## Error Handling

- Serial output for debugging (always use Serial.print for important messages)
- Display shows node status and errors
- Mesh failures handled by painlessMesh auto-recovery
- State conflicts resolved deterministically

## Best Practices for Contributors

1. **Maintain simplicity**: This is a library for makers, keep it accessible
2. **Minimal dependencies**: Only add if absolutely necessary
3. **Backward compatibility**: Don't break existing examples
4. **Document examples**: Good examples are the best documentation
5. **Test with real hardware**: Simulators don't catch mesh behavior
6. **Comment sparingly**: Code should be self-documenting, comments for "why" not "what"
7. **Follow Arduino conventions**: This library targets Arduino users
8. **Keep state management simple**: Avoid complex data types in shared state

## Common Pitfalls to Avoid

- **Don't block in loop()**: swarm.update() must be called frequently
- **Don't use delay()**: Use millis() timers instead
- **Don't store large data in state**: Keep values small (strings, numbers)
- **Don't forget swarm.update()**: Required in every loop() for mesh to function
- **Don't assume node roles are stable**: Coordinators can change on network events
- **Don't use blocking Serial reads**: Use serial.available() pattern

## Version Numbering

Follow Semantic Versioning (semver):
- **MAJOR**: Breaking API changes
- **MINOR**: New features, backward compatible
- **PATCH**: Bug fixes, backward compatible

Update version in:
- library.properties
- CHANGELOG.md
- README.md (if showing version-specific features)
