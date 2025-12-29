# Power Management & Screen Wake Specification

## Overview

Implement power management for ESP32 display nodes that allows screens to enter low-power mode after inactivity and wake on button press or touch input. The primary goal is extending battery life for portable devices while maintaining responsiveness.

## Research Summary

### ESP32 Sleep Modes

| Mode | Power Consumption | Wake Latency | RAM Retention | Use Case |
|------|-------------------|--------------|---------------|----------|
| **Active** | 80-260mA | N/A | Full | Normal operation |
| **Light Sleep** | ~0.8mA | <1ms | Full | Short idle periods, quick wake |
| **Deep Sleep** | 10-150uA | ~200ms | RTC only | Extended idle, max power savings |

### Wake-Up Sources

| Source | GPIO Requirements | Power Draw | Notes |
|--------|-------------------|------------|-------|
| **ext0** (single pin) | RTC GPIO only | ~50-100uA | Simple, can't combine with touch |
| **ext1** (multiple pins) | RTC GPIO only | ~50-100uA | Multiple buttons, any/all trigger modes |
| **Touch** | Touch-capable GPIO | ~30uA | Capacitive touch pads, ESP32 has 10 |
| **Timer** | None | ~10uA | Periodic wake for sensor polling |
| **ULP Coprocessor** | RTC GPIO | ~100uA | Complex monitoring while sleeping |

### Boot Button (GPIO0) Availability

**GPIO0 is an RTC GPIO** and can be used as a wake-up source via ext0 or ext1. This is the "boot" button on most ESP32 dev boards.

**Considerations:**
- GPIO0 is used for boot mode selection (LOW = bootloader mode)
- Safe to use as wake source since wake triggers after boot strapping
- On ESP32 DevKit boards, this is the only built-in button (besides EN/reset)

### Touch Wake Limitations

- **ESP32**: Multiple touch pads can wake simultaneously
- **ESP32-S2/S3**: Only ONE touch pad can be configured as wake source
- **ext0 cannot be combined with touch wake** on original ESP32

## Requirements

### Functional Requirements

1. **Screen Timeout**
   - Display turns off after configurable period of inactivity (default: 30 seconds)
   - Any button press resets the timeout
   - Mesh state updates can optionally reset timeout (e.g., motion detected)

2. **Wake Sources**
   - Boot button (GPIO0) wake on devices without dedicated buttons
   - Any configured button wakes screen on display nodes
   - Optional touch wake for touch-enabled displays
   - Timer wake for periodic state sync (optional)

3. **Sleep Modes**
   - Light sleep for display-only timeout (display off, ESP32 active)
   - Deep sleep for extended power saving (full node sleep)
   - Mode selection per node type or configuration

4. **State Preservation**
   - Current screen/menu position saved before sleep
   - Mesh state cached in RTC memory for deep sleep
   - Seamless restoration on wake

### Non-Functional Requirements

1. **Battery Life**
   - Target: 1000+ hours standby with light sleep
   - Target: 8000+ hours standby with deep sleep
   - Wake response time: <500ms for light sleep, <1s for deep sleep

2. **User Experience**
   - Immediate visual feedback on wake (within 100ms of button press)
   - No "bootloader mode" issues from GPIO0 wake
   - Graceful degradation if wake source unavailable

## Design

### Display Timeout (Light Sleep Mode)

For display nodes like Clock that need to stay on the mesh but save power:

```cpp
// Display timeout - screen off, node active
#define DISPLAY_TIMEOUT_MS 30000   // 30 seconds default
#define DISPLAY_DIM_MS     20000   // Dim at 20 seconds before off

class DisplayPowerManager {
private:
    unsigned long lastActivity = 0;
    bool displayOn = true;
    bool displayDimmed = false;

public:
    void resetTimeout() {
        lastActivity = millis();
        if (!displayOn) wakeDisplay();
        if (displayDimmed) setBrightness(100);
    }

    void update() {
        unsigned long idle = millis() - lastActivity;

        if (displayOn && idle > DISPLAY_DIM_MS && !displayDimmed) {
            setBrightness(30);  // Dim to 30%
            displayDimmed = true;
        }

        if (displayOn && idle > DISPLAY_TIMEOUT_MS) {
            sleepDisplay();
        }
    }

    void sleepDisplay() {
        // Turn off backlight/display power
        displayOn = false;
        displayDimmed = false;
        // TFT-specific: set display sleep mode
    }

    void wakeDisplay() {
        displayOn = true;
        // TFT-specific: wake display, restore backlight
    }
};
```

