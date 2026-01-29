# M5Stack CoreS3 Reaction Wheel Inverted Pendulum
# M5Stack CoreS3 リアクションホイール倒立振子

Reaction wheel inverted pendulum control system using M5Stack CoreS3, BNO055 IMU, and Unit Roller motor driver with stable ESP-IDF I2C implementation.

M5Stack CoreS3、BNO055 IMU、Unit Rollerモータードライバを使用したリアクションホイール倒立振子制御システム。安定したESP-IDF I2C実装を採用。

---

## 📋 Table of Contents / 目次

- [Features / 特徴](#features--特徴)
- [Hardware Requirements / 必要なハードウェア](#hardware-requirements--必要なハードウェア)
- [Quick Start / クイックスタート](#quick-start--クイックスタート)
  - [1. Environment Setup / 環境構築](#1-environment-setup--環境構築)
  - [2. Build / ビルド](#2-build--ビルド)
  - [3. Flash / フラッシュ](#3-flash--フラッシュ)
- [Key Improvements / 主要な改善](#key-improvements--主要な改善)
  - [I2C Driver Improvement / I2Cドライバ改善](#i2c-driver-improvement--i2cドライバ改善)
  - [Control System Improvement / 制御システム改善](#control-system-improvement--制御システム改善)
- [Documentation / ドキュメント](#documentation--ドキュメント)
- [Project Structure / プロジェクト構成](#project-structure--プロジェクト構成)
- [Safety Features / 安全機能](#safety-features--安全機能)
- [Tuning Guide / 調整ガイド](#tuning-guide--調整ガイド)
- [Troubleshooting / トラブルシューティング](#troubleshooting--トラブルシューティング)
- [License / ライセンス](#license--ライセンス)

---

## Features / 特徴

- ✅ **Stable ESP-IDF I2C Driver** - Both BNO055 and Unit Roller use ESP-IDF low-level I2C driver (proven reliability)
- ✅ **PD Control Implementation** - Proper Proportional-Derivative control for inverted pendulum stability
- ✅ **Dual I2C Bus Architecture** - Separate I2C buses for sensor (I2C_NUM_1) and motor (I2C_NUM_0)
- ✅ **Safety Limits** - Automatic motor shutdown when tilt angle or angular velocity exceeds limits
- ✅ **Real-time Monitoring** - Display shows control status, sensor data, and motor feedback
- ✅ **Conditional Compilation** - Easy switching between ESP-IDF and Wire drivers for comparison

- ✅ **安定したESP-IDF I2Cドライバ** - BNO055とUnit Roller両方でESP-IDF低レベルI2Cドライバを使用（実証済みの信頼性）
- ✅ **PD制御実装** - 倒立振子の安定化のための適切な比例微分制御
- ✅ **デュアルI2Cバスアーキテクチャ** - センサー（I2C_NUM_1）とモーター（I2C_NUM_0）で独立したI2Cバス
- ✅ **安全リミット** - 傾き角度または角速度がリミットを超えた際の自動モーター停止
- ✅ **リアルタイムモニタリング** - 制御状態、センサーデータ、モーターフィードバックを表示
- ✅ **条件付きコンパイル** - ESP-IDFとWireドライバの比較用切り替えが容易

---

## Hardware Requirements / 必要なハードウェア

| Component | Description |
|-----------|-------------|
| **M5Stack CoreS3** | ESP32-S3 based controller with display |
| **BNO055 IMU** | 9-axis absolute orientation sensor |
| **Unit Roller (485)** | M5Stack motor driver unit (I2C address: 0x64) |
| **Reaction Wheel** | Flywheel for generating stabilizing torque |

### Pin Connections / ピン接続

| Device | I2C Port | SDA Pin | SCL Pin | I2C Address |
|--------|----------|---------|---------|-------------|
| Unit Roller | I2C_NUM_0 | GPIO 2 | GPIO 1 | 0x64 |
| BNO055 IMU | I2C_NUM_1 | GPIO 6 | GPIO 7 | 0x28 |

---

## Quick Start / クイックスタート

### 1. Environment Setup / 環境構築

#### Prerequisites / 前提条件

- Python 3.10 or later / Python 3.10以降
- Git

#### Install PlatformIO / PlatformIOのインストール

```bash
# Install PlatformIO Core
# PlatformIO Coreをインストール
pip install platformio

# Verify installation
# インストール確認
pio --version
```

#### Clone Repository / リポジトリのクローン

```bash
git clone https://github.com/yourusername/simple_cores3_roller485_bno055.git
cd simple_cores3_roller485_bno055
```

---

### 2. Build / ビルド

```bash
cd simple_cores3_roller485_bno055
pio run
```

**Expected Output / 期待される出力:**
```
SUCCESS
RAM:   [=         ]   6.7% (used 22112 bytes from 327680 bytes)
Flash: [=         ]   6.9% (used 449589 bytes from 6553600 bytes)
```

#### Build Configuration / ビルド設定

- **Platform**: `espressif32@6.7.0`
- **Board**: `m5stack-cores3`
- **Framework**: `arduino`

Dependencies are automatically installed on first build:
- M5Unified
- Adafruit BNO055
- Adafruit Unified Sensor
- Adafruit BusIO

初回ビルド時に依存関係が自動的にインストールされます。

---

### 3. Flash / フラッシュ

#### Connect Device / デバイス接続

1. Connect M5Stack CoreS3 to PC via USB-C cable
   USB-CケーブルでM5Stack CoreS3をPCに接続

2. Check serial port / シリアルポート確認
   ```bash
   pio device list
   ```

#### Upload Firmware / ファームウェア書き込み

```bash
cd simple_cores3_roller485_bno055
pio run --target upload
```

#### Monitor Serial Output / シリアル出力監視

```bash
pio device monitor --baud 115200
```

---

## Key Improvements / 主要な改善

### I2C Driver Improvement / I2Cドライバ改善

**Problem / 問題:**
- Original implementation used Arduino Wire library for BNO055
- Wire library proven unstable in Unit Roller testing (communication failures)
- Unit Roller successfully migrated to ESP-IDF I2C driver

元の実装ではBNO055にArduino Wireライブラリを使用していましたが、Unit Rollerのテストで不安定性が証明されており（通信失敗）、Unit RollerはESP-IDF I2Cドライバへの移行に成功していました。

**Solution / 解決策:**
- Migrated BNO055 driver to ESP-IDF I2C (I2C_NUM_1)
- Created `BNO055_ESPIDF` class using ESP-IDF low-level I2C API
- Reused proven Adafruit_BNO055 initialization logic (Thin Wrapper Pattern)
- Separate I2C buses prevent interference between sensor and motor

BNO055ドライバをESP-IDF I2C（I2C_NUM_1）に移行し、ESP-IDF低レベルI2C APIを使用した`BNO055_ESPIDF`クラスを作成。実証済みのAdafruit_BNO055初期化ロジックを再利用（Thin Wrapperパターン）。独立したI2Cバスによりセンサーとモーター間の干渉を防止。

**Benefits / 利点:**
- ✅ Stable I2C communication for both devices / 両デバイスで安定したI2C通信
- ✅ Precise timing control with ESP-IDF API / ESP-IDF APIによる正確なタイミング制御
- ✅ No I2C bus contention / I2Cバスの競合なし
- ✅ Proven reliability pattern from Unit Roller / Unit Rollerで実証済みの信頼性パターン

**📖 Details:** See [I2C_DRIVER_ANALYSIS.md](I2C_DRIVER_ANALYSIS.md) for complete technical analysis
**📖 詳細:** 完全な技術分析は [I2C_DRIVER_ANALYSIS.md](I2C_DRIVER_ANALYSIS.md) を参照

---

### Control System Improvement / 制御システム改善

**Problem / 問題:**
- Original implementation used P-only control: `cmd = pitch * 100`
- No derivative term (damping) → unstable for inverted pendulum
- Gyro unit comment error ([rad/s] vs actual [dps]) → potential 57x gain error
- No safety limits → risk of motor damage or system damage

元の実装はP制御のみ（`cmd = pitch * 100`）で、微分項（減衰）がなく倒立振子では不安定。Gyro単位のコメント誤り（[rad/s] vs 実際の [dps]）により57倍のゲイン誤差のリスク。安全リミットがなくモーター損傷やシステム損傷のリスク。

**Solution / 解決策:**
- Implemented PD control: `cmd = Kp*pitch + Kd*pitch_rate`
- Added control parameters with clear documentation
- Corrected gyro unit to [dps]
- Added safety limits for angle (±30°) and angular velocity (±300 dps)
- Real-time control status display

PD制御を実装（`cmd = Kp*pitch + Kd*pitch_rate`）。明確なドキュメントを持つ制御パラメータを追加。Gyro単位を [dps] に修正。角度（±30°）と角速度（±300 dps）の安全リミットを追加。リアルタイム制御状態表示。

**Control Parameters / 制御パラメータ:**
```cpp
CONTROL_FREQ_HZ = 100.0 Hz    // Sampling frequency / サンプリング周波数
KP_PITCH        = 100.0       // Proportional gain / 比例ゲイン
KD_PITCH        = 5.0         // Derivative gain / 微分ゲイン
PITCH_LIMIT_DEG = 30.0°       // Safety angle limit / 安全角度リミット
GYRO_LIMIT_DPS  = 300.0 dps   // Safety rate limit / 安全角速度リミット
```

**📖 Details:** See [CONTROL_REVIEW.md](CONTROL_REVIEW.md) for complete control theory analysis
**📖 詳細:** 完全な制御理論分析は [CONTROL_REVIEW.md](CONTROL_REVIEW.md) を参照

---

## Documentation / ドキュメント

| Document | Description |
|----------|-------------|
| **[CLAUDE.md](CLAUDE.md)** | Project overview and architecture for Claude Code sessions<br>プロジェクト概要とアーキテクチャ（Claude Codeセッション用） |
| **[I2C_DRIVER_ANALYSIS.md](I2C_DRIVER_ANALYSIS.md)** | I2C driver conflict analysis and ESP-IDF migration guide<br>I2Cドライバ競合分析とESP-IDF移行ガイド |
| **[CONTROL_REVIEW.md](CONTROL_REVIEW.md)** | Control theory review and PD controller design<br>制御理論レビューとPDコントローラ設計 |

---

## Project Structure / プロジェクト構成

```
simple_cores3_roller485_bno055/
├── simple_cores3_roller485_bno055/
│   ├── platformio.ini              # PlatformIO configuration
│   ├── src/
│   │   ├── main.cpp                # Main control loop with PD control
│   │   ├── bno055_espidf.hpp       # BNO055 ESP-IDF I2C driver header
│   │   ├── bno055_espidf.cpp       # BNO055 ESP-IDF I2C driver implementation
│   │   ├── unit_rolleri2c.hpp      # Unit Roller I2C driver header
│   │   └── unit_rolleri2c.cpp      # Unit Roller I2C driver implementation
│   └── include/
├── CLAUDE.md                        # Project documentation
├── I2C_DRIVER_ANALYSIS.md           # I2C technical analysis
├── CONTROL_REVIEW.md                # Control theory review
└── README.md                        # This file
```

---

## Safety Features / 安全機能

### Automatic Motor Shutdown / 自動モーター停止

The system monitors pitch angle and angular velocity in real-time and automatically stops the motor if safety limits are exceeded:

システムはピッチ角と角速度をリアルタイムで監視し、安全リミットを超えた場合は自動的にモーターを停止します：

```cpp
if (|pitch| > 30° || |pitch_rate| > 300 dps) {
    motorStopSafe();  // Set current=0, output=0
    Display: "SAFETY STOP!"
}
```

### Safety Limit Configuration / 安全リミット設定

Edit in `src/main.cpp`:
```cpp
static constexpr float PITCH_LIMIT_DEG = 30.0f;   // Angle limit [deg]
static constexpr float GYRO_LIMIT_DPS  = 300.0f;  // Rate limit [dps]
```

---

## Tuning Guide / 調整ガイド

### Before First Test / 初回テスト前

**⚠️ CRITICAL: Verify control axis direction / 制御軸方向の確認**

1. Set `ENABLE_CONTROL = false` in `src/main.cpp` (monitor mode)
   `src/main.cpp` で `ENABLE_CONTROL = false` に設定（モニタモード）

2. Observe pitch angle on display when tilting device
   デバイスを傾けた際のピッチ角を表示で確認

3. Verify sign convention matches expected behavior
   符号規則が期待される動作と一致することを確認

4. If control diverges after enabling, reverse sign:
   制御有効化後に発散する場合は符号を反転：
   ```cpp
   float cmd_f = -(KP_PITCH * pitch + KD_PITCH * pitch_rate);
   ```

### PD Gain Tuning / PDゲイン調整

#### If System Oscillates / システムが振動する場合
→ **Increase Kd** (add more damping)
```cpp
static constexpr float KD_PITCH = 10.0f;  // Increase from 5.0
```

#### If Response is Too Slow / 応答が遅すぎる場合
→ **Increase Kp** (faster response)
```cpp
static constexpr float KP_PITCH = 150.0f;  // Increase from 100.0
```

#### If Motor Saturates / モーターが飽和する場合
→ **Decrease both gains** (gentler control)
```cpp
static constexpr float KP_PITCH = 50.0f;
static constexpr float KD_PITCH = 2.5f;
```

### System Identification / システム同定

For optimal control performance, measure system parameters:
最適な制御性能のため、システムパラメータを測定：

- Reaction wheel inertia J_wheel [kg⋅m²]
- Pendulum mass m [kg]
- Center of mass distance l [m]
- Pendulum inertia J_pendulum [kg⋅m²]

See CONTROL_REVIEW.md for pole placement and LQR design methods.

---

## Troubleshooting / トラブルシューティング

### Build Issues / ビルド問題

**Error: "command not found: pio"**
```bash
pip install platformio
```

**Error: "Platform espressif32 not found"**
```bash
pio platform install espressif32@6.7.0
```

### Upload Issues / アップロード問題

**Error: "No serial ports found"**
1. Check USB-C cable connection / USB-Cケーブル接続を確認
2. Install CP210x USB driver if needed / 必要に応じてCP210x USBドライバをインストール
3. Try different USB port / 別のUSBポートを試す

**Error: "Permission denied on serial port"** (Linux/Mac)
```bash
sudo chmod 666 /dev/ttyUSB0  # Replace with your port
```

### Runtime Issues / 実行時問題

**BNO055 initialization fails / BNO055初期化失敗**
1. Check I2C wiring (SDA=GPIO6, SCL=GPIO7)
2. Verify BNO055 I2C address (0x28 or 0x29)
3. Check power supply to BNO055

**Control diverges immediately / 制御がすぐに発散**
1. Verify control axis direction (see Tuning Guide)
2. Try reversing control sign
3. Set ENABLE_CONTROL = false to monitor sensor readings

**"SAFETY STOP" appears frequently / "SAFETY STOP"が頻繁に表示**
1. Check if device is properly mounted / デバイスが適切に取り付けられているか確認
2. Increase safety limits if appropriate
3. Check for sensor noise or calibration issues

---

## License / ライセンス

This project incorporates code from:

- **Adafruit_BNO055** library (MIT License) - Copyright (c) K.Townsend (Adafruit Industries)
- **M5Unified** library (MIT License) - Copyright (c) M5Stack

ESP-IDF I2C driver implementation:
- Copyright (c) 2026
- Based on proven Unit Roller I2C pattern

---

## Contact / お問い合わせ

For questions, issues, or contributions, please:
質問、問題報告、貢献については：

- Open an issue on GitHub
- Check existing documentation in `CLAUDE.md`, `I2C_DRIVER_ANALYSIS.md`, or `CONTROL_REVIEW.md`

---

## Acknowledgments / 謝辞

- Adafruit Industries for BNO055 library and documentation
- M5Stack for CoreS3 hardware and M5Unified library
- ESP-IDF team for robust I2C driver implementation

---

**Built with ❤️ for stable inverted pendulum control**
**安定した倒立振子制御のために ❤️ を込めて開発**
