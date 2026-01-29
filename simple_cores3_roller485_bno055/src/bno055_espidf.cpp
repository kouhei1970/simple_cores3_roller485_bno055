/*!
 * BNO055 ESP-IDF I2C Driver Implementation
 * Based on Adafruit_BNO055 library with ESP-IDF I2C transport layer
 *
 * This implementation reuses Adafruit's proven initialization sequence,
 * data conversion logic, and timing delays, ensuring reliable operation.
 *
 * Only the I2C communication layer (write8, read8, readLen) is replaced
 * with ESP-IDF's low-level I2C driver following the Unit Roller pattern.
 */

#include "bno055_espidf.hpp"
#include <string.h>

// Static initialization guard (same pattern as Unit Roller)
// 静的初期化ガード（Unit Rollerと同じパターン）
bool BNO055_ESPIDF::initialized_i2c1 = false;

/*!
 * @brief Constructor
 * @brief コンストラクタ
 */
BNO055_ESPIDF::BNO055_ESPIDF() {
    _addr = BNO055_ADDRESS_A;
    _sda = 0;
    _scl = 0;
    _speed = 100000;
    _port = I2C_NUM_1;
    _mode = OPERATION_MODE_NDOF;
}

// ============================================================================
// ESP-IDF I2C Transport Layer (置き換え部分)
// ============================================================================

/*!
 * @brief Write a single byte to a register
 * @brief レジスタに1バイト書き込み
 * @param reg Register address / レジスタアドレス
 * @param value Value to write / 書き込む値
 * @return true if successful / 成功時true
 */
bool BNO055_ESPIDF::write8(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK);
}

/*!
 * @brief Read a single byte from a register
 * @brief レジスタから1バイト読み取り
 * @param reg Register address / レジスタアドレス
 * @return Register value / レジスタ値
 */
uint8_t BNO055_ESPIDF::read8(uint8_t reg) {
    uint8_t data = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);  // Repeated start
    i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return data;
}

/*!
 * @brief Read multiple bytes from a register
 * @brief レジスタから複数バイト読み取り
 * @param reg Register address / レジスタアドレス
 * @param buffer Buffer to store data / データ格納バッファ
 * @param len Number of bytes to read / 読み取るバイト数
 * @return true if successful / 成功時true
 */
bool BNO055_ESPIDF::readLen(uint8_t reg, uint8_t *buffer, uint8_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);  // Repeated start
    i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, buffer, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK);
}

// ============================================================================
// Initialization (Adafruit_BNO055 logic reused)
// ============================================================================

/*!
 * @brief Initialize BNO055 with ESP-IDF I2C driver
 * @brief BNO055をESP-IDF I2Cドライバで初期化
 *
 * Initialization sequence based on Adafruit_BNO055::begin()
 * 初期化シーケンスはAdafruit_BNO055::begin()に基づく
 *
 * @param addr I2C address (0x28 or 0x29) / I2Cアドレス
 * @param sda SDA pin / SDAピン
 * @param scl SCL pin / SCLピン
 * @param speed I2C clock speed / I2Cクロック速度
 * @param port I2C port (default: I2C_NUM_1) / I2Cポート
 * @param mode Operating mode / 動作モード
 * @return true if successful / 成功時true
 */
