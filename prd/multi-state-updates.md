# Multi-State Updates Specification

**Date:** December 24, 2025  
**Status:** Proposed  
**Version:** 1.0

## Overview

This specification defines the adoption of the `setStates()` function in SensorNode and GatewayNode to enable atomic batch updates of multiple related state values. This improves efficiency, reduces telemetry traffic, and ensures related values are synchronized together.

## Current State

### SensorNode Behavior
- Reads DHT11 sensor data (temperature and humidity) every 5 seconds
- Currently publishes each reading with separate `setState()` calls:
  ```cpp
  swarm.setState("temp", String(temp, 1));
  swarm.setState("humid", String(humid, 1));
  ```
- Results in two independent `MSG_STATE_SET` broadcasts
- Triggers telemetry push twice per reading cycle (with debouncing)

### GatewayNode Behavior
- Acts as a bridge between mesh and external telemetry server
- Receives state updates from all mesh nodes
- Currently publishes full state via implicit `broadcastFullState()`
- Sends telemetry data individually for each state change

## Proposed Changes

### Benefits
1. **Atomic Updates**: Multiple related values updated together, preventing race conditions
2. **Reduced Traffic**: Single telemetry push per sensor reading instead of multiple pushes
3. **Better Semantics**: Temperature and humidity are logically grouped
4. **Cleaner Code**: More expressive intent with batch API
5. **Extensibility**: Easier to add more sensors (pressure, air quality, etc.)

### SensorNode Enhancement

#### Implementation
Replace dual `setState()` calls with `setStates()`:

```cpp
swarm.setStates({
  {"temp", String(temp, 1)},
  {"humid", String(humid, 1)}
});
```

#### Behavior Changes
- Both readings published in single telemetry batch
- Watchers still triggered individually for each key
- Display updates show both values (no change needed)
- Serial commands remain unchanged

#### Message Protocol Impact
- Still sends individual `MSG_STATE_SET` messages (no protocol change)
- Single telemetry push instead of two per reading cycle
- Reduces mesh traffic by ~50% for sensor readings

### GatewayNode Enhancement

#### Implementation
Batch-publish all state changes:

```cpp
// When receiving state updates from peers
// Collect updates and batch them
swarm.setStates({
  {"node_1_temp", temp},
  {"node_1_humid", humid},
  {"node_2_temp", temp}
  // ... etc
});
```

#### Alternative: Explicit Sync
Optionally replace individual state updates with:

```cpp
swarm.broadcastFullState();  // Already batched
```

#### Behavior Changes
- Reduces telemetry pushes to server
- Better reflects actual network state changes
- No changes to external server API expected

## Message Format

No protocol changes required. `MSG_STATE_SET` remains single key-value format:

```json
{
  "type": 2,
  "from": 12345,
  "data": {
    "k": "temp",
    "v": "23.5",
    "ver": 1,
    "org": 12345
  }
}
```

The batching is a **local optimization** that affects:
- How many individual `MSG_STATE_SET` messages are sent
- How many telemetry pushes occur
- Not the wire protocol itself

## Implementation Details

### SensorNode.ino Changes
**File:** `examples/SensorNode/SensorNode.ino`

**Change Location:** In the sensor reading callback (around line 68)

```cpp
// OLD:
swarm.setState("temp", String(temp, 1));
swarm.setState("humid", String(humid, 1));

// NEW:
swarm.setStates({
  {"temp", String(temp, 1)},
  {"humid", String(humid, 1)}
});
```

**No other changes required:**
- onDisplayUpdate callback remains unchanged
- onSerialCommand callback remains unchanged
- readInterval remains 5 seconds
- DHT sensor configuration unchanged

### GatewayNode.ino Changes
**File:** `examples/GatewayNode/GatewayNode.ino`

**Option A (Recommended):** No code changes needed
- Gateway already benefits from `broadcastFullState()` 
- Telemetry system is event-driven
- May revisit if/when gateway implements active polling

**Option B (Future):** If implementing active state collection:
- Add state aggregation from peer states
- Batch them with `setStates()` before telemetry push
- Requires state inspection of peer map

## Testing Strategy

### Unit Testing
- Verify `setStates()` updates all values correctly
- Verify watchers triggered for each key
- Verify telemetry batched as expected

### Integration Testing

#### SensorNode Test
1. Deploy SensorNode with modification
2. Monitor mesh broadcasts:
   - Should still see individual `MSG_STATE_SET` for each key
   - Should see fewer telemetry pushes (1 per cycle vs 2)
3. Verify display shows both temp and humid
4. Verify watchers on both keys still fire

#### Mesh Network Test
1. Deploy multiple SensorNodes with batched updates
2. Deploy GatewayNode
3. Verify telemetry server receives all state changes
4. Monitor network traffic reduction
5. Verify state convergence is correct

### Serial Command Verification
```
> status          # Should show all states
> state           # Should list temp and humid
> get temp        # Should return current value
> set temp 99     # Should update (for testing)
```

## Compatibility

### Backward Compatibility
- ✅ No breaking changes to public API
- ✅ Existing `setState()` calls continue working
- ✅ No protocol changes
- ✅ Mesh can mix nodes using `setState()` and `setStates()`

### Version Impact
- Patch version bump (1.x.0 → 1.x.1)
- Example code update, no library changes needed

## Future Enhancements

### Extended Sensor Node
Could add additional sensors using same batching:
```cpp
swarm.setStates({
  {"temp", String(temp, 1)},
  {"humid", String(humid, 1)},
  {"pressure", String(pressure, 1)},
  {"co2", String(co2_ppm)}
});
```

### Sensor Data Timestamp Batching
Could batch sensor readings with timestamp:
```cpp
swarm.setStates({
  {"sensor_reading_ts", String(readingTime)},
  {"temp", String(temp, 1)},
  {"humid", String(humid, 1)}
});
```

### Gateway Active Collection
If gateway needs to actively collect and republish:
```cpp
std::initializer_list<std::pair<String, String>> updates;
// Collect from peers...
swarm.setStates(updates);
```

## Success Criteria

- ✅ SensorNode code adopts `setStates()`
- ✅ No regression in state propagation
- ✅ Telemetry traffic reduced by ~50% for sensor nodes
- ✅ All tests pass
- ✅ Examples in documentation reflect new pattern
- ✅ No breaking changes to API

## References

- [MeshSwarm State Management](../README.md)
- [setStates() Implementation](../src/MeshSwarm.cpp#L229)
- [SensorNode Example](../examples/SensorNode/SensorNode.ino)
- [GatewayNode Example](../examples/GatewayNode/GatewayNode.ino)
