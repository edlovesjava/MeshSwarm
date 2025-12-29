/**
 * DisplayPowerManager - Display sleep/wake management for ESP32 nodes
 *
 * Manages display power states including:
 * - Automatic sleep after configurable timeout
 * - Wake on button press
 * - Optional wake on mesh state changes
 *
 * Supports SSD1306 OLED and TFT displays (GC9A01, ILI9341)
 */

#ifndef DISPLAY_POWER_MANAGER_H
#define DISPLAY_POWER_MANAGER_H

#include <Arduino.h>
#include <functional>

// Forward declarations
class Adafruit_SSD1306;

// Build-time configuration defaults
#ifndef DISPLAY_SLEEP_TIMEOUT_MS
#define DISPLAY_SLEEP_TIMEOUT_MS 30000
#endif

#ifndef DISPLAY_WAKE_ON_STATE_CHANGE
#define DISPLAY_WAKE_ON_STATE_CHANGE 0
#endif

// Display type enumeration
enum class DisplayType {
    NONE,
    SSD1306_OLED,
    GC9A01_TFT,
    ILI9341_TFT
};

// Maximum number of wake buttons supported
#define DPM_MAX_WAKE_BUTTONS 4

// Debounce time for button presses
#ifndef DPM_DEBOUNCE_MS
#define DPM_DEBOUNCE_MS 50
#endif

/**
 * DisplayPowerManager class
 *
 * Usage:
 *   DisplayPowerManager power;
 *   power.begin(DisplayType::SSD1306_OLED);
 *   power.setOledDisplay(&display);
 *   power.setSleepTimeout(30000);
 *   power.addWakeButton(0);  // Boot button
 *   // In loop: power.update();
 */
class DisplayPowerManager {
public:
    DisplayPowerManager();

    // ============== Configuration ==============

    /**
     * Initialize with display type
     * @param type The display type (SSD1306_OLED, GC9A01_TFT, etc.)
     */
    void begin(DisplayType type);

    /**
     * Set OLED display reference (for SSD1306)
     * @param display Pointer to Adafruit_SSD1306 display
     */
    void setOledDisplay(Adafruit_SSD1306* display);

    /**
     * Set TFT command callback (for TFT displays)
     * @param sendCommand Function that sends a command byte to the TFT
     */
    void setTftCommandCallback(std::function<void(uint8_t)> sendCommand);

    /**
     * Set sleep timeout in milliseconds
     * @param timeoutMs Time of inactivity before sleep (0 = never sleep)
     */
    void setSleepTimeout(unsigned long timeoutMs);

    /**
     * Add a button that wakes the display
     * @param pin GPIO pin number
     * @param activeLow true if button reads LOW when pressed (default)
     */
    void addWakeButton(uint8_t pin, bool activeLow = true);

    /**
     * Enable wake on mesh state changes
     * @param enable true to wake display when state changes
     */
    void enableWakeOnStateChange(bool enable);

    // ============== Runtime ==============

    /**
     * Update - call this in loop()
     * Polls buttons and checks sleep timeout
     */
    void update();

    /**
     * Reset activity timer - call on any user activity
     * Wakes display if sleeping
     */
    void resetActivity();

    /**
     * Force wake the display
     */
    void wake();

    /**
     * Force sleep the display
     */
    void sleep();

    // ============== State Queries ==============

    /**
     * Check if display is currently sleeping
     * @return true if display is asleep
     */
    bool isAsleep() const;

    /**
     * Get time since last activity in milliseconds
     * @return Idle time in ms
     */
    unsigned long getIdleTime() const;

    // ============== Callbacks ==============

    /**
     * Set callback for when display goes to sleep
     * @param callback Function to call on sleep
     */
    void onSleep(std::function<void()> callback);

    /**
     * Set callback for when display wakes up
     * @param callback Function to call on wake
     */
    void onWake(std::function<void()> callback);

private:
    // Display configuration
    DisplayType _type = DisplayType::NONE;
    Adafruit_SSD1306* _oled = nullptr;
    std::function<void(uint8_t)> _sendTftCommand = nullptr;

    // Sleep commands for TFT displays
    uint8_t _sleepInCmd = 0x10;   // SLPIN
    uint8_t _sleepOutCmd = 0x11;  // SLPOUT
    uint16_t _wakeDelayMs = 120;  // Delay after wake command

    // Timeout configuration
    unsigned long _sleepTimeoutMs = DISPLAY_SLEEP_TIMEOUT_MS;

    // State tracking
    unsigned long _lastActivityMs = 0;
    bool _asleep = false;

    // Wake buttons
    struct WakeButton {
        uint8_t pin = 0;
        bool activeLow = true;
        bool lastState = true;
        unsigned long lastChangeMs = 0;
        bool configured = false;
    };
    WakeButton _wakeButtons[DPM_MAX_WAKE_BUTTONS];
    uint8_t _wakeButtonCount = 0;

    // State change wake
    bool _wakeOnStateChange = DISPLAY_WAKE_ON_STATE_CHANGE;

    // Callbacks
    std::function<void()> _onSleep = nullptr;
    std::function<void()> _onWake = nullptr;

    // Internal methods
    void pollWakeButtons();
    void sendSleepCommand();
    void sendWakeCommand();
};

#endif // DISPLAY_POWER_MANAGER_H
