# Persistent Node Configuration

**Status**: Proposed
**Priority**: High
**Created**: December 2024

## Overview

Implement a persistent configuration system that stores node-specific settings in non-volatile memory (NVS), allowing configuration to survive reboots and OTA firmware updates.

## Problem Statement

MeshSwarm's OTA system distributes firmware by **role** (SensorNode, LedNode, GatewayNode, etc.). However, individual nodes have unique configuration that is NOT part of the firmware:

- Custom node names (e.g., "Kitchen Sensor" vs auto-generated "N1234")
- Room/zone assignments for home automation
- Device-specific parameters (sensor calibration, thresholds)
- Feature toggles and preferences
- Credentials or API keys for integrations

**Current behavior:** All of this configuration is lost when:
- The node reboots
- An OTA update is applied
- The node loses power

This forces users to either:
1. Hardcode config in firmware (defeats purpose of OTA)
2. Manually reconfigure after every update
3. Build complex external config management systems

## Goals

1. Store key-value configuration in ESP32 NVS (survives reboot + OTA)
2. Provide simple API for reading/writing config
3. Integrate with Remote Command Protocol for remote configuration
4. Support config backup/restore for disaster recovery
5. Maintain backward compatibility with existing code

## Non-Goals

- Large file storage (use LittleFS for that)
- Encrypted storage (may add later)
- Config synchronization across nodes (use shared state for that)
- Schema validation (keep it simple)

---

## Architecture

### Storage Layer

```
┌─────────────────────────────────────────────────────────┐
│                    ESP32 Flash                          │
├─────────────────────────────────────────────────────────┤
│  NVS Partition (~20KB default)                          │
│  ┌───────────────────────────────────────────────────┐  │
│  │  Namespace: "meshswarm"                           │  │
│  │  ┌─────────────┬─────────────────────────────┐   │  │
│  │  │ Key         │ Value                       │   │  │
│  │  ├─────────────┼─────────────────────────────┤   │  │
│  │  │ name        │ "Kitchen Sensor"            │   │  │
│  │  │ room        │ "kitchen"                   │   │  │
│  │  │ zone        │ "downstairs"                │   │  │
│  │  │ temp_offset │ "-1.5"                      │   │  │
│  │  │ led_bright  │ "80"                        │   │  │
│  │  └─────────────┴─────────────────────────────┘   │  │
│  └───────────────────────────────────────────────────┘  │
│                                                         │
│  App Partition (Firmware - replaced on OTA)             │
│  OTA Partition (Staging area)                           │
└─────────────────────────────────────────────────────────┘
```

### Integration with MeshSwarm

```
┌─────────────────────────────────────────────────────────┐
│                     MeshSwarm                           │
│                                                         │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │
│  │   Config    │  │   State     │  │   Remote    │     │
│  │   Store     │  │   Sync      │  │   Command   │     │
│  │  (NVS)      │  │  (Mesh)     │  │  Protocol   │     │
│  └──────┬──────┘  └─────────────┘  └──────┬──────┘     │
│         │                                  │            │
│         │         config commands          │            │
│         └──────────────────────────────────┘            │
│                                                         │
│  Config: Local to node, persists across reboots        │
│  State:  Shared across mesh, volatile                  │
└─────────────────────────────────────────────────────────┘
```

### Config vs State

| Aspect | Config (NVS) | State (Mesh) |
|--------|--------------|--------------|
| **Scope** | Local to node | Shared across mesh |
| **Persistence** | Survives reboot/OTA | Volatile (lost on reboot) |
| **Purpose** | Node identity & settings | Runtime data & coordination |
| **Examples** | name, room, calibration | temperature, led_status |
| **Updates** | Infrequent, manual | Frequent, automatic |

---

## Public API

### Core Methods

```cpp
class MeshSwarm {
public:
  // === Configuration Storage ===

  // Store a config value (persists to NVS)
  bool setConfig(const String& key, const String& value);

  // Retrieve a config value (with optional default)
  String getConfig(const String& key, const String& defaultValue = "");

  // Check if a config key exists
  bool hasConfig(const String& key);

  // Remove a config entry
  bool removeConfig(const String& key);

  // Get all config keys
  std::vector<String> getConfigKeys();

  // Clear all config (factory reset)
  bool clearConfig();

  // === Convenience Methods ===

  // Get config as specific types
  int getConfigInt(const String& key, int defaultValue = 0);
  float getConfigFloat(const String& key, float defaultValue = 0.0f);
  bool getConfigBool(const String& key, bool defaultValue = false);

  // Set config with type conversion
  bool setConfigInt(const String& key, int value);
  bool setConfigFloat(const String& key, float value);
  bool setConfigBool(const String& key, bool value);

  // === Bulk Operations ===

  // Export all config as JSON string
  String exportConfig();

  // Import config from JSON string (merges with existing)
  bool importConfig(const String& json);

  // Import config, replacing all existing
  bool importConfigReplace(const String& json);
};
```

