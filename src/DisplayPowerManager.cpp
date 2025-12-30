/**
 * DisplayPowerManager - Implementation
 */

#include "DisplayPowerManager.h"
#include <Adafruit_SSD1306.h>

DisplayPowerManager::DisplayPowerManager() {
    // Initialize wake buttons array
    for (uint8_t i = 0; i < DPM_MAX_WAKE_BUTTONS; i++) {
        _wakeButtons[i].configured = false;
    }
}

// ============== Configuration ==============

void DisplayPowerManager::begin(DisplayType type) {
    _type = type;
    _lastActivityMs = millis();
    _asleep = false;

    // Set TFT-specific commands based on display type
    switch (type) {
        case DisplayType::GC9A01_TFT:
        case DisplayType::ILI9341_TFT:
            _sleepInCmd = 0x10;   // SLPIN
            _sleepOutCmd = 0x11;  // SLPOUT
            _wakeDelayMs = 120;
            break;
        default:
            break;
    }

    Serial.printf("[DPM] Initialized, type=%d, timeout=%lu ms\n",
                  static_cast<int>(type), _sleepTimeoutMs);
}

void DisplayPowerManager::setOledDisplay(Adafruit_SSD1306* display) {
    _oled = display;
}

void DisplayPowerManager::setTftCommandCallback(std::function<void(uint8_t)> sendCommand) {
    _sendTftCommand = sendCommand;
}

void DisplayPowerManager::setSleepTimeout(unsigned long timeoutMs) {
    _sleepTimeoutMs = timeoutMs;
    Serial.printf("[DPM] Sleep timeout set to %lu ms\n", timeoutMs);
}

void DisplayPowerManager::addWakeButton(uint8_t pin, bool activeLow) {
    if (_wakeButtonCount >= DPM_MAX_WAKE_BUTTONS) {
        Serial.println("[DPM] ERROR: Max wake buttons reached");
        return;
    }

    // Configure pin
    pinMode(pin, INPUT_PULLUP);

    // Store button config
    WakeButton& btn = _wakeButtons[_wakeButtonCount];
    btn.pin = pin;
    btn.activeLow = activeLow;
    btn.lastState = digitalRead(pin);
    btn.lastChangeMs = millis();
    btn.configured = true;

    _wakeButtonCount++;
    Serial.printf("[DPM] Wake button added on GPIO%d (activeLow=%d)\n", pin, activeLow);
}

void DisplayPowerManager::enableWakeOnStateChange(bool enable) {
    _wakeOnStateChange = enable;
    Serial.printf("[DPM] Wake on state change: %s\n", enable ? "enabled" : "disabled");
}

// ============== Runtime ==============

void DisplayPowerManager::update() {
    // Only run if display manager was initialized
    if (_type == DisplayType::NONE) {
        return;
    }

    // Poll wake buttons
    pollWakeButtons();

    // Check sleep timeout
    if (!_asleep && _sleepTimeoutMs > 0) {
        if (millis() - _lastActivityMs > _sleepTimeoutMs) {
            sleep();
        }
    }
}

void DisplayPowerManager::resetActivity() {
    _lastActivityMs = millis();

    // Wake if sleeping
    if (_asleep) {
        wake();
    }
}

void DisplayPowerManager::wake() {
    if (!_asleep) {
        // Not sleeping, just reset timer
        _lastActivityMs = millis();
        return;
    }

    _asleep = false;
    _lastActivityMs = millis();

    sendWakeCommand();

    Serial.println("[DPM] Display woke up");

    // Call callback
    if (_onWake) {
        _onWake();
    }
}

void DisplayPowerManager::sleep() {
    if (_asleep) {
        return;
    }

    _asleep = true;

    sendSleepCommand();

    Serial.println("[DPM] Display sleeping");

    // Call callback
    if (_onSleep) {
        _onSleep();
    }
}

// ============== State Queries ==============

bool DisplayPowerManager::isAsleep() const {
    return _asleep;
}

unsigned long DisplayPowerManager::getIdleTime() const {
    return millis() - _lastActivityMs;
}

// ============== Callbacks ==============

void DisplayPowerManager::onSleep(std::function<void()> callback) {
    _onSleep = callback;
}

void DisplayPowerManager::onWake(std::function<void()> callback) {
    _onWake = callback;
}

// ============== Internal Methods ==============

void DisplayPowerManager::pollWakeButtons() {
    unsigned long now = millis();

    for (uint8_t i = 0; i < _wakeButtonCount; i++) {
        WakeButton& btn = _wakeButtons[i];
        if (!btn.configured) continue;

        bool currentState = digitalRead(btn.pin);

        // Check for state change with debounce
        if (currentState != btn.lastState) {
            if (now - btn.lastChangeMs > DPM_DEBOUNCE_MS) {
                btn.lastChangeMs = now;
                btn.lastState = currentState;

                // Check if button is pressed
                bool pressed = btn.activeLow ? (currentState == LOW) : (currentState == HIGH);

                if (pressed) {
                    // Wake display on button press
                    resetActivity();
                }
            }
        }
    }
}

void DisplayPowerManager::sendSleepCommand() {
    switch (_type) {
        case DisplayType::SSD1306_OLED:
            if (_oled) {
                _oled->ssd1306_command(SSD1306_DISPLAYOFF);
            }
            break;

        case DisplayType::GC9A01_TFT:
        case DisplayType::ILI9341_TFT:
            if (_sendTftCommand) {
                _sendTftCommand(_sleepInCmd);
            }
            break;

        default:
            break;
    }
}

void DisplayPowerManager::sendWakeCommand() {
    switch (_type) {
        case DisplayType::SSD1306_OLED:
            if (_oled) {
                _oled->ssd1306_command(SSD1306_DISPLAYON);
            }
            break;

        case DisplayType::GC9A01_TFT:
        case DisplayType::ILI9341_TFT:
            if (_sendTftCommand) {
                _sendTftCommand(_sleepOutCmd);
                delay(_wakeDelayMs);  // Wait for display to wake
            }
            break;

        default:
            break;
    }
}
