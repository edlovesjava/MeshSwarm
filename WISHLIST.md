# MeshSwarm Feature Wishlist

This document tracks ideas and enhancements for future development of the MeshSwarm library.

---

## 🔥 Remote Command Protocol (Priority)

**Status**: Proposed | [Full Specification](prd/remote-command-protocol.md)

Enable nodes to send commands to other nodes through the mesh, and allow external apps to control nodes via gateway HTTP API.

**Key Features:**
- Node-to-node command messaging with request/response correlation
- Gateway HTTP API (`/api/command`) for external app control
- Built-in commands: `status`, `peers`, `state`, `get`, `set`, `sync`, `reboot`, `info`, `ping`
- Custom command handlers via `onCommand()` callback
- Broadcast support for mesh-wide commands

**Enables:**
- Dashboard/web app control of any node
- Display nodes sending commands to controller nodes
- Sensor-triggered automation
- Remote debugging and diagnostics

---

## 🔥 Persistent Node Configuration (Priority)

**Status**: Proposed | [Full Specification](prd/persistent-node-config.md)

Store node-specific configuration in ESP32 NVS to survive reboots and OTA updates.

**Problem:** OTA firmware is role-based, but nodes have unique config (names, rooms, calibration) that's lost on update.

**Key Features:**
- Key-value config store using ESP32 Preferences (NVS)
- Simple API: `setConfig()`, `getConfig()`, `hasConfig()`, `removeConfig()`
- Typed helpers: `getConfigInt()`, `getConfigFloat()`, `getConfigBool()`
- Export/import as JSON for backup and bulk provisioning
- Serial commands: `config get/set/remove/clear/export/import`
- Remote command integration for configuring nodes over the mesh

**Enables:**
- Assign friendly names without reflashing
- Configure room/zone assignments remotely
- Store sensor calibration that survives OTA
- Bulk provision new nodes via gateway

---

## Network Abstraction Layer

**Status**: Idea

Abstract the networking layer to decouple MeshSwarm from painlessMesh and support multiple transport protocols.

**Current State:**
- Tightly coupled to painlessMesh WiFi mesh
- Direct calls to `mesh.sendBroadcast()`, `mesh.getNodeId()`, etc. throughout codebase

**Proposed Transports:**

| Transport | Range | Power | Speed | Use Case |
|-----------|-------|-------|-------|----------|
| **painlessMesh** (WiFi) | ~100m | High | Fast | Indoor, high bandwidth |
| **ESP-NOW** | ~200m | Low | Fast | Low latency, peer-to-peer |
| **BLE Mesh** | ~30m | Very Low | Slow | Battery devices, wearables |
| **LoRa** | ~10km | Low | Very Slow | Long range, outdoor |
| **Thread/Matter** | ~100m | Low | Medium | Smart home interop |

**Interface Concept:**
```cpp
class MeshTransport {
public:
  virtual void begin() = 0;
  virtual void update() = 0;
  virtual uint32_t getNodeId() = 0;
  virtual void sendBroadcast(const String& msg) = 0;
  virtual void sendTo(uint32_t target, const String& msg) = 0;
  virtual void onReceive(ReceiveCallback cb) = 0;
  virtual void onConnectionChange(ConnectionCallback cb) = 0;
  virtual std::list<uint32_t> getNodeList() = 0;
};
```

**Benefits:**
- Swap transports without changing application code
- Mix transports (WiFi mesh + LoRa bridge)
- Easier testing with mock transport
- Support new protocols as they emerge

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