### Usage Examples

```cpp
#include <MeshSwarm.h>

MeshSwarm swarm;

void setup() {
  swarm.begin("MySensor");

  // Load custom name from config (or use default)
  String customName = swarm.getConfig("name", swarm.getNodeName());

  // Check if this is first boot
  if (!swarm.hasConfig("initialized")) {
    // First time setup
    swarm.setConfig("initialized", "true");
    swarm.setConfig("room", "unknown");
    Serial.println("First boot - please configure via remote command");
  }

  // Load calibration
  float tempOffset = swarm.getConfigFloat("temp_offset", 0.0);
  int reportInterval = swarm.getConfigInt("report_interval", 30000);

  Serial.printf("Node: %s, Room: %s\n",
    customName.c_str(),
    swarm.getConfig("room", "unassigned").c_str());
}

void loop() {
  swarm.update();

  // Read sensor with calibration offset
  float rawTemp = readSensor();
  float calibratedTemp = rawTemp + swarm.getConfigFloat("temp_offset", 0.0);

  swarm.setState("temperature", String(calibratedTemp));
}
```

### Serial Commands

Extend existing serial interface:

```
config                    List all config keys and values
config get <key>          Get specific config value
config set <key> <value>  Set config value
config remove <key>       Remove config entry
config clear              Clear all config (factory reset)
config export             Export config as JSON
config import <json>      Import config from JSON
```

---

## Remote Command Integration

### New Commands

Integrate with the Remote Command Protocol (when implemented):

| Command | Args | Response | Description |
|---------|------|----------|-------------|
| `config` | - | `{keys: [...], count: N}` | List all config keys |
| `config_get` | `{key}` | `{key, value, found}` | Get config value |
| `config_set` | `{key, value}` | `{key, value, success}` | Set config value |
| `config_remove` | `{key}` | `{key, removed}` | Remove config entry |
| `config_export` | - | `{config: {...}}` | Export all config |
| `config_import` | `{config: {...}}` | `{imported, count}` | Import config |
| `config_clear` | `{confirm: true}` | `{cleared}` | Factory reset |

### Example Remote Interactions

**List all config:**
```json
// Request
{"target": "N1234", "command": "config"}

// Response
{
  "success": true,
  "result": {
    "keys": ["name", "room", "zone", "temp_offset"],
    "count": 4
  }
}
```

**Get specific config:**
```json
// Request
{"target": "N1234", "command": "config_get", "args": {"key": "room"}}

// Response
{
  "success": true,
  "result": {
    "key": "room",
    "value": "kitchen",
    "found": true
  }
}
```

**Set config remotely:**
```json
// Request
{"target": "N1234", "command": "config_set", "args": {"key": "room", "value": "bedroom"}}

// Response
{
  "success": true,
  "result": {
    "key": "room",
    "value": "bedroom",
    "success": true
  }
}
```

**Bulk configure a new node:**
```json
// Request
{
  "target": "N1234",
  "command": "config_import",
  "args": {
    "config": {
      "name": "Bedroom Sensor",
      "room": "bedroom",
      "zone": "upstairs",
      "temp_offset": "-0.5",
      "report_interval": "60000"
    }
  }
}

// Response
{
  "success": true,
  "result": {
    "imported": true,
    "count": 5
  }
}
```

---

## Implementation Details

### NVS Integration

Use ESP32 Preferences library (wrapper around NVS):

```cpp
#include <Preferences.h>

class MeshSwarm {
private:
  Preferences configStore;
  bool configInitialized;

  void initConfig() {
    if (!configInitialized) {
      configStore.begin("meshswarm", false);  // RW mode
      configInitialized = true;
    }
  }

public:
  bool setConfig(const String& key, const String& value) {
    initConfig();
    size_t written = configStore.putString(key.c_str(), value);
    return written > 0;
  }

  String getConfig(const String& key, const String& defaultValue) {
    initConfig();
    return configStore.getString(key.c_str(), defaultValue);
  }

  bool hasConfig(const String& key) {
    initConfig();
    return configStore.isKey(key.c_str());
  }

  bool removeConfig(const String& key) {
    initConfig();
    return configStore.remove(key.c_str());
  }

  bool clearConfig() {
    initConfig();
    return configStore.clear();
  }
};
```

### Getting All Keys

NVS doesn't have a built-in "list keys" function. Options:

**Option A: Maintain key index**
```cpp
// Store list of keys as comma-separated string
void updateKeyIndex(const String& key, bool adding) {
  String index = configStore.getString("_keys", "");
  // Add or remove key from index
  configStore.putString("_keys", newIndex);
}
```

