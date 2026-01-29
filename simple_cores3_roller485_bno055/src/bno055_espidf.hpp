/*!
 * BNO055 ESP-IDF I2C Driver
 * Based on Adafruit_BNO055 library with ESP-IDF I2C transport layer
 *
 * This driver reuses the proven initialization sequence and data conversion
 * logic from Adafruit_BNO055, but replaces the Wire-based I2C communication
 * with ESP-IDF's low-level I2C driver for stable operation.
 *
 * Original Adafruit_BNO055 library:
 *   Copyright (c) K.Townsend (Adafruit Industries)
 *   MIT License
 *
 * ESP-IDF I2C implementation:
 *   Copyright (c) 2026
 *   Follows Unit Roller I2C pattern
 */

#ifndef __BNO055_ESPIDF_HPP__
#define __BNO055_ESPIDF_HPP__

#include <Arduino.h>
#include <driver/i2c.h>
#include <stdint.h>

// Import IMU math types from Adafruit library
#include <utility/imumaths.h>

// BNO055 I2C Addresses
#define BNO055_ADDRESS_A           (0x28)
#define BNO055_ADDRESS_B           (0x29)
#define BNO055_ID                  (0xA0)

// BNO055 Register Map (Page 0)
#define BNO055_CHIP_ID_ADDR        0x00
#define BNO055_ACCEL_REV_ID_ADDR   0x01
#define BNO055_MAG_REV_ID_ADDR     0x02
#define BNO055_GYRO_REV_ID_ADDR    0x03
#define BNO055_SW_REV_ID_LSB_ADDR  0x04
#define BNO055_SW_REV_ID_MSB_ADDR  0x05
#define BNO055_BL_REV_ID_ADDR      0x06
#define BNO055_PAGE_ID_ADDR        0x07

// Data Registers
#define BNO055_ACCEL_DATA_X_LSB_ADDR       0x08
#define BNO055_MAG_DATA_X_LSB_ADDR         0x0E
#define BNO055_GYRO_DATA_X_LSB_ADDR        0x14
#define BNO055_EULER_H_LSB_ADDR            0x1A
#define BNO055_QUATERNION_DATA_W_LSB_ADDR  0x20
#define BNO055_LINEAR_ACCEL_DATA_X_LSB_ADDR 0x28
#define BNO055_GRAVITY_DATA_X_LSB_ADDR     0x2E

// Status Registers
#define BNO055_TEMP_ADDR           0x34
#define BNO055_CALIB_STAT_ADDR     0x35
#define BNO055_SELFTEST_RESULT_ADDR 0x36
#define BNO055_INTR_STAT_ADDR      0x37
#define BNO055_SYS_CLK_STAT_ADDR   0x38
#define BNO055_SYS_STAT_ADDR       0x39
#define BNO055_SYS_ERR_ADDR        0x3A

// Control Registers
#define BNO055_UNIT_SEL_ADDR       0x3B
#define BNO055_OPR_MODE_ADDR       0x3D
#define BNO055_PWR_MODE_ADDR       0x3E
#define BNO055_SYS_TRIGGER_ADDR    0x3F
#define BNO055_TEMP_SOURCE_ADDR    0x40

// Axis Remap Registers
#define BNO055_AXIS_MAP_CONFIG_ADDR 0x41
#define BNO055_AXIS_MAP_SIGN_ADDR   0x42

// Offset Registers (Calibration Data)
#define ACCEL_OFFSET_X_LSB_ADDR    0x55
#define ACCEL_OFFSET_X_MSB_ADDR    0x56
#define ACCEL_OFFSET_Y_LSB_ADDR    0x57
#define ACCEL_OFFSET_Y_MSB_ADDR    0x58
#define ACCEL_OFFSET_Z_LSB_ADDR    0x59
#define ACCEL_OFFSET_Z_MSB_ADDR    0x5A

#define MAG_OFFSET_X_LSB_ADDR      0x5B
#define MAG_OFFSET_X_MSB_ADDR      0x5C
#define MAG_OFFSET_Y_LSB_ADDR      0x5D
#define MAG_OFFSET_Y_MSB_ADDR      0x5E
#define MAG_OFFSET_Z_LSB_ADDR      0x5F
#define MAG_OFFSET_Z_MSB_ADDR      0x60

#define GYRO_OFFSET_X_LSB_ADDR     0x61
#define GYRO_OFFSET_X_MSB_ADDR     0x62
#define GYRO_OFFSET_Y_LSB_ADDR     0x63
#define GYRO_OFFSET_Y_MSB_ADDR     0x64
#define GYRO_OFFSET_Z_LSB_ADDR     0x65
#define GYRO_OFFSET_Z_MSB_ADDR     0x66

#define ACCEL_RADIUS_LSB_ADDR      0x67
#define ACCEL_RADIUS_MSB_ADDR      0x68
#define MAG_RADIUS_LSB_ADDR        0x69
#define MAG_RADIUS_MSB_ADDR        0x6A

// Operation Modes
typedef enum {
    OPERATION_MODE_CONFIG       = 0x00,
    OPERATION_MODE_ACCONLY      = 0x01,
    OPERATION_MODE_MAGONLY      = 0x02,
    OPERATION_MODE_GYRONLY      = 0x03,
    OPERATION_MODE_ACCMAG       = 0x04,
    OPERATION_MODE_ACCGYRO      = 0x05,
    OPERATION_MODE_MAGGYRO      = 0x06,
    OPERATION_MODE_AMG          = 0x07,
    OPERATION_MODE_IMUPLUS      = 0x08,
    OPERATION_MODE_COMPASS      = 0x09,
    OPERATION_MODE_M4G          = 0x0A,
    OPERATION_MODE_NDOF_FMC_OFF = 0x0B,
    OPERATION_MODE_NDOF         = 0x0C
} bno055_opmode_t;

