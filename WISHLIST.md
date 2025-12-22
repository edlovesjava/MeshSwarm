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

## Peripheral Bus Abstraction

**Status**: Idea

Standardized way to address and interact with sensors, actuators, and expansion devices across different bus protocols.

### Supported Bus Types

| Bus | Addressing | Speed | Wires | Common Devices |
|-----|------------|-------|-------|----------------|
| **I2C** | 7-bit address (0x00-0x7F) | 100-400kHz | 2 (SDA, SCL) | Most sensors, displays, expanders |
| **1-Wire** | 64-bit ROM code | 16kbps | 1 (+ GND) | DS18B20 temp, iButton |
| **SPI** | CS pin select | 1-80MHz | 4 (MOSI, MISO, SCK, CS) | Fast sensors, displays, SD cards |
| **Single-Wire** | GPIO pin | Varies | 1 | DHT11/22, WS2812 LEDs |
| **Analog** | ADC channel | N/A | 1 | Potentiometers, light sensors |
| **Digital GPIO** | Pin number | N/A | 1 | Buttons, relays, simple sensors |

### Device Abstraction Concept

```cpp
// Unified device interface
class MeshDevice {
public:
  virtual String getType() = 0;      // "sensor", "actuator", "display"
  virtual String getBus() = 0;       // "i2c", "1wire", "spi", "gpio"
  virtual String getAddress() = 0;   // Bus-specific address
  virtual JsonObject read() = 0;     // Read current value(s)
  virtual bool write(JsonObject) = 0; // Write value(s)
  virtual JsonObject getInfo() = 0;  // Device metadata
};

// Example: I2C temperature sensor
class I2CTempSensor : public MeshDevice {
  String getType() { return "sensor"; }
  String getBus() { return "i2c"; }
  String getAddress() { return "0x48"; }
  JsonObject read() {
    // Returns {"temperature": 23.5, "unit": "C"}
  }
};
```

### I2C Device Registry

```cpp
// Auto-discover and register I2C devices
swarm.scanI2C();  // Scan bus for devices

// Register known device types
swarm.registerI2CDevice(0x48, "TMP102", "temperature");
swarm.registerI2CDevice(0x76, "BME280", "environment");
swarm.registerI2CDevice(0x3C, "SSD1306", "display");
swarm.registerI2CDevice(0x20, "PCF8574", "gpio_expander");

// Access devices by address or name
MeshDevice* sensor = swarm.getDevice("0x48");
MeshDevice* sensor = swarm.getDeviceByName("TMP102");
```

### 1-Wire Support

```cpp
// Scan for 1-Wire devices
std::vector<OneWireAddress> devices = swarm.scanOneWire(ONE_WIRE_PIN);

// Register by ROM code
swarm.registerOneWireDevice("28:FF:64:1E:2A:16:04:12", "DS18B20", "outside_temp");

// Read temperature
float temp = swarm.readOneWireTemp("outside_temp");
```

### Common Device Drivers

**Sensors:**
| Device | Bus | Type | Measures |
|--------|-----|------|----------|
| BME280/BMP280 | I2C | Environment | Temp, humidity, pressure |
| SHT30/SHT31 | I2C | Environment | Temp, humidity |
| TMP102/TMP117 | I2C | Temperature | High precision temp |
| DS18B20 | 1-Wire | Temperature | Waterproof temp |
| DHT11/DHT22 | Single-Wire | Environment | Temp, humidity |
| BH1750 | I2C | Light | Lux level |
| VL53L0X | I2C | Distance | ToF ranging |
| MPU6050 | I2C | Motion | Accel, gyro |
| ADS1115 | I2C | ADC | 4-ch 16-bit ADC |

**Actuators:**
| Device | Bus | Type | Controls |
|--------|-----|------|----------|
| PCA9685 | I2C | PWM | 16-ch servo/LED |
| PCF8574 | I2C | GPIO | 8-ch digital I/O |
| MCP23017 | I2C | GPIO | 16-ch digital I/O |
| WS2812/SK6812 | Single-Wire | LED | Addressable RGB |
| Relay modules | GPIO | Switch | High-power loads |

### Auto-Discovery

```cpp
// Scan all buses on startup
void setup() {
  swarm.begin("SensorNode");

  // Auto-discover devices
  swarm.enableAutoDiscovery(true);
  int found = swarm.discoverDevices();
  Serial.printf("Found %d devices\n", found);

  // List discovered devices
  for (auto& dev : swarm.getDevices()) {
    Serial.printf("  %s @ %s (%s)\n",
      dev.getType().c_str(),
      dev.getAddress().c_str(),
      dev.getBus().c_str());
  }
}
```

### Mesh State Integration

Auto-publish device readings to mesh state:

```cpp
swarm.enableDeviceReporting(true);

// Creates state keys like:
// "N1234_temp_0x48" = "23.5"
// "N1234_humidity_0x76" = "65"
// "N1234_light_0x23" = "450"
```

### Remote Commands

| Command | Args | Description |
|---------|------|-------------|
| `devices` | - | List all devices |
| `device_read` | `{address}` | Read device value |
| `device_write` | `{address, value}` | Write to device |
| `i2c_scan` | - | Scan I2C bus |
| `1wire_scan` | `{pin}` | Scan 1-Wire bus |
| `gpio_read` | `{pin}` | Read GPIO state |
| `gpio_write` | `{pin, value}` | Set GPIO state |