**Option B: Use NVS iterator API (ESP-IDF)**
```cpp
std::vector<String> getConfigKeys() {
  std::vector<String> keys;
  nvs_iterator_t it = nvs_entry_find("nvs", "meshswarm", NVS_TYPE_STR);
  while (it != NULL) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    keys.push_back(String(info.key));
    it = nvs_entry_next(it);
  }
  return keys;
}
```

**Recommendation:** Option B is cleaner but requires ESP-IDF functions. Option A is more portable.

### Export/Import Format

JSON format for config export/import:

```json
{
  "_version": 1,
  "_node": "N1234",
  "_exported": "2024-12-22T10:30:00Z",
  "config": {
    "name": "Kitchen Sensor",
    "room": "kitchen",
    "zone": "downstairs",
    "temp_offset": "-1.5"
  }
}
```

---

## Reserved Keys

Some keys have special meaning:

| Key | Purpose | Set By |
|-----|---------|--------|
| `_version` | Config schema version | System |
| `_keys` | Index of all keys (if using Option A) | System |
| `name` | Custom node name (overrides auto-generated) | User |
| `room` | Room assignment | User |
| `zone` | Zone assignment | User |
| `role` | Node role hint | User/System |

User keys should avoid underscore prefix.

---

## Feature Flag

Add new feature flag for conditional compilation:

```cpp
// MeshSwarmConfig.h
#ifndef MESHSWARM_ENABLE_CONFIG
#define MESHSWARM_ENABLE_CONFIG 1
#endif
```

Disabling saves ~3-5KB flash (Preferences library overhead).

---

## Implementation Plan

### Phase 1: Core Storage
- [ ] Add `Preferences` include and member variable
- [ ] Implement `initConfig()` private method
- [ ] Implement `setConfig()` / `getConfig()`
- [ ] Implement `hasConfig()` / `removeConfig()`
- [ ] Implement `clearConfig()`
- [ ] Add feature flag `MESHSWARM_ENABLE_CONFIG`

### Phase 2: Convenience Methods
- [ ] Add typed getters: `getConfigInt()`, `getConfigFloat()`, `getConfigBool()`
- [ ] Add typed setters: `setConfigInt()`, `setConfigFloat()`, `setConfigBool()`
- [ ] Implement `getConfigKeys()` (with key index)

### Phase 3: Serial Commands
- [ ] Add `config` command to serial handler
- [ ] Implement subcommands: get, set, remove, clear
- [ ] Add export/import commands

### Phase 4: Remote Commands
- [ ] Add config commands to Remote Command Protocol
- [ ] Implement `config`, `config_get`, `config_set`
- [ ] Implement `config_remove`, `config_clear`
- [ ] Implement `config_export`, `config_import`

### Phase 5: Integration
- [ ] Use config for node name in `begin()` if set
- [ ] Add config backup to mesh state (optional)
- [ ] Create example: ConfigurableNode
- [ ] Update documentation

---

## Testing Plan

### Unit Tests
1. Set/get string values
2. Set/get typed values (int, float, bool)
3. Default values when key missing
4. Remove key
5. Clear all config
6. Key listing
7. Export/import JSON

### Integration Tests
1. Config survives reboot
2. Config survives OTA update
3. Serial commands work correctly
4. Remote commands work correctly
5. Multiple nodes with different configs

### Edge Cases
1. Very long keys (max 15 chars in NVS)
2. Very long values (max 4000 bytes in NVS)
3. Special characters in values
4. Unicode in values
5. NVS full condition
6. Concurrent access (not applicable - single threaded)

---

## Migration Path

For existing deployments:

1. **New nodes**: Config system available immediately after library update
2. **Existing nodes**: No config exists, use defaults
3. **Hardcoded values**: Gradually migrate to config-based
4. **Bulk provisioning**: Use remote commands to configure fleet

---

## Security Considerations

1. **No encryption**: Config stored in plaintext in NVS
   - Don't store sensitive secrets without additional encryption
   - Consider adding encrypted config option in future

2. **Remote access**: Config commands should respect ACL (when implemented)
   - `config_clear` requires confirmation flag
   - Consider read-only mode for certain deployments

3. **Validation**: No schema validation
   - Application code should validate config values
   - Invalid config should fall back to defaults

---

## Future Enhancements

1. **Encrypted config**: Store sensitive values encrypted
2. **Config schemas**: Define expected keys and types
3. **Config versioning**: Handle schema migrations
4. **Config sync**: Optional backup to cloud/gateway
5. **Config profiles**: Switch between config sets

---

## References

- [ESP32 Preferences Library](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html)
- [ESP-IDF NVS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)
- [Remote Command Protocol PRD](remote-command-protocol.md)
- [MeshSwarm Library](../README.md)
