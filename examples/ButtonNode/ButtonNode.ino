/**
 * ButtonNode - Button input with shared state and display sleep/wake
 *
 * Demonstrates:
 * - Reading button input
 * - Publishing state to mesh
 * - Using onLoop callback
 * - Display sleep after timeout (using DisplayPowerManager)
 * - Instant wake on button press
 *
 * Hardware:
 * - ESP32
 * - SSD1306 OLED (I2C: SDA=21, SCL=22)
 * - Momentary button on GPIO0 (pulled high, active low)
 */

#include <MeshSwarm.h>

MeshSwarm swarm;

// ============== BUTTON CONFIGURATION ==============
#ifndef BUTTON_PIN
#define BUTTON_PIN 0
#endif

#ifndef DEBOUNCE_MS
#define DEBOUNCE_MS 50
#endif

// ============== DISPLAY SLEEP CONFIGURATION ==============
#ifndef DISPLAY_SLEEP_TIMEOUT_MS
#define DISPLAY_SLEEP_TIMEOUT_MS 15000  // Sleep after 15 seconds of inactivity
#endif

#ifndef DISPLAY_WAKE_ON_STATE_CHANGE
#define DISPLAY_WAKE_ON_STATE_CHANGE 0  // Default: don't wake on state change
#endif

// ============== BUTTON STATE ==============
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;

// ============== BUTTON HANDLING ==============
void handleButton() {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_MS) {
    static bool buttonState = HIGH;
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == LOW) {
        // Reset activity on button press (wakes display if sleeping)
        swarm.getPowerManager().resetActivity();

        // Publish button state to mesh
        swarm.setState("button", "1");
        Serial.println("Button: pressed");
      } else {
        swarm.setState("button", "0");
        Serial.println("Button: released");
      }
    }
  }

  lastButtonState = reading;
}

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  swarm.begin("Button");

  // Enable display sleep with timeout
  swarm.enableDisplaySleep(DISPLAY_SLEEP_TIMEOUT_MS);

  // Add button as wake source (in addition to handleButton's resetActivity)
  swarm.addDisplayWakeButton(BUTTON_PIN);

  // Register button handling
  swarm.onLoop(handleButton);

  // Optional: Wake on state changes
  #if DISPLAY_WAKE_ON_STATE_CHANGE
  swarm.getPowerManager().enableWakeOnStateChange(true);
  swarm.watchState("*", [](const String& key, const String& value, const String& oldValue) {
    swarm.getPowerManager().resetActivity();
  });
  #endif

  Serial.println("ButtonNode started");
  Serial.printf("[CONFIG] Display sleep timeout: %d ms\n", DISPLAY_SLEEP_TIMEOUT_MS);
  Serial.printf("[CONFIG] Wake on state change: %s\n", DISPLAY_WAKE_ON_STATE_CHANGE ? "enabled" : "disabled");
}

// ============== MAIN LOOP ==============
void loop() {
  swarm.update();
}
