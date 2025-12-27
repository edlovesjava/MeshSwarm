# Remote Command Protocol - Implementation Plan

**Status**: Planning
**Created**: December 2024
**Based on**: [Remote Command Protocol PRD](remote-command-protocol.md)

---

## Overview

This document outlines the step-by-step implementation plan for adding the Remote Command Protocol (RCP) to MeshSwarm. The plan is organized into phases that build incrementally, allowing for testing at each stage.

---

## Current State Analysis

### What Exists
- `MSG_COMMAND = 5` is defined in the enum but not implemented
- Serial command processing exists in `MeshSwarmSerial.inc`
- Gateway mode exists but only as HTTP client (telemetry push)
- State synchronization and peer management are fully functional

### What's Missing
- `MSG_COMMAND_RESPONSE` message type (Type 7)
- Command handling in `onReceive()`
- `sendCommand()` public API method
- `onCommand()` custom handler registration
- Request/response correlation
- HTTP server for gateway API

---

## Implementation Phases

### Phase 1: Core Command Message Handling (Foundation)

**Goal**: Enable basic command sending and receiving through the mesh.

#### Step 1.1: Add MSG_COMMAND_RESPONSE Type

**File**: `src/MeshSwarm.h`

```cpp
enum MsgType {
  MSG_HEARTBEAT  = 1,
  MSG_STATE_SET  = 2,
  MSG_STATE_SYNC = 3,
  MSG_STATE_REQ  = 4,
  MSG_COMMAND    = 5,
  MSG_TELEMETRY  = 6,
  MSG_COMMAND_RESPONSE = 7  // NEW
};
```

#### Step 1.2: Add Command Data Structures

**File**: `src/MeshSwarm.h`

```cpp
// Command callback types
typedef std::function<void(bool success, const String& node, JsonObject& result)> CommandCallback;
typedef std::function<JsonDocument(const String& from, JsonObject& args)> CommandHandler;

// Pending command request tracking
struct PendingCommand {
  String requestId;
  String target;
  String command;
  CommandCallback callback;
  unsigned long timestamp;
  unsigned long timeout;
};
```

#### Step 1.3: Add Member Variables

**File**: `src/MeshSwarm.h`

```cpp
private:
  // Command protocol
  std::map<String, CommandHandler> commandHandlers;
  std::list<PendingCommand> pendingCommands;

  // Helper methods
  String generateRequestId();
  void handleCommand(JsonObject& doc);
  void handleCommandResponse(JsonObject& doc);
  void processCommandTimeouts();
```

#### Step 1.4: Implement Command Message Handler

**File**: `src/MeshSwarm.cpp`

In `onReceive()`, replace the stub:
```cpp
case MSG_COMMAND:
  handleCommand(doc);
  break;

case MSG_COMMAND_RESPONSE:
  handleCommandResponse(doc);
  break;
```

Implement `handleCommand()`:
1. Extract target from message
2. Check if this node matches target (by name, ID, or broadcast "*")
3. Extract command name and args
4. Look up handler in `commandHandlers` map
5. Execute handler and get result
6. Send `MSG_COMMAND_RESPONSE` back to sender

#### Step 1.5: Implement Built-in Command Handlers

Register these in `begin()`:
- `status` - Node status info
- `peers` - List known peers
- `state` - Get all state
- `get` - Get specific state key
- `set` - Set state key
- `sync` - Force state broadcast
- `ping` - Simple connectivity test
- `info` - Node capabilities
- `reboot` - Restart node

**Testing Checkpoint 1.1**:
- [ ] Send command from node A to node B via serial
- [ ] Node B executes command and sends response
- [ ] Node A receives and prints response

---

### Phase 2: Public API and Fire-and-Forget

**Goal**: Expose command sending to user code.

#### Step 2.1: Add sendCommand() Method (Fire-and-Forget)

**File**: `src/MeshSwarm.h`

```cpp
public:
  void sendCommand(const String& target, const String& command);
  void sendCommand(const String& target, const String& command, JsonObject& args);
```

**Implementation**:
1. Generate unique request ID
2. Build MSG_COMMAND JSON
3. Determine if unicast (specific node) or broadcast ("*")
4. Send via mesh

#### Step 2.2: Add Serial Command for Remote Commands

**File**: `src/features/MeshSwarmSerial.inc`