bool BNO055_ESPIDF::begin(uint8_t addr, uint8_t sda, uint8_t scl, uint32_t speed,
                          i2c_port_t port, bno055_opmode_t mode) {
    _addr = addr;
    _sda = sda;
    _scl = scl;
    _speed = speed;
    _port = port;

    // Configure ESP-IDF I2C driver
    // ESP-IDF I2Cドライバを設定
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.scl_io_num = scl;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = speed;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;

    i2c_param_config(port, &conf);

    // Install I2C driver (prevent double initialization)
    // I2Cドライバをインストール（二重初期化を防ぐ）
    if (!initialized_i2c1) {
        i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
        initialized_i2c1 = true;
    }

    delay(10);

    // ========================================================================
    // Following initialization sequence from Adafruit_BNO055::begin()
    // Adafruit_BNO055::begin()の初期化シーケンスに従う
    // ========================================================================

    // BNO055 can take 850ms to boot
    // BNO055は起動に最大850msかかる
    int timeout = 850;  // in ms
    uint8_t id = 0;

    while (timeout > 0) {
        id = read8(BNO055_CHIP_ID_ADDR);
        if (id == BNO055_ID) {
            break;
        }
        delay(10);
        timeout -= 10;
    }

    if (timeout <= 0) {
        return false;  // Chip not detected / チップ検出失敗
    }

    // Make sure we have the right device
    // 正しいデバイスであることを確認
    if (id != BNO055_ID) {
        delay(1000);  // Hold on for boot / 起動を待つ
        id = read8(BNO055_CHIP_ID_ADDR);
        if (id != BNO055_ID) {
            return false;  // Still not? OK bail / まだ違う？中止
        }
    }

    // Switch to config mode (just in case since this is the default)
    // コンフィグモードに切り替え（念のため、デフォルト）
    setMode(OPERATION_MODE_CONFIG);

    // Reset
    // リセット
    write8(BNO055_SYS_TRIGGER_ADDR, 0x20);
    // Delay increased to 30ms due to power issues
    // 電源問題のため30msに増加
    delay(30);

    while (read8(BNO055_CHIP_ID_ADDR) != BNO055_ID) {
        delay(10);
    }
    delay(50);

    // Set to normal power mode
    // 通常電源モードに設定
    write8(BNO055_PWR_MODE_ADDR, POWER_MODE_NORMAL);
    delay(10);

    write8(BNO055_PAGE_ID_ADDR, 0);

    // Set the output units
    // 出力単位を設定
    /*
    uint8_t unitsel = (0 << 7) |  // Orientation = Android
                      (0 << 4) |  // Temperature = Celsius
                      (0 << 2) |  // Euler = Degrees
                      (1 << 1) |  // Gyro = Rads
                      (0 << 0);   // Accelerometer = m/s^2
    write8(BNO055_UNIT_SEL_ADDR, unitsel);
    */

    // Configure axis mapping (see section 3.4)
    // 軸マッピングを設定
    /*
    write8(BNO055_AXIS_MAP_CONFIG_ADDR, REMAP_CONFIG_P2);  // P0-P7, Default is P1
    delay(10);
    write8(BNO055_AXIS_MAP_SIGN_ADDR, REMAP_SIGN_P2);  // P0-P7, Default is P1
    delay(10);
    */

    write8(BNO055_SYS_TRIGGER_ADDR, 0x0);
    delay(10);

    // Set the requested operating mode (see section 3.3)
    // 要求された動作モードを設定
    setMode(mode);
    delay(20);

    return true;
}

// ============================================================================
// Mode Control (Adafruit_BNO055 logic reused)
// ============================================================================

/*!
 * @brief Set operating mode
 * @brief 動作モードを設定
 * @param mode Operating mode / 動作モード
 */
void BNO055_ESPIDF::setMode(bno055_opmode_t mode) {
    _mode = mode;
    write8(BNO055_OPR_MODE_ADDR, _mode);
    delay(30);
}

/*!
 * @brief Get current operating mode
 * @brief 現在の動作モードを取得
 * @return Operating mode / 動作モード
 */
bno055_opmode_t BNO055_ESPIDF::getMode() {
    return (bno055_opmode_t)read8(BNO055_OPR_MODE_ADDR);
}

// ============================================================================
// Data Acquisition (Adafruit_BNO055 logic reused)
// ============================================================================

/*!
 * @brief Get vector reading from specified source
 * @brief 指定されたソースからベクトル値を取得
 *
 * Data conversion based on Adafruit_BNO055::getVector()
 * データ変換はAdafruit_BNO055::getVector()に基づく
 *
 * @param vector_type Type of vector data / ベクトルデータの種類
 * @return 3D vector / 3Dベクトル
 */
