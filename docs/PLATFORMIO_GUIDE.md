# PlatformIO Development Guide

This guide explains how to use PlatformIO with the MeshSwarm library for local development and testing.

---

## Overview

PlatformIO provides a professional development environment with:
- Automated dependency management
- Multiple board configurations
- Built-in testing framework
- Command-line and IDE integration
- Faster compilation than Arduino IDE

---

## Installation

### Option 1: PlatformIO Core (Command Line)

```bash
# Install via pip
pip install platformio

# Verify installation
pio --version
```

### Option 2: PlatformIO IDE (Visual Studio Code)

1. Install Visual Studio Code
2. Open Extensions (Ctrl+Shift+X / Cmd+Shift+X)
3. Search for "PlatformIO IDE"
4. Click Install

---

## Quick Start

### Building for ESP32

```bash
# Navigate to project root
cd MeshSwarm

# Compile for ESP32 (default)
pio run

# Compile for specific board
pio run -e esp32
pio run -e esp32s3
pio run -e esp32c3

# Upload to device
pio run -e esp32 --target upload

# Open serial monitor
pio device monitor
```

### Testing Feature Flags

```bash
# Minimal build (core only)
pio run -e test-minimal

# Core + Display
pio run -e test-core-display

# Core + Serial
pio run -e test-core-serial

# Gateway mode (all features)
pio run -e test-gateway
```

---

## Available Environments

### Board Environments

| Environment | Board | Description |
|-------------|-------|-------------|
| `esp32` | esp32dev | Standard ESP32 (default, recommended) |
| `esp32s3` | esp32-s3-devkitc-1 | ESP32-S3 with USB Serial/JTAG |
| `esp32c3` | esp32-c3-devkitc-02 | ESP32-C3 RISC-V variant |

> **Note**: The `esp32` environment is the primary tested configuration. Other board variants are available but may require additional testing.

### Test Environments (Feature Flags)

| Environment | Features Enabled | Flash Usage |
|-------------|------------------|-------------|
| `test-minimal` | Core only | ~15-20KB |
| `test-core-display` | Core + Display | ~30-40KB |
| `test-core-serial` | Core + Serial | ~25-35KB |
| `test-telemetry` | Core + Serial + Telemetry | ~40-50KB |
| `test-ota` | Core + Serial + OTA | ~45-55KB |
| `test-gateway` | All features | ~58-84KB |

### Log Level Tests

| Environment | Log Level | Description |
|-------------|-----------|-------------|
| `test-log-none` | NONE (0) | No logging |
| `test-log-error` | ERROR (1) | Errors only |
| Default | INFO (3) | Full logging |

---

## Common Tasks

### Clean Build

```bash
# Clean build artifacts
pio run --target clean

# Clean everything (including dependencies)
pio run --target fullclean
```

### Update Dependencies

```bash
# Update all library dependencies
pio pkg update

# Update platform (ESP32 core)
pio pkg update -p espressif32
```

### List Devices

```bash
# Show connected devices
pio device list
```

### Upload and Monitor

```bash
# Upload and immediately open monitor
pio run -e esp32 --target upload --target monitor
```

---

## Compiling Examples

### Method 1: Change src_dir in platformio.ini

Edit `platformio.ini`:
```ini
[platformio]
src_dir = examples/SensorNode  # Change to desired example
```

Then compile:
```bash
pio run -e esp32
```

### Method 2: Use pio ci command

```bash
# Compile specific example
cd examples/BasicNode
pio ci --lib="../.." --board=esp32dev BasicNode.ino

# Compile with feature flags
cd examples/MinimalNode
pio ci --lib="../.." --project-option="env:test-minimal" MinimalNode.ino
```

---

## Integration with VS Code

### Using PlatformIO IDE Extension

1. Open MeshSwarm folder in VS Code
2. PlatformIO will detect `platformio.ini`
3. Use PlatformIO toolbar at bottom:
   - Click environment to select (e.g., `esp32`)
   - Click ✓ to compile
   - Click → to upload
   - Click 🔌 to monitor

