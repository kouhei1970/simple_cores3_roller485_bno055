#include <Arduino.h>
#include "unit_rolleri2c.hpp"

#include <Arduino.h>
#include <M5Unified.h>
#include <Wire.h>

#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

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



UnitRollerI2C Roller;
bool UnitRollerI2C::initialized = false;

static Adafruit_BNO055 bno(55, BNO_ADDR, &Wire);

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

  Roller.begin(ROLLER_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, I2C_HZ);

  Wire.begin(SDA_PIN, SCL_PIN, I2C_HZ);

  bno.begin();

  Roller.setMode(3);
  Roller.setCurrent(0);
  Roller.setOutput(1);

  delay(200);
}

void loop() {

  M5.update();

  imu::Vector<3> eul  = bno.getVector(Adafruit_BNO055::VECTOR_EULER);       // [deg] eul.x() eul.y() eul.z()
  imu::Quaternion q   = bno.getQuat();                                      // q.w() q.x() q.y() q.z()
  imu::Vector<3> gyro = bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);   // [rad/s] gyro.x() gyro.y() gyro.z()

  float cmd_f = eul.y() * 100.0f;
  int32_t cmd = (int32_t)cmd_f;

  const int32_t lim = 100000;
  if (cmd >  lim) cmd =  lim;
  if (cmd < -lim) cmd = -lim;

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
    M5.Display.printf("BNO055 + Roller485\n");
    M5.Display.setTextSize(1);
    M5.Display.printf("cmd:%ld  addrR:0x%02X addrB:0x%02X\n",
                      (long)cmd, ROLLER_ADDR, BNO_ADDR);

    M5.Display.printf("[BNO]\n");
    M5.Display.printf("Euler H:%7.2f R:%7.2f P:%7.2f (deg)\n",
                      eul.x(), eul.y(), eul.z());
    M5.Display.printf("Gyro  x:%+7.2f y:%+7.2f z:%+7.2f\n",
                      gyro.x(), gyro.y(), gyro.z());
    M5.Display.printf("Quat  w:%+.3f x:%+.3f y:%+.3f z:%+.3f\n",
                      q.w(), q.x(), q.y(), q.z());

    M5.Display.printf("[Roller]\n");
    M5.Display.printf("vin:%ld cur:%ld spd:%ld pos:%ld\n",
                      (long)vin, (long)cur, (long)spd, (long)pos);

    M5.Display.endWrite();

  delay(10);
}
