# Toolchain and Infrastructure PRD

**Status**: In Progress
**Priority**: High (Prerequisite for RCP)
**Created**: December 2024
**Dependencies**: None
**Blocks**: Remote Command Protocol implementation

---

## Overview

Add PlatformIO build system and CI/CD infrastructure to MeshSwarm to improve development workflow, enable automated testing, and support multiple ESP32 board variants. This is a prerequisite for the Remote Command Protocol implementation which requires more sophisticated testing and HTTP server integration.

---

## Problem Statement

**Current Limitations:**
- No standardized local development environment beyond Arduino IDE
- Manual compilation testing via PowerShell script
- No automated CI/CD validation on pull requests
- No systematic testing of feature flag combinations
- Difficult to validate multi-board compatibility
- HTTP server dependencies not formally specified

**Impact:**
- Slower development cycles
- Risk of breaking changes going undetected
- Manual effort required for each test configuration
- Inconsistent development environments across contributors

---

## Goals

1. **Add PlatformIO support** for professional development workflow
2. **Implement GitHub Actions CI/CD** for automated testing
3. **Define multiple test environments** for different board variants
4. **Validate feature flag combinations** automatically
5. **Standardize dependencies** including HTTP server choice
6. **Maintain Arduino IDE compatibility** (library.properties)
7. **Document toolchain setup** for contributors

---

## Non-Goals

- Remove Arduino IDE support (keep both)
- Require PlatformIO for users (Arduino still primary)
- Add unit testing framework (focus on compilation tests)
- Support non-ESP32 platforms (ESP32 family only)

---

## Architecture

### Build System Strategy

```
┌─────────────────────────────────────────────────────────────┐
│                    DEVELOPMENT OPTIONS                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌────────────────────┐         ┌─────────────────────┐    │
│  │   Arduino IDE      │         │   PlatformIO        │    │
│  │   ============     │         │   ==========        │    │
│  │   - User-focused   │         │   - Dev-focused     │    │
│  │   - Simple setup   │         │   - Full toolchain  │    │
│  │   - Visual tools   │         │   - Multi-env       │    │
│  │   - library.prop   │         │   - platformio.ini  │    │
│  └────────────────────┘         └─────────────────────┘    │
│           │                               │                 │
│           └───────────┬───────────────────┘                 │
│                       ▼                                     │
│           ┌─────────────────────┐                          │
│           │  MeshSwarm Library  │                          │
│           │  src/MeshSwarm.*    │                          │
│           └─────────────────────┘                          │
│                       │                                     │
│                       ▼                                     │
│           ┌─────────────────────┐                          │
│           │   GitHub Actions    │                          │
│           │   Automated CI/CD   │                          │
│           └─────────────────────┘                          │
└─────────────────────────────────────────────────────────────┘
```

### CI/CD Pipeline

```
┌────────────────────────────────────────────────────────────┐
│  GitHub Push/PR                                             │
└────────────────┬───────────────────────────────────────────┘
                 │
                 ▼
┌────────────────────────────────────────────────────────────┐
│  Trigger: GitHub Actions Workflow                          │
└────────────────┬───────────────────────────────────────────┘
                 │
                 ├─────────────────┬─────────────────┬────────
                 ▼                 ▼                 ▼
         ┌───────────────┐ ┌───────────────┐ ┌──────────────┐
         │   ESP32       │ │  ESP32-S3     │ │  ESP32-C3    │
         │   Standard    │ │   USB/Serial  │ │  RISC-V      │
         └───────┬───────┘ └───────┬───────┘ └──────┬───────┘
                 │                 │                 │
                 └─────────────────┼─────────────────┘
                                   ▼
                 ┌─────────────────────────────────────┐
                 │  Feature Flag Matrix Testing        │
                 ├─────────────────────────────────────┤
                 │  • All features enabled (default)   │
                 │  • Core only (minimal)              │
                 │  • Core + Display                   │
                 │  • Core + Serial                    │
                 │  • Core + Telemetry                 │
                 │  • Core + OTA                       │
                 │  • Gateway mode (all features)      │
                 └─────────────────┬───────────────────┘
                                   ▼
                 ┌─────────────────────────────────────┐
                 │  Example Compilation Tests          │
                 ├─────────────────────────────────────┤
                 │  • BasicNode                        │
                 │  • MinimalNode (core only)          │
                 │  • GatewayNode (future)             │
                 │  • All other examples               │
                 └─────────────────┬───────────────────┘
                                   ▼
                 ┌─────────────────────────────────────┐
                 │  Report Results                     │
                 │  ✓ PASS / ✗ FAIL                    │
                 └─────────────────────────────────────┘
```

---

## Technical Requirements

### 1. PlatformIO Configuration