// Power Modes
typedef enum {
    POWER_MODE_NORMAL   = 0x00,
    POWER_MODE_LOWPOWER = 0x01,
    POWER_MODE_SUSPEND  = 0x02
} bno055_powermode_t;

// Vector Types for getVector()
typedef enum {
    VECTOR_ACCELEROMETER = BNO055_ACCEL_DATA_X_LSB_ADDR,
    VECTOR_MAGNETOMETER  = BNO055_MAG_DATA_X_LSB_ADDR,
    VECTOR_GYROSCOPE     = BNO055_GYRO_DATA_X_LSB_ADDR,
    VECTOR_EULER         = BNO055_EULER_H_LSB_ADDR,
    VECTOR_LINEARACCEL   = BNO055_LINEAR_ACCEL_DATA_X_LSB_ADDR,
    VECTOR_GRAVITY       = BNO055_GRAVITY_DATA_X_LSB_ADDR
} bno055_vector_type_t;

// Axis Remap Configuration
typedef enum {
    REMAP_CONFIG_P0 = 0x21,
    REMAP_CONFIG_P1 = 0x24,  // default
    REMAP_CONFIG_P2 = 0x24,
    REMAP_CONFIG_P3 = 0x21,
    REMAP_CONFIG_P4 = 0x24,
    REMAP_CONFIG_P5 = 0x21,
    REMAP_CONFIG_P6 = 0x21,
    REMAP_CONFIG_P7 = 0x24
} bno055_axis_remap_config_t;

// Axis Remap Sign
typedef enum {
    REMAP_SIGN_P0 = 0x04,
    REMAP_SIGN_P1 = 0x00,  // default
    REMAP_SIGN_P2 = 0x06,
    REMAP_SIGN_P3 = 0x02,
    REMAP_SIGN_P4 = 0x03,
    REMAP_SIGN_P5 = 0x01,
    REMAP_SIGN_P6 = 0x07,
    REMAP_SIGN_P7 = 0x05
} bno055_axis_remap_sign_t;

// Revision Info Structure
typedef struct {
    uint8_t  accel_rev;
    uint8_t  mag_rev;
    uint8_t  gyro_rev;
    uint16_t sw_rev;
    uint8_t  bl_rev;
} bno055_rev_info_t;

// Calibration Offsets Structure
typedef struct {
    int16_t accel_offset_x;
    int16_t accel_offset_y;
    int16_t accel_offset_z;
    int16_t mag_offset_x;
    int16_t mag_offset_y;
    int16_t mag_offset_z;
    int16_t gyro_offset_x;
    int16_t gyro_offset_y;
    int16_t gyro_offset_z;
    int16_t accel_radius;
    int16_t mag_radius;
} bno055_offsets_t;

/*!
 * @brief BNO055 ESP-IDF I2C Driver Class
 *
 * This class provides BNO055 sensor access using ESP-IDF's low-level I2C driver.
 * The initialization sequence and data conversion logic are based on Adafruit_BNO055,
 * ensuring compatibility and proven reliability.
 */
class BNO055_ESPIDF {
public:
    BNO055_ESPIDF();

    // Initialization
    // Initialize I2C communication with ESP-IDF driver
    // ESP-IDF I2Cドライバで通信を初期化
    bool begin(uint8_t addr, uint8_t sda, uint8_t scl, uint32_t speed,
               i2c_port_t port = I2C_NUM_1,
               bno055_opmode_t mode = OPERATION_MODE_NDOF);

    // Mode Control
    // 動作モード制御
    void setMode(bno055_opmode_t mode);
    bno055_opmode_t getMode();

    // Axis Remapping
    // 軸マッピング
    void setAxisRemap(bno055_axis_remap_config_t remapcode);
    void setAxisSign(bno055_axis_remap_sign_t remapsign);

    // External Crystal
    // 外部クリスタル設定
    void setExtCrystalUse(bool usextal);

    // Status and Diagnostics
    // ステータスと診断
    void getSystemStatus(uint8_t *system_status, uint8_t *self_test_result,
                        uint8_t *system_error);
    void getCalibration(uint8_t *system, uint8_t *gyro, uint8_t *accel, uint8_t *mag);
    void getRevInfo(bno055_rev_info_t *info);
    bool isFullyCalibrated();

    // Data Acquisition
    // データ取得
    imu::Vector<3> getVector(bno055_vector_type_t vector_type);
    imu::Quaternion getQuat();
    int8_t getTemp();

    // Calibration Data Management
    // キャリブレーションデータ管理
    bool getSensorOffsets(uint8_t *calibData);
    bool getSensorOffsets(bno055_offsets_t &offsets_type);
    void setSensorOffsets(const uint8_t *calibData);
    void setSensorOffsets(const bno055_offsets_t &offsets_type);

    // Power Management
    // 電源管理
    void enterSuspendMode();
    void enterNormalMode();

    // I2C Driver Initialization Guard
    // I2Cドライバ初期化ガード（Unit Rollerと同じパターン）
    static bool initialized_i2c1;

private:
    // ESP-IDF I2C Transport Layer (置き換え対象)
    // ESP-IDF I2Cトランスポート層
    bool write8(uint8_t reg, uint8_t value);
    uint8_t read8(uint8_t reg);
    bool readLen(uint8_t reg, uint8_t *buffer, uint8_t len);

    // I2C Configuration
    // I2C設定
    uint8_t _addr;
    uint8_t _sda;
    uint8_t _scl;
    uint32_t _speed;
    i2c_port_t _port;

    // Current Operating Mode
    // 現在の動作モード
    bno055_opmode_t _mode;
};

#endif  // __BNO055_ESPIDF_HPP__
