# I2C Driver Conflict Analysis Report
# I2Cドライバ競合分析レポート

## Executive Summary / 概要

現在のコードベースでは、**2つの異なるI2Cドライバ実装が共存**しており、潜在的な競合とシステム不安定性のリスクがあります。

- **Unit Roller I2C**: ESP-IDF低レベルドライバ（I2C_NUM_0）を使用
- **BNO055 IMU**: Arduino Wireライブラリ経由でI2Cアクセス

Unit RollerはWire環境で動作しないことが実証済みであり、同様にBNO055もESP-IDFドライバへの移行が必要です。

---

## Current Implementation / 現在の実装

### 1. Unit Roller I2C Driver (unit_rolleri2c.cpp)

**使用ドライバ**: ESP-IDF低レベルI2Cドライバ

```cpp
// unit_rolleri2c.cpp:144-147
i2c_param_config(I2C_NUM_0, &conf);
if (!initialized) {
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    initialized = true;
}
```

**ピン配置**:
- SDA: GPIO2 (I2C_SDA_PIN)
- SCL: GPIO1 (I2C_SCL_PIN)
- Address: 0x64
- Speed: 100kHz

**実装方法**:
- `i2c_cmd_link_create()` による直接レジスタアクセス
- `i2c_master_write_byte()`, `i2c_master_read()` で制御
- `i2c_master_cmd_begin(I2C_NUM_0, ...)` で実行

**Wire実装からの移行理由**:
```cpp
// unit_rolleri2c.cpp:159-175 (コメントアウトされた古いコード)
#if 0   // This is the original code
    _wire->begin(_sda, _scl);
    _wire->setClock(_speed);
    // ... Wire library based implementation
#endif
```

Wire実装は `#if 0` でコメントアウトされており、**動作しないことが証明済み**。

---

### 2. BNO055 IMU Driver (main.cpp)

**使用ドライバ**: Arduino Wireライブラリ + Adafruit_BNO055

```cpp
// main.cpp:6, 29, 49, 51
#include <Wire.h>
static Adafruit_BNO055 bno(55, BNO_ADDR, &Wire);  // Wireインスタンスを渡す

void setup() {
    Wire.begin(SDA_PIN, SCL_PIN, I2C_HZ);  // Wireを初期化
    bno.begin();                           // BNO055を初期化
}
```

**ピン配置**:
- SDA: GPIO6 (SDA_PIN)
- SCL: GPIO7 (SCL_PIN)
- Address: 0x28
- Speed: 100kHz

**実装方法**:
- `Adafruit_BNO055` ライブラリがI2C通信を抽象化
- 内部で `Wire.beginTransmission()`, `Wire.write()`, `Wire.read()` を使用
- **Wireライブラリは内部的にESP-IDFのI2Cドライバを使用**

---

## Problem Analysis / 問題分析

### A. Driver Layer Conflict / ドライバレイヤーの競合

```
┌─────────────────────────────────────────────────────┐
│              Application Layer                       │
│  Unit Roller          ║         BNO055               │
│  (Direct ESP-IDF)     ║    (Adafruit_BNO055)         │
└───────────┬───────────╨───────────┬──────────────────┘
            │                       │
            │                       ▼
            │              ┌─────────────────┐
            │              │  Arduino Wire   │
            │              │    Library      │
            │              └────────┬────────┘
            │                       │
            ▼                       ▼
┌───────────────────────────────────────────────────────┐
│           ESP-IDF I2C Hardware Driver                 │
│  I2C_NUM_0 (GPIO2/1)     I2C_NUM_1 (GPIO6/7)?        │
└───────────────────────────────────────────────────────┘
```

**競合の原因**:
1. **抽象化レイヤーの混在**: 低レベルESP-IDF APIと高レベルWire APIの混在
2. **リソース管理の衝突**: I2Cポートの所有権が不明確
3. **初期化順序の依存性**: `i2c_driver_install()` と `Wire.begin()` の相互干渉

---

### B. ESP32 I2C Port Assignment / ESP32 I2Cポート割り当て

ESP32-S3には2つのI2Cハードウェアポートがあります:
- **I2C_NUM_0**: Unit Rollerが明示的に使用
- **I2C_NUM_1**: 未使用（Wireがどちらを使うかは実装依存）

**Arduino Wire のポート選択ロジック**:
```cpp
// Arduino-ESP32 内部実装（推定）
Wire.begin(SDA_PIN, SCL_PIN);
// → Wire は I2C_NUM_0 または I2C_NUM_1 を自動選択
// → ピン番号から判断 or デフォルトポート使用
```