### Keyboard Shortcuts

| Action | Shortcut |
|--------|----------|
| Build | Ctrl+Alt+B / Cmd+Alt+B |
| Upload | Ctrl+Alt+U / Cmd+Alt+U |
| Monitor | Ctrl+Alt+S / Cmd+Alt+S |
| Clean | Ctrl+Alt+C / Cmd+Alt+C |

---

## Advanced Configuration

### Custom Build Flags

Add to specific environment in `platformio.ini`:

```ini
[env:custom]
board = esp32dev
build_flags = 
    -DMESHSWARM_ENABLE_DISPLAY=1
    -DMESHSWARM_ENABLE_SERIAL=1
    -DMESHSWARM_LOG_LEVEL=4
    -DMESH_PREFIX='"mynetwork"'
    -DMESH_PASSWORD='"mypassword"'
```

### Monitor Configuration

```ini
[env:esp32]
monitor_speed = 115200
monitor_filters = 
    esp32_exception_decoder
    colorize
```

### Upload Speed

```ini
[env:esp32]
upload_speed = 921600  # Fast upload
# upload_speed = 115200  # Slower but more reliable
```

---

## Troubleshooting

### "Platform espressif32 is not installed"

```bash
pio pkg install -p espressif32@^6.0.0
```

### "Library not found"

```bash
# Update library dependencies
pio pkg install
```

### Serial Port Permission (Linux)

```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

### Upload Failed

```bash
# Try slower upload speed
pio run -e esp32 --target upload --upload-port /dev/ttyUSB0
```

### Clean Build If Errors Persist

```bash
pio run --target clean
rm -rf .pio
pio run
```

---

## CI/CD Integration

The GitHub Actions workflow automatically tests compilation on every push and pull request:

- **Environments**: All feature flag configurations (esp32, test-minimal, test-gateway, etc.)
- **Examples**: All example sketches (BasicNode, MinimalNode, SensorNode, etc.)

The CI uses the same `platformio.ini` configuration with pinned dependency versions to ensure consistent builds.

View results: https://github.com/edlovesjava/MeshSwarm/actions

[![CI](https://github.com/edlovesjava/MeshSwarm/actions/workflows/ci.yml/badge.svg)](https://github.com/edlovesjava/MeshSwarm/actions/workflows/ci.yml)

---

## Comparison: PlatformIO vs Arduino IDE

| Feature | PlatformIO | Arduino IDE |
|---------|------------|-------------|
| Dependency Management | Automatic | Manual |
| Multi-board Support | Built-in | Manual setup |
| Build Speed | Fast | Slower |
| CI/CD Integration | Native | Requires arduino-cli |
| Feature Flag Testing | Easy | Manual |
| Editor Integration | VS Code, others | Custom IDE |
| Learning Curve | Moderate | Easy |
| **Best For** | Development & CI | Quick prototyping |

---

## Tips & Best Practices

1. **Use environments for testing** - Don't modify code, change environment
2. **Cache builds** - PlatformIO caches dependencies, use `.pio` folder
3. **Monitor during development** - Use `--target monitor` to see serial output
4. **Test before commit** - Run `pio run -e test-minimal` to catch errors
5. **Keep platformio.ini clean** - Comment unused environments

---

## Additional Resources

- [PlatformIO Documentation](https://docs.platformio.org/)
- [PlatformIO CLI Reference](https://docs.platformio.org/en/latest/core/userguide/index.html)
- [ESP32 Platform Documentation](https://docs.platformio.org/en/latest/platforms/espressif32.html)
- [MeshSwarm GitHub](https://github.com/edlovesjava/MeshSwarm)

---

## Getting Help

- **PlatformIO Community**: https://community.platformio.org/
- **MeshSwarm Issues**: https://github.com/edlovesjava/MeshSwarm/issues
- **ESP32 Arduino**: https://github.com/espressif/arduino-esp32

---

*Last updated: December 2024*