### Deep Sleep Mode

For battery-operated nodes that can disconnect from mesh:

```cpp
#include <esp_sleep.h>

// Wake sources
#define WAKE_BUTTON_PIN GPIO_NUM_0  // Boot button
#define WAKE_TOUCH_PIN  T0          // GPIO4 touch pad

void enterDeepSleep() {
    // Save state to RTC memory
    saveStateToRTC();

    // Configure wake source - choose ONE approach

    // Option 1: Button wake (ext0)
    esp_sleep_enable_ext0_wakeup(WAKE_BUTTON_PIN, 0);  // Wake on LOW

    // Option 2: Touch wake (cannot combine with ext0)
    // touchSleepWakeUpEnable(WAKE_TOUCH_PIN, TOUCH_THRESHOLD);
    // esp_sleep_enable_touchpad_wakeup();

    // Option 3: Multiple buttons (ext1)
    // uint64_t mask = (1ULL << GPIO_NUM_32) | (1ULL << GPIO_NUM_33);
    // esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);

    // Optional: Timer wake for periodic sync
    // esp_sleep_enable_timer_wakeup(SYNC_INTERVAL_US);

    // Disconnect from mesh cleanly
    swarm.prepareForSleep();

    // Enter deep sleep
    esp_deep_sleep_start();
}

void setup() {
    // Check wake reason
    esp_sleep_wakeup_cause_t wakeReason = esp_sleep_get_wakeup_cause();

    switch (wakeReason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("Woke from button press");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            Serial.println("Woke from touch");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("Woke from timer - sync and sleep again");
            break;
        default:
            Serial.println("Normal boot");
    }

    // Restore state from RTC memory if applicable
    if (wakeReason != ESP_SLEEP_WAKEUP_UNDEFINED) {
        restoreStateFromRTC();
    }
}
```

### RTC Memory State Storage

```cpp
// Store in RTC memory (survives deep sleep, not reset)
RTC_DATA_ATTR ScreenMode savedScreen = SCREEN_CLOCK;
RTC_DATA_ATTR int savedMenuSelection = 0;
RTC_DATA_ATTR bool savedAlarmEnabled = false;
RTC_DATA_ATTR int savedAlarmHour = 7;
RTC_DATA_ATTR int savedAlarmMinute = 0;
RTC_DATA_ATTR unsigned long savedMeshTimeBase = 0;
RTC_DATA_ATTR uint32_t bootCount = 0;

void saveStateToRTC() {
    savedScreen = currentScreen;
    savedMenuSelection = menuSelection;
    savedAlarmEnabled = alarmEnabled;
    savedAlarmHour = alarmHour;
    savedAlarmMinute = alarmMinute;
    savedMeshTimeBase = meshTimeBase + (millis() - meshTimeMillis) / 1000;
}

void restoreStateFromRTC() {
    currentScreen = savedScreen;
    menuSelection = savedMenuSelection;
    alarmEnabled = savedAlarmEnabled;
    alarmHour = savedAlarmHour;
    alarmMinute = savedAlarmMinute;
    setMeshTime(savedMeshTimeBase);
    bootCount++;
}
```

### Node-Specific Wake Configurations

| Node Type | Wake Source | Default Timeout | Sleep Mode |
|-----------|-------------|-----------------|------------|
| **Clock** | BTN_MODE (GPIO4) or any button | 30s | Display-only |
| **Watcher** | Boot button (GPIO0) | 60s | Display-only |
| **Gateway** | Never sleep | N/A | Always active |
| **PIR** | Timer (periodic) + ext1 | N/A | Deep sleep between polls |
| **Button** | ext0 on button GPIO | N/A | Deep sleep always |
| **DHT** | Timer only | N/A | Deep sleep between reads |
| **LED** | Never sleep (actuator) | N/A | Light sleep between updates |