Add `cmd <target> <command> [args]`:
```
> cmd N1234 status
> cmd N5678 set key=led value=on
> cmd * ping
```

**Testing Checkpoint 2.1**:
- [ ] Serial command `cmd N1234 status` sends to target
- [ ] Target receives and responds
- [ ] `cmd * ping` broadcasts to all nodes

---

### Phase 3: Response Handling with Callbacks

**Goal**: Enable asynchronous command/response patterns.

#### Step 3.1: Add Callback-Enabled sendCommand()

**File**: `src/MeshSwarm.h`

```cpp
public:
  void sendCommand(
    const String& target,
    const String& command,
    JsonObject& args,
    CommandCallback callback,
    unsigned long timeout = 5000
  );
```

#### Step 3.2: Implement Pending Request Tracking

**Implementation**:
1. Store pending command with callback in list
2. In `handleCommandResponse()`, match `rid` to pending requests
3. Invoke callback with result
4. Remove from pending list

#### Step 3.3: Implement Timeout Handling

**In `update()` loop**:
1. Check timestamps of pending commands
2. If expired, invoke callback with failure
3. Remove from pending list

#### Step 3.4: Add Broadcast Response Collection

For broadcast commands (`target = "*"`):
1. Track expected responders (all known alive peers)
2. Collect responses until timeout
3. Invoke callback once with aggregated results

**Testing Checkpoint 3.1**:
- [ ] sendCommand() with callback receives response
- [ ] Timeout properly triggers callback with failure
- [ ] Broadcast collects responses from multiple nodes

---

### Phase 4: Custom Command Handlers

**Goal**: Allow users to add their own commands.

#### Step 4.1: Add onCommand() Registration

**File**: `src/MeshSwarm.h`

```cpp
public:
  void onCommand(const String& command, CommandHandler handler);
```

**Implementation**:
```cpp
void MeshSwarm::onCommand(const String& command, CommandHandler handler) {
  commandHandlers[command] = handler;
}
```

#### Step 4.2: Update info Command

Include registered commands in response:
```json
{
  "commands": ["status", "peers", "state", ..., "custom_cmd_1", "custom_cmd_2"]
}
```

**Testing Checkpoint 4.1**:
- [ ] User can register custom command handler
- [ ] Custom command is callable from remote nodes
- [ ] `info` command lists custom commands

---

### Phase 5: Gateway HTTP Server

**Goal**: External apps can control mesh via HTTP API.

#### Step 5.1: Create New Feature File

**File**: `src/features/MeshSwarmHTTPServer.inc`

Add HTTP server using ESPAsyncWebServer:
```cpp
#if MESHSWARM_ENABLE_GATEWAY
#include <ESPAsyncWebServer.h>

AsyncWebServer gatewayServer(80);
```

#### Step 5.2: Add Configuration Flag

**File**: `src/MeshSwarmConfig.h`

```cpp
#ifndef MESHSWARM_ENABLE_GATEWAY
#define MESHSWARM_ENABLE_GATEWAY 1
#endif
```

#### Step 5.3: Implement POST /api/command

**Request**:
```json
{
  "target": "N1234",
  "command": "status",
  "args": {},
  "timeout": 5000
}
```

**Implementation**:
1. Parse request JSON
2. Call internal `sendCommand()` with callback
3. Wait for response (async)
4. Return aggregated response to HTTP client

#### Step 5.4: Implement GET /api/nodes

**Response**:
```json
{
  "nodes": [
    {"id": 12345678, "name": "N1234", "role": "COORD", "alive": true},
    ...
  ],
  "count": 5
}
```

#### Step 5.5: Add Gateway Initialization

**In gateway mode**:
```cpp
void MeshSwarm::startGatewayServer() {
  gatewayServer.on("/api/command", HTTP_POST, handleApiCommand);
  gatewayServer.on("/api/nodes", HTTP_GET, handleApiNodes);
  gatewayServer.begin();
}
```

**Testing Checkpoint 5.1**:
- [ ] HTTP POST to /api/command sends mesh command
- [ ] HTTP response contains node response
- [ ] /api/nodes returns list of mesh nodes
- [ ] Broadcast commands aggregate multiple responses

---

### Phase 6: Integration & Examples

**Goal**: Demonstrate complete functionality.

