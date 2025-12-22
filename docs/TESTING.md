# MeshSwarm Modular Build Testing Guide

This document describes the testing procedures for validating the modular build system.

## Test Configurations

### 1. Default Build (All Features Enabled)

**Configuration:**
```cpp
// No flags needed - all features enabled by default
#include <MeshSwarm.h>
```

**Expected Results:**
- ✅ All methods compile successfully
- ✅ Display methods available: `getDisplay()`, `setStatusLine()`, `onDisplayUpdate()`
- ✅ Serial methods available: `onSerialCommand()`, serial debugging output
- ✅ Telemetry methods available: `setTelemetryServer()`, `enableTelemetry()`, `pushTelemetry()`, `connectToWiFi()`, `setGatewayMode()`
- ✅ OTA methods available: `enableOTADistribution()`, `checkForOTAUpdates()`, `enableOTAReceive()`
- ✅ Callback methods available: `onLoop()`, `onSerialCommand()`, `onDisplayUpdate()`
- ✅ Compiles with all examples: BasicNode, ButtonNode, LedNode, etc.

**Flash Usage:** ~58-84KB

---

### 2. Core Only (All Features Disabled)

**Configuration:**
```cpp
#define MESHSWARM_ENABLE_DISPLAY 0
#define MESHSWARM_ENABLE_SERIAL 0
#define MESHSWARM_ENABLE_TELEMETRY 0
#define MESHSWARM_ENABLE_OTA 0
#include <MeshSwarm.h>
```

**Expected Results:**
- ✅ Core methods compile: `begin()`, `update()`, `setState()`, `getState()`, `watchState()`
- ❌ Display methods unavailable: compile errors if called
- ❌ Serial methods unavailable: no serial output
- ❌ Telemetry methods unavailable: compile errors if called
- ❌ OTA methods unavailable: compile errors if called
- ❌ Callback methods unavailable (except core watchState)
- ✅ Compiles with MinimalNode example
- ✅ No compilation errors for core functionality

**Flash Usage:** ~15-20KB  
**Savings:** ~40-65KB

---

### 3. Core + Display

**Configuration:**
```cpp
#define MESHSWARM_ENABLE_SERIAL 0
#define MESHSWARM_ENABLE_TELEMETRY 0
#define MESHSWARM_ENABLE_OTA 0
// DISPLAY defaults to enabled
#include <MeshSwarm.h>
```

**Expected Results:**
- ✅ Core methods compile
- ✅ Display methods available: `getDisplay()`, `setStatusLine()`, `onDisplayUpdate()`
- ❌ Serial methods unavailable
- ❌ Telemetry methods unavailable
- ❌ OTA methods unavailable
- ✅ Display callbacks available
- ✅ Compiles with DisplayOnlyNode example

**Flash Usage:** ~30-40KB  
**Savings:** ~30-45KB

---

### 4. Core + Serial

**Configuration:**
```cpp
#define MESHSWARM_ENABLE_DISPLAY 0
#define MESHSWARM_ENABLE_TELEMETRY 0
#define MESHSWARM_ENABLE_OTA 0
// SERIAL defaults to enabled
#include <MeshSwarm.h>
```

**Expected Results:**
- ✅ Core methods compile
- ❌ Display methods unavailable
- ✅ Serial methods available: `onSerialCommand()`, serial debugging
- ✅ Built-in commands work: status, peers, state, set, get, sync
- ❌ Telemetry commands unavailable: telem, push
- ❌ OTA methods unavailable

**Flash Usage:** ~25-35KB  
**Savings:** ~35-50KB

---

### 5. Core + Telemetry

**Configuration:**
```cpp
#define MESHSWARM_ENABLE_DISPLAY 0
#define MESHSWARM_ENABLE_SERIAL 0
#define MESHSWARM_ENABLE_OTA 0
// TELEMETRY defaults to enabled
#include <MeshSwarm.h>
```

**Expected Results:**
- ✅ Core methods compile
- ❌ Display methods unavailable
- ❌ Serial output unavailable
- ✅ Telemetry methods available: `setTelemetryServer()`, `enableTelemetry()`, `pushTelemetry()`
- ✅ Gateway methods available: `setGatewayMode()`, `connectToWiFi()`
- ❌ OTA methods unavailable

