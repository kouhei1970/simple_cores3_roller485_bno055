# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a PlatformIO-based Arduino project for M5Stack CoreS3 that integrates:
- **BNO055 IMU sensor** (9-axis absolute orientation sensor) via I2C
- **Unit Roller I2C motor** (M5Stack's motor driver unit) for motion control
- Real-time feedback control using IMU orientation data to drive the motor

The application reads pitch angle (Euler Y) from BNO055 and uses it to control the motor current, creating a simple tilt-based control system.

**⚠️ Known Issue**: BNO055 currently uses Arduino Wire library, which is unstable. Migration to ESP-IDF I2C driver is required. See "Critical Implementation Notes" section below.

## Hardware Configuration

**Target Board:** M5Stack CoreS3 (ESP32-S3)

**I2C Buses:**
- **Bus 0 (Roller):** GPIO2 (SDA), GPIO1 (SCL) - Unit Roller motor at 0x64
- **Bus 1 (BNO055):** GPIO6 (SDA), GPIO7 (SCL) - BNO055 IMU at 0x28

**Motor Control Mode:** Mode 3 (Current control mode)

## Build System

**PlatformIO Commands:**

```bash
# Build the project
pio run

# Upload to device
pio run --target upload

# Monitor serial output (115200 baud)
pio run --target monitor

# Build + Upload + Monitor
pio run --target upload --target monitor

# Clean build
pio run --target clean
```

**Environment:** `m5stack-cores3` (defined in platformio.ini)

**Key Build Flags:**
- USB CDC on boot enabled
- PSRAM support with cache fix

## Project Structure

```
simple_cores3_roller485_bno055/
├── platformio.ini          # PlatformIO configuration
├── src/
│   ├── main.cpp           # Main application (setup/loop)
│   ├── unit_rolleri2c.cpp # Roller I2C implementation
│   └── unit_rolleri2c.hpp # Roller I2C driver header
├── include/               # Empty (headers in src/)
├── lib/                   # Empty (using external deps)
└── test/                  # Empty (no tests defined)
```

## Code Architecture

### Main Control Flow (main.cpp)

**Initialization (`setup()`):**
1. Initialize M5Unified display
2. Initialize Unit Roller on I2C bus 0
3. Initialize BNO055 on Wire (I2C bus 1)
4. Set motor to current control mode (mode 3)
5. Enable motor output

**Main Loop (`loop()`):**
1. Read BNO055 orientation (Euler angles, quaternion, gyroscope)
2. Calculate motor current command: `cmd = pitch_angle * 100.0`
3. Clamp command to ±100000 range
4. Send current command to Roller motor via `setCurrent()`
5. Read motor telemetry (voltage, current, speed, position)
6. Update M5Stack display with sensor and motor data
7. Repeat at ~100Hz (10ms delay)

### Unit Roller I2C Driver (unit_rolleri2c.cpp/hpp)

**Custom I2C Implementation:**
- Uses ESP-IDF's low-level I2C driver (`driver/i2c.h`) instead of Arduino Wire
- Direct register access via `i2c_cmd_link_create()` for precise control
- Singleton initialization pattern to prevent I2C driver re-initialization

**Migration History:**
- Originally implemented with Arduino Wire library (see `#if 0` blocks in code)
- Wire implementation **failed in production** due to timing issues and unreliable communication
- Migrated to ESP-IDF driver for stability - **this same pattern must be applied to BNO055**

**Motor Control Modes:**
- Mode 1: Speed control with PID
- Mode 2: Position control with PID
- Mode 3: Current (torque) control - **used in this project**
- Mode 4: Dial mode

**Key Methods:**
- `begin(addr, sda, scl, speed)` - Initialize I2C and motor
- `setMode(mode)` - Set control mode (1-4)
- `setOutput(en)` - Enable/disable motor output
- `setCurrent(current)` - Set motor current (-100000 to +100000)
- `getCurrent/Readback()` - Read commanded/actual current
- `getSpeed/Pos/Vin/Temp()` - Read motor telemetry

**Register Definitions:**
- Comprehensive register map defined in header (0x00-0xFF)
- Separate write/read registers for setpoints vs feedback

## Dependencies

**PlatformIO lib_deps:**
- `M5Unified` (M5Stack display/hardware abstraction)
- `Adafruit BNO055` (IMU driver) - **⚠️ TO BE REMOVED - unstable Wire implementation**
- `Adafruit Unified Sensor` (Sensor abstraction layer) - **⚠️ TO BE REMOVED**
- `Adafruit BusIO` (I2C/SPI helpers) - **⚠️ TO BE REMOVED**

**Platform:** espressif32@6.7.0

**Migration Plan:**
- Replace Adafruit_BNO055 with custom ESP-IDF I2C driver (see `I2C_DRIVER_ANALYSIS.md`)
- Remove all Wire-based dependencies after BNO055 migration
- Keep only M5Unified for display/hardware abstraction

## Serial Monitor

**Baud Rate:** 115200

**Display Output Format:**
```
BNO055 + Roller485
cmd:<value>  addrR:0x64 addrB:0x28
[BNO]
Euler H:<heading> R:<roll> P:<pitch> (deg)
Gyro  x:<gx> y:<gy> z:<gz>
Quat  w:<qw> x:<qx> y:<qy> z:<qz>
[Roller]
vin:<voltage> cur:<current> spd:<speed> pos:<position>
```

## Critical Implementation Notes

### ⚠️ I2C Driver Conflict Issue - **ACTION REQUIRED**

**CRITICAL**: The current codebase has **conflicting I2C driver implementations** that pose stability risks:

- **Unit Roller**: Uses ESP-IDF low-level I2C driver (I2C_NUM_0) - **working correctly**
- **BNO055**: Uses Arduino Wire library - **unstable, requires migration**

**Why Wire doesn't work:**
- Unit Roller's `unit_rolleri2c.cpp` contains commented-out Wire implementation (`#if 0` blocks at lines 28-35, 73-82, 159-175) that **failed in production**
- Wire was replaced with ESP-IDF driver for precise timing, reliable error handling, and resource management
- BNO055 is experiencing the same Wire-related issues and **must migrate to ESP-IDF driver**

**Evidence of Wire failure:**
```cpp
// unit_rolleri2c.cpp:159-175
#if 0   // This is the original code - DOESN'T WORK
    _wire->begin(_sda, _scl);
    _wire->setClock(_speed);
    // ... (removed implementation)
#endif
```

**Required Action:**
- Implement BNO055 driver using ESP-IDF I2C (I2C_NUM_1) following Unit Roller pattern
- Remove Adafruit_BNO055 and Wire dependencies
- See `I2C_DRIVER_ANALYSIS.md` for detailed implementation plan

**Reference Documentation:**
- **Full Analysis Report**: `I2C_DRIVER_ANALYSIS.md` (comprehensive technical details, implementation examples, test plan)
- **Unit Roller ESP-IDF Pattern**: `src/unit_rolleri2c.cpp` lines 17-101 (writeBytes/readBytes methods)

### Other Critical Notes

1. **I2C Port Separation:** Unit Roller uses I2C_NUM_0 (GPIO2/1), BNO055 should use I2C_NUM_1 (GPIO6/7). Never share the same I2C port between ESP-IDF and Wire drivers.

2. **Safety:** Motor is set to current mode with initial current 0. The `motorStopSafe()` function is defined but not currently used in error handling.

3. **No Display Clearing:** Main loop uses `startWrite()/endWrite()` without `fillScreen()` for performance - text overwrites previous content at same cursor positions.

4. **Control Law:** Simple proportional control (pitch × 100) without limits beyond clamping. No PID, no integral windup protection, no derivative filtering.

5. **I2C Initialization Guard:** `UnitRollerI2C::initialized` static flag prevents double-initialization of I2C driver, which would cause ESP-IDF errors. Same pattern needed for BNO055 ESP-IDF driver.

## Common Development Workflow

1. Modify code in `src/main.cpp` or motor driver files
2. Build: `pio run`
3. Upload and monitor: `pio run -t upload -t monitor`
4. Observe sensor/motor telemetry on serial output
5. Press Ctrl+C to exit monitor

## BNO055 ESP-IDF Migration Guide

**Status**: Required - current Wire implementation is unstable

**Implementation Steps**:
1. Create `src/bno055_espidf.hpp` and `src/bno055_espidf.cpp`
2. Implement ESP-IDF I2C driver pattern (follow `unit_rolleri2c.cpp` structure)
3. Use I2C_NUM_1 explicitly (separate from Unit Roller's I2C_NUM_0)
4. Implement minimal register access:
   - Chip ID verification (0x00 register, expected 0xA0)
   - Operation mode setting (0x3D register, NDOF mode = 0x0C)
   - Euler angle readout (0x1A-0x1F registers, 6 bytes)
   - Quaternion readout (0x20-0x27 registers, 8 bytes)
   - Gyroscope readout (0x14-0x19 registers, 6 bytes)

**Reference Files**:
- **Implementation Guide**: `I2C_DRIVER_ANALYSIS.md` (complete code examples, register map, test plan)
- **Working Pattern**: `src/unit_rolleri2c.cpp` (ESP-IDF I2C methods: writeBytes, readBytes, begin)

**Testing Strategy**:
1. Add `#define USE_ESPIDF_BNO055` for conditional compilation
2. Keep Wire implementation during development for fallback
3. Verify I2C communication with chip ID check
4. Test concurrent I2C access (Roller + BNO055)
5. Remove Wire dependencies after successful migration

## Hardware Wiring Reference

Ensure connections match these pin definitions before upload:
- Roller I2C: SDA→GPIO2, SCL→GPIO1
- BNO055 I2C: SDA→GPIO6, SCL→GPIO7
- Verify Roller address is 0x64 (default)
- Verify BNO055 address is 0x28 (ADR pin LOW)