### Clock Node Implementation

The clock node has three buttons available:
- **BTN_LEFT** (GPIO32) - RTC GPIO, can wake
- **BTN_RIGHT** (GPIO33) - RTC GPIO, can wake
- **BTN_MODE** (GPIO4) - RTC GPIO, can wake

```cpp
// In clock/main.cpp

#define DISPLAY_TIMEOUT_MS 30000  // 30 seconds
#define DISPLAY_DIM_MS     20000  // Dim after 20 seconds

unsigned long lastActivity = 0;
bool displayAsleep = false;
bool displayDimmed = false;

void wakeDisplay() {
    if (!displayAsleep) return;

    displayAsleep = false;
    displayDimmed = false;

    // Wake TFT from sleep mode
    tft.sendCommand(0x11);  // SLPOUT command for GC9A01
    delay(120);             // Wait for wake

    // Restore backlight (if PWM controlled)
    // analogWrite(TFT_BL, 255);

    // Force screen redraw
    screenChanged = true;

    Serial.println("[BUTTON] Display woke");
}

void sleepDisplay() {
    if (displayAsleep) return;

    displayAsleep = true;

    // Put TFT in sleep mode
    tft.sendCommand(0x10);  // SLPIN command for GC9A01

    // Turn off backlight (if PWM controlled)
    // analogWrite(TFT_BL, 0);

    Serial.println("[BUTTON] Display sleeping");
}

void dimDisplay() {
    if (displayDimmed || displayAsleep) return;

    displayDimmed = true;
    // Reduce backlight to 30% (if PWM controlled)
    // analogWrite(TFT_BL, 76);

    Serial.println("[BUTTON] Display dimmed");
}

void resetActivityTimeout() {
    lastActivity = millis();
    wakeDisplay();
}

void checkDisplayTimeout() {
    if (displayAsleep) return;

    unsigned long idle = millis() - lastActivity;

    if (idle > DISPLAY_DIM_MS && !displayDimmed) {
        dimDisplay();
    }

    if (idle > DISPLAY_TIMEOUT_MS) {
        sleepDisplay();
    }
}

// Modify handleButtons() to call resetActivityTimeout()
void handleButtons() {
    // ... existing button handling ...

    // Any button activity wakes/resets display
    if (btnLeftPressed || btnRightPressed || btnModePressed) {
        resetActivityTimeout();
    }

    // ... rest of handler ...
}

// Add to loop()
void loop() {
    // ... existing code ...

    checkDisplayTimeout();
}
```

### MeshSwarm Library Integration

Add power management support to MeshSwarm:

```cpp
// In MeshSwarm.h

class MeshSwarm {
public:
    // Power management
    void setDisplayTimeout(unsigned long timeoutMs);
    void setDimTimeout(unsigned long dimMs);
    void enableTouchWake(int touchPin, int threshold);
    void enableButtonWake(gpio_num_t pin, bool activeLow = true);
    void prepareForSleep();  // Clean mesh disconnect
    void enterLightSleep();
    void enterDeepSleep();

    // Activity tracking
    void resetActivityTimer();
    bool isDisplaySleeping();
    unsigned long getIdleTime();

    // Callbacks
    void onWake(std::function<void(esp_sleep_wakeup_cause_t)> callback);
    void onSleep(std::function<void()> callback);

private:
    unsigned long _displayTimeout = 30000;
    unsigned long _dimTimeout = 20000;
    unsigned long _lastActivity = 0;
    bool _displaySleeping = false;
};
```

### Configuration Options

```cpp
// Build-time configuration in platformio.ini
build_flags =
    -DDISPLAY_TIMEOUT_MS=30000
    -DDISPLAY_DIM_MS=20000
    -DENABLE_DEEP_SLEEP=0
    -DWAKE_PIN=GPIO_NUM_0

// Runtime configuration via mesh state
swarm.setState("_config_timeout", "30000");
swarm.setState("_config_dim", "20000");
```

## Implementation Plan

### Phase 1: Display Timeout (Button Node - POC Complete)

