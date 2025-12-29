/*
 * MeshSwarm Library - Display Module
 * 
 * OLED display functionality for MeshSwarm nodes.
 * Only compiled when MESHSWARM_ENABLE_DISPLAY is enabled.
 */

#include "../MeshSwarm.h"

#if MESHSWARM_ENABLE_DISPLAY

// ============== DISPLAY INITIALIZATION ==============
void MeshSwarm::initDisplay() {
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
#if MESHSWARM_ENABLE_SERIAL
    Serial.println("[OLED] Init failed!");
#endif
  } else {
#if MESHSWARM_ENABLE_SERIAL
    Serial.println("[OLED] Initialized");
#endif
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Mesh Swarm");
    display.println("Starting...");
    display.display();
  }
}

// ============== DISPLAY UPDATE ==============
void MeshSwarm::updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  // Line 1: Identity
  uint32_t uptime = (millis() - bootTime) / 1000;
  display.printf("%s [%s] %d:%02d\n", myName.c_str(), myRole.c_str(), uptime/60, uptime%60);

  // Line 2: Network
  display.printf("Peers:%d States:%d\n", getPeerCount(), sharedState.size());

  // Line 3: Custom status or separator
  if (customStatus.length() > 0) {
    display.println(customStatus.substring(0, 21));
  } else {
    display.println("---------------------");
  }

#if MESHSWARM_ENABLE_CALLBACKS
  // Call custom display handlers (lines 4+)
  int startLine = 3;
  for (auto& handler : displayHandlers) {
    handler(display, startLine);
  }

  // If no custom handlers, show state values
  if (displayHandlers.empty()) {
#endif
    // Lines 4-7: State values (up to 4)
    int shown = 0;
    for (auto& kv : sharedState) {
      if (shown >= 4) break;
      String line = kv.first + "=" + kv.second.value;
      if (line.length() > 21) line = line.substring(0, 21);
      display.println(line);
      shown++;
    }

    while (shown < 4) {
      display.println();
      shown++;
    }

    // Line 8: Last change
    if (lastStateChange.length() > 0) {
      display.printf("Last:%s\n", lastStateChange.substring(0, 16).c_str());
    }
#if MESHSWARM_ENABLE_CALLBACKS
  }
#endif

  display.display();
}

// ============== DISPLAY CUSTOMIZATION ==============
void MeshSwarm::setStatusLine(const String& status) {
  customStatus = status;
}

// ============== DISPLAY POWER MANAGEMENT ==============
void MeshSwarm::enableDisplaySleep(unsigned long timeoutMs) {
  powerManager.setOledDisplay(&display);
  powerManager.begin(DisplayType::SSD1306_OLED);
  powerManager.setSleepTimeout(timeoutMs);
}

void MeshSwarm::addDisplayWakeButton(uint8_t pin, bool activeLow) {
  powerManager.addWakeButton(pin, activeLow);
}

#endif // MESHSWARM_ENABLE_DISPLAY