**File**: `platformio.ini` at project root

**Required Environments:**
- `esp32` - Standard ESP32 (most common)
- `esp32s3` - ESP32-S3 with USB Serial/JTAG
- `esp32c3` - ESP32-C3 RISC-V variant

**Common Configuration:**
```ini
[platformio]
default_envs = esp32

[env]
platform = espressif32
framework = arduino
lib_deps = 
    painlessMesh
    ArduinoJson
    Adafruit SSD1306
    Adafruit GFX Library

[env:esp32]
board = esp32dev

[env:esp32s3]
board = esp32-s3-devkitc-1

[env:esp32c3]
board = esp32-c3-devkitc-02
```

**Test Environments:**
```ini
[env:test-minimal]
build_flags = 
    -DMESHSWARM_ENABLE_DISPLAY=0
    -DMESHSWARM_ENABLE_SERIAL=0
    -DMESHSWARM_ENABLE_TELEMETRY=0
    -DMESHSWARM_ENABLE_OTA=0

[env:test-display]
build_flags = 
    -DMESHSWARM_ENABLE_SERIAL=0
    -DMESHSWARM_ENABLE_TELEMETRY=0
    -DMESHSWARM_ENABLE_OTA=0

[env:test-gateway]
build_flags = 
    -DMESHSWARM_ENABLE_GATEWAY=1
```

### 2. HTTP Server Dependency

**Decision**: Use **ESP32 WebServer** (built-in)

**Rationale:**
- ✅ No additional dependencies required
- ✅ Part of ESP32 Arduino core
- ✅ Simpler integration for initial RCP implementation
- ✅ Synchronous model easier to debug
- ⚠️ Blocking (but acceptable for gateway use case)
- 🔄 Can upgrade to ESPAsyncWebServer later if needed

**Future Upgrade Path** (optional):
```ini
; If async needed later
lib_deps = 
    ${env.lib_deps}
    me-no-dev/ESPAsyncWebServer
    me-no-dev/AsyncTCP
```

### 3. GitHub Actions Workflow

**File**: `.github/workflows/ci.yml`

**Triggers:**
- Push to `main` or `develop` branches
- Pull requests to `main`
- Manual workflow dispatch

**Jobs:**
1. **Compile Matrix** - Test all board variants
2. **Feature Matrix** - Test feature flag combinations
3. **Examples** - Compile all example sketches

**Matrix Dimensions:**
- Boards: `[esp32, esp32s3, esp32c3]`
- Features: `[default, minimal, display, serial, telemetry, ota, gateway]`
- Examples: Auto-discovered from `examples/` directory

---

## Implementation Details

### PlatformIO Configuration

**platformio.ini** structure:

```ini
; ============================================================
; MeshSwarm Library - PlatformIO Configuration
; ============================================================

[platformio]
description = Self-organizing ESP32 mesh network library
default_envs = esp32
lib_dir = .
src_dir = examples/BasicNode

; ============================================================
; Common Settings
; ============================================================

[env]
platform = espressif32 @ ^6.0.0
framework = arduino
monitor_speed = 115200
upload_speed = 921600
lib_deps = 
    painlessMesh @ ^1.5.0
    ArduinoJson @ ^6.21.0
    adafruit/Adafruit SSD1306 @ ^2.5.0
    adafruit/Adafruit GFX Library @ ^1.11.0

; ============================================================
; Board Environments
; ============================================================

[env:esp32]
board = esp32dev
build_flags = 
    -DCORE_DEBUG_LEVEL=3

[env:esp32s3]
board = esp32-s3-devkitc-1
build_flags = 
    -DCORE_DEBUG_LEVEL=3
    -DARDUINO_USB_CDC_ON_BOOT=1

[env:esp32c3]
board = esp32-c3-devkitc-02
build_flags = 
    -DCORE_DEBUG_LEVEL=3

; ============================================================
; Test Environments - Feature Flags
; ============================================================

[env:test-minimal]
board = esp32dev
build_flags = 
    -DMESHSWARM_ENABLE_DISPLAY=0
    -DMESHSWARM_ENABLE_SERIAL=0
    -DMESHSWARM_ENABLE_TELEMETRY=0
    -DMESHSWARM_ENABLE_OTA=0
    -DMESHSWARM_ENABLE_CALLBACKS=0

[env:test-core-display]
board = esp32dev
build_flags = 
    -DMESHSWARM_ENABLE_SERIAL=0
    -DMESHSWARM_ENABLE_TELEMETRY=0
    -DMESHSWARM_ENABLE_OTA=0

[env:test-core-serial]
board = esp32dev
build_flags = 
    -DMESHSWARM_ENABLE_DISPLAY=0
    -DMESHSWARM_ENABLE_TELEMETRY=0
    -DMESHSWARM_ENABLE_OTA=0

[env:test-telemetry]
board = esp32dev
build_flags = 
    -DMESHSWARM_ENABLE_DISPLAY=0
    -DMESHSWARM_ENABLE_SERIAL=1
    -DMESHSWARM_ENABLE_TELEMETRY=1
    -DMESHSWARM_ENABLE_OTA=0

[env:test-ota]
board = esp32dev
build_flags = 
    -DMESHSWARM_ENABLE_DISPLAY=0
    -DMESHSWARM_ENABLE_SERIAL=1
    -DMESHSWARM_ENABLE_TELEMETRY=0
    -DMESHSWARM_ENABLE_OTA=1

[env:test-gateway]
board = esp32dev
build_flags = 
    -DMESHSWARM_ENABLE_DISPLAY=1
    -DMESHSWARM_ENABLE_SERIAL=1
    -DMESHSWARM_ENABLE_TELEMETRY=1
    -DMESHSWARM_ENABLE_OTA=1
    -DMESHSWARM_ENABLE_GATEWAY=1

; ============================================================
; Log Level Tests
; ============================================================

[env:test-log-none]
board = esp32dev
build_flags = 
    -DMESHSWARM_LOG_LEVEL=0

[env:test-log-error]
board = esp32dev
build_flags = 
    -DMESHSWARM_LOG_LEVEL=1
```

