# MeshSwarm Modular Build System

MeshSwarm now supports **modular compilation** with feature flags that allow you to disable unused components and significantly reduce flash memory usage. This is especially valuable for resource-constrained ESP32 projects.

## Feature Flags

All features are **enabled by default** for backward compatibility. You can disable individual features by defining their flags as `0` before including the library.

### Available Feature Flags

| Flag | Default | Description | Flash Savings |
|------|---------|-------------|---------------|
| `MESHSWARM_ENABLE_DISPLAY` | 1 | OLED display support (Adafruit SSD1306) | ~10-15KB |
| `MESHSWARM_ENABLE_SERIAL` | 1 | Serial command interface and debugging | ~8-12KB |
| `MESHSWARM_ENABLE_TELEMETRY` | 1 | HTTP telemetry and gateway mode | ~12-18KB |
| `MESHSWARM_ENABLE_OTA` | 1 | OTA firmware distribution | ~15-20KB |
| `MESHSWARM_ENABLE_CALLBACKS` | 1 | Custom callback hooks (onLoop, onSerial, onDisplay) | ~3-5KB |

### Core Features (Always Enabled)

The following features are **always compiled** and cannot be disabled:

- Mesh networking (painlessMesh integration)
- State management (key-value store with versioning)
- Peer discovery and tracking
- Coordinator election
- Basic message handling
- Heartbeat system

## Usage Examples

### PlatformIO

Add build flags to your `platformio.ini` file:

```ini
[env:minimal]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    edlovesjava/MeshSwarm
    painlessMesh
    ArduinoJson
build_flags = 
    -DMESHSWARM_ENABLE_DISPLAY=0
    -DMESHSWARM_ENABLE_SERIAL=0
    -DMESHSWARM_ENABLE_TELEMETRY=0
    -DMESHSWARM_ENABLE_OTA=0
```

### Arduino IDE

Define flags **before** including the library in your sketch:

```cpp
// Disable all optional features for minimal build
#define MESHSWARM_ENABLE_DISPLAY 0
#define MESHSWARM_ENABLE_SERIAL 0
#define MESHSWARM_ENABLE_TELEMETRY 0
#define MESHSWARM_ENABLE_OTA 0

#include <MeshSwarm.h>

MeshSwarm swarm;

void setup() {
  swarm.begin("MinimalNode");
}

void loop() {
  swarm.update();
}
```

## Build Configurations

### 1. Full-Featured Build (Default)

**All features enabled** - maximum functionality, largest binary size.

```cpp
// No flags needed - this is the default
#include <MeshSwarm.h>
```

**Approximate flash usage:** 58-84KB

**Use cases:**
- Development and debugging
- Gateway nodes with WiFi connectivity
- Nodes with OLED displays
- Full-featured deployments

---

### 2. Core + Display + Serial

**Display and serial enabled, telemetry and OTA disabled** - good for standalone nodes with debugging.

```cpp
#define MESHSWARM_ENABLE_TELEMETRY 0
#define MESHSWARM_ENABLE_OTA 0
#include <MeshSwarm.h>
```

**PlatformIO:**
```ini
build_flags = 
    -DMESHSWARM_ENABLE_TELEMETRY=0
    -DMESHSWARM_ENABLE_OTA=0
```

**Approximate flash usage:** 38-50KB  
**Flash savings:** ~25-35KB

**Use cases:**
- Standalone nodes with OLED feedback
- Debugging and development
- Nodes without internet connectivity

---

### 3. Core + Display Only

**Display enabled, serial/telemetry/OTA disabled** - minimal but with visual feedback.

```cpp
#define MESHSWARM_ENABLE_SERIAL 0
#define MESHSWARM_ENABLE_TELEMETRY 0
#define MESHSWARM_ENABLE_OTA 0
#include <MeshSwarm.h>
```

**PlatformIO:**
```ini
build_flags = 
    -DMESHSWARM_ENABLE_SERIAL=0
    -DMESHSWARM_ENABLE_TELEMETRY=0
    -DMESHSWARM_ENABLE_OTA=0
```

**Approximate flash usage:** 30-40KB  
**Flash savings:** ~30-45KB

**Use cases:**
- Production nodes with OLED display
- Battery-powered nodes
- Simple sensor/actuator nodes

---

### 4. Core Only (Minimal)

**All optional features disabled** - absolute minimum flash usage.

```cpp
#define MESHSWARM_ENABLE_DISPLAY 0
#define MESHSWARM_ENABLE_SERIAL 0
#define MESHSWARM_ENABLE_TELEMETRY 0
#define MESHSWARM_ENABLE_OTA 0
#include <MeshSwarm.h>
```

**PlatformIO:**
```ini
build_flags = 
    -DMESHSWARM_ENABLE_DISPLAY=0
    -DMESHSWARM_ENABLE_SERIAL=0
    -DMESHSWARM_ENABLE_TELEMETRY=0
    -DMESHSWARM_ENABLE_OTA=0
```

**Approximate flash usage:** 15-20KB  
**Flash savings:** ~40-65KB

**Use cases:**
- Ultra-low-memory deployments
- Headless sensor nodes
- Production nodes without debugging
- Maximum number of nodes on constrained devices

---

### 5. Gateway Build

**Telemetry and serial enabled** for gateway/relay nodes.

```cpp
#define MESHSWARM_ENABLE_DISPLAY 0
#define MESHSWARM_ENABLE_OTA 0
#include <MeshSwarm.h>
```

**PlatformIO:**
```ini
build_flags = 
    -DMESHSWARM_ENABLE_DISPLAY=0
    -DMESHSWARM_ENABLE_OTA=0
```