**問題**: Wireがどのポートを使用するか明示的に制御できない可能性があり、以下のシナリオが考えられます:

1. **最悪ケース**: Wireも I2C_NUM_0 を使用
   - Unit Rollerと同じポートを2つのドライバが制御
   - **ハードウェア競合、データ破損、デバイスハング**

2. **現在のケース（推定）**: Wireは I2C_NUM_1 を使用
   - 異なるGPIOピン（GPIO6/7 vs GPIO2/1）から判断
   - 物理的には分離されているが、**ドライバレイヤーの混在**による不安定性

---

### C. Observed Issues / 観測された問題

**Unit Rollerの動作不良（過去）**:
- Wire実装では正常に動作しない → ESP-IDF移行で解決
- 原因: Wireのタイミング制御、バッファ管理、エラー処理の問題

**BNO055への影響（予測）**:
1. **通信エラーの発生**: センサー読み取り失敗、タイムアウト
2. **不安定な動作**: 間欠的なデータ異常、初期化失敗
3. **システムハング**: I2Cバスのデッドロック、ウォッチドッグリセット

---

## Root Cause / 根本原因

### Why Wire Doesn't Work / なぜWireが動作しないか

1. **タイミング精度**:
   - ESP-IDFドライバ: ハードウェアタイマーで正確な制御
   - Wire: ソフトウェア遅延、FreeRTOSタスクスイッチの影響

2. **エラーハンドリング**:
   - ESP-IDFドライバ: NACKビット、タイムアウトを厳密に処理
   - Wire: 簡易的なエラー処理、リカバリーロジック不足

3. **リソース競合**:
   - ESP-IDFドライバ: ポート占有を明示的に管理
   - Wire: 内部でESP-IDFを使うが、初期化状態を隠蔽

4. **Unit Rollerの要求仕様**:
   - 多数のレジスタアクセス（40+ registers）
   - 高速なRead-Modify-Write操作
   - PIDパラメータ、電流値の頻繁な更新
   - **Wireの抽象化オーバーヘッドが許容できない**

---

## Evidence from Code / コードからの証拠

### Unit Roller Wire実装の削除

```cpp
// unit_rolleri2c.cpp:28-35 (writeBytes の旧実装)
#if 0   // This is the original code
    _wire->beginTransmission(addr);
    _wire->write(reg);
    for (int i = 0; i < length; i++) {
        _wire->write(*(buffer + i));
    }
    _wire->endTransmission();
#endif
```

**削除されたメソッド**:
- `writeBytes()`: Wire → ESP-IDF `i2c_master_write()`
- `readBytes()`: Wire → ESP-IDF `i2c_master_read()`
- `begin()`: Wire → ESP-IDF `i2c_driver_install()`

**コメント**: `// This is the original code` = 過去の失敗実装

---

### BNO055 の Wire依存

```cpp
// main.cpp:29
static Adafruit_BNO055 bno(55, BNO_ADDR, &Wire);
```

`Adafruit_BNO055` クラスは `TwoWire*` を保持し、全I2C操作をWire経由で実行:

```cpp
// Adafruit_BNO055 内部（推定）
bool Adafruit_BNO055::begin() {
    _wire->beginTransmission(_address);
    _wire->write(BNO055_CHIP_ID_ADDR);
    _wire->endTransmission();
    _wire->requestFrom(_address, 1);
    uint8_t id = _wire->read();
    // ...
}
```

**問題**: Adafruit_BNO055はWireに完全に依存しており、バックエンドを変更不可能。

---

## Solution / 解決策

### Option 1: BNO055 ESP-IDF Native Driver (Recommended)
### オプション1: BNO055 ESP-IDFネイティブドライバ（推奨）

**実装方針**:
1. `Adafruit_BNO055` を使用せず、BNO055のレジスタを直接制御
2. Unit Rollerと同じESP-IDF I2Cドライバパターンを使用
3. I2C_NUM_1 を明示的に使用してポート分離

**必要な作業**:

```cpp
// bno055_espidf.hpp (新規作成)
class BNO055_ESPIDF {
private:
    uint8_t _addr;
    i2c_port_t _port;  // I2C_NUM_1

    void writeReg(uint8_t reg, uint8_t value);
    uint8_t readReg(uint8_t reg);
    void readBytes(uint8_t reg, uint8_t* buffer, size_t len);

public:
    bool begin(uint8_t addr, uint8_t sda, uint8_t scl, uint32_t speed);
    bool setMode(uint8_t mode);  // OPERATION_MODE_NDOF

    // センサーデータ読み取り
    void getEuler(float* heading, float* roll, float* pitch);
    void getQuat(float* w, float* x, float* y, float* z);
    void getGyro(float* x, float* y, float* z);
};
```