#### Step 6.1: Create RemoteControlExample

**File**: `examples/RemoteControl/RemoteControl.ino`

Demonstrate:
- Sending commands from one node to another
- Custom command handler registration
- Callback-based response handling

#### Step 6.2: Create GatewayServer Example

**File**: `examples/GatewayServer/GatewayServer.ino`

Demonstrate:
- Gateway with HTTP server enabled
- External curl/Postman commands
- Web dashboard potential

#### Step 6.3: Update Documentation

- README.md: Add RCP section
- MODULAR_BUILD.md: Document GATEWAY feature flag
- API reference for new methods

---

## File Changes Summary

| File | Changes |
|------|---------|
| `src/MeshSwarm.h` | Add types, structs, method declarations |
| `src/MeshSwarm.cpp` | Implement command handling, sendCommand, timeouts |
| `src/MeshSwarmConfig.h` | Add MESHSWARM_ENABLE_GATEWAY flag |
| `src/features/MeshSwarmSerial.inc` | Add `cmd` serial command |
| `src/features/MeshSwarmHTTPServer.inc` | New file - HTTP server for gateway |
| `examples/RemoteControl/` | New example |
| `examples/GatewayServer/` | New example |

---

## Dependencies

### Required Libraries (for Gateway HTTP Server)
- **ESPAsyncWebServer** - Async HTTP server
- **AsyncTCP** (ESP32) or **ESPAsyncTCP** (ESP8266) - Async TCP layer

```ini
; platformio.ini
lib_deps =
    me-no-dev/ESPAsyncWebServer
    me-no-dev/AsyncTCP  ; ESP32
```

---

## Testing Strategy

### Unit Tests
- Message serialization/deserialization
- Request ID generation uniqueness
- Timeout tracking logic

### Integration Tests

| Test | Description |
|------|-------------|
| Node-to-Node Command | A sends command to B, B responds |
| Broadcast Command | A broadcasts, all nodes respond |
| Timeout Handling | A sends to offline node, timeout fires |
| HTTP API Command | External app → gateway → mesh → response |
| Custom Handler | Register custom command, call remotely |

### Manual Testing Steps

1. **Two-Node Setup**:
   - Flash Node A and Node B
   - On A: `cmd N<B-name> status`
   - Verify B receives and responds

2. **Gateway Test**:
   - Flash gateway node with HTTP server
   - `curl -X POST http://gateway-ip/api/command -d '{"target":"*","command":"status"}'`
   - Verify aggregated response

---

## Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| Memory usage from pending requests | Limit max pending requests (e.g., 10) |
| Response flooding on broadcast | Rate limit responses, stagger by node ID |
| HTTP server conflicts | Only enable on gateway role |
| Blocking HTTP handlers | Use async server with callbacks |

---

## Implementation Order Recommendation

For incremental development:

1. **Start with Phase 1** - Get basic message flow working
2. **Add Phase 2** - Make it usable via serial for testing
3. **Add Phase 3** - Enable full async patterns
4. **Add Phase 4** - Make it extensible
5. **Add Phase 5** - External API access
6. **Complete Phase 6** - Documentation and examples

Each phase can be tested independently before moving to the next.

---

## Estimated Effort

| Phase | Complexity | Dependencies |
|-------|------------|--------------|
| Phase 1 | Medium | None |
| Phase 2 | Low | Phase 1 |
| Phase 3 | Medium | Phase 2 |
| Phase 4 | Low | Phase 3 |
| Phase 5 | High | Phase 3, new library deps |
| Phase 6 | Medium | All phases |

---

## Open Questions for Implementation

1. **Request ID format**: UUID-like or simpler counter-based?
   - *Recommendation*: Use `millis()` + `nodeId` suffix for simplicity

2. **Max pending requests**: How many concurrent requests to allow?
   - *Recommendation*: 10-20 for resource-constrained devices

3. **Broadcast response timeout**: Fixed or configurable?
   - *Recommendation*: Configurable with 3-second default

4. **Command handler thread safety**: Single-threaded assumed?
   - *Recommendation*: Yes, ESP32 Arduino is single-core by default

---

## Next Steps

1. Review this plan and approve approach
2. Create feature branch: `feature/remote-command-protocol`
3. Begin Phase 1 implementation
4. Test each phase before proceeding

---

*Last updated: December 2024*
