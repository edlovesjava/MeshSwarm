# MeshSwarm Display Abstraction Layer Specification

**Date:** December 24, 2025  
**Status:** Proposed  
**Version:** 1.0  
**Scope:** Configurable SPI and I2C Display Support

## Overview

This specification defines a flexible display abstraction layer that allows MeshSwarm to support multiple display types through configuration. The goal is to make it easy to add new displays (OLED, LCD, e-ink, round displays, etc.) without changing core library code.

## Supported Display Types

### Current & Planned Support

| Display | Type | Interface | Resolution | Colors | Driver IC | Status |
|---------|------|-----------|------------|--------|-----------|--------|
| SSD1306 | OLED | I2C | 128×64 | Mono | SSD1306 | Current |
| ESP32-2432S028R | IPS LCD | SPI | 240×320 | 16-bit RGB | ILI9341 | Planned |
| AITRIP 1.28" Round | IPS LCD | SPI | 240×240 | 16-bit RGB | GC9A01 | New |
| Generic SPI TFT | Various | SPI | Configurable | 16-bit RGB | Various | Planned |

### Why Multiple Display Types?

**Different Use Cases Require Different Displays:**
- **OLED (128×64)**: Low power, simple status display, existing nodes
- **IPS LCD (240×320)**: Dashboard, data visualization, touch support
- **Round Display (240×240)**: Aesthetic appeal, compact form factor, unique UI
- **E-Ink**: Ultra-low power, passive display, outdoor visibility

**User Choice:**
- Users may have specific hardware constraints
- Cost vs. capability tradeoff
- Availability and sourcing preferences
- Power budget considerations

## Display Abstraction Architecture

### Core Interface

Define a minimal display interface that all drivers implement:

```cpp
class IMeshSwarmDisplay {
public:
  virtual ~IMeshSwarmDisplay() = default;
  
  // Initialization
  virtual void init() = 0;
  virtual bool isReady() = 0;
  
  // Basic drawing
  virtual void clear(uint16_t color = 0) = 0;
  virtual void fillScreen(uint16_t color) = 0;
  virtual void drawPixel(int16_t x, int16_t y, uint16_t color) = 0;
  
  // Text drawing (simplified)
  virtual void drawText(int16_t x, int16_t y, const char* text, 
                        uint16_t textColor, uint16_t bgColor = 0) = 0;
  
  // Display dimensions
  virtual uint16_t width() const = 0;
  virtual uint16_t height() const = 0;
  
  // Update display
  virtual void update() = 0;
  
  // Optional: touch support
  virtual bool hasTouch() const { return false; }
  virtual bool getTouchPoint(int16_t& x, int16_t& y) { return false; }
};
```

### Driver Implementation Pattern

Each display driver implements `IMeshSwarmDisplay`:

```cpp
// SSD1306_Driver.h
class SSD1306_Driver : public IMeshSwarmDisplay {
private:
  Adafruit_SSD1306 display;
  // ... driver-specific members
  
public:
  SSD1306_Driver();
  void init() override;
  bool isReady() override;
  void clear(uint16_t color = 0) override;
  void fillScreen(uint16_t color) override;
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void drawText(int16_t x, int16_t y, const char* text,
                uint16_t textColor, uint16_t bgColor = 0) override;
  uint16_t width() const override;
  uint16_t height() const override;
  void update() override;
};
```

### Factory Pattern for Display Selection

```cpp
// DisplayFactory.h
enum DisplayType {
  DISPLAY_NONE      = 0,
  DISPLAY_SSD1306   = 1,
  DISPLAY_ILI9341   = 2,
  DISPLAY_GC9A01    = 3,
  DISPLAY_CUSTOM    = 99
};

class DisplayFactory {
public:
  static IMeshSwarmDisplay* createDisplay(DisplayType type);
};
```

### Usage in MeshSwarm

```cpp
// In MeshSwarm.h
class MeshSwarm {
private:
  std::unique_ptr<IMeshSwarmDisplay> displayDriver;
  
public:
  IMeshSwarmDisplay* getDisplay() { return displayDriver.get(); }
};

// In MeshSwarm.cpp setup
void MeshSwarm::initDisplay() {
  #if MESHSWARM_ENABLE_DISPLAY
    displayDriver = std::unique_ptr<IMeshSwarmDisplay>(
      DisplayFactory::createDisplay(DISPLAY_TYPE)
    );
    
    if (displayDriver) {
      displayDriver->init();
    }
  #endif
}
```