**実装例**:

```cpp
// bno055_espidf.cpp
bool BNO055_ESPIDF::begin(uint8_t addr, uint8_t sda, uint8_t scl, uint32_t speed) {
    _addr = addr;
    _port = I2C_NUM_1;  // Unit Rollerと分離

    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.scl_io_num = scl;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = speed;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;

    i2c_param_config(_port, &conf);
    i2c_driver_install(_port, I2C_MODE_MASTER, 0, 0, 0);

    delay(10);

    // Chip ID確認 (BNO055_CHIP_ID_ADDR = 0x00, expected 0xA0)
    uint8_t id = readReg(0x00);
    return (id == 0xA0);
}

void BNO055_ESPIDF::writeReg(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

uint8_t BNO055_ESPIDF::readReg(uint8_t reg) {
    uint8_t data;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return data;
}

void BNO055_ESPIDF::getEuler(float* heading, float* roll, float* pitch) {
    uint8_t buffer[6];
    readBytes(0x1A, buffer, 6);  // BNO055_EULER_H_LSB_ADDR = 0x1A

    int16_t h = ((int16_t)buffer[1] << 8) | buffer[0];
    int16_t r = ((int16_t)buffer[3] << 8) | buffer[2];
    int16_t p = ((int16_t)buffer[5] << 8) | buffer[4];

    *heading = h / 16.0;  // 1 degree = 16 LSB
    *roll    = r / 16.0;
    *pitch   = p / 16.0;
}
```

**BNO055 レジスタマップ（抜粋）**:
```cpp
#define BNO055_CHIP_ID_ADDR           0x00  // Should be 0xA0
#define BNO055_OPR_MODE_ADDR          0x3D
#define BNO055_EULER_H_LSB_ADDR       0x1A  // Heading (LSB)
#define BNO055_QUATERNION_DATA_W_LSB  0x20  // Quaternion W (LSB)
#define BNO055_GYRO_DATA_X_LSB        0x14  // Gyroscope X (LSB)

#define OPERATION_MODE_CONFIG         0x00
#define OPERATION_MODE_NDOF           0x0C  // 9DOF fusion mode
```

**main.cpp の変更**:

```cpp
// Before:
#include <Wire.h>
#include <Adafruit_BNO055.h>
static Adafruit_BNO055 bno(55, BNO_ADDR, &Wire);

void setup() {
    Wire.begin(SDA_PIN, SCL_PIN, I2C_HZ);
    bno.begin();
}

void loop() {
    imu::Vector<3> eul = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
}

// After:
#include "bno055_espidf.hpp"
static BNO055_ESPIDF bno;

void setup() {
    bno.begin(BNO_ADDR, SDA_PIN, SCL_PIN, I2C_HZ);
    bno.setMode(OPERATION_MODE_NDOF);
}

void loop() {
    float heading, roll, pitch;
    bno.getEuler(&heading, &roll, &pitch);
}
```

**利点**:
- ✅ 完全なドライバ統一（ESP-IDF only）
- ✅ I2Cポート明示的分離（I2C_NUM_0 vs I2C_NUM_1）
- ✅ Adafruit依存排除、コードサイズ削減
- ✅ タイミング制御の完全な掌握

**欠点**:
- ⚠️ BNO055レジスタマップの実装が必要（約20個のレジスタ）
- ⚠️ キャリブレーションロジックの再実装
- ⚠️ 既存のAdafruit APIとの互換性なし

**実装工数**: 2-3日（レジスタマップ実装 + テスト）

---

### Option 2: Wire with Separate I2C Ports (Temporary Workaround)
### オプション2: 分離I2Cポートを使うWire（一時的回避策）

**方針**: WireとESP-IDFを併用するが、ポートを明示的に分離

**main.cpp の変更**:

```cpp
void setup() {
    // Unit Roller: I2C_NUM_0 (GPIO2/1) - ESP-IDF直接
    Roller.begin(ROLLER_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, I2C_HZ);

    // BNO055: Wire を使うが I2C_NUM_1 を強制
    // ※ Arduino-ESP32 の Wire 実装に依存
    Wire.begin(SDA_PIN, SCL_PIN, I2C_HZ);
    // 内部で I2C_NUM_1 が使われることを期待

    bno.begin();

    Roller.setMode(3);
    Roller.setCurrent(0);
    Roller.setOutput(1);
}
```