**Approximate flash usage:** 40-52KB  
**Flash savings:** ~20-30KB

**Use cases:**
- Gateway nodes with WiFi and HTTP telemetry
- Debugging without display
- Relay nodes

## Feature Dependencies

Some features depend on others and will automatically be enabled if needed:

- **Serial callbacks** require `MESHSWARM_ENABLE_CALLBACKS`
- **Display callbacks** require `MESHSWARM_ENABLE_CALLBACKS`

The library will emit compile-time warnings if dependencies are missing and automatically enable required features.

## Compile-Time Messages

The library provides helpful compile-time information about your build:

```
MeshSwarm: Core-only build (minimal flash usage)
```

or

```
MeshSwarm: Basic build (Display + Serial)
```

These messages appear during compilation to confirm your configuration.

## API Availability

### When Features Are Disabled

If you disable a feature, its associated API methods become unavailable:

#### Display Disabled (`MESHSWARM_ENABLE_DISPLAY=0`)
❌ Not available:
- `getDisplay()`
- `setStatusLine()`
- `onDisplayUpdate()`

#### Serial Disabled (`MESHSWARM_ENABLE_SERIAL=0`)
❌ Not available:
- `onSerialCommand()`
- Built-in serial commands (status, peers, state, etc.)
- Serial debugging output

#### Telemetry Disabled (`MESHSWARM_ENABLE_TELEMETRY=0`)
❌ Not available:
- `setTelemetryServer()`
- `enableTelemetry()`
- `pushTelemetry()`
- `connectToWiFi()`
- `setGatewayMode()`

#### OTA Disabled (`MESHSWARM_ENABLE_OTA=0`)
❌ Not available:
- `enableOTADistribution()`
- `checkForOTAUpdates()`
- `enableOTAReceive()`

#### Callbacks Disabled (`MESHSWARM_ENABLE_CALLBACKS=0`)
❌ Not available:
- `onLoop()`
- `onSerialCommand()` (if serial enabled)
- `onDisplayUpdate()` (if display enabled)

### Always Available (Core API)

These methods are **always available** regardless of build configuration:

✅ Core API:
- `begin()`
- `update()`
- `setState()`
- `getState()`
- `watchState()`
- `getNodeId()`
- `getNodeName()`
- `getRole()`
- `isCoordinator()`
- `getPeerCount()`
- `getPeers()`
- `getMesh()`
- `broadcastFullState()`
- `requestStateSync()`

## Flash Memory Analysis

### Typical ESP32 Compilation Results

Based on ESP32 with 4MB flash:

| Configuration | Binary Size | SRAM | Flash % | Savings |
|---------------|-------------|------|---------|---------|
| Full Featured | 58-84KB | 35-42KB | ~6.5% | Baseline |
| Core + Display + Serial | 38-50KB | 30-36KB | ~4.5% | ~25-35KB |
| Core + Display | 30-40KB | 28-34KB | ~3.5% | ~30-45KB |
| **Core Only** | **15-20KB** | **25-30KB** | **~2.0%** | **~40-65KB** |

### When to Use Minimal Builds

Consider disabling features when:

1. **Flash memory is tight** - Multiple libraries competing for space
2. **No display hardware** - Why compile display code?
3. **No WiFi/Internet** - Telemetry and OTA aren't useful
4. **Production deployment** - Serial debugging not needed
5. **Battery-powered** - Minimize code for power efficiency
6. **Maximum nodes** - Deploy more nodes with smaller binaries

## Migration from Full Build

### Gradual Migration

You can gradually reduce features:

**Step 1:** Disable OTA (if not using)
```cpp
#define MESHSWARM_ENABLE_OTA 0
```

**Step 2:** Disable telemetry (if not using)
```cpp
#define MESHSWARM_ENABLE_TELEMETRY 0
```

**Step 3:** Disable serial (production)
```cpp
#define MESHSWARM_ENABLE_SERIAL 0
```

**Step 4:** Disable display (if no OLED)
```cpp
#define MESHSWARM_ENABLE_DISPLAY 0
```

### Compile Errors

If you call a disabled feature's method, you'll get a compile error:

```
error: 'class MeshSwarm' has no member named 'pushTelemetry'
```

This is intentional - it prevents runtime issues and ensures you know what's available.

## Examples

See the `examples/` directory for complete examples:

- `examples/MinimalNode/` - Core-only minimal build
- `examples/DisplayOnlyNode/` - Core + Display
- `examples/BasicNode/` - Full-featured (default)

## Best Practices

1. **Start with defaults** - Use full-featured build during development
2. **Profile your needs** - Identify which features you actually use
3. **Test configurations** - Verify functionality after disabling features
4. **Document your build** - Note which flags are used in your project
5. **Consider dependencies** - Check library dependency requirements

## Troubleshooting

### Issue: Compilation errors about missing methods

**Solution:** You're calling methods from a disabled feature. Either enable the feature or remove those calls.

### Issue: Serial output missing

**Solution:** Enable `MESHSWARM_ENABLE_SERIAL=1` or remove Serial-dependent debugging code.

### Issue: Display not working

**Solution:** Enable `MESHSWARM_ENABLE_DISPLAY=1` or verify you have the display hardware connected.

### Issue: No size reduction

**Solution:** Ensure you're defining flags **before** including `MeshSwarm.h`, not after.

## Support

- **Documentation:** [GitHub README](https://github.com/edlovesjava/MeshSwarm)
- **Issues:** [GitHub Issues](https://github.com/edlovesjava/MeshSwarm/issues)
- **Examples:** See `examples/` directory

## Contributing

We welcome contributions! If you identify additional opportunities for modular compilation or have suggestions for improving the build system, please open an issue or pull request.