## Configuration System

### Configuration Header

Create `src/DisplayConfig.h` for all display-related settings:

```cpp
#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

// ============== DISPLAY TYPE SELECTION ==============
// Options: DISPLAY_NONE, DISPLAY_SSD1306, DISPLAY_ILI9341, DISPLAY_GC9A01
#ifndef DISPLAY_TYPE
#define DISPLAY_TYPE DISPLAY_SSD1306
#endif

// ============== DISPLAY DIMENSIONS ==============
#if DISPLAY_TYPE == DISPLAY_SSD1306
  #ifndef DISPLAY_WIDTH
  #define DISPLAY_WIDTH   128
  #endif
  #ifndef DISPLAY_HEIGHT
  #define DISPLAY_HEIGHT  64
  #endif
  
#elif DISPLAY_TYPE == DISPLAY_ILI9341
  #ifndef DISPLAY_WIDTH
  #define DISPLAY_WIDTH   240
  #endif
  #ifndef DISPLAY_HEIGHT
  #define DISPLAY_HEIGHT  320
  #endif
  
#elif DISPLAY_TYPE == DISPLAY_GC9A01
  #ifndef DISPLAY_WIDTH
  #define DISPLAY_WIDTH   240
  #endif
  #ifndef DISPLAY_HEIGHT
  #define DISPLAY_HEIGHT  240
  #endif
#endif

// ============== I2C CONFIGURATION (for OLED) ==============
#if DISPLAY_TYPE == DISPLAY_SSD1306
  #ifndef I2C_SDA
  #define I2C_SDA         21
  #endif
  #ifndef I2C_SCL
  #define I2C_SCL         22
  #endif
  #ifndef OLED_ADDR
  #define OLED_ADDR       0x3C
  #endif
#endif

// ============== SPI CONFIGURATION (for LCD) ==============
#if DISPLAY_TYPE == DISPLAY_ILI9341 || DISPLAY_TYPE == DISPLAY_GC9A01
  #ifndef SPI_MISO
  #define SPI_MISO        12
  #endif
  #ifndef SPI_MOSI
  #define SPI_MOSI        13
  #endif
  #ifndef SPI_SCLK
  #define SPI_SCLK        14
  #endif
  #ifndef SPI_CS
  #define SPI_CS          15
  #endif
  #ifndef SPI_DC
  #define SPI_DC          2
  #endif
  #ifndef SPI_RST
  #define SPI_RST         -1  // Optional reset pin
  #endif
  #ifndef SPI_BL
  #define SPI_BL          -1  // Optional backlight pin
  #endif
#endif

// ============== TOUCH CONFIGURATION ==============
#if DISPLAY_TYPE == DISPLAY_ILI9341
  #ifndef TOUCH_ENABLED
  #define TOUCH_ENABLED   0   // 1 = enable, 0 = disable
  #endif
  #if TOUCH_ENABLED
    #ifndef TOUCH_SDA
    #define TOUCH_SDA     21
    #endif
    #ifndef TOUCH_SCL
    #define TOUCH_SCL     22
    #endif
  #endif
#endif

// ============== DISPLAY TIMING ==============
#ifndef DISPLAY_UPDATE_INTERVAL
#define DISPLAY_UPDATE_INTERVAL 500  // milliseconds
#endif

#endif // DISPLAY_CONFIG_H
```

### User Configuration Pattern

Users override configuration **before** including MeshSwarm.h:

```cpp
// In their sketch or platformio.ini
#define DISPLAY_TYPE DISPLAY_GC9A01
#define SPI_CS 15
#define SPI_DC 2
#include <MeshSwarm.h>

MeshSwarm swarm;

void setup() {
  swarm.begin("MyNode");
}
```

## Driver Implementation Examples

### Example 1: SSD1306 OLED Driver