**Flash Usage:** ~30-40KB  
**Savings:** ~30-45KB

---

### 6. Core + OTA

**Configuration:**
```cpp
#define MESHSWARM_ENABLE_DISPLAY 0
#define MESHSWARM_ENABLE_SERIAL 0
#define MESHSWARM_ENABLE_TELEMETRY 0
// OTA defaults to enabled
#include <MeshSwarm.h>
```

**Expected Results:**
- ✅ Core methods compile
- ❌ Display methods unavailable
- ❌ Serial output unavailable
- ❌ Telemetry methods unavailable (but OTA needs HTTP, so some overlap)
- ✅ OTA methods available: `enableOTADistribution()`, `checkForOTAUpdates()`, `enableOTAReceive()`

**Flash Usage:** ~35-45KB  
**Savings:** ~25-40KB

---

### 7. Display + Serial (No Network Features)

**Configuration:**
```cpp
#define MESHSWARM_ENABLE_TELEMETRY 0
#define MESHSWARM_ENABLE_OTA 0
// DISPLAY and SERIAL default to enabled
#include <MeshSwarm.h>
```

**Expected Results:**
- ✅ Core methods compile
- ✅ Display methods available
- ✅ Serial methods available
- ❌ Telemetry methods unavailable
- ❌ OTA methods unavailable
- ✅ Good for standalone/development nodes

**Flash Usage:** ~38-50KB  
**Savings:** ~25-35KB

---

## Manual Testing Procedure

Since automated compilation testing requires build tools, perform these manual checks:

### Syntax Validation

1. **Check preprocessor balance:**
   ```bash
   # In src/MeshSwarm.h
   grep -c "#if" MeshSwarm.h    # Should equal #endif count
   grep -c "#endif" MeshSwarm.h
   
   # In src/MeshSwarm.cpp
   grep -c "#if" MeshSwarm.cpp    # Should equal #endif count
   grep -c "#endif" MeshSwarm.cpp
   ```

2. **Check for syntax errors:**
   - Review all conditional blocks for proper closure
   - Verify no dangling commas in member initialization lists
   - Check that all class members are properly guarded

### Code Review Checklist

- [ ] All feature flags default to `1` (enabled)
- [ ] Config file is included first in MeshSwarm.h
- [ ] All optional includes are conditionally compiled
- [ ] Class members are wrapped with appropriate guards
- [ ] Method implementations match header declarations
- [ ] No Serial.print() calls outside `#if MESHSWARM_ENABLE_SERIAL` guards
- [ ] Display operations only in `#if MESHSWARM_ENABLE_DISPLAY` blocks
- [ ] Telemetry code only in `#if MESHSWARM_ENABLE_TELEMETRY` blocks
- [ ] OTA code only in `#if MESHSWARM_ENABLE_OTA` blocks
- [ ] Core functionality (mesh, state) always available
- [ ] Examples reference correct feature flags

### Functional Tests (with ESP32 hardware)

If you have ESP32 hardware and Arduino IDE/PlatformIO:

1. **Test Default Build:**
   - Compile BasicNode example
   - Verify all features work
   - Check serial output
   - Verify display updates

2. **Test Minimal Build:**
   - Compile MinimalNode example
   - Verify core functionality works
   - Verify no serial output
   - Check reduced flash usage in build output

3. **Test Display Only:**
   - Compile DisplayOnlyNode example
   - Verify display works
   - Verify no serial output
   - Check flash usage

4. **Test Various Combinations:**
   - Try disabling different feature combinations
   - Verify compilation succeeds
   - Check that disabled features cause compile errors if called

## Expected Compilation Errors

When features are disabled, calling their methods **should** produce compile errors:

### Display Disabled
```cpp
error: 'class MeshSwarm' has no member named 'getDisplay'
error: 'class MeshSwarm' has no member named 'setStatusLine'
error: 'class MeshSwarm' has no member named 'onDisplayUpdate'
```

### Serial Disabled
```cpp
error: 'class MeshSwarm' has no member named 'onSerialCommand'
```

### Telemetry Disabled
```cpp
error: 'class MeshSwarm' has no member named 'setTelemetryServer'
error: 'class MeshSwarm' has no member named 'enableTelemetry'
error: 'class MeshSwarm' has no member named 'pushTelemetry'
error: 'class MeshSwarm' has no member named 'connectToWiFi'
error: 'class MeshSwarm' has no member named 'setGatewayMode'
```