imu::Vector<3> BNO055_ESPIDF::getVector(bno055_vector_type_t vector_type) {
    imu::Vector<3> xyz;
    uint8_t buffer[6];
    memset(buffer, 0, 6);

    int16_t x, y, z;
    x = y = z = 0;

    // Read vector data (6 bytes)
    // ベクトルデータを読み取り（6バイト）
    readLen((uint8_t)vector_type, buffer, 6);

    x = ((int16_t)buffer[0]) | (((int16_t)buffer[1]) << 8);
    y = ((int16_t)buffer[2]) | (((int16_t)buffer[3]) << 8);
    z = ((int16_t)buffer[4]) | (((int16_t)buffer[5]) << 8);

    // Convert the value to an appropriate range (section 3.6.4)
    // 値を適切な範囲に変換（BNO055データシート 3.6.4節）
    switch (vector_type) {
    case VECTOR_MAGNETOMETER:
        // 1uT = 16 LSB
        xyz[0] = ((double)x) / 16.0;
        xyz[1] = ((double)y) / 16.0;
        xyz[2] = ((double)z) / 16.0;
        break;
    case VECTOR_GYROSCOPE:
        // 1dps = 16 LSB (or 1 rad/s = 900 LSB depending on unit setting)
        // Default: degrees per second
        xyz[0] = ((double)x) / 16.0;
        xyz[1] = ((double)y) / 16.0;
        xyz[2] = ((double)z) / 16.0;
        break;
    case VECTOR_EULER:
        // 1 degree = 16 LSB
        xyz[0] = ((double)x) / 16.0;
        xyz[1] = ((double)y) / 16.0;
        xyz[2] = ((double)z) / 16.0;
        break;
    case VECTOR_ACCELEROMETER:
    case VECTOR_LINEARACCEL:
    case VECTOR_GRAVITY:
        // 1m/s^2 = 100 LSB
        xyz[0] = ((double)x) / 100.0;
        xyz[1] = ((double)y) / 100.0;
        xyz[2] = ((double)z) / 100.0;
        break;
    }

    return xyz;
}

/*!
 * @brief Get quaternion reading
 * @brief クォータニオンを取得
 *
 * Data conversion based on Adafruit_BNO055::getQuat()
 * データ変換はAdafruit_BNO055::getQuat()に基づく
 *
 * @return Quaternion / クォータニオン
 */
imu::Quaternion BNO055_ESPIDF::getQuat() {
    uint8_t buffer[8];
    memset(buffer, 0, 8);

    int16_t x, y, z, w;
    x = y = z = w = 0;

    // Read quat data (8 bytes)
    // クォータニオンデータを読み取り（8バイト）
    readLen(BNO055_QUATERNION_DATA_W_LSB_ADDR, buffer, 8);

    w = (((uint16_t)buffer[1]) << 8) | ((uint16_t)buffer[0]);
    x = (((uint16_t)buffer[3]) << 8) | ((uint16_t)buffer[2]);
    y = (((uint16_t)buffer[5]) << 8) | ((uint16_t)buffer[4]);
    z = (((uint16_t)buffer[7]) << 8) | ((uint16_t)buffer[6]);

    // Assign to Quaternion
    // クォータニオンに割り当て
    // See BNO055 datasheet 3.6.5.5 Orientation (Quaternion)
    // 1 Quaternion = 2^14 LSB
    const double scale = (1.0 / (1 << 14));
    imu::Quaternion quat(scale * w, scale * x, scale * y, scale * z);
    return quat;
}

/*!
 * @brief Get temperature
 * @brief 温度を取得
 * @return Temperature in Celsius / 摂氏温度
 */
int8_t BNO055_ESPIDF::getTemp() {
    int8_t temp = (int8_t)(read8(BNO055_TEMP_ADDR));
    return temp;
}