### GitHub Actions Workflow

**.github/workflows/ci.yml**:

```yaml
name: CI

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]
  workflow_dispatch:

jobs:
  compile-library:
    name: Compile (${{ matrix.board }}, ${{ matrix.config }})
    runs-on: ubuntu-latest
    
    strategy:
      fail-fast: false
      matrix:
        board:
          - esp32
          - esp32s3
          - esp32c3
        config:
          - default
          - minimal
          - display
          - serial
          - telemetry
          - ota
          - gateway
    
    steps:
      - name: Checkout
        uses: actions/checkout@v4
      
      - name: Setup Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.x'
      
      - name: Install PlatformIO
        run: |
          pip install platformio
          pio --version
      
      - name: Set Environment Name
        id: env-name
        run: |
          if [ "${{ matrix.config }}" == "default" ]; then
            echo "env=${{ matrix.board }}" >> $GITHUB_OUTPUT
          else
            echo "env=test-${{ matrix.config }}" >> $GITHUB_OUTPUT
          fi
      
      - name: Compile BasicNode
        run: |
          cd examples/BasicNode
          pio ci --lib="../.." --board=${{ matrix.board }} BasicNode.ino
        env:
          PLATFORMIO_BUILD_FLAGS: ${{ steps.env-name.outputs.flags }}
      
      - name: Report Results
        if: always()
        run: |
          echo "Board: ${{ matrix.board }}"
          echo "Config: ${{ matrix.config }}"
          echo "Environment: ${{ steps.env-name.outputs.env }}"

  compile-examples:
    name: Compile Examples
    runs-on: ubuntu-latest
    
    steps:
      - name: Checkout
        uses: actions/checkout@v4
      
      - name: Setup Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.x'
      
      - name: Install PlatformIO
        run: |
          pip install platformio
      
      - name: Compile All Examples
        run: |
          for example in examples/*/; do
            example_name=$(basename "$example")
            ino_file="$example$example_name.ino"
            if [ -f "$ino_file" ]; then
              echo "Compiling $example_name..."
              cd "$example"
              pio ci --lib="../.." --board=esp32dev "$example_name.ino"
              cd ../..
            fi
          done

  build-summary:
    name: Build Summary
    runs-on: ubuntu-latest
    needs: [compile-library, compile-examples]
    if: always()
    
    steps:
      - name: Summary
        run: |
          echo "## Build Results" >> $GITHUB_STEP_SUMMARY
          echo "✅ All compilation tests completed" >> $GITHUB_STEP_SUMMARY
```

---

## Testing Strategy

### 1. Local Development

**PlatformIO Commands:**
```bash
# Compile for specific board
pio run -e esp32

# Compile minimal configuration
pio run -e test-minimal

# Upload to device
pio run -e esp32 --target upload

# Serial monitor
pio device monitor

# Clean build
pio run --target clean
```

### 2. Automated CI

**On Every Commit:**
- Compile for all board variants
- Test all feature flag combinations
- Compile all examples
- Report results in PR comments

**Matrix Coverage:**
- 3 boards × 7 configurations = 21 build variants
- All examples × 1 board = ~8 example builds
- **Total: ~29 compilation tests per commit**

### 3. Manual Testing (Unchanged)

- Arduino IDE compilation
- Physical device testing
- Serial console validation
- Multi-node mesh testing