### OTA Disabled
```cpp
error: 'class MeshSwarm' has no member named 'enableOTADistribution'
error: 'class MeshSwarm' has no member named 'checkForOTAUpdates'
error: 'class MeshSwarm' has no member named 'enableOTAReceive'
```

These errors are **expected and correct** - they prevent runtime issues.

## Flash Memory Measurement

To measure actual flash savings:

1. Compile with Arduino IDE or PlatformIO
2. Note the "Sketch uses X bytes" message
3. Compare different configurations
4. Expected relative sizes:
   - Full: 100% (baseline)
   - Core only: ~25-30% (70-75% reduction)
   - Core + Display: ~50-60%
   - Core + Serial: ~40-50%

## Regression Testing

Ensure no breaking changes:

- [ ] Existing sketches compile without changes (default config)
- [ ] All public API methods work when features are enabled
- [ ] State management works in all configurations
- [ ] Mesh networking works in all configurations
- [ ] Peer discovery works in all configurations
- [ ] Coordinator election works in all configurations

## Success Criteria

✅ **Pass Conditions:**
- All preprocessor directives balanced
- Default build compiles with all features
- Minimal build compiles with no features
- Various combinations compile successfully
- Disabled features produce compile errors when called
- Core functionality always available
- Flash savings measurable (20-65KB depending on config)
- No breaking changes to default configuration

## Continuous Integration

For automated testing, create a CI workflow that:

1. Tests compilation of all example sketches
2. Tests various feature flag combinations
3. Measures and reports flash usage
4. Verifies no warnings or errors

Example GitHub Actions workflow structure:
```yaml
- name: Test Full Build
  run: compile BasicNode.ino
  
- name: Test Minimal Build
  run: compile MinimalNode.ino
  
- name: Test Display Only
  run: compile DisplayOnlyNode.ino
  
- name: Test Custom Combinations
  run: |
    compile with DISPLAY=0 SERIAL=1
    compile with TELEMETRY=0 OTA=0
```

---

## Automated Testing with CI/CD

### GitHub Actions Workflow

The project includes automated testing via GitHub Actions (`.github/workflows/ci.yml`):

**Test Matrix:**
- **Board Variants**: ESP32, ESP32-S3, ESP32-C3
- **Feature Configurations**: Default, Minimal, Display, Serial, Telemetry, OTA, Gateway
- **Examples**: All example sketches
- **Log Levels**: None, Error, Info (default)

**Total Tests per Commit:** ~30+ compilation tests

### Viewing Results

1. Go to https://github.com/edlovesjava/MeshSwarm/actions
2. Click on latest workflow run
3. View job results:
   - ✅ Green = All tests passed
   - ❌ Red = Compilation failed
4. Click failed jobs to see error details

### Running CI Tests Locally

Using PlatformIO:

```bash
# Test all board variants
pio run -e esp32
pio run -e esp32s3
pio run -e esp32c3

# Test feature flags
pio run -e test-minimal
pio run -e test-core-display
pio run -e test-telemetry
pio run -e test-gateway

# Test log levels
pio run -e test-log-none
pio run -e test-log-error

# Compile all examples
for example in examples/*/; do
  cd "$example"
  pio ci --lib="../.." --board=esp32dev *.ino
  cd ../..
done
```

See **[PLATFORMIO_GUIDE.md](PLATFORMIO_GUIDE.md)** for complete PlatformIO documentation.

---

## Troubleshooting

### Issue: Compile errors with default build
**Solution:** Check that config file defaults all flags to `1`

### Issue: No flash savings
**Solution:** Verify flags are defined **before** `#include <MeshSwarm.h>`

### Issue: Unexpected behavior
**Solution:** Check that feature code is properly guarded in both .h and .cpp files

### Issue: Linker errors
**Solution:** Verify conditional compilation is consistent between header and implementation

### Issue: CI tests failing on PR
**Solution:** 
1. Check GitHub Actions tab for error details
2. Run failing environment locally: `pio run -e <env-name>`
3. Fix compilation errors
4. Push fix to trigger re-run

---

*Last updated: December 2024*