// ============================================================================
// Status and Diagnostics (Adafruit_BNO055 logic reused)
// ============================================================================

/*!
 * @brief Get system status information
 * @brief システムステータス情報を取得
 * @param system_status System status / システムステータス
 * @param self_test_result Self-test result / セルフテスト結果
 * @param system_error System error / システムエラー
 */
void BNO055_ESPIDF::getSystemStatus(uint8_t *system_status,
                                    uint8_t *self_test_result,
                                    uint8_t *system_error) {
    write8(BNO055_PAGE_ID_ADDR, 0);

    // System Status (see section 4.3.58)
    // 0 = Idle
    // 1 = System Error
    // 2 = Initializing Peripherals
    // 3 = System Initialization
    // 4 = Executing Self-Test
    // 5 = Sensor fusion algorithm running
    // 6 = System running without fusion algorithms
    if (system_status != 0) {
        *system_status = read8(BNO055_SYS_STAT_ADDR);
    }

    // Self Test Results (see section 4.3.59)
    // 1 = test passed, 0 = test failed
    // Bit 0 = Accelerometer self test
    // Bit 1 = Magnetometer self test
    // Bit 2 = Gyroscope self test
    // Bit 3 = MCU self test
    // 0x0F = all good!
    if (self_test_result != 0) {
        *self_test_result = read8(BNO055_SELFTEST_RESULT_ADDR);
    }

    // System Error (see section 4.3.59)
    // 0 = No error
    // 1 = Peripheral initialization error
    // 2 = System initialization error
    // 3 = Self test result failed
    // 4 = Register map value out of range
    // 5 = Register map address out of range
    // 6 = Register map write error
    // 7 = BNO low power mode not available for selected operation mode
    // 8 = Accelerometer power mode not available
    // 9 = Fusion algorithm configuration error
    // A = Sensor configuration error
    if (system_error != 0) {
        *system_error = read8(BNO055_SYS_ERR_ADDR);
    }

    delay(200);
}

/*!
 * @brief Get calibration status
 * @brief キャリブレーションステータスを取得
 * @param system System calibration (0-3) / システムキャリブレーション
 * @param gyro Gyroscope calibration (0-3) / ジャイロキャリブレーション
 * @param accel Accelerometer calibration (0-3) / 加速度計キャリブレーション
 * @param mag Magnetometer calibration (0-3) / 磁気計キャリブレーション
 */
void BNO055_ESPIDF::getCalibration(uint8_t *system, uint8_t *gyro,
                                   uint8_t *accel, uint8_t *mag) {
    uint8_t calData = read8(BNO055_CALIB_STAT_ADDR);
    if (system != 0) {
        *system = (calData >> 6) & 0x03;
    }
    if (gyro != 0) {
        *gyro = (calData >> 4) & 0x03;
    }
    if (accel != 0) {
        *accel = (calData >> 2) & 0x03;
    }
    if (mag != 0) {
        *mag = calData & 0x03;
    }
}

/*!
 * @brief Get chip revision information
 * @brief チップリビジョン情報を取得
 * @param info Revision info structure / リビジョン情報構造体
 */
void BNO055_ESPIDF::getRevInfo(bno055_rev_info_t *info) {
    uint8_t a, b;
    memset(info, 0, sizeof(bno055_rev_info_t));

    info->accel_rev = read8(BNO055_ACCEL_REV_ID_ADDR);
    info->mag_rev = read8(BNO055_MAG_REV_ID_ADDR);
    info->gyro_rev = read8(BNO055_GYRO_REV_ID_ADDR);
    info->bl_rev = read8(BNO055_BL_REV_ID_ADDR);

    a = read8(BNO055_SW_REV_ID_LSB_ADDR);
    b = read8(BNO055_SW_REV_ID_MSB_ADDR);
    info->sw_rev = (((uint16_t)b) << 8) | ((uint16_t)a);
}