```cpp
// src/drivers/SSD1306_Driver.h
#ifndef SSD1306_DRIVER_H
#define SSD1306_DRIVER_H

#include "../DisplayConfig.h"
#if DISPLAY_TYPE == DISPLAY_SSD1306

#include "../IMeshSwarmDisplay.h"
#include <Adafruit_SSD1306.h>

class SSD1306_Driver : public IMeshSwarmDisplay {
private:
  Adafruit_SSD1306 display;
  bool ready;
  
public:
  SSD1306_Driver();
  void init() override;
  bool isReady() override { return ready; }
  void clear(uint16_t color = 0) override;
  void fillScreen(uint16_t color) override;
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void drawText(int16_t x, int16_t y, const char* text,
                uint16_t textColor, uint16_t bgColor = 0) override;
  uint16_t width() const override;
  uint16_t height() const override;
  void update() override;
};

#endif
#endif
```

### Example 2: GC9A01 Round Display Driver

```cpp
// src/drivers/GC9A01_Driver.h
#ifndef GC9A01_DRIVER_H
#define GC9A01_DRIVER_H

#include "../DisplayConfig.h"
#if DISPLAY_TYPE == DISPLAY_GC9A01

#include "../IMeshSwarmDisplay.h"
#include <TFT_eSPI.h>

class GC9A01_Driver : public IMeshSwarmDisplay {
private:
  TFT_eSPI display;
  bool ready;
  
public:
  GC9A01_Driver();
  void init() override;
  bool isReady() override { return ready; }
  void clear(uint16_t color = 0) override;
  void fillScreen(uint16_t color) override;
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void drawText(int16_t x, int16_t y, const char* text,
                uint16_t textColor, uint16_t bgColor = 0) override;
  uint16_t width() const override;
  uint16_t height() const override;
  void update() override;
};

#endif
#endif
```

### Example 3: ILI9341 LCD Driver (with Touch)

```cpp
// src/drivers/ILI9341_Driver.h
#ifndef ILI9341_DRIVER_H
#define ILI9341_DRIVER_H

#include "../DisplayConfig.h"
#if DISPLAY_TYPE == DISPLAY_ILI9341

#include "../IMeshSwarmDisplay.h"
#include <TFT_eSPI.h>

class ILI9341_Driver : public IMeshSwarmDisplay {
private:
  TFT_eSPI display;
  bool ready;
  bool touchEnabled;
  
public:
  ILI9341_Driver();
  void init() override;
  bool isReady() override { return ready; }
  void clear(uint16_t color = 0) override;
  void fillScreen(uint16_t color) override;
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void drawText(int16_t x, int16_t y, const char* text,
                uint16_t textColor, uint16_t bgColor = 0) override;
  uint16_t width() const override;
  uint16_t height() const override;
  void update() override;
  
  // Touch support
  bool hasTouch() const override { return touchEnabled; }
  bool getTouchPoint(int16_t& x, int16_t& y) override;
};

#endif
#endif
```

## File Structure

### New Project Layout

```
src/
  MeshSwarm.h                      # Main header (no display code)
  MeshSwarm.cpp                    # Main implementation
  MeshSwarmConfig.h                # Feature flags
  DisplayConfig.h                  # NEW: Display configuration
  
  display/
    IMeshSwarmDisplay.h            # NEW: Display interface
    DisplayFactory.h               # NEW: Display factory
    DisplayFactory.cpp             # NEW: Factory implementation
    
    drivers/
      SSD1306_Driver.h             # NEW: OLED driver
      SSD1306_Driver.cpp           # NEW: OLED implementation
      ILI9341_Driver.h             # NEW: ILI9341 driver
      ILI9341_Driver.cpp           # NEW: ILI9341 implementation
      GC9A01_Driver.h              # NEW: GC9A01 driver
      GC9A01_Driver.cpp            # NEW: GC9A01 implementation
  
  features/
    MeshSwarmDisplay.inc           # Updated: Uses new abstraction
    # ... other features

examples/
  BasicNode/
  SensorNode/
  DisplayOnlyNode/
  GatewayNode/
  
  # NEW device-specific examples
  ESP32S028RNode/
    ESP32S028RNode.ino
    config.h                       # Board-specific pins
  
  AITRIP128RoundNode/
    AITRIP128RoundNode.ino
    config.h                       # Board-specific pins

prd/
  display-abstraction-layer.md     # This specification
  esp32-2432s028r-device.md
  esp32-2432s028r-device.md
```

## Adding a New Display Type

### Step-by-Step Guide

#### Step 1: Add Display Type Enum
In `DisplayConfig.h`:
```cpp
enum DisplayType {
  // ... existing types ...
  DISPLAY_MY_DISPLAY = 10
};
```