### Pin Configuration

Store pin assignments in persistent config:

```cpp
// Configure in NVS
swarm.setConfig("i2c_sda", "21");
swarm.setConfig("i2c_scl", "22");
swarm.setConfig("1wire_pin", "4");
swarm.setConfig("dht_pin", "15");

// Or via remote command
// config_set {"key": "1wire_pin", "value": "4"}
```

### Features

- [ ] I2C bus scanner and device registry
- [ ] 1-Wire bus scanner (Dallas temperature sensors)
- [ ] Common sensor drivers (BME280, SHT31, BH1750, etc.)
- [ ] GPIO expander support (PCF8574, MCP23017)
- [ ] PWM controller support (PCA9685)
- [ ] Addressable LED support (WS2812, SK6812)
- [ ] Auto-discovery on boot
- [ ] Device readings to mesh state
- [ ] Remote device read/write commands
- [ ] Pin configuration via persistent config

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

## Battery Management

**Status**: Idea

Per-node battery monitoring and network-wide power management for battery-operated mesh nodes.

### Battery Level Detection Methods

| Method | Accuracy | Cost | Complexity | Notes |
|--------|----------|------|------------|-------|
| **ADC + Voltage Divider** | ±5-10% | Low | Simple | Direct battery voltage reading via ESP32 ADC |
| **Fuel Gauge IC** | ±1% | Medium | I2C | MAX17048, BQ27441 - tracks charge/discharge |
| **Coulomb Counter** | ±1% | Medium | I2C | LTC2941, BQ27421 - measures actual current flow |
| **Smart Battery (SMBus)** | ±1% | High | I2C | Full battery info, common in laptops |
| **PMIC Reporting** | Varies | Included | I2C | AXP192/202 on some ESP32 boards (M5Stack, TTGO) |

### Per-Node Features

```cpp
// Battery API concept
swarm.setBatteryPin(34, 3.3, 4.2);  // ADC pin, min voltage, max voltage
swarm.setBatteryDivider(2.0);       // Voltage divider ratio

int percent = swarm.getBatteryPercent();
float voltage = swarm.getBatteryVoltage();
bool charging = swarm.isCharging();
bool lowBattery = swarm.isLowBattery();  // Below threshold

// Auto-publish to mesh state
swarm.enableBatteryReporting(true);  // Publishes "node_battery" state
```

**Features:**
- [ ] ADC-based voltage reading with configurable pins
- [ ] Voltage divider ratio configuration
- [ ] Non-linear discharge curve compensation (LiPo/Li-Ion)
- [ ] Fuel gauge IC support (MAX17048, BQ27441)
- [ ] Charging detection (if hardware supports)
- [ ] Low battery threshold alerts
- [ ] Auto-publish battery level to mesh state
- [ ] Battery state in heartbeat messages
- [ ] Deep sleep on critical battery

### Network-Wide Power Management

```cpp
// Network battery overview (coordinator/gateway)
std::vector<NodeBattery> batteries = swarm.getAllBatteryLevels();
NodeBattery lowest = swarm.getLowestBattery();
std::vector<String> critical = swarm.getCriticalBatteryNodes(10);  // Below 10%
```

**Features:**
- [ ] Aggregate battery status across all nodes
- [ ] Identify nodes with low/critical battery
- [ ] Dashboard view of network power status
- [ ] Alerts when any node reaches critical level
- [ ] Predictive battery life estimation
- [ ] Historical battery drain tracking

### Power Optimization

| Strategy | Savings | Trade-off |
|----------|---------|-----------|
| **Deep Sleep** | 90%+ | Reduced responsiveness |
| **Light Sleep** | 50-70% | Moderate latency |
| **Reduce TX Power** | 20-40% | Reduced range |
| **Longer Heartbeat** | 10-30% | Slower peer detection |
| **Disable Features** | Varies | Reduced functionality |

**Features:**
- [ ] Configurable sleep modes
- [ ] Wake on mesh message (ESP-NOW)
- [ ] Coordinated sleep schedules
- [ ] Adaptive heartbeat interval based on battery
- [ ] TX power reduction on low battery
- [ ] Feature disable on critical battery

### Mesh State Keys

| Key | Value | Description |
|-----|-------|-------------|
| `{node}_battery` | `85` | Battery percentage |
| `{node}_voltage` | `3.92` | Battery voltage |
| `{node}_charging` | `true/false` | Charging status |
| `{node}_power` | `battery/usb/solar` | Power source |

### Remote Commands

| Command | Args | Description |
|---------|------|-------------|
| `battery` | - | Get battery status |
| `battery_calibrate` | `{full_v, empty_v}` | Calibrate voltage range |
| `sleep` | `{duration_ms}` | Enter deep sleep |
| `power_mode` | `{mode}` | Set power mode |

### Hardware Considerations

**Common Battery Setups:**
- 18650 Li-Ion (3.7V nominal, 3.0-4.2V range)
- LiPo pouch cells (3.7V nominal)
- 2x AA/AAA with boost converter
- Solar + battery combination

**ESP32 ADC Notes:**
- ADC is non-linear, especially at extremes
- Use attenuation for higher voltages
- Consider averaging multiple readings
- Calibrate per-device for accuracy

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