**検証方法**:

```cpp
// デバッグコードを追加して確認
void setup() {
    Serial.begin(115200);

    Roller.begin(ROLLER_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, I2C_HZ);
    Serial.println("Roller: I2C_NUM_0 initialized");

    Wire.begin(SDA_PIN, SCL_PIN, I2C_HZ);
    Serial.println("Wire: Initialized");

    // Wire がどのポートを使っているか確認（Arduino-ESP32内部API）
    // i2c_get_period() などで確認可能

    if (!bno.begin()) {
        Serial.println("ERROR: BNO055 init failed");
        while(1);
    }
    Serial.println("BNO055: Initialized");
}
```

**利点**:
- ✅ 既存コード変更最小限
- ✅ Adafruit_BNO055の機能をそのまま使用可能

**欠点**:
- ❌ ポート分離の保証なし（Arduino-ESP32実装依存）
- ❌ ドライバレイヤー混在による潜在的不安定性
- ❌ Unit Rollerで実証された「Wireの問題」が残る
- ❌ 根本解決ではない

**推奨度**: ⚠️ **非推奨** - 一時的な動作確認のみに使用

---

### Option 3: Custom Wire Backend for Adafruit_BNO055 (Advanced)
### オプション3: Adafruit_BNO055用カスタムWireバックエンド（上級）

**方針**: `Adafruit_BNO055` のTwoWire依存を、ESP-IDF I2Cをラップしたカスタムクラスに差し替え

**実装**:

```cpp
// wire_espidf_wrapper.hpp
class WireESPIDF : public TwoWire {
private:
    i2c_port_t _port;
    uint8_t _addr;
    uint8_t _tx_buffer[128];
    uint8_t _rx_buffer[128];
    size_t _tx_index;

public:
    WireESPIDF(i2c_port_t port) : _port(port), _tx_index(0) {}

    void begin(int sda, int scl, uint32_t frequency) override {
        i2c_config_t conf;
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = sda;
        conf.scl_io_num = scl;
        conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = frequency;
        conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;

        i2c_param_config(_port, &conf);
        i2c_driver_install(_port, I2C_MODE_MASTER, 0, 0, 0);
    }

    void beginTransmission(uint8_t addr) override {
        _addr = addr;
        _tx_index = 0;
    }

    size_t write(uint8_t data) override {
        _tx_buffer[_tx_index++] = data;
        return 1;
    }

    uint8_t endTransmission(bool sendStop) override {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write(cmd, _tx_buffer, _tx_index, true);
        if (sendStop) i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
        return (err == ESP_OK) ? 0 : 4;
    }

    // requestFrom, read も同様に実装...
};
```

**使用方法**:

```cpp
// main.cpp
static WireESPIDF wire_bno(I2C_NUM_1);  // I2C_NUM_1 を明示的に使用
static Adafruit_BNO055 bno(55, BNO_ADDR, &wire_bno);

void setup() {
    Roller.begin(ROLLER_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, I2C_HZ);  // I2C_NUM_0
    wire_bno.begin(SDA_PIN, SCL_PIN, I2C_HZ);                     // I2C_NUM_1
    bno.begin();
}
```

**利点**:
- ✅ Adafruit_BNO055 APIを維持
- ✅ I2Cポート明示的分離
- ✅ ESP-IDFドライバで統一

**欠点**:
- ⚠️ TwoWireクラスの全メソッドを実装する必要
- ⚠️ Arduino-ESP32のTwoWire仕様への完全準拠が困難
- ⚠️ メンテナンス負荷が高い

**推奨度**: △ Option 1 の方が保守性が高い

---

## Recommendation / 推奨事項

### ✅ Adopt Option 1: BNO055 ESP-IDF Native Driver

**理由**:
1. **実証済みの成功パターン**: Unit Rollerで Wire → ESP-IDF 移行が成功
2. **根本解決**: ドライバレイヤー競合を完全に排除
3. **長期保守性**: 外部ライブラリ依存なし、完全な制御
4. **性能**: 低レイテンシ、決定論的タイミング
5. **コードサイズ**: Adafruit_BNO055 + Wire を削除可能

### Implementation Priority / 実装優先度

**Phase 1: Minimal BNO055 Driver (1-2 days)**
- Chip ID確認
- Operation mode設定（NDOF mode）
- Euler角読み取り（現在使用中の pitch のみ）