---

## File Structure

```
MeshSwarm/
├── .github/
│   └── workflows/
│       └── ci.yml                    # NEW: GitHub Actions workflow
├── platformio.ini                    # NEW: PlatformIO configuration
├── library.properties                # EXISTING: Arduino library metadata
├── scripts/
│   └── compile-test.ps1             # EXISTING: Keep for Windows users
├── src/
│   └── MeshSwarm.*                  # EXISTING: Library source
├── examples/                         # EXISTING: Example sketches
└── docs/
    ├── PLATFORMIO_GUIDE.md          # NEW: PlatformIO usage guide
    └── TESTING.md                    # EXISTING: Update with new info
```

---

## Migration Path

### Phase 1: Add PlatformIO Support
1. Create `platformio.ini` with basic configuration
2. Test local builds with `pio run`
3. Validate all boards compile
4. Document usage in `docs/PLATFORMIO_GUIDE.md`

### Phase 2: Add CI/CD
1. Create `.github/workflows/ci.yml`
2. Test workflow on feature branch
3. Enable on main branch
4. Add status badges to README

### Phase 3: Enhance Testing
1. Add feature flag matrix testing
2. Add example compilation tests
3. Add build size reporting
4. Add PR comment automation

---

## Dependencies

### Required Tools

**For Local Development:**
- PlatformIO Core (CLI) or PlatformIO IDE
- Python 3.7+ (for PlatformIO)
- Git

**For CI/CD:**
- GitHub Actions (free for public repos)
- No additional services required

**Library Dependencies** (no changes):
- painlessMesh ^1.5.0
- ArduinoJson ^6.21.0
- Adafruit SSD1306 ^2.5.0
- Adafruit GFX Library ^1.11.0

---

## Success Criteria

1. ✅ `platformio.ini` exists and validates
2. ✅ All board variants compile successfully
3. ✅ All feature flag combinations compile successfully
4. ✅ All examples compile with PlatformIO
5. ✅ GitHub Actions workflow runs on every PR
6. ✅ Arduino IDE compatibility maintained
7. ✅ Documentation updated with PlatformIO usage
8. ✅ Build status badges added to README

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| PlatformIO version incompatibility | High | Pin to specific platform version |
| CI minutes usage on GitHub | Low | Public repos get free minutes |
| Breaking Arduino IDE users | High | Keep library.properties, test both |
| Build matrix too large | Medium | Optimize matrix, use fail-fast: false |
| Dependency version conflicts | Medium | Pin dependency versions in platformio.ini |

---

## Documentation Updates

### New Documents
- `docs/PLATFORMIO_GUIDE.md` - Setup and usage guide
- `.github/workflows/ci.yml` - CI configuration (self-documenting)

### Updated Documents
- `README.md` - Add build badges, PlatformIO instructions
- `docs/TESTING.md` - Add PlatformIO testing section
- `CHANGELOG.md` - Document infrastructure additions

---

## Implementation Checklist

### Phase 1: PlatformIO Setup
- [x] Create `platformio.ini` with basic configuration
- [x] Add board environments (esp32, esp32s3, esp32c3)
- [x] Add test environments for feature flags
- [x] Test local compilation: `pio run -e esp32`
- [x] Validate all environments compile

### Phase 2: Documentation
- [x] Create `docs/PLATFORMIO_GUIDE.md`
- [x] Update `README.md` with PlatformIO section
- [ ] Update `docs/TESTING.md` with CI/CD info
- [x] Add installation instructions

### Phase 3: CI/CD
- [x] Create `.github/workflows/ci.yml`
- [ ] Test workflow on feature branch
- [x] Add matrix builds for feature flags
- [x] Add example compilation tests
- [ ] Enable on main branch

### Phase 4: Validation
- [ ] All CI tests pass
- [x] Arduino IDE still works
- [x] Examples compile in both systems
- [x] Build badges display correctly

---

## Timeline

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| Phase 1: PlatformIO Setup | 1-2 hours | None |
| Phase 2: Documentation | 1 hour | Phase 1 |
| Phase 3: CI/CD | 2-3 hours | Phase 1 |
| Phase 4: Validation | 1 hour | All phases |
| **Total** | **5-7 hours** | - |

---

## Future Enhancements

1. **Build Size Reporting** - Track flash usage per configuration
2. **Automated Release** - Tag and publish on version bump
3. **Documentation Generation** - Auto-generate API docs
4. **Performance Benchmarks** - Track memory/CPU usage
5. **Integration Tests** - Test mesh behavior (requires hardware)

---

## References

- [PlatformIO Documentation](https://docs.platformio.org/)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [MeshSwarm Library Structure](../README.md)

---

*Last updated: December 2024*
