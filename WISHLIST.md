# MeshSwarm Feature Wishlist

This document tracks ideas and enhancements for future development of the MeshSwarm library.

---

## 🔥 Remote Command Protocol (Priority)

**Status**: MSG_COMMAND exists in the codebase but is NOT IMPLEMENTED

Currently, serial console commands (status, peers, state, set/get, etc.) only work locally on the node you're physically connected to. There's no way to:
- Send commands to a specific node through the mesh
- Query a remote node's status
- Control nodes from external apps via the gateway

### Proposed Architecture

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  External   │────▶│   Gateway   │────▶│  Target     │
│  App/API    │◀────│   Node      │◀────│  Node       │
└─────────────┘     └─────────────┘     └─────────────┘
                          │
                          ▼
                    ┌─────────────┐
                    │  Other      │
                    │  Nodes      │
                    └─────────────┘
```

### Command Message Format

```json
{
  "t": 5,                    // MSG_COMMAND
  "n": "sender_name",
  "d": {
    "cmd": "status",         // Command name
    "target": "N1234",       // Target node (or "*" for broadcast)
    "rid": "abc123",         // Request ID for response correlation
    "args": {}               // Command-specific arguments
  }
}
```

### Response Message Format

```json
{
  "t": 7,                    // MSG_COMMAND_RESPONSE (new)
  "n": "responder_name",
  "d": {
    "rid": "abc123",         // Matching request ID
    "success": true,
    "result": {}             // Command-specific result data
  }
}
```

### Core Commands to Implement

| Command | Description | Args | Response |
|---------|-------------|------|----------|
| `status` | Get node status | - | id, name, role, heap, uptime |
| `peers` | List known peers | - | Array of peer info |
| `state` | Get all shared state | - | Key-value map |
| `get` | Get specific state key | `key` | Value or null |
| `set` | Set state key | `key`, `value` | Success boolean |
| `sync` | Force state broadcast | - | Ack |
| `reboot` | Restart node | - | Ack (then reboot) |
| `info` | Get node capabilities | - | Features, version, hardware |

### Gateway API Endpoints

The gateway should expose HTTP/REST endpoints for external apps:

```
POST /api/command
{
  "target": "N1234",    // or "*" for broadcast
  "command": "status",
  "args": {}
}

Response:
{
  "request_id": "abc123",
  "responses": [
    { "node": "N1234", "success": true, "result": {...} }
  ]
}
```

### Implementation Tasks

- [ ] Define MSG_COMMAND_RESPONSE message type
- [ ] Implement command handler in onReceive()
- [ ] Add sendCommand(target, cmd, args) method
- [ ] Add command response callback system
- [ ] Implement request/response correlation (request IDs)
- [ ] Add timeout handling for responses
- [ ] Gateway: Add HTTP API endpoints for external control
- [ ] Gateway: Route commands from HTTP to mesh
- [ ] Gateway: Collect and return responses to HTTP caller
- [ ] Add command ACL/permissions (optional)

### Use Cases Enabled

1. **Dashboard Control**: Web app sends commands through gateway to any node
2. **Node-to-Node**: Button node tells LED node to toggle
3. **Automation**: Sensor node triggers controller node
4. **Remote Debugging**: Query any node's status from anywhere
5. **Display Controllers**: Touch screen sends commands to target nodes

---

## Displays

Displays are primarily for presenting status or information, but can also serve as controllers to direct actions.

### Display Types

| Type | Description | Use Cases |
|------|-------------|-----------|
| **StatusDisplay** | Shows mesh health, node count, coordinator info | Dashboard, monitoring stations |
| **InfoDisplay** | Presents sensor data, state values, alerts | Environmental readouts, system status |
| **MenuDisplay** | Interactive menus for configuration/control | Settings, device selection, mode switching |
| **TouchDisplay** | Touch-enabled displays for direct interaction | Control panels, user interfaces |

### Display Features to Consider

- [ ] Support for various display types (OLED, TFT, e-Paper, LCD)
- [ ] Configurable layouts and themes
- [ ] Multi-page/screen navigation
- [ ] Touch input support for interactive displays
- [ ] Remote display updates (push content from other nodes)
- [ ] Display templates for common use cases
- [ ] Brightness/power management for battery operation

### Display as Controller

Displays can also act as controllers when they have input capability:

- Touch screens can send commands to other nodes
- Button-equipped displays can trigger mesh actions
- Rotary encoder displays for value adjustment
- Gesture recognition for advanced control

---

## Controllers

Controllers manage and control other devices through various protocols and interfaces.

### Controller Types

| Type | Description | Controlled Devices |
|------|-------------|-------------------|
| **IRController** | Infrared remote control | TVs, ACs, fans, audio equipment |
| **RFController** | Radio frequency control (433MHz, 315MHz) | Outlets, garage doors, weather stations |
| **ServoController** | PWM servo control | Robotics, pan/tilt, actuators |
| **RelayController** | Digital relay switching | Lights, appliances, motors |
| **MotorController** | DC/stepper motor control | Robotics, automation, blinds |
| **DMXController** | DMX512 lighting control | Stage lighting, LED strips |
| **BLEController** | Bluetooth Low Energy control | Smart devices, beacons |

### Controller Features to Consider

- [ ] IR code learning and transmission (NEC, RC5, Sony, etc.)
- [ ] RF protocol support (433MHz, 315MHz)
- [ ] Servo angle control with smooth transitions
- [ ] Relay scheduling and timers
- [ ] Motor speed and direction control
- [ ] Command sequences/macros
- [ ] State feedback from controlled devices
- [ ] Universal remote functionality

### Controller Integration

Controllers should integrate with the mesh to enable:

- Any node can send commands to any controller
- Centralized control from gateway/dashboard
- Automation rules (sensor triggers controller)
- Scene/macro support (one command triggers multiple actions)
- State synchronization (know what devices are on/off)

---

## Combined Display-Controller Nodes

Some nodes may serve dual purpose as both display and controller:

| Node Type | Description |
|-----------|-------------|
| **TouchPanel** | Touch display that controls other mesh devices |
| **RemoteStation** | Display + buttons/encoder for remote control |
| **Dashboard** | Central control hub with display and multiple outputs |
| **Thermostat** | Temperature display + HVAC controller |

---

## Future Ideas

### Protocols & Interfaces
- [ ] I2C device support (sensors, displays, expanders)
- [ ] SPI device support
- [ ] UART device bridging
- [ ] PWM output management
- [ ] Analog input/output expansion

### Automation & Logic
- [ ] Rule engine (if sensor X then controller Y)
- [ ] Scheduling system (time-based triggers)
- [ ] Scene management (grouped actions)
- [ ] Presence detection integration
- [ ] Voice assistant integration (Alexa, Google Home)

### Communication
- [ ] MQTT bridge node
- [ ] Home Assistant integration
- [ ] WebSocket real-time updates
- [ ] REST API for external control
- [ ] Bluetooth mesh bridging

### Power & Battery
- [ ] Deep sleep coordination
- [ ] Battery level monitoring
- [ ] Solar charging support
- [ ] Power consumption optimization

### Security
- [ ] Encrypted mesh communication
- [ ] Authentication for sensitive commands
- [ ] Access control levels

---

## Contributing Ideas

Have an idea for MeshSwarm? Add it to this wishlist! Consider:

1. **What problem does it solve?**
2. **What hardware would it require?**
3. **How would it integrate with the mesh?**
4. **What state would it share/consume?**

---

*Last updated: December 2024*
