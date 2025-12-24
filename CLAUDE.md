# CLAUDE.md

This file provides context for Claude Code when working on the MeshSwarm project.

## Project Overview

MeshSwarm is a self-organizing ESP32 mesh network library built on painlessMesh with distributed shared state synchronization. It enables swarms of ESP32 devices to automatically discover, connect, and share state without a central authority.

**Core Principles:**
- Privacy and autonomy first
- Collective intelligence in action
- Small and cheap but powerful in the aggregate
- Play as a skill: always have fun

## Architecture

### Directory Structure

```
MeshSwarm/
├── src/                    # Library source code
│   ├── MeshSwarm.h         # Main header with class definition
│   ├── MeshSwarm.cpp       # Core implementation
│   ├── MeshSwarmConfig.h   # Feature flags and configuration
│   └── features/           # Optional modular features (.inc files)
│       ├── MeshSwarmDisplay.inc
│       ├── MeshSwarmSerial.inc
│       ├── MeshSwarmTelemetry.inc
│       ├── MeshSwarmOTA.inc
│       ├── MeshSwarmCallbacks.inc
│       └── MeshSwarmHTTP.inc
├── examples/               # Example node implementations
├── docs/                   # Documentation
├── prd/                    # Product requirement documents for planned features
└── WISHLIST.md             # Feature ideas and roadmap
```

### Modular Feature System

Features are conditionally compiled using flags in `MeshSwarmConfig.h`:
- `MESHSWARM_ENABLE_DISPLAY` - OLED SSD1306 support
- `MESHSWARM_ENABLE_SERIAL` - Serial command interface
- `MESHSWARM_ENABLE_TELEMETRY` - HTTP telemetry to server
- `MESHSWARM_ENABLE_OTA` - Over-the-air firmware updates
- `MESHSWARM_ENABLE_CALLBACKS` - Custom callback hooks

Feature implementations use `.inc` extension to prevent Arduino from compiling them separately.

### Key Concepts

- **Coordinator Election**: Lowest node ID automatically becomes coordinator
- **Distributed State**: Key-value store with version numbers + origin ID for conflict resolution
- **State Watchers**: Callbacks triggered on state changes
- **Gateway Mode**: Node with WiFi station mode for telemetry and OTA distribution
- **Peers**: Other nodes in the mesh, tracked with heartbeats

### Message Types (in MeshSwarm.h)

```cpp
enum MsgType {
  MSG_HEARTBEAT  = 1,
  MSG_STATE_SET  = 2,
  MSG_STATE_SYNC = 3,
  MSG_STATE_REQ  = 4,
  MSG_COMMAND    = 5,  // NOT YET IMPLEMENTED
  MSG_TELEMETRY  = 6
};
```

## Development Guidelines

### Code Style
- Use Arduino/ESP32 conventions
- Feature code goes in `src/features/` as `.inc` files
- Wrap feature code in `#if MESHSWARM_ENABLE_*` guards
- Use the logging macros: `MESH_LOG()`, `STATE_LOG()`, `TELEM_LOG()`

### Adding New Features
1. Add feature flag to `MeshSwarmConfig.h`
2. Create implementation in `src/features/MeshSwarm<Feature>.inc`
3. Include the `.inc` file at the bottom of `MeshSwarm.cpp`
4. Add any new methods to `MeshSwarm.h` with proper guards
5. Create an example in `examples/`

### Testing
- Test with multiple ESP32 devices on the mesh
- Verify feature can be disabled without breaking core functionality
- Check flash size impact with feature enabled/disabled

## Current Priorities

See `WISHLIST.md` and `prd/` for planned features. Priority items:

1. **Remote Command Protocol** (`prd/remote-command-protocol.md`)
   - Implement MSG_COMMAND handling in `onReceive()`
   - Add `sendCommand()` API
   - Gateway HTTP API for external control

2. **Persistent Node Configuration** (`prd/persistent-node-config.md`)
   - ESP32 Preferences/NVS storage
   - `setConfig()`/`getConfig()` API
   - Survives OTA updates

## Common Tasks

### Building Examples
```bash
# Using PlatformIO
cd examples/BasicNode
pio run

# Using Arduino IDE
# Open .ino file and compile
```

### Serial Commands (when MESHSWARM_ENABLE_SERIAL=1)
```
status    - Node status
peers     - List peers
state     - Show shared state
set <k> <v> - Set state
get <k>   - Get state
sync      - Broadcast state
scan      - I2C scan (if display enabled)
reboot    - Restart node
```

## Related Repositories

- **[iotmesh](https://github.com/edlovesjava/iotmesh)** - Current mesh implementation using this library. Contains working node firmware, gateway server, and deployment examples.

This library is part of a larger mesh ecosystem. The iotmesh repository demonstrates real-world usage including:
- Node firmware configurations
- Gateway/server implementation
- OTA update infrastructure
- Telemetry collection
