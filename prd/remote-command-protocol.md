# Remote Command Protocol

**Status**: Proposed
**Priority**: High
**Created**: December 2024

## Overview

Implement a remote command protocol that enables nodes to send commands to other nodes through the mesh network, and allows external applications to control nodes via the gateway's HTTP API.

## Problem Statement

Currently, serial console commands (`status`, `peers`, `state`, `set/get`, etc.) only work locally on the node you're physically connected to. The `MSG_COMMAND` message type exists in the codebase but is not implemented.

**Current limitations:**
- No way to send commands to a specific node through the mesh
- No way to query a remote node's status
- No way to control nodes from external apps via the gateway
- Displays and controllers cannot communicate commands to each other

## Goals

1. Enable any node to send commands to any other node in the mesh
2. Provide HTTP API on gateway for external application control
3. Support request/response correlation for reliable command execution
4. Mirror existing serial console commands for consistency
5. Enable extensibility for custom node-specific commands

## Non-Goals

- Real-time streaming (use state synchronization instead)
- File transfer (handled by OTA system)
- Replacing state synchronization for data sharing

---

## Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         EXTERNAL                                │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐                  │
│  │ Web App  │    │ Mobile   │    │ Home     │                  │
│  │          │    │ App      │    │ Assistant│                  │
│  └────┬─────┘    └────┬─────┘    └────┬─────┘                  │
│       │               │               │                         │
│       └───────────────┼───────────────┘                         │
│                       │ HTTP/REST                               │
└───────────────────────┼─────────────────────────────────────────┘
                        ▼
┌───────────────────────────────────────────────────────────────┐
│                      GATEWAY NODE                              │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐       │
│  │ HTTP Server │───▶│ Command     │───▶│ Response    │       │
│  │ /api/cmd    │    │ Router      │    │ Collector   │       │
│  └─────────────┘    └──────┬──────┘    └─────────────┘       │
└────────────────────────────┼──────────────────────────────────┘
                             │ Mesh Broadcast/Unicast
                             ▼
