#include <Arduino.h>
#include "unit_rolleri2c.hpp"

#include <Arduino.h>
#include <M5Unified.h>

// ============================================================================
// BNO055 Driver Selection
// BNO055ドライバ選択
// ============================================================================
// Uncomment to use ESP-IDF I2C driver (recommended for stability)
// ESP-IDF I2Cドライバを使用する場合はコメント解除（安定性のため推奨）
#define USE_ESPIDF_BNO055

#ifdef USE_ESPIDF_BNO055
  // ESP-IDF I2C Driver (stable, proven pattern from Unit Roller)
  // ESP-IDF I2Cドライバ（安定、Unit Rollerで実証済み）
  #include "bno055_espidf.hpp"
  #include <utility/imumaths.h>
#else
  // Arduino Wire Driver (unstable, for comparison only)
  // Arduino Wireドライバ（不安定、比較用のみ）
  #include <Wire.h>
  #include <Adafruit_BNO055.h>
  #include <utility/imumaths.h>
#endif

// =====================================
static constexpr uint8_t  ROLLER_ADDR = 0x64;
static constexpr uint8_t  I2C_SDA_PIN = 2;
static constexpr uint8_t  I2C_SCL_PIN = 1;
static constexpr uint32_t I2C_HZ      = 100000;
// ======================================

// ======================================
static constexpr int      SDA_PIN = 6;
static constexpr int      SCL_PIN = 7;
static constexpr uint8_t  BNO_ADDR = 0x28;  // 0x29
// ======================================

// ============================================================================
// Reaction Wheel Inverted Pendulum Control Parameters
// リアクションホイール倒立振子制御パラメータ
// ============================================================================
// Control Loop Timing
// 制御ループタイミング
static constexpr float CONTROL_FREQ_HZ = 100.0f;       // [Hz] サンプリング周波数
static constexpr float CONTROL_DT_SEC  = 0.01f;        // [s] サンプリング時間 (1/CONTROL_FREQ_HZ)
static constexpr int   CONTROL_DT_MS   = 10;           // [ms] delay()用

// PD Control Gains (adjust based on system identification)
// PD制御ゲイン（システム同定に基づいて調整）
static constexpr float KP_PITCH        = 100.0f;       // [current/deg] 比例ゲイン
static constexpr float KD_PITCH        = 5.0f;         // [current/(deg/s)] 微分ゲイン

// NOTE: Gyro output unit is [dps] (degrees per second), NOT [rad/s]
// 注意: Gyro出力単位は [dps] (degrees per second)、[rad/s] ではない
// Both Adafruit_BNO055 and BNO055_ESPIDF output dps by default
// Adafruit_BNO055とBNO055_ESPIDFの両方がデフォルトでdpsを出力

// Current Limits
// 電流リミット
static constexpr float MAX_CURRENT     = 100000.0f;    // 最大電流 [motor unit]

// Safety Limits (reaction wheel inverted pendulum protection)
// 安全リミット（リアクションホイール倒立振子保護）
static constexpr float PITCH_LIMIT_DEG = 30.0f;        // 角度リミット [deg]
static constexpr float GYRO_LIMIT_DPS  = 300.0f;       // 角速度リミット [dps]

// Control Enable Flag (set to true to enable closed-loop control)
// 制御有効化フラグ（trueで閉ループ制御を有効化）
// WARNING: Verify control axis direction before enabling!
// 警告: 有効化前に制御軸方向を確認すること！
static constexpr bool ENABLE_CONTROL   = true;         // false: open-loop (monitor only)

// ============================================================================

UnitRollerI2C Roller;
bool UnitRollerI2C::initialized = false;

// BNO055 Driver Instance
// BNO055ドライバインスタンス
#ifdef USE_ESPIDF_BNO055
  static BNO055_ESPIDF bno;
#else
  static Adafruit_BNO055 bno(55, BNO_ADDR, &Wire);
#endif

static void motorStopSafe() {
  Roller.setCurrent(0);
  Roller.setOutput(0);
}

void setup() {

  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.setTextSize(2);
  M5.Display.printf("Init...\n");

  // Initialize Unit Roller (I2C_NUM_0: GPIO2/GPIO1)
  // Unit Rollerを初期化（I2C_NUM_0: GPIO2/GPIO1）
  Roller.begin(ROLLER_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, I2C_HZ);
  M5.Display.printf("Roller: OK\n");

  // Initialize BNO055 (I2C_NUM_1: GPIO6/GPIO7)
  // BNO055を初期化（I2C_NUM_1: GPIO6/GPIO7）
#ifdef USE_ESPIDF_BNO055
  // ESP-IDF I2C Driver
  if (!bno.begin(BNO_ADDR, SDA_PIN, SCL_PIN, I2C_HZ)) {
    M5.Display.printf("BNO055: FAILED\n");
    while (1) {
      delay(100);
    }
  }
  M5.Display.printf("BNO055: OK (ESP-IDF)\n");
#else
  // Arduino Wire Driver
  Wire.begin(SDA_PIN, SCL_PIN, I2C_HZ);
  if (!bno.begin()) {
    M5.Display.printf("BNO055: FAILED\n");
    while (1) {
      delay(100);
    }
  }
  M5.Display.printf("BNO055: OK (Wire)\n");
#endif

  // Configure motor
  // モーター設定
  Roller.setMode(3);
  Roller.setCurrent(0);
  Roller.setOutput(1);

  delay(200);
}