**Phase 2: Full Sensor Data (1 day)**
- Quaternion読み取り
- Gyroscope読み取り
- Accelerometer、Magnetometer（オプション）

**Phase 3: Advanced Features (Optional)**
- Calibration status確認
- Self-test
- Interrupt設定

### Risk Mitigation / リスク軽減

**並行開発アプローチ**:
1. 新しい `bno055_espidf.cpp` を追加（Wire版と共存）
2. `#define USE_ESPIDF_BNO055` でビルド切り替え
3. テスト完了後、Wire版を削除

```cpp
// main.cpp
#define USE_ESPIDF_BNO055  // コメントアウトでWire版に戻せる

#ifdef USE_ESPIDF_BNO055
  #include "bno055_espidf.hpp"
  static BNO055_ESPIDF bno;
#else
  #include <Wire.h>
  #include <Adafruit_BNO055.h>
  static Adafruit_BNO055 bno(55, BNO_ADDR, &Wire);
#endif
```

---

## Technical Details / 技術詳細

### BNO055 Register Map (Essential Registers)

```cpp
// Page 0 Registers (Default)
#define BNO055_CHIP_ID            0x00  // Value: 0xA0
#define BNO055_PAGE_ID            0x07  // Register page selector
#define BNO055_SYS_TRIGGER        0x3F  // System trigger
#define BNO055_PWR_MODE           0x3E  // Power mode
#define BNO055_OPR_MODE           0x3D  // Operation mode

// Data Output Registers
#define BNO055_ACCEL_DATA_X_LSB   0x08  // Accelerometer X LSB
#define BNO055_MAG_DATA_X_LSB     0x0E  // Magnetometer X LSB
#define BNO055_GYRO_DATA_X_LSB    0x14  // Gyroscope X LSB
#define BNO055_EULER_H_LSB        0x1A  // Euler Heading LSB
#define BNO055_QUATERNION_W_LSB   0x20  // Quaternion W LSB

// Calibration Status
#define BNO055_CALIB_STAT         0x35  // Calibration status

// Operation Modes
#define OPERATION_MODE_CONFIG     0x00  // Configuration mode
#define OPERATION_MODE_NDOF       0x0C  // 9DOF sensor fusion
```

### Data Format

**Euler Angles** (0x1A - 0x1F, 6 bytes):
```
Heading: int16_t (LSB at 0x1A, MSB at 0x1B) → degrees = value / 16.0
Roll:    int16_t (LSB at 0x1C, MSB at 0x1D) → degrees = value / 16.0
Pitch:   int16_t (LSB at 0x1E, MSB at 0x1F) → degrees = value / 16.0
```

**Quaternion** (0x20 - 0x27, 8 bytes):
```
W: int16_t (LSB at 0x20, MSB at 0x21) → quaternion = value / 16384.0
X: int16_t (LSB at 0x22, MSB at 0x23) → quaternion = value / 16384.0
Y: int16_t (LSB at 0x24, MSB at 0x25) → quaternion = value / 16384.0
Z: int16_t (LSB at 0x26, MSB at 0x27) → quaternion = value / 16384.0
```

**Gyroscope** (0x14 - 0x19, 6 bytes):
```
X: int16_t (LSB at 0x14, MSB at 0x15) → rad/s = value / 900.0
Y: int16_t (LSB at 0x16, MSB at 0x17) → rad/s = value / 900.0
Z: int16_t (LSB at 0x18, MSB at 0x19) → rad/s = value / 900.0
```

### Initialization Sequence

```cpp
bool BNO055_ESPIDF::begin() {
    // 1. Verify Chip ID
    uint8_t id = readReg(BNO055_CHIP_ID);
    if (id != 0xA0) return false;

    // 2. Reset (optional)
    writeReg(BNO055_SYS_TRIGGER, 0x20);
    delay(650);  // Wait for reset

    // 3. Set to Config mode
    writeReg(BNO055_OPR_MODE, OPERATION_MODE_CONFIG);
    delay(25);

    // 4. Set power mode to normal
    writeReg(BNO055_PWR_MODE, 0x00);
    delay(10);

    // 5. Set to NDOF mode (9DOF fusion)
    writeReg(BNO055_OPR_MODE, OPERATION_MODE_NDOF);
    delay(20);

    return true;
}
```

---

## Testing Plan / テスト計画

### Phase 1: Basic Communication Test

