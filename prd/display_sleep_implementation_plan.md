# Display Sleep/Wake Implementation Plan

## Current Status

**Phase 1 POC Complete**: Basic display sleep/wake implemented inline in `firmware/nodes/button/main.cpp` and `MeshSwarm/examples/ButtonNode/ButtonNode.ino`.

**What's Working**:
- SSD1306 OLED sleep via `ssd1306_command(SSD1306_DISPLAYOFF/ON)`
- Activity timeout tracking with `millis()`
- Boot button (GPIO0) wake with debounce
- Configurable timeout via `DISPLAY_SLEEP_TIMEOUT_MS` define

**What's Missing** (per PRD):
- `DisplayPowerManager` class in MeshSwarm library
- Display dimming before sleep
- Deep sleep support
- RTC memory state persistence
- Touch wake support
- MeshSwarm library integration (`setDisplayTimeout()`, `onWake()`, etc.)

---

## Implementation Approaches

### Option A: Inline Code Pattern (Current)

Copy/paste the sleep/wake functions into each node. Simple but duplicates code.

**Pros**:
- Works now
- No library changes required
- Easy to customize per node

**Cons**:
- Code duplication across 8+ nodes
- Bug fixes require updating every node
- No dimming or advanced features
- Doesn't follow PRD design

### Option B: DisplayPowerManager Class (PRD Design)

Add `DisplayPowerManager` class to MeshSwarm library per PRD specification.

**Pros**:
- Single source of truth
- Supports dimming, callbacks, deep sleep
- Follows PRD design
- Easier to maintain

**Cons**:
- More upfront work
- Requires MeshSwarm library changes
- Need to handle different display types (SSD1306 vs TFT)

### Option C: Hybrid Approach

1. First, complete inline implementation for all nodes (quick wins)
2. Then, refactor to `DisplayPowerManager` class
3. Update nodes to use new class

**Pros**:
- Get working code deployed quickly
- Then improve architecture
- Iterative approach

**Cons**:
- Some throwaway work
- Two implementation phases

---

## Recommended Approach: Option B (PRD Design)

The PRD already specifies a clean `DisplayPowerManager` design. We should implement it properly rather than creating technical debt.

---

## Detailed Implementation Plan

### Milestone 1: DisplayPowerManager Class

**Goal**: Create reusable display power management in MeshSwarm library.

#### 1.1 Create DisplayPowerManager Header

**File**: `MeshSwarm/src/DisplayPowerManager.h`

```cpp
#ifndef DISPLAY_POWER_MANAGER_H
#define DISPLAY_POWER_MANAGER_H

#include <Arduino.h>
#include <functional>

// Forward declarations
class Adafruit_SSD1306;

// Display type enum
enum class DisplayType {
    SSD1306_OLED,
    GC9A01_TFT,
    ILI9341_TFT,
    NONE
};

// Sleep commands for different displays
struct DisplayCommands {
    uint8_t sleepIn;   // Command to enter sleep
    uint8_t sleepOut;  // Command to exit sleep
    uint16_t wakeDelayMs; // Delay after wake command
};

class DisplayPowerManager {
public:
    DisplayPowerManager();

    // Configuration
    void begin(DisplayType type);
    void setDisplay(Adafruit_SSD1306* display);  // For OLED
    void setTftCommandCallback(std::function<void(uint8_t)> sendCommand);  // For TFT

    void setSleepTimeout(unsigned long timeoutMs);
    void setDimTimeout(unsigned long dimMs);
    void setDimLevel(uint8_t percent);  // 0-100

    // Wake sources
    void addWakeButton(uint8_t pin, bool activeLow = true);
    void enableWakeOnStateChange(bool enable);

    // Runtime
    void update();  // Call in loop()
    void resetActivity();  // Call on any user activity
    void wake();   // Force wake
    void sleep();  // Force sleep

    // State queries
    bool isAsleep() const;
    bool isDimmed() const;
    unsigned long getIdleTime() const;

    // Callbacks
    void onSleep(std::function<void()> callback);
    void onWake(std::function<void()> callback);
    void onDim(std::function<void(uint8_t)> callback);

private:
    DisplayType _type = DisplayType::NONE;
    Adafruit_SSD1306* _oled = nullptr;
    std::function<void(uint8_t)> _sendTftCommand = nullptr;

    unsigned long _sleepTimeoutMs = 30000;
    unsigned long _dimTimeoutMs = 0;  // 0 = disabled
    uint8_t _dimLevel = 30;

    unsigned long _lastActivityMs = 0;
    bool _asleep = false;
    bool _dimmed = false;

    // Wake button tracking
    static const uint8_t MAX_WAKE_BUTTONS = 4;
    struct WakeButton {
        uint8_t pin;
        bool activeLow;
        bool lastState;
        unsigned long lastChange;
    };
    WakeButton _wakeButtons[MAX_WAKE_BUTTONS];
    uint8_t _wakeButtonCount = 0;

    bool _wakeOnStateChange = false;

    // Callbacks
    std::function<void()> _onSleep = nullptr;
    std::function<void()> _onWake = nullptr;
    std::function<void(uint8_t)> _onDim = nullptr;

    // Display commands
    DisplayCommands _commands;

    void pollWakeButtons();
    void sendSleepCommand();
    void sendWakeCommand();
};

#endif
```