void loop() {

  M5.update();

  // Read BNO055 sensor data
  // BNO055センサーデータを読み取り
#ifdef USE_ESPIDF_BNO055
  imu::Vector<3> eul  = bno.getVector(VECTOR_EULER);       // [deg] eul.x() eul.y() eul.z()
  imu::Quaternion q   = bno.getQuat();                     // q.w() q.x() q.y() q.z()
  imu::Vector<3> gyro = bno.getVector(VECTOR_GYROSCOPE);   // [dps] gyro.x() gyro.y() gyro.z()
#else
  imu::Vector<3> eul  = bno.getVector(Adafruit_BNO055::VECTOR_EULER);       // [deg] eul.x() eul.y() eul.z()
  imu::Quaternion q   = bno.getQuat();                                      // q.w() q.x() q.y() q.z()
  imu::Vector<3> gyro = bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);   // [dps] gyro.x() gyro.y() gyro.z()
#endif

  // Extract control variables
  // 制御変数を抽出
  float pitch      = eul.y();      // [deg] ピッチ角
  float pitch_rate = gyro.y();     // [dps] ピッチ角速度

  // Safety check: stop if tilted too much or rotating too fast
  // 安全チェック：傾きすぎ、または回転速度が速すぎる場合は停止
  if (abs(pitch) > PITCH_LIMIT_DEG || abs(pitch_rate) > GYRO_LIMIT_DPS) {
    motorStopSafe();
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(3);
    M5.Display.setTextColor(TFT_RED);
    M5.Display.printf("SAFETY STOP!\n");
    M5.Display.setTextSize(2);
    M5.Display.printf("Pitch: %.1f deg\n", pitch);
    M5.Display.printf("Rate: %.1f dps\n", pitch_rate);
    M5.Display.printf("\nLimits exceeded\n");
    while(1) { delay(100); }
  }

  // Calculate control command
  // 制御指令を計算
  int32_t cmd = 0;
  if (ENABLE_CONTROL) {
    // PD Control Law (Reaction Wheel Inverted Pendulum)
    // PD制御則（リアクションホイール倒立振子）
    float cmd_f = KP_PITCH * pitch + KD_PITCH * pitch_rate;

    // Saturate command to maximum current
    // 最大電流にコマンドを飽和
    if (cmd_f >  MAX_CURRENT) cmd_f =  MAX_CURRENT;
    if (cmd_f < -MAX_CURRENT) cmd_f = -MAX_CURRENT;

    cmd = (int32_t)cmd_f;
  }

  Roller.setCurrent(cmd);


  const int32_t vin   = Roller.getVin();
  const int32_t cur   = Roller.getCurrentReadback();  
  const int32_t spd   = Roller.getSpeedReadback();
  const int32_t pos   = Roller.getPosReadback();


  M5.Display.startWrite();
    //M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setCursor(0, 0);

    M5.Display.setTextSize(2);
#ifdef USE_ESPIDF_BNO055
    M5.Display.printf("BNO(ESP-IDF)+Roller\n");
#else
    M5.Display.printf("BNO(Wire)+Roller\n");
#endif
    M5.Display.setTextSize(1);
    M5.Display.printf("I2C: R=0x%02X B=0x%02X\n", ROLLER_ADDR, BNO_ADDR);

    // Control Status
    // 制御状態
    M5.Display.printf("[Control: %s]\n", ENABLE_CONTROL ? "ENABLED" : "DISABLED");
    M5.Display.printf("P:%+6.2f Pd:%+7.2f cmd:%ld\n", pitch, pitch_rate, (long)cmd);
    M5.Display.printf("Kp:%.0f Kd:%.1f @%.0fHz\n", KP_PITCH, KD_PITCH, CONTROL_FREQ_HZ);

    M5.Display.printf("[BNO Sensors]\n");
    M5.Display.printf("Euler H:%7.2f R:%7.2f P:%7.2f\n",
                      eul.x(), eul.y(), eul.z());
    M5.Display.printf("Gyro  x:%+7.2f y:%+7.2f z:%+7.2f\n",
                      gyro.x(), gyro.y(), gyro.z());
    M5.Display.printf("Quat  w:%+.3f x:%+.3f y:%+.3f z:%+.3f\n",
                      q.w(), q.x(), q.y(), q.z());

    M5.Display.printf("[Roller Motor]\n");
    M5.Display.printf("vin:%ld cur:%ld spd:%ld pos:%ld\n",
                      (long)vin, (long)cur, (long)spd, (long)pos);

    M5.Display.endWrite();

  delay(10);
}
