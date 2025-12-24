# ESP32-2432S028R Device Specification

**Date:** December 24, 2025  
**Status:** Proposed  
**Version:** 1.0  
**Device:** ESP32-2432S028R (Sunton 2.4" Display Module)

## Hardware Overview

### Device Specifications

**Processor**
- Chipset: ESP32 (dual-core)
- CPU Frequency: 160/240 MHz
- RAM: 520 KB SRAM
- Flash: 4 MB (typical)
- Voltage: 3.3V (USB or battery)

**Display**
- Type: 2.4" IPS TFT LCD
- Resolution: 240 × 320 pixels (QVGA)
- Interface: SPI (8-bit parallel capable)
- Color Depth: 16-bit (RGB565)
- Brightness: 500+ nits
- Touch Screen: Capacitive (optional on some variants)
- Driver IC: ILI9341 (display) / CST816S (touch, if equipped)

**Storage**
- Micro SD Card Slot (SDIO interface)
- Supports FAT32 filesystem
- Max tested: 32 GB

**GPIO Availability**
- Accessible pins for sensors and external devices
- I2C, SPI, UART interfaces available
- ADC pins for analog sensors

**Power**
- USB-C power input
- Battery connector (optional, board-dependent)
- Low-power modes supported

**Wireless**
- 802.11 b/g/n WiFi
- Bluetooth 4.2 / Bluetooth Low Energy
- Same painlessMesh compatibility as standard ESP32

### Display Advantage
Unlike the SSD1306 OLED (128×64 pixels), the 2.4" display provides:
- **8× larger screen area** (240×320 vs 128×64)
- **Color support** (16-bit vs monochrome)
- **Better visibility** in varied lighting
- **Touch input** capability (on some variants)
- Ideal for dashboard applications, data visualization, and user interaction

## MeshSwarm Integration

### Compatibility Matrix

| Feature | SSD1306 OLED | ESP32-2432S028R |
|---------|--------------|-----------------|
| Library | Adafruit_SSD1306 | TFT_eSPI |
| Resolution | 128×64 | 240×320 |
| Color | Monochrome | 16-bit RGB |
| Touch | No | Yes (variant-dependent) |
| Draw Speed | Fast | Very Fast |
| Power | Low | Moderate |
| Cost | Low | Moderate |

### Library Requirements

**Display Rendering**
- Primary: TFT_eSPI library (Bodmer)
  - Optimized SPI rendering
  - Large community support
  - Excellent ESP32 performance
  
- Alternative: LVGL
  - GUI framework
  - Widget library
  - More heavyweight but feature-rich

**Optional Features**
- Touch support: CST816S driver library
- SD Card: SD.h (standard Arduino)
- WiFi: Same as standard ESP32

### Pin Configuration

**Standard SPI Pins (must be customized in TFT_eSPI setup)**
```cpp
// Display connections (SPI)
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  -1  // Reset pin (optional)

// Touch (I2C)
#define TOUCH_SDA 21
#define TOUCH_SCL 22
#define TOUCH_INT 16

// SD Card (SDIO)
#define SD_CMD   11
#define SD_CLK   10
#define SD_D0    9
#define SD_D1    8
#define SD_D2    7
#define SD_D3    6
```

**Note:** Pin assignments vary by board revision. Users must verify with their specific board documentation.

## MeshSwarm Adaptation Strategy

### Display Abstraction Layer

To support both SSD1306 and TFT displays, we need to:

1. **Create display abstraction interface**
   ```cpp
   class DisplayDriver {
   public:
     virtual void init() = 0;
     virtual void clear() = 0;
     virtual void fillScreen(uint16_t color) = 0;
     virtual void drawPixel(int x, int y, uint16_t color) = 0;
     virtual void drawText(int x, int y, const String& text, uint16_t color) = 0;
     virtual void display() = 0;
   };
   ```

2. **Implement drivers for each display type**
   - `SSD1306Driver` (wraps Adafruit_SSD1306)
   - `TFT_eSPI_Driver` (wraps TFT_eSPI)

3. **Allow runtime selection via configuration**
   ```cpp
   #define DISPLAY_TYPE DISPLAY_SSD1306    // or DISPLAY_TFT_240x320
   ```

### Minimal Implementation Approach

**Option A: Device-Specific Example**
- Create `ESP32S028RNode` example
- Uses TFT_eSPI directly
- Separate from core MeshSwarm library
- Reduces library complexity

**Option B: Full Library Support**
- Add display abstraction to core
- Support multiple display types
- Higher complexity, more flexible

**Recommendation:** Start with Option A (device-specific example), move to Option B if multiple display types added.

## Use Cases

### 1. **Dashboard Display Node**
- Real-time mesh network status
- State visualization with graphs
- Touch menu for node commands
- Large, visible display for monitoring

### 2. **Sensor Hub with Visualization**
- Temperature/humidity trends over time
- Multiple sensor readings on one screen
- Selectable display modes (touch)
- Logging to SD card

### 3. **Control Interface**
- Touch-based commands to mesh peers
- State manipulation via UI
- Wireless bridge to WiFi
- Local data storage on SD

### 4. **Gateway Dashboard**
- Network topology visualization
- Telemetry graphs
- Firmware update status
- System health metrics

## File Structure

### New Files to Create

```
examples/
  ESP32S028RNode/
    ESP32S028RNode.ino           # Main example sketch
    README.md                     # Device-specific setup
    config.h                      # Pin configuration
    DisplayManager.h              # Display abstraction
    DisplayManager.cpp            # Implementation

prd/
  esp32-2432s028r-device.md      # This specification
  esp32-2432s028r-display-layer.md  # (Optional) Display abstraction design
```

### Updated Files

```
library.properties          # Add note about optional display support
README.md                   # Add ESP32-2432S028R section
CHANGELOG.md               # Version notes
```

## Implementation Roadmap

### Phase 1: Basic Support (v1.1.0)
- [ ] Create example sketch for ESP32-2432S028R
- [ ] Document pin configuration
- [ ] Basic display output (status, peers, state)
- [ ] No touch support in Phase 1

### Phase 2: Enhanced Features (v1.2.0)
- [ ] Add touch input support
- [ ] Create display manager abstraction
- [ ] Data visualization (graphs, trends)
- [ ] SD card logging example

### Phase 3: Library-Level Support (v2.0.0)
- [ ] Add display abstraction to core library
- [ ] Support multiple display types
- [ ] Widget library for common UI elements
- [ ] Potentially break compatibility (major version)

## Development Notes

### Configuration Management

Create `examples/ESP32S028RNode/config.h` to encapsulate pin definitions:

```cpp
#ifndef CONFIG_H
#define CONFIG_H

// Display (SPI)
#define DISPLAY_MISO 12
#define DISPLAY_MOSI 13
#define DISPLAY_SCLK 14
#define DISPLAY_CS   15
#define DISPLAY_DC   2

// Touch (I2C)
#define TOUCH_SDA 21
#define TOUCH_SCL 22

// SD Card (SDIO)
#define SD_CMD   11
#define SD_CLK   10
#define SD_D0    9
#define SD_D1    8
#define SD_D2    7
#define SD_D3    6

#endif
```

### Library Dependencies

Add to example's README:
```
Additional Libraries Required:
- TFT_eSPI (Bodmer) - Display driver
- CST816S (optional) - Touch support
- SD (built-in) - SD card access
```

### Display Update Strategy

Unlike SSD1306 (small, low refresh rate), TFT displays can update faster:

```cpp
// Example update timing
#define DISPLAY_REFRESH_RATE 1000  // 1 Hz (vs 500ms for OLED)
#define DISPLAY_PARTIAL_UPDATE true  // Only update changed areas
```

## Testing Checklist

### Hardware Verification
- [ ] Display powers on correctly
- [ ] SPI communication established
- [ ] Text rendering works
- [ ] Colors display correctly
- [ ] Touch input responsive (if equipped)
- [ ] SD card detected

### MeshSwarm Integration
- [ ] Node joins mesh correctly
- [ ] Display shows node status
- [ ] State changes reflected on display
- [ ] Serial commands work
- [ ] Telemetry functions (if enabled)
- [ ] OTA updates work

### Performance
- [ ] No blocking display updates
- [ ] Mesh update frequency maintained
- [ ] Memory usage acceptable
- [ ] Display refresh smooth (no flickering)

## Compatibility Notes

### With Existing Code
- Fully backward compatible (separate example)
- No changes to core MeshSwarm API
- Can coexist with SSD1306-based nodes

### Variant Handling
Different manufacturers produce ESP32-2432S028R variants with:
- Different pin assignments
- Touch vs. no-touch versions
- Various SD card support levels

**User Action Required:**
- Verify pin assignments for your specific board
- Update `config.h` accordingly
- Test before deployment

## Cost and Availability

**Estimated Cost:** $8-15 USD (vs $3-5 for OLED + SSD1306)

**Availability:** 
- Widely available from AliExpress, Amazon, electronics distributors
- Multiple manufacturers (Sunton, MH-ET Live, etc.)
- Usually includes USB cable

**Quality Variance:** Manufacturing quality varies; test carefully before relying on for critical applications.

## References

### Documentation
- [ESP32-2432S028R Pinout & Datasheet](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/esp32_datasheet.pdf)
- [TFT_eSPI Library](https://github.com/Bodmer/TFT_eSPI)
- [ILI9341 Display Controller](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf)

### Similar Projects
- [Bodmer's TFT_eSPI Examples](https://github.com/Bodmer/TFT_eSPI/tree/master/examples)
- [LVGL ESP32 Examples](https://github.com/lvgl/lvgl)

### Community
- PlatformIO forums
- Arduino forums (ESP32 section)
- GitHub discussions

## Success Criteria

- ✅ Example sketch fully functional
- ✅ Display shows mesh status clearly
- ✅ No performance degradation vs OLED version
- ✅ Documentation complete
- ✅ Touch support functional (Phase 2+)
- ✅ Backward compatible with existing nodes
- ✅ Community feedback positive

## Approval & Sign-off

| Role | Approval | Date |
|------|----------|------|
| Specification Author | | |
| Technical Lead | | |
| Project Owner | | |

---

**Next Steps:**
1. Gather feedback on this specification
2. Approve for Phase 1 development
3. Create example sketch
4. Test with actual hardware
5. Document pin configuration for common variants