#### 1.2 Create DisplayPowerManager Implementation

**File**: `MeshSwarm/src/DisplayPowerManager.cpp`

Key methods:
- `begin()` - Initialize with display type, set appropriate sleep/wake commands
- `update()` - Poll buttons, check timeouts, trigger dim/sleep
- `resetActivity()` - Reset timeout on user input
- `wake()/sleep()` - Send display commands, trigger callbacks

#### 1.3 Integrate with MeshSwarm Class

**File**: `MeshSwarm/src/MeshSwarm.h` (additions)

```cpp
class MeshSwarm {
public:
    // Existing methods...

    // Display power management
    DisplayPowerManager& getPowerManager();
    void enableDisplaySleep(unsigned long timeoutMs = 30000);
    void setDisplayDimTimeout(unsigned long dimMs);
    void addDisplayWakeButton(uint8_t pin, bool activeLow = true);

private:
    DisplayPowerManager _powerManager;
};
```

#### 1.4 Test DisplayPowerManager

- Unit test with mock display
- Integration test with SSD1306
- Integration test with GC9A01 TFT

---

### Milestone 2: Update Nodes to Use DisplayPowerManager

#### 2.1 Update Button Node (Reference Implementation)

**File**: `firmware/nodes/button/main.cpp`

Replace inline sleep/wake code with:
```cpp
void setup() {
    swarm.begin(NODE_NAME);

    // Enable display sleep with 15s timeout
    swarm.enableDisplaySleep(15000);
    swarm.addDisplayWakeButton(BOOT_BUTTON_PIN);
    swarm.addDisplayWakeButton(EXT_BUTTON_PIN);

    // Optional: Wake on state changes
    #if DISPLAY_WAKE_ON_STATE_CHANGE
    swarm.getPowerManager().enableWakeOnStateChange(true);
    #endif
}
```

#### 2.2 Update OLED Nodes

| Node | File | Wake Buttons |
|------|------|--------------|
| watcher | `firmware/nodes/watcher/main.cpp` | GPIO0 |
| dht | `firmware/nodes/dht/main.cpp` | GPIO0 |
| pir | `firmware/nodes/pir/main.cpp` | GPIO0 |
| led | `firmware/nodes/led/main.cpp` | GPIO0 |
| light | `firmware/nodes/light/main.cpp` | GPIO0 |

Each node adds:
```cpp
swarm.enableDisplaySleep(30000);
swarm.addDisplayWakeButton(0);  // Boot button
```

#### 2.3 Update TFT Nodes

| Node | File | Wake Buttons | Notes |
|------|------|--------------|-------|
| clock | `firmware/nodes/clock/main.cpp` | GPIO32, 33, 4 | GC9A01 TFT |
| remote | `firmware/nodes/remote/main.cpp` | Touch | ILI9341 TFT |