#### Step 2: Add Configuration
In `DisplayConfig.h`:
```cpp
#elif DISPLAY_TYPE == DISPLAY_MY_DISPLAY
  #ifndef DISPLAY_WIDTH
  #define DISPLAY_WIDTH   <width>
  #endif
  #ifndef DISPLAY_HEIGHT
  #define DISPLAY_HEIGHT  <height>
  #endif
  // Add pin definitions...
#endif
```

#### Step 3: Create Driver Header
Create `src/drivers/MyDisplay_Driver.h`:
```cpp
#if DISPLAY_TYPE == DISPLAY_MY_DISPLAY

#include "../IMeshSwarmDisplay.h"
#include <YourLibrary.h>

class MyDisplay_Driver : public IMeshSwarmDisplay {
  // ... implement interface ...
};

#endif
```

#### Step 4: Create Driver Implementation
Create `src/drivers/MyDisplay_Driver.cpp`:
```cpp
#if DISPLAY_TYPE == DISPLAY_MY_DISPLAY

#include "MyDisplay_Driver.h"

// ... implement all methods ...

#endif
```

#### Step 5: Update DisplayFactory
In `src/display/DisplayFactory.cpp`:
```cpp
IMeshSwarmDisplay* DisplayFactory::createDisplay(DisplayType type) {
  switch(type) {
    case DISPLAY_SSD1306:
      return new SSD1306_Driver();
    case DISPLAY_ILI9341:
      return new ILI9341_Driver();
    case DISPLAY_GC9A01:
      return new GC9A01_Driver();
    case DISPLAY_MY_DISPLAY:      // NEW
      return new MyDisplay_Driver(); // NEW
    default:
      return nullptr;
  }
}
```

#### Step 6: Create Example Sketch
Create `examples/MyDisplayNode/MyDisplayNode.ino`:
```cpp
#define DISPLAY_TYPE DISPLAY_MY_DISPLAY
#define DISPLAY_WIDTH   <width>
#define DISPLAY_HEIGHT  <height>
#define SPI_MOSI        <pin>
#define SPI_SCLK        <pin>
#define SPI_CS          <pin>
#define SPI_DC          <pin>
// ... other pins ...

#include <MeshSwarm.h>

MeshSwarm swarm;

void setup() {
  swarm.begin("MyDisplayNode");
}

void loop() {
  swarm.update();
}
```

## AITRIP 1.28" Round Display Integration

### Specifications

| Property | Value |
|----------|-------|
| Display Type | IPS LCD (TFT) |
| Resolution | 240 × 240 pixels (square resolution) |
| Screen Size | 1.28 inches diagonal |
| Driver IC | GC9A01 |
| Interface | 4-wire SPI |
| Color Depth | 16-bit RGB565 |
| Viewing Angle | 80° |
| Response Time | <5ms |
| Brightness | 500+ nits |
| Operating Voltage | 3.3V |
| Power Consumption | ~50mA (typical) |

### Driver Implementation

The AITRIP uses the GC9A01 driver, which is SPI-based and easy to integrate with TFT_eSPI library.

**Recommended Library:** TFT_eSPI (Bodmer)

**Typical Wiring:**
```
GC9A01  ->  ESP32
VCC     ->  3.3V
GND     ->  GND
DIN     ->  GPIO 13 (MOSI)
CLK     ->  GPIO 14 (SCLK)
CS      ->  GPIO 15
DC      ->  GPIO 2
```

### GC9A01_Driver Features

```cpp
class GC9A01_Driver : public IMeshSwarmDisplay {
  // Basic drawing fully supported
  // Circular graphics can be implemented for aesthetic round display UI
  // Display rotations supported (0, 90, 180, 270)
};
```

## Library Dependencies

### By Display Type

| Display | Library | Version | Required |
|---------|---------|---------|----------|
| SSD1306 | Adafruit_SSD1306 | 2.x | Yes |
| SSD1306 | Adafruit-GFX-Library | 1.x | Yes |
| ILI9341 | TFT_eSPI | Latest | Yes |
| GC9A01 | TFT_eSPI | Latest | Yes |
| Touch | CST816S | Latest | No |

### Conditional Compilation

In `library.properties` or `platformio.ini`:

```ini
[env:esp32-ssd1306]
lib_deps = 
  adafruit/Adafruit SSD1306@^2.5.0
  adafruit/Adafruit GFX Library@^1.11.0

[env:esp32-gc9a01]
lib_deps = 
  bodmer/TFT_eSPI@^2.5.0

[env:esp32-ili9341]
lib_deps = 
  bodmer/TFT_eSPI@^2.5.0
```

## Performance Considerations

### Update Rates by Display Type

| Display | Pixels | Color Depth | Update Rate |
|---------|--------|-------------|-------------|
| SSD1306 | 8,192 | 1-bit | 1 Hz (500ms) |
| ILI9341 | 153,600 | 16-bit | 2-4 Hz (250-500ms) |
| GC9A01 | 57,600 | 16-bit | 4 Hz (250ms) |

### Memory Footprint

| Display | Frame Buffer | Driver Code | Total |
|---------|--------------|-------------|-------|
| SSD1306 | 1 KB | 2 KB | 3 KB |
| ILI9341 | 120 KB | 5 KB | 125 KB |
| GC9A01 | 120 KB | 5 KB | 125 KB |

**Note:** Larger displays may require double-buffering for smooth animations.

## Backward Compatibility

### Existing Code
- SSD1306 remains default: `#define DISPLAY_TYPE DISPLAY_SSD1306`
- No breaking API changes
- Existing examples continue working unchanged
- Can mix display types in same mesh network

### Migration Path
1. Phase 1: Add abstraction layer (internal refactor)
2. Phase 2: Add new driver implementations
3. Phase 3: Deprecate old Adafruit_SSD1306 direct access (v2.0)
4. Phase 4: Full abstraction enforced

## Testing Strategy

### Unit Testing
- Each driver tested independently
- Mock display interface for CI/CD
- Pixel verification for basic shapes

### Integration Testing
- Each example sketch compiled for target display
- Actual hardware testing with real displays
- Display output validation (visual inspection)
- Touch input testing (where supported)

### Regression Testing
- All existing examples tested with default SSD1306
- Mesh functionality unchanged
- State synchronization works correctly

## Implementation Roadmap

### Phase 1: Abstraction Layer (v1.1.0)
- [ ] Define IMeshSwarmDisplay interface
- [ ] Create DisplayConfig.h
- [ ] Implement DisplayFactory
- [ ] Refactor existing SSD1306 to use abstraction
- [ ] Update core library to use display interface
- [ ] No breaking changes

### Phase 2: SPI Display Support (v1.2.0)
- [ ] Implement ILI9341_Driver
- [ ] Implement GC9A01_Driver
- [ ] Create ESP32S028RNode example
- [ ] Create AITRIP128RoundNode example
- [ ] Update documentation

### Phase 3: Advanced Features (v1.3.0)
- [ ] Touch support abstraction
- [ ] Graphics primitives (lines, circles, etc.)
- [ ] Font support (multiple font sizes)
- [ ] Display rotation

### Phase 4: Full Abstraction (v2.0)
- [ ] Remove legacy Adafruit_SSD1306 direct access
- [ ] Make display interface primary API
- [ ] Add more display types
- [ ] LVGL integration (optional)

## Success Criteria

- ✅ All display types work with minimal configuration
- ✅ Adding new display type requires <100 lines of code
- ✅ No performance degradation vs direct library use
- ✅ Backward compatible with existing SSD1306 nodes
- ✅ All examples compile and run correctly
- ✅ Documentation clear and complete
- ✅ Community can easily add custom displays

## References

### Display Datasheets
- [GC9A01 Display Controller](https://cdn-shop.adafruit.com/product-files/GC9A01.pdf)
- [ILI9341 Display Controller](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf)
- [SSD1306 OLED Controller](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)

### Libraries
- [TFT_eSPI Repository](https://github.com/Bodmer/TFT_eSPI)
- [Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306)
- [LVGL Graphics Library](https://lvgl.io/)

### Design Patterns
- [Factory Pattern](https://refactoring.guru/design-patterns/factory-method)
- [Strategy Pattern](https://refactoring.guru/design-patterns/strategy)
- [Dependency Injection](https://en.wikipedia.org/wiki/Dependency_injection)

---

**Next Steps:**
1. Get feedback on this specification
2. Approve for Phase 1 implementation
3. Begin refactoring display code to abstraction layer
4. Create SSD1306_Driver adapter
5. Implement DisplayFactory
6. Begin Phase 2 with new drivers