/*!
 * @brief Check if sensor is fully calibrated
 * @brief センサーが完全にキャリブレーションされているかチェック
 * @return true if fully calibrated / 完全キャリブレーション時true
 */
bool BNO055_ESPIDF::isFullyCalibrated() {
    uint8_t system, gyro, accel, mag;
    getCalibration(&system, &gyro, &accel, &mag);

    switch (_mode) {
    case OPERATION_MODE_ACCONLY:
        return (accel == 3);
    case OPERATION_MODE_MAGONLY:
        return (mag == 3);
    case OPERATION_MODE_GYRONLY:
    case OPERATION_MODE_M4G:  // No magnetometer calibration required
        return (gyro == 3);
    case OPERATION_MODE_ACCMAG:
    case OPERATION_MODE_COMPASS:
        return (accel == 3 && mag == 3);
    case OPERATION_MODE_ACCGYRO:
    case OPERATION_MODE_IMUPLUS:
        return (accel == 3 && gyro == 3);
    case OPERATION_MODE_MAGGYRO:
        return (mag == 3 && gyro == 3);
    default:
        return (system == 3 && gyro == 3 && accel == 3 && mag == 3);
    }
}

// ============================================================================
// Axis Remapping (Adafruit_BNO055 logic reused)
// ============================================================================

/*!
 * @brief Set axis remap configuration
 * @brief 軸マッピング設定
 * @param remapcode Remap configuration / マッピング設定
 */
void BNO055_ESPIDF::setAxisRemap(bno055_axis_remap_config_t remapcode) {
    bno055_opmode_t modeback = _mode;

    setMode(OPERATION_MODE_CONFIG);
    delay(25);
    write8(BNO055_AXIS_MAP_CONFIG_ADDR, remapcode);
    delay(10);
    setMode(modeback);
    delay(20);
}

/*!
 * @brief Set axis remap sign
 * @brief 軸マッピング符号設定
 * @param remapsign Remap sign / マッピング符号
 */
void BNO055_ESPIDF::setAxisSign(bno055_axis_remap_sign_t remapsign) {
    bno055_opmode_t modeback = _mode;

    setMode(OPERATION_MODE_CONFIG);
    delay(25);
    write8(BNO055_AXIS_MAP_SIGN_ADDR, remapsign);
    delay(10);
    setMode(modeback);
    delay(20);
}

// ============================================================================
// External Crystal (Adafruit_BNO055 logic reused)
// ============================================================================

/*!
 * @brief Use external 32.768KHz crystal
 * @brief 外部32.768KHzクリスタルを使用
 * @param usextal true to use external crystal / 外部クリスタル使用時true
 */
void BNO055_ESPIDF::setExtCrystalUse(bool usextal) {
    bno055_opmode_t modeback = _mode;

    setMode(OPERATION_MODE_CONFIG);
    delay(25);
    write8(BNO055_PAGE_ID_ADDR, 0);
    if (usextal) {
        write8(BNO055_SYS_TRIGGER_ADDR, 0x80);
    } else {
        write8(BNO055_SYS_TRIGGER_ADDR, 0x00);
    }
    delay(10);
    setMode(modeback);
    delay(20);
}

// ============================================================================
// Power Management (Adafruit_BNO055 logic reused)
// ============================================================================

/*!
 * @brief Enter suspend mode (sleep)
 * @brief サスペンドモードに入る（スリープ）
 */
void BNO055_ESPIDF::enterSuspendMode() {
    bno055_opmode_t modeback = _mode;

    setMode(OPERATION_MODE_CONFIG);
    delay(25);
    write8(BNO055_PWR_MODE_ADDR, 0x02);
    setMode(modeback);
    delay(20);
}

/*!
 * @brief Enter normal mode (wake)
 * @brief ノーマルモードに入る（ウェイク）
 */
void BNO055_ESPIDF::enterNormalMode() {
    bno055_opmode_t modeback = _mode;

    setMode(OPERATION_MODE_CONFIG);
    delay(25);
    write8(BNO055_PWR_MODE_ADDR, 0x00);
    setMode(modeback);
    delay(20);
}