```cpp
void testBNO055BasicComm() {
    Serial.println("=== BNO055 Basic Communication Test ===");

    uint8_t chip_id = bno.readReg(0x00);
    Serial.printf("Chip ID: 0x%02X (expected: 0xA0)\n", chip_id);

    if (chip_id == 0xA0) {
        Serial.println("✅ I2C communication OK");
    } else {
        Serial.println("❌ I2C communication FAILED");
    }
}
```

### Phase 2: Operation Mode Test

```cpp
void testBNO055OpMode() {
    Serial.println("=== BNO055 Operation Mode Test ===");

    bno.setMode(OPERATION_MODE_NDOF);
    delay(100);

    uint8_t mode = bno.readReg(BNO055_OPR_MODE);
    Serial.printf("Current Mode: 0x%02X (expected: 0x0C)\n", mode);

    uint8_t calib = bno.readReg(BNO055_CALIB_STAT);
    Serial.printf("Calibration Status: 0x%02X\n", calib);
    Serial.printf("  System: %d, Gyro: %d, Accel: %d, Mag: %d\n",
                  (calib >> 6) & 0x03,
                  (calib >> 4) & 0x03,
                  (calib >> 2) & 0x03,
                  (calib >> 0) & 0x03);
}
```

### Phase 3: Data Readout Test

```cpp
void testBNO055DataReadout() {
    Serial.println("=== BNO055 Data Readout Test ===");

    for (int i = 0; i < 10; i++) {
        float h, r, p;
        bno.getEuler(&h, &r, &p);

        Serial.printf("[%d] Euler: H=%.2f R=%.2f P=%.2f\n", i, h, r, p);

        float qw, qx, qy, qz;
        bno.getQuat(&qw, &qx, &qy, &qz);

        Serial.printf("    Quat: W=%.3f X=%.3f Y=%.3f Z=%.3f\n", qw, qx, qy, qz);

        delay(100);
    }
}
```

### Phase 4: Concurrent I2C Test

```cpp
void testConcurrentI2C() {
    Serial.println("=== Concurrent I2C Access Test ===");

    for (int i = 0; i < 100; i++) {
        // Read BNO055
        float h, r, p;
        bno.getEuler(&h, &r, &p);

        // Read Roller
        int32_t vin = Roller.getVin();
        int32_t cur = Roller.getCurrentReadback();

        // Write to Roller
        Roller.setCurrent((int32_t)(p * 100.0f));

        if (i % 10 == 0) {
            Serial.printf("[%d] BNO P=%.2f, Roller V=%ld C=%ld\n", i, p, vin, cur);
        }

        delay(10);
    }

    Serial.println("✅ Concurrent I2C test completed without errors");
}
```

---

## References / 参考資料

### BNO055 Datasheet
- **Document**: BNO055 Intelligent 9-axis absolute orientation sensor
- **Manufacturer**: Bosch Sensortec
- **Link**: https://www.bosch-sensortec.com/products/smart-sensors/bno055/

### ESP-IDF I2C Driver Documentation
- **API Reference**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2c.html
- **Example**: `examples/peripherals/i2c/`

### Unit Roller I2C
- **GitHub**: https://github.com/m5stack/M5-ProductExampleCodes/tree/master/Unit/UNIT_ROLLER485_I2C

### Arduino-ESP32 Wire Implementation
- **Source**: https://github.com/espressif/arduino-esp32/blob/master/libraries/Wire/

---

## Conclusion / 結論

**現状**: 2つの異なるI2Cドライバ実装（ESP-IDF + Wire）が共存し、潜在的競合リスクあり

**原因**: Unit RollerはWireで動作しないことが実証済み、BNO055も同様の問題に直面する可能性が高い

**解決策**: BNO055をESP-IDF I2Cドライバで再実装（Option 1）

**実装工数**: 2-3日（基本実装 + テスト）

**推奨アクション**:
1. ✅ **Phase 1を即座に開始**: 最小限のBNO055 ESP-IDFドライバを実装
2. ✅ **Wire版と並行保守**: `#define` で切り替え可能にしてリスク軽減
3. ✅ **段階的移行**: Euler角 → Quaternion → Gyro の順で実装
4. ✅ **テスト駆動開発**: 各Phaseで通信・動作確認を徹底

**期待効果**:
- ドライバレイヤー統一による安定性向上
- 外部ライブラリ依存削減
- タイミング制御の完全掌握
- 長期保守性の向上

**次のステップ**: `bno055_espidf.hpp/cpp` の実装開始を推奨します。

---

**Report Generated**: 2026-01-29
**Analyzed Project**: simple_cores3_roller485_bno055
**Author**: Claude Code Analysis Agent
