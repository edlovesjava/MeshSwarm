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

// HTTP Server for gateway API
// Includes: REST API endpoints for remote command protocol (/api/command, /api/nodes)
// Requires: ESPAsyncWebServer, AsyncTCP libraries
// Flash savings when disabled: ~15-20KB
#ifndef MESHSWARM_ENABLE_HTTP_SERVER
#define MESHSWARM_ENABLE_HTTP_SERVER 1
#endif

// ============== FEATURE DEPENDENCY CHECKS ==============

// Note: Callbacks are optional but enhance functionality when enabled with features
// Display and Serial work without callbacks, but callbacks allow customization

// ============== LOG LEVELS ==============
// Control verbosity of serial output to reduce flash usage
// Set to lower level to exclude higher-verbosity messages
//
// MESHSWARM_LOG_NONE  (0) - No logging (maximum flash savings)
// MESHSWARM_LOG_ERROR (1) - Errors only
// MESHSWARM_LOG_WARN  (2) - Errors + Warnings
// MESHSWARM_LOG_INFO  (3) - Errors + Warnings + Info (default)
// MESHSWARM_LOG_DEBUG (4) - All messages including debug

#define MESHSWARM_LOG_NONE   0
#define MESHSWARM_LOG_ERROR  1
#define MESHSWARM_LOG_WARN   2
#define MESHSWARM_LOG_INFO   3
#define MESHSWARM_LOG_DEBUG  4

#ifndef MESHSWARM_LOG_LEVEL
#define MESHSWARM_LOG_LEVEL MESHSWARM_LOG_INFO
#endif

// ============== LOGGING MACROS ==============
// These macros compile to nothing when log level is below threshold
// Saves flash by not including unused format strings

#if MESHSWARM_ENABLE_SERIAL && MESHSWARM_LOG_LEVEL >= MESHSWARM_LOG_ERROR
  #define MESH_LOG_ERROR(fmt, ...) Serial.printf("[ERR] " fmt "\n", ##__VA_ARGS__)
#else
  #define MESH_LOG_ERROR(fmt, ...)
#endif

#if MESHSWARM_ENABLE_SERIAL && MESHSWARM_LOG_LEVEL >= MESHSWARM_LOG_WARN
  #define MESH_LOG_WARN(fmt, ...) Serial.printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#else
  #define MESH_LOG_WARN(fmt, ...)
#endif

#if MESHSWARM_ENABLE_SERIAL && MESHSWARM_LOG_LEVEL >= MESHSWARM_LOG_INFO
  #define MESH_LOG_INFO(fmt, ...) Serial.printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#else
  #define MESH_LOG_INFO(fmt, ...)
#endif

#if MESHSWARM_ENABLE_SERIAL && MESHSWARM_LOG_LEVEL >= MESHSWARM_LOG_DEBUG
  #define MESH_LOG_DEBUG(fmt, ...) Serial.printf("[DBG] " fmt "\n", ##__VA_ARGS__)
#else
  #define MESH_LOG_DEBUG(fmt, ...)
#endif

// Prefixed variants for specific subsystems
#if MESHSWARM_ENABLE_SERIAL && MESHSWARM_LOG_LEVEL >= MESHSWARM_LOG_INFO
  #define MESH_LOG(fmt, ...) Serial.printf("[MESH] " fmt "\n", ##__VA_ARGS__)
  #define STATE_LOG(fmt, ...) Serial.printf("[STATE] " fmt "\n", ##__VA_ARGS__)
  #define TELEM_LOG(fmt, ...) Serial.printf("[TELEM] " fmt "\n", ##__VA_ARGS__)
  #define OTA_LOG(fmt, ...) Serial.printf("[OTA] " fmt "\n", ##__VA_ARGS__)
  #define GATEWAY_LOG(fmt, ...) Serial.printf("[GATEWAY] " fmt "\n", ##__VA_ARGS__)
  #define CMD_LOG(fmt, ...) Serial.printf("[CMD] " fmt "\n", ##__VA_ARGS__)
  #define HTTP_LOG(fmt, ...) Serial.printf("[HTTP] " fmt "\n", ##__VA_ARGS__)
#else
  #define MESH_LOG(fmt, ...)
  #define STATE_LOG(fmt, ...)
  #define TELEM_LOG(fmt, ...)
  #define OTA_LOG(fmt, ...)
  #define GATEWAY_LOG(fmt, ...)
  #define CMD_LOG(fmt, ...)
  #define HTTP_LOG(fmt, ...)
#endif

#if MESHSWARM_ENABLE_SERIAL && MESHSWARM_LOG_LEVEL >= MESHSWARM_LOG_DEBUG
  #define MESH_LOG_D(fmt, ...) Serial.printf("[MESH] " fmt "\n", ##__VA_ARGS__)
  #define STATE_LOG_D(fmt, ...) Serial.printf("[STATE] " fmt "\n", ##__VA_ARGS__)
  #define TELEM_LOG_D(fmt, ...) Serial.printf("[TELEM] " fmt "\n", ##__VA_ARGS__)
  #define OTA_LOG_D(fmt, ...) Serial.printf("[OTA] " fmt "\n", ##__VA_ARGS__)
  #define CMD_LOG_D(fmt, ...) Serial.printf("[CMD] " fmt "\n", ##__VA_ARGS__)
  #define HTTP_LOG_D(fmt, ...) Serial.printf("[HTTP] " fmt "\n", ##__VA_ARGS__)
#else
  #define MESH_LOG_D(fmt, ...)
  #define STATE_LOG_D(fmt, ...)
  #define TELEM_LOG_D(fmt, ...)
  #define OTA_LOG_D(fmt, ...)
  #define CMD_LOG_D(fmt, ...)
  #define HTTP_LOG_D(fmt, ...)
#endif

// ============== COMPILE-TIME INFORMATION ==============

#if !MESHSWARM_ENABLE_DISPLAY && !MESHSWARM_ENABLE_SERIAL && !MESHSWARM_ENABLE_TELEMETRY && !MESHSWARM_ENABLE_OTA
#pragma message "MeshSwarm: Core-only build (minimal flash usage)"
#elif !MESHSWARM_ENABLE_TELEMETRY && !MESHSWARM_ENABLE_OTA
#pragma message "MeshSwarm: Basic build (Display + Serial)"
#endif

#if MESHSWARM_LOG_LEVEL == MESHSWARM_LOG_NONE
#pragma message "MeshSwarm: Logging disabled (maximum flash savings)"
#elif MESHSWARM_LOG_LEVEL == MESHSWARM_LOG_ERROR
#pragma message "MeshSwarm: Error-only logging"
#endif

#endif // MESHSWARM_CONFIG_H