Clock node:
```cpp
// Custom TFT command sender
swarm.getPowerManager().setTftCommandCallback([](uint8_t cmd) {
    tft.sendCommand(cmd);
});
swarm.getPowerManager().begin(DisplayType::GC9A01_TFT);
swarm.enableDisplaySleep(30000);
swarm.addDisplayWakeButton(BTN_LEFT);
swarm.addDisplayWakeButton(BTN_RIGHT);
swarm.addDisplayWakeButton(BTN_MODE);
```

---

### Milestone 3: Advanced Features

#### 3.1 Display Dimming

Add brightness control for displays that support it:
- TFT with PWM backlight
- OLED contrast adjustment

#### 3.2 Deep Sleep Support

Add to DisplayPowerManager:
```cpp
void enableDeepSleep(bool enable);
void enterDeepSleep();
void onDeepWake(std::function<void(esp_sleep_wakeup_cause_t)> callback);
```

#### 3.3 RTC State Persistence

Add to MeshSwarm:
```cpp
void saveStateToRTC();
void restoreStateFromRTC();
```

---

## Node Summary

| Node | Display | Wake Source | Timeout | Library Method |
|------|---------|-------------|---------|----------------|
| button | SSD1306 | Boot + External | 15s | `enableDisplaySleep()` |
| watcher | SSD1306 | Boot (GPIO0) | 30s | `enableDisplaySleep()` |
| dht | SSD1306 | Boot (GPIO0) | 30s | `enableDisplaySleep()` |
| pir | SSD1306 | Boot (GPIO0) | 30s | `enableDisplaySleep()` |
| led | SSD1306 | Boot (GPIO0) | 30s | `enableDisplaySleep()` |
| light | SSD1306 | Boot (GPIO0) | 30s | `enableDisplaySleep()` |
| clock | GC9A01 TFT | 3 buttons | 30s | `setTftCommandCallback()` |
| remote | ILI9341 TFT | Touch | 30s | Touch integration |
| gateway | SSD1306 | Never sleep | N/A | Skip |

---

## File Changes Summary

### MeshSwarm Library (New Files)
- `src/DisplayPowerManager.h` - Header
- `src/DisplayPowerManager.cpp` - Implementation

### MeshSwarm Library (Modified)
- `src/MeshSwarm.h` - Add power manager integration
- `src/MeshSwarm.cpp` - Implement power manager methods

### Firmware Nodes (Modified)
- `firmware/nodes/button/main.cpp` - Use DisplayPowerManager
- `firmware/nodes/watcher/main.cpp` - Add sleep support
- `firmware/nodes/dht/main.cpp` - Add sleep support
- `firmware/nodes/pir/main.cpp` - Add sleep support
- `firmware/nodes/led/main.cpp` - Add sleep support
- `firmware/nodes/light/main.cpp` - Add sleep support
- `firmware/nodes/clock/main.cpp` - Add TFT sleep support
- `firmware/nodes/remote/main.cpp` - Add touch wake support

### Examples (Modified)
- `examples/ButtonNode/ButtonNode.ino` - Demonstrate DisplayPowerManager

---

## Validation Checklist

For each milestone:

### Milestone 1 (DisplayPowerManager)
- [ ] Class compiles without errors
- [ ] SSD1306 sleep/wake works
- [ ] TFT sleep/wake works (via callback)
- [ ] Button wake polling works
- [ ] Timeout tracking works
- [ ] Callbacks fire correctly

### Milestone 2 (Node Updates)
- [ ] All OLED nodes sleep after timeout
- [ ] Boot button wakes all OLED nodes
- [ ] Clock node buttons wake display
- [ ] No regressions in node functionality
- [ ] Serial logs show sleep/wake events

### Milestone 3 (Advanced)
- [ ] Dimming works before sleep
- [ ] Deep sleep reduces power consumption
- [ ] State persists across deep sleep
- [ ] Touch wake works on remote node

---

## Decision Point

**Which approach should we take?**

1. **Continue with inline code** - Finish rolling out current pattern to all nodes, defer library class to later
2. **Pause and implement DisplayPowerManager** - Create library class first, then update nodes to use it
3. **Hybrid** - Finish inline for OLED nodes (quick), then create class for TFT nodes

The inline approach is faster but creates technical debt. The library approach is cleaner but requires more upfront work.