┌───────────────────────────────────────────────────────────────┐
│                        MESH NETWORK                            │
│                                                                │
│  ┌─────────┐      ┌─────────┐      ┌─────────┐               │
│  │ Sensor  │◀────▶│ Display │◀────▶│ Control │               │
│  │ Node    │      │ Node    │      │ Node    │               │
│  └─────────┘      └─────────┘      └─────────┘               │
│       ▲                                  │                     │
│       │          Node-to-Node            │                     │
│       └──────────Commands────────────────┘                     │
└───────────────────────────────────────────────────────────────┘
```

### Message Flow

**External App → Node:**
```
1. App sends HTTP POST to gateway /api/command
2. Gateway creates MSG_COMMAND with unique request ID
3. Gateway sends to mesh (unicast to target or broadcast)
4. Target node(s) execute command
5. Target node(s) send MSG_COMMAND_RESPONSE
6. Gateway collects responses (with timeout)
7. Gateway returns aggregated response to HTTP caller
```

**Node → Node:**
```
1. Source node calls sendCommand(target, cmd, args, callback)
2. Library creates MSG_COMMAND with request ID
3. Message sent via mesh (unicast or broadcast)
4. Target node executes command
5. Target node sends MSG_COMMAND_RESPONSE
6. Source node's callback invoked with result
```

---

## Message Formats

### MSG_COMMAND (Type 5)

```json
{
  "t": 5,
  "n": "sender_name",
  "d": {
    "cmd": "status",
    "target": "N1234",
    "rid": "abc123",
    "args": {
      "key": "value"
    }
  }
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `cmd` | string | Yes | Command name |
| `target` | string | Yes | Target node name, node ID, or `"*"` for broadcast |
| `rid` | string | Yes | Request ID for response correlation |
| `args` | object | No | Command-specific arguments |

### MSG_COMMAND_RESPONSE (Type 7 - New)

```json
{
  "t": 7,
  "n": "responder_name",
  "d": {
    "rid": "abc123",
    "success": true,
    "error": null,
    "result": {
      "id": 12345678,
      "name": "N1234",
      "role": "PEER",
      "heap": 45000,
      "uptime": 3600
    }
  }
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `rid` | string | Yes | Matching request ID |
| `success` | bool | Yes | Whether command succeeded |
| `error` | string | No | Error message if failed |
| `result` | object | No | Command-specific result data |

---

## Core Commands

### status

Get node status information.

**Args:** None

**Response:**
```json
{
  "id": 12345678,
  "name": "N1234",
  "role": "COORD",
  "heap": 45000,
  "uptime": 3600,
  "peers": 5,
  "states": 12,
  "version": "1.0.0"
}
```

### peers

List known peers.

**Args:** None

**Response:**
```json
{
  "peers": [
    { "id": 11111111, "name": "N1111", "role": "PEER", "alive": true },
    { "id": 22222222, "name": "N2222", "role": "COORD", "alive": true }
  ]
}
```

### state

Get all shared state.

**Args:** None

**Response:**
```json
{
  "state": {
    "temp": { "value": "23.5", "version": 5, "origin": "N1234" },
    "led": { "value": "on", "version": 2, "origin": "N5678" }
  }
}
```

### get

Get specific state key.

**Args:**
```json
{ "key": "temp" }
```

**Response:**
```json
{
  "key": "temp",
  "value": "23.5",
  "version": 5,
  "origin": "N1234",
  "found": true
}
```

### set

Set state key (triggers mesh synchronization).

**Args:**
```json
{ "key": "led", "value": "on" }
```

**Response:**
```json
{
  "key": "led",
  "value": "on",
  "changed": true
}
```

### sync

Force broadcast of full state.

**Args:** None

**Response:**
```json
{ "ack": true }
```

### reboot

Restart the node.

**Args:** None

**Response:**
```json
{ "ack": true, "rebooting": true }
```

*Note: Response sent before reboot occurs.*

### info

Get node capabilities and configuration.

**Args:** None

**Response:**
```json
{
  "version": "1.0.0",
  "hardware": "ESP32",
  "features": {
    "display": true,
    "serial": true,
    "telemetry": false,
    "ota": true,
    "callbacks": true
  },
  "mesh": {
    "prefix": "swarm",
    "port": 5555
  }
}
```

### ping

Simple connectivity test.

**Args:** None

**Response:**
```json
{ "pong": true, "time": 1234567890 }
```

---

## Gateway HTTP API

### POST /api/command

Send a command to one or more nodes.

**Request:**
```json
{
  "target": "N1234",
  "command": "status",
  "args": {},
  "timeout": 5000
}
```

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `target` | string | Yes | - | Node name, ID, or `"*"` for all |
| `command` | string | Yes | - | Command to execute |
| `args` | object | No | `{}` | Command arguments |
| `timeout` | int | No | 5000 | Response timeout in ms |

**Response (Success):**
```json
{
  "request_id": "abc123",
  "target": "N1234",
  "command": "status",
  "responses": [
    {
      "node": "N1234",
      "success": true,
      "result": { "id": 12345678, "name": "N1234", "role": "PEER" }
    }
  ],
  "timeout_nodes": []
}
```

**Response (Broadcast):**
```json
{
  "request_id": "def456",
  "target": "*",
  "command": "status",
  "responses": [
    { "node": "N1234", "success": true, "result": {...} },
    { "node": "N5678", "success": true, "result": {...} }
  ],
  "timeout_nodes": ["N9999"]
}
```

**Response (Error):**
```json
{
  "error": "Invalid command",
  "code": 400
}
```

### GET /api/nodes

List all known nodes in the mesh.

**Response:**
```json
{
  "nodes": [
    { "id": 12345678, "name": "N1234", "role": "COORD", "alive": true },
    { "id": 87654321, "name": "N8765", "role": "PEER", "alive": true }
  ],
  "count": 2
}
```

---

## Public API

### sendCommand()

Send a command to a target node.

```cpp
void sendCommand(
  const String& target,           // Node name, ID, or "*"
  const String& command,          // Command name
  JsonObject& args,               // Command arguments (optional)
  CommandCallback callback = nullptr,  // Response callback (optional)
  unsigned long timeout = 5000    // Timeout in ms
);
```

**Callback signature:**
```cpp
typedef std::function<void(
  bool success,
  const String& target,
  JsonObject& result
)> CommandCallback;
```

**Example usage:**
```cpp
// Fire-and-forget
swarm.sendCommand("N1234", "set", {{"key", "led"}, {"value", "on"}});

// With callback
swarm.sendCommand("N5678", "status", {}, [](bool ok, const String& node, JsonObject& result) {
  if (ok) {
    int heap = result["heap"];
    Serial.printf("Node %s has %d bytes free\n", node.c_str(), heap);
  }
});

// Broadcast to all
swarm.sendCommand("*", "sync", {});
```

### onCommand()

Register a custom command handler.

```cpp
void onCommand(const String& command, CommandHandler handler);

typedef std::function<JsonObject(const String& from, JsonObject& args)> CommandHandler;
```

**Example:**
```cpp
swarm.onCommand("ir_send", [](const String& from, JsonObject& args) {
  String code = args["code"];
  irSend(code);

  JsonDocument doc;
  doc["sent"] = true;
  return doc.as<JsonObject>();
});
```

---

## Implementation Plan

### Phase 1: Core Protocol
- [ ] Add `MSG_COMMAND_RESPONSE` to enum
- [ ] Implement command handler in `onReceive()`
- [ ] Add `sendCommand()` method (fire-and-forget)
- [ ] Implement built-in commands (status, peers, state, get, set, sync, ping)

### Phase 2: Response Handling
- [ ] Add pending request tracking (map of rid → callback)
- [ ] Implement response correlation
- [ ] Add timeout handling with cleanup
- [ ] Support broadcast response collection

### Phase 3: Gateway API
- [ ] Add HTTP server to gateway mode
- [ ] Implement `/api/command` endpoint
- [ ] Implement `/api/nodes` endpoint
- [ ] Add response aggregation for broadcast commands

### Phase 4: Extensibility
- [ ] Add `onCommand()` for custom handlers
- [ ] Add command ACL/permissions (optional)
- [ ] Add command logging/audit (optional)

---

## Security Considerations

1. **No authentication by default** - mesh is assumed trusted
2. **Optional command ACL** - restrict sensitive commands (reboot, set)
3. **Rate limiting** - prevent command flooding
4. **Audit logging** - track command history for debugging

---

## Testing Plan

1. **Unit tests** for message serialization/deserialization
2. **Integration tests** for node-to-node commands
3. **End-to-end tests** for HTTP API through gateway
4. **Stress tests** for broadcast commands to many nodes
5. **Timeout tests** for unreachable nodes

---

## Open Questions

1. Should commands be retried on failure?
2. Maximum number of pending requests?
3. Should broadcast responses have a configurable wait time?
4. How to handle nodes joining mid-broadcast?

---

## References

- [MeshSwarm Library](../README.md)
- [Modular Build Guide](../docs/MODULAR_BUILD.md)
- [painlessMesh Documentation](https://gitlab.com/painlessMesh/painlessMesh)
