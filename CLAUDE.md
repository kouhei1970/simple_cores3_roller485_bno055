# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## 📖 Quick Reference

**New to this project?** Start with [README.md](README.md) for setup instructions, build commands, and tuning guide.

**このプロジェクトが初めての方は** [README.md](README.md) でセットアップ手順、ビルドコマンド、調整ガイドを参照してください。

---

## Project Overview

This is a **Reaction Wheel Inverted Pendulum** control system using PlatformIO for M5Stack CoreS3:

- **BNO055 IMU sensor** (9-axis absolute orientation sensor) via ESP-IDF I2C
- **Unit Roller I2C motor** (M5Stack's motor driver unit) for torque generation
- **PD control** (Proportional-Derivative) for inverted pendulum stabilization
- **Real-time safety monitoring** with automatic shutdown on limit violation

The application implements a closed-loop PD controller that reads pitch angle and angular velocity from BNO055 and commands motor current to stabilize the inverted pendulum.

**✅ Implementation Status**:
- ESP-IDF I2C driver for BNO055: ✅ Complete (stable communication)
- PD control implementation: ✅ Complete (Kp=100, Kd=5)
- Safety limits: ✅ Complete (±30° pitch, ±300 dps rate)
- Real-time monitoring display: ✅ Complete

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
- `Adafruit BNO055` (for IMU math types: imu::Vector, imu::Quaternion)
- `Adafruit Unified Sensor` (sensor abstraction layer)
- `Adafruit BusIO` (I2C/SPI helpers)

**Note:** Adafruit libraries are kept for their math types and data structures, but I2C communication uses custom ESP-IDF driver implementation.

**Platform:** espressif32@6.7.0

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

### ✅ ESP-IDF I2C Driver Implementation - **COMPLETE**

**Status**: Both BNO055 and Unit Roller now use stable ESP-IDF I2C drivers:

- **Unit Roller**: ESP-IDF I2C driver (I2C_NUM_0, GPIO2/1) - proven reliable
- **BNO055**: ESP-IDF I2C driver (I2C_NUM_1, GPIO6/7) - newly implemented

**Implementation Details:**
- Custom `BNO055_ESPIDF` class created with ESP-IDF I2C transport layer
- Reuses proven Adafruit_BNO055 initialization logic (Thin Wrapper Pattern)
- Separate I2C buses prevent interference between sensor and motor
- Conditional compilation allows switching between ESP-IDF and Wire for testing

**Key Files:**
- `src/bno055_espidf.hpp` - BNO055 ESP-IDF driver header
- `src/bno055_espidf.cpp` - ESP-IDF I2C implementation + Adafruit logic reuse
- `src/main.cpp` - Driver selection via `#define USE_ESPIDF_BNO055`

**Reference Documentation:**
- **Technical Analysis**: [I2C_DRIVER_ANALYSIS.md](I2C_DRIVER_ANALYSIS.md) - Why Wire failed, ESP-IDF migration rationale
- **Control Theory**: [CONTROL_REVIEW.md](CONTROL_REVIEW.md) - PD control design for inverted pendulum

### ✅ PD Control Implementation - **COMPLETE**

**Status**: Proper PD control for inverted pendulum stabilization:

- Control law: `cmd = Kp*pitch + Kd*pitch_rate`
- Proportional gain: Kp = 100.0 [current/deg]
- Derivative gain: Kd = 5.0 [current/(deg/s)]
- Control frequency: 100 Hz (10ms sampling)
- Safety limits: ±30° pitch, ±300 dps angular velocity

**Key Features:**
- Real-time safety monitoring with automatic motor shutdown
- Control enable/disable flag for testing
- Display shows control status, gains, and sensor data
- Gyro unit correction ([dps] not [rad/s])

**Tuning Required:**
- Verify control axis direction on first power-up
- Adjust Kp/Kd gains based on system response
- See [README.md](README.md) Tuning Guide section

### Other Critical Notes

1. **I2C Port Separation**: Unit Roller (I2C_NUM_0, GPIO2/1), BNO055 (I2C_NUM_1, GPIO6/7) - never share I2C buses between different drivers.

2. **Safety System**: Motor runs in current mode (Mode 3). The `motorStopSafe()` function sets current=0 and output=0, and is called when safety limits are exceeded (pitch > ±30° or rate > ±300 dps).

3. **Display Performance**: Main loop uses `startWrite()/endWrite()` without `fillScreen()` - text overwrites previous content at same cursor positions for 100Hz refresh rate.

4. **Control Axis Verification**: Before enabling closed-loop control, verify that control direction is correct. If system diverges, reverse control sign in main.cpp:185.

5. **I2C Initialization Guard**: Both `UnitRollerI2C::initialized` and `BNO055_ESPIDF::initialized_i2c1` static flags prevent double-initialization of I2C drivers, which would cause ESP-IDF errors.

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
