/*
 * MeshSwarm Library - Configuration and Feature Flags
 *
 * This file controls which features are compiled into the library.
 * All features are enabled by default for backward compatibility.
 *
 * To disable a feature, define it as 0 before including MeshSwarm.h:
 *
 *   #define MESHSWARM_ENABLE_DISPLAY 0
 *   #include <MeshSwarm.h>
 *
 * Or add to build_flags in platformio.ini:
 *   -DMESHSWARM_ENABLE_DISPLAY=0
 */

#ifndef MESHSWARM_CONFIG_H
#define MESHSWARM_CONFIG_H

// ============== FEATURE FLAGS ==============
// Set to 0 to disable a feature and reduce flash memory usage

// OLED display support (Adafruit SSD1306)
// Includes: Display initialization, rendering, update loop, custom display handlers
// Flash savings when disabled: ~10-15KB
#ifndef MESHSWARM_ENABLE_DISPLAY
#define MESHSWARM_ENABLE_DISPLAY 1
#endif

// Serial command interface
// Includes: Command processing (status, peers, state, set, get, sync, reboot, scan)
// Custom serial command handlers
// Flash savings when disabled: ~8-12KB
#ifndef MESHSWARM_ENABLE_SERIAL
#define MESHSWARM_ENABLE_SERIAL 1
#endif

// HTTP telemetry and gateway mode
// Includes: Telemetry pushing to HTTP server, WiFi station mode, gateway relay
// Telemetry batching and debouncing
// Flash savings when disabled: ~12-18KB
#ifndef MESHSWARM_ENABLE_TELEMETRY
#define MESHSWARM_ENABLE_TELEMETRY 1
#endif

// OTA firmware distribution
// Includes: OTA update polling, firmware download/distribution, progress reporting
// OTA reception for nodes
// Flash savings when disabled: ~15-20KB
#ifndef MESHSWARM_ENABLE_OTA
#define MESHSWARM_ENABLE_OTA 1
#endif

// Custom callback hooks
// Includes: onLoop, onSerialCommand, onDisplayUpdate callbacks
// Flash savings when disabled: ~3-5KB
#ifndef MESHSWARM_ENABLE_CALLBACKS
#define MESHSWARM_ENABLE_CALLBACKS 1
#endif

// ============== FEATURE DEPENDENCY CHECKS ==============

// Note: Callbacks are optional but enhance functionality when enabled with features
// Display and Serial work without callbacks, but callbacks allow customization

// ============== COMPILE-TIME INFORMATION ==============

#if !MESHSWARM_ENABLE_DISPLAY && !MESHSWARM_ENABLE_SERIAL && !MESHSWARM_ENABLE_TELEMETRY && !MESHSWARM_ENABLE_OTA
#pragma message "MeshSwarm: Core-only build (minimal flash usage)"
#elif !MESHSWARM_ENABLE_TELEMETRY && !MESHSWARM_ENABLE_OTA
#pragma message "MeshSwarm: Basic build (Display + Serial)"
#endif

#endif // MESHSWARM_CONFIG_H