// ============================================================================
// Calibration Data Management (Adafruit_BNO055 logic reused)
// ============================================================================

/*!
 * @brief Get sensor calibration offsets (raw bytes)
 * @brief センサーキャリブレーションオフセットを取得（生バイト）
 * @param calibData Buffer for 22 bytes of calibration data / 22バイトのキャリブレーションデータバッファ
 * @return true if successful / 成功時true
 */
bool BNO055_ESPIDF::getSensorOffsets(uint8_t *calibData) {
    if (isFullyCalibrated()) {
        bno055_opmode_t lastMode = _mode;
        setMode(OPERATION_MODE_CONFIG);

        readLen(ACCEL_OFFSET_X_LSB_ADDR, calibData, 22);

        setMode(lastMode);
        return true;
    }
    return false;
}

/*!
 * @brief Get sensor calibration offsets (structured)
 * @brief センサーキャリブレーションオフセットを取得（構造化）
 * @param offsets_type Offsets structure / オフセット構造体
 * @return true if successful / 成功時true
 */
bool BNO055_ESPIDF::getSensorOffsets(bno055_offsets_t &offsets_type) {
    if (isFullyCalibrated()) {
        bno055_opmode_t lastMode = _mode;
        setMode(OPERATION_MODE_CONFIG);
        delay(25);

        // Read offsets (22 bytes total)
        // オフセットを読み取り（合計22バイト）
        offsets_type.accel_offset_x = (read8(ACCEL_OFFSET_X_MSB_ADDR) << 8) |
                                      (read8(ACCEL_OFFSET_X_LSB_ADDR));
        offsets_type.accel_offset_y = (read8(ACCEL_OFFSET_Y_MSB_ADDR) << 8) |
                                      (read8(ACCEL_OFFSET_Y_LSB_ADDR));
        offsets_type.accel_offset_z = (read8(ACCEL_OFFSET_Z_MSB_ADDR) << 8) |
                                      (read8(ACCEL_OFFSET_Z_LSB_ADDR));

        offsets_type.mag_offset_x = (read8(MAG_OFFSET_X_MSB_ADDR) << 8) |
                                    (read8(MAG_OFFSET_X_LSB_ADDR));
        offsets_type.mag_offset_y = (read8(MAG_OFFSET_Y_MSB_ADDR) << 8) |
                                    (read8(MAG_OFFSET_Y_LSB_ADDR));
        offsets_type.mag_offset_z = (read8(MAG_OFFSET_Z_MSB_ADDR) << 8) |
                                    (read8(MAG_OFFSET_Z_LSB_ADDR));

        offsets_type.gyro_offset_x = (read8(GYRO_OFFSET_X_MSB_ADDR) << 8) |
                                     (read8(GYRO_OFFSET_X_LSB_ADDR));
        offsets_type.gyro_offset_y = (read8(GYRO_OFFSET_Y_MSB_ADDR) << 8) |
                                     (read8(GYRO_OFFSET_Y_LSB_ADDR));
        offsets_type.gyro_offset_z = (read8(GYRO_OFFSET_Z_MSB_ADDR) << 8) |
                                     (read8(GYRO_OFFSET_Z_LSB_ADDR));

        offsets_type.accel_radius = (read8(ACCEL_RADIUS_MSB_ADDR) << 8) |
                                    (read8(ACCEL_RADIUS_LSB_ADDR));
        offsets_type.mag_radius = (read8(MAG_RADIUS_MSB_ADDR) << 8) |
                                  (read8(MAG_RADIUS_LSB_ADDR));

        setMode(lastMode);
        return true;
    }
    return false;
}

/*!
 * @brief Set sensor calibration offsets (raw bytes)
 * @brief センサーキャリブレーションオフセットを設定（生バイト）
 * @param calibData Buffer with 22 bytes of calibration data / 22バイトのキャリブレーションデータバッファ
 */