1. Add display sleep/wake functions using SSD1306 commands (SSD1306_DISPLAYON/OFF)
2. Add activity timer that resets on button press (boot GPIO0 or external GPIO5)
3. Wake display instantly on any button press
4. Test validates OLED pattern for all other OLED-based nodes

### Phase 2: Boot Button Wake (Generic)

1. Add GPIO0 wake support for nodes without dedicated buttons
2. Ensure no bootloader conflicts
3. Test on ESP32 DevKit with only boot button

### Phase 3: MeshSwarm Library Integration

1. Add `DisplayPowerManager` class to library
2. Add sleep/wake callbacks
3. Add `prepareForSleep()` for clean mesh disconnect
4. Add state persistence helpers

### Phase 4: Deep Sleep Support

1. Add RTC memory state storage
2. Add deep sleep with button wake
3. Add timer wake for sensor nodes
4. Test mesh reconnection after wake

### Phase 5: Touch Wake (Optional)

1. Add capacitive touch wake for touch-enabled displays
2. Configure touch threshold
3. Test on ESP32 with touch pads

## Hardware Considerations

### Backlight Control

Most TFT displays have separate backlight control:
- GC9A01 round displays: Some have direct PWM backlight on a dedicated pin
- ILI9341 displays: Often have BL pin for PWM or on/off control

If no dedicated backlight pin:
- Use display sleep command only (reduces power but may not fully blank)
- Consider adding transistor switch for backlight power

### RTC GPIO Pin Reference

| GPIO | RTC GPIO | Touch | Wake Capable |
|------|----------|-------|--------------|
| 0 | RTC_GPIO11 | T1 | Yes |
| 2 | RTC_GPIO12 | T2 | Yes |
| 4 | RTC_GPIO10 | T0 | Yes |
| 12 | RTC_GPIO15 | T5 | Yes |
| 13 | RTC_GPIO14 | T4 | Yes |
| 14 | RTC_GPIO16 | T6 | Yes |
| 15 | RTC_GPIO13 | T3 | Yes |
| 25 | RTC_GPIO6 | - | Yes |
| 26 | RTC_GPIO7 | - | Yes |
| 27 | RTC_GPIO17 | T7 | Yes |
| 32 | RTC_GPIO9 | T9 | Yes |
| 33 | RTC_GPIO8 | T8 | Yes |
| 34 | RTC_GPIO4 | - | Yes (input only) |
| 35 | RTC_GPIO5 | - | Yes (input only) |
| 36 | RTC_GPIO0 | - | Yes (input only) |
| 39 | RTC_GPIO3 | - | Yes (input only) |

### Power Consumption Targets

| State | Target | Measurement Point |
|-------|--------|-------------------|
| Active + Display | <100mA | Full operation |
| Active + Display Off | <50mA | Screen sleeping |
| Light Sleep | <1mA | ESP32 in light sleep |
| Deep Sleep | <50uA | Full deep sleep |

## Testing Checklist

- [ ] Display dims after DIM_TIMEOUT seconds of inactivity
- [ ] Display sleeps after DISPLAY_TIMEOUT seconds of inactivity
- [ ] Any button press wakes display immediately
- [ ] Screen state preserved after wake (same screen, same data)
- [ ] Boot button (GPIO0) can wake from sleep without entering bootloader
- [ ] Mesh reconnects successfully after deep sleep wake
- [ ] Touch wake works on touch-enabled displays (where applicable)
- [ ] Battery life improves measurably in sleep mode
- [ ] No visual glitches during dim/sleep/wake transitions

## References

- [ESP32 Deep Sleep with Arduino IDE](https://randomnerdtutorials.com/esp32-deep-sleep-arduino-ide-wake-up-sources/)
- [ESP32 Touch Wake Up](https://randomnerdtutorials.com/esp32-touch-wake-up-deep-sleep/)
- [ESP32 External Wake Up](https://randomnerdtutorials.com/esp32-external-wake-up-deep-sleep/)
- [ESP-IDF Sleep Modes Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/sleep_modes.html)
- [ESP32 Sleep Modes Power Consumption](https://lastminuteengineers.com/esp32-sleep-modes-power-consumption/)