void BNO055_ESPIDF::setSensorOffsets(const uint8_t *calibData) {
    bno055_opmode_t lastMode = _mode;
    setMode(OPERATION_MODE_CONFIG);
    delay(25);

    // Write offsets (22 bytes total)
    // オフセットを書き込み（合計22バイト）
    for (int i = 0; i < 22; i++) {
        write8(ACCEL_OFFSET_X_LSB_ADDR + i, calibData[i]);
    }

    setMode(lastMode);
}

/*!
 * @brief Set sensor calibration offsets (structured)
 * @brief センサーキャリブレーションオフセットを設定（構造化）
 * @param offsets_type Offsets structure / オフセット構造体
 */
void BNO055_ESPIDF::setSensorOffsets(const bno055_offsets_t &offsets_type) {
    bno055_opmode_t lastMode = _mode;
    setMode(OPERATION_MODE_CONFIG);
    delay(25);

    // Write offsets
    // オフセットを書き込み
    write8(ACCEL_OFFSET_X_LSB_ADDR, (offsets_type.accel_offset_x) & 0x0FF);
    write8(ACCEL_OFFSET_X_MSB_ADDR, (offsets_type.accel_offset_x >> 8) & 0x0FF);
    write8(ACCEL_OFFSET_Y_LSB_ADDR, (offsets_type.accel_offset_y) & 0x0FF);
    write8(ACCEL_OFFSET_Y_MSB_ADDR, (offsets_type.accel_offset_y >> 8) & 0x0FF);
    write8(ACCEL_OFFSET_Z_LSB_ADDR, (offsets_type.accel_offset_z) & 0x0FF);
    write8(ACCEL_OFFSET_Z_MSB_ADDR, (offsets_type.accel_offset_z >> 8) & 0x0FF);

    write8(MAG_OFFSET_X_LSB_ADDR, (offsets_type.mag_offset_x) & 0x0FF);
    write8(MAG_OFFSET_X_MSB_ADDR, (offsets_type.mag_offset_x >> 8) & 0x0FF);
    write8(MAG_OFFSET_Y_LSB_ADDR, (offsets_type.mag_offset_y) & 0x0FF);
    write8(MAG_OFFSET_Y_MSB_ADDR, (offsets_type.mag_offset_y >> 8) & 0x0FF);
    write8(MAG_OFFSET_Z_LSB_ADDR, (offsets_type.mag_offset_z) & 0x0FF);
    write8(MAG_OFFSET_Z_MSB_ADDR, (offsets_type.mag_offset_z >> 8) & 0x0FF);

    write8(GYRO_OFFSET_X_LSB_ADDR, (offsets_type.gyro_offset_x) & 0x0FF);
    write8(GYRO_OFFSET_X_MSB_ADDR, (offsets_type.gyro_offset_x >> 8) & 0x0FF);
    write8(GYRO_OFFSET_Y_LSB_ADDR, (offsets_type.gyro_offset_y) & 0x0FF);
    write8(GYRO_OFFSET_Y_MSB_ADDR, (offsets_type.gyro_offset_y >> 8) & 0x0FF);
    write8(GYRO_OFFSET_Z_LSB_ADDR, (offsets_type.gyro_offset_z) & 0x0FF);
    write8(GYRO_OFFSET_Z_MSB_ADDR, (offsets_type.gyro_offset_z >> 8) & 0x0FF);

    write8(ACCEL_RADIUS_LSB_ADDR, (offsets_type.accel_radius) & 0x0FF);
    write8(ACCEL_RADIUS_MSB_ADDR, (offsets_type.accel_radius >> 8) & 0x0FF);

    write8(MAG_RADIUS_LSB_ADDR, (offsets_type.mag_radius) & 0x0FF);
    write8(MAG_RADIUS_MSB_ADDR, (offsets_type.mag_radius >> 8) & 0x0FF);

    setMode(lastMode);
}
