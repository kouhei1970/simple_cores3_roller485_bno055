# M5Stack CoreS3 リアクションホイール倒立振子
# M5Stack CoreS3 Reaction Wheel Inverted Pendulum

M5Stack CoreS3、BNO055 IMU、Unit Rollerモータードライバを使用したリアクションホイール倒立振子制御システム。安定したESP-IDF I2C実装を採用。

Reaction wheel inverted pendulum control system using M5Stack CoreS3, BNO055 IMU, and Unit Roller motor driver with stable ESP-IDF I2C implementation.

---

## 📖 Language / 言語選択

- **[日本語版ドキュメント](#日本語版)** - このページの前半に日本語のドキュメントがあります
- **[English Documentation](#english-version)** - English documentation is in the latter half of this page

---
---

<a id="日本語版"></a>

# 📘 日本語版ドキュメント

## 📋 目次

- [特徴](#特徴)
- [必要なハードウェア](#必要なハードウェア)
- [クイックスタート](#クイックスタート)
  - [1. 環境構築](#1-環境構築)
  - [2. ビルド](#2-ビルド)
  - [3. フラッシュ](#3-フラッシュ)
- [主要な改善](#主要な改善)
  - [I2Cドライバ改善](#i2cドライバ改善)
  - [制御システム改善](#制御システム改善)
- [ドキュメント](#ドキュメント)
- [プロジェクト構成](#プロジェクト構成)
- [安全機能](#安全機能)
- [調整ガイド](#調整ガイド)
- [トラブルシューティング](#トラブルシューティング)
- [ライセンス](#ライセンス)

---

## 特徴

- ✅ **安定したESP-IDF I2Cドライバ** - BNO055とUnit Roller両方でESP-IDF低レベルI2Cドライバを使用（実証済みの信頼性）
- ✅ **PD制御実装** - 倒立振子の安定化のための適切な比例微分制御
- ✅ **デュアルI2Cバスアーキテクチャ** - センサー（I2C_NUM_1）とモーター（I2C_NUM_0）で独立したI2Cバス
- ✅ **安全リミット** - 傾き角度または角速度がリミットを超えた際の自動モーター停止
- ✅ **リアルタイムモニタリング** - 制御状態、センサーデータ、モーターフィードバックを表示
- ✅ **条件付きコンパイル** - ESP-IDFとWireドライバの比較用切り替えが容易

---

## 必要なハードウェア

| コンポーネント | 説明 |
|--------------|------|
| **M5Stack CoreS3** | ディスプレイ付きESP32-S3ベースコントローラ |
| **BNO055 IMU** | 9軸絶対方位センサー |
| **Unit Roller (485)** | M5Stackモータードライバユニット（I2Cアドレス: 0x64） |
| **リアクションホイール** | 安定化トルクを生成するフライホイール |

### ピン接続

| デバイス | I2Cポート | SDAピン | SCLピン | I2Cアドレス |
|---------|----------|---------|---------|------------|
| Unit Roller | I2C_NUM_0 | GPIO 2 | GPIO 1 | 0x64 |
| BNO055 IMU | I2C_NUM_1 | GPIO 6 | GPIO 7 | 0x28 |

---

## クイックスタート

### 1. 環境構築

#### 前提条件

- Python 3.10以降
- Git

#### PlatformIOのインストール

```bash
# PlatformIO Coreをインストール
pip install platformio

# インストール確認
pio --version
```

#### リポジトリのクローン

```bash
git clone https://github.com/kouhei1970/simple_cores3_roller485_bno055.git
cd simple_cores3_roller485_bno055
```

---

### 2. ビルド

```bash
cd simple_cores3_roller485_bno055
pio run
```

**期待される出力:**
```
SUCCESS
RAM:   [=         ]   6.7% (used 22112 bytes from 327680 bytes)
Flash: [=         ]   6.9% (used 449589 bytes from 6553600 bytes)
```

#### ビルド設定

- **Platform**: `espressif32@6.7.0`
- **Board**: `m5stack-cores3`
- **Framework**: `arduino`

初回ビルド時に依存関係が自動的にインストールされます：
- M5Unified
- Adafruit BNO055
- Adafruit Unified Sensor
- Adafruit BusIO

---

### 3. フラッシュ

#### デバイス接続

1. USB-CケーブルでM5Stack CoreS3をPCに接続

2. シリアルポート確認
   ```bash
   pio device list
   ```

#### ファームウェア書き込み

```bash
cd simple_cores3_roller485_bno055
pio run --target upload
```

#### シリアル出力監視

```bash
pio device monitor --baud 115200
```

---

## 主要な改善

### I2Cドライバ改善

**問題:**

元の実装ではBNO055にArduino Wireライブラリを使用していましたが、Unit Rollerのテストで不安定性が証明されており（通信失敗）、Unit RollerはESP-IDF I2Cドライバへの移行に成功していました。

**解決策:**

BNO055ドライバをESP-IDF I2C（I2C_NUM_1）に移行し、ESP-IDF低レベルI2C APIを使用した`BNO055_ESPIDF`クラスを作成。実証済みのAdafruit_BNO055初期化ロジックを再利用（Thin Wrapperパターン）。独立したI2Cバスによりセンサーとモーター間の干渉を防止。

**利点:**
- ✅ 両デバイスで安定したI2C通信
- ✅ ESP-IDF APIによる正確なタイミング制御
- ✅ I2Cバスの競合なし
- ✅ Unit Rollerで実証済みの信頼性パターン

**📖 詳細:** 完全な技術分析は [I2C_DRIVER_ANALYSIS.md](I2C_DRIVER_ANALYSIS.md) を参照

---

### 制御システム改善

**問題:**

元の実装はP制御のみ（`cmd = pitch * 100`）で、微分項（減衰）がなく倒立振子では不安定。Gyro単位のコメント誤り（[rad/s] vs 実際の [dps]）により57倍のゲイン誤差のリスク。安全リミットがなくモーター損傷やシステム損傷のリスク。

**解決策:**

PD制御を実装（`cmd = Kp*pitch + Kd*pitch_rate`）。明確なドキュメントを持つ制御パラメータを追加。Gyro単位を [dps] に修正。角度（±30°）と角速度（±300 dps）の安全リミットを追加。リアルタイム制御状態表示。

**制御パラメータ:**
```cpp
CONTROL_FREQ_HZ = 100.0 Hz    // サンプリング周波数
KP_PITCH        = 100.0       // 比例ゲイン
KD_PITCH        = 5.0         // 微分ゲイン
PITCH_LIMIT_DEG = 30.0°       // 安全角度リミット
GYRO_LIMIT_DPS  = 300.0 dps   // 安全角速度リミット
```

**📖 詳細:** 完全な制御理論分析は [CONTROL_REVIEW.md](CONTROL_REVIEW.md) を参照

---

## ドキュメント

| ドキュメント | 説明 |
|------------|------|
| **[CLAUDE.md](CLAUDE.md)** | プロジェクト概要とアーキテクチャ（Claude Codeセッション用） |
| **[I2C_DRIVER_ANALYSIS.md](I2C_DRIVER_ANALYSIS.md)** | I2Cドライバ競合分析とESP-IDF移行ガイド |
| **[CONTROL_REVIEW.md](CONTROL_REVIEW.md)** | 制御理論レビューとPDコントローラ設計 |

---

## プロジェクト構成

```
simple_cores3_roller485_bno055/
├── simple_cores3_roller485_bno055/
│   ├── platformio.ini              # PlatformIO設定
│   ├── src/
│   │   ├── main.cpp                # PD制御付きメイン制御ループ
│   │   ├── bno055_espidf.hpp       # BNO055 ESP-IDF I2Cドライバヘッダ
│   │   ├── bno055_espidf.cpp       # BNO055 ESP-IDF I2Cドライバ実装
│   │   ├── unit_rolleri2c.hpp      # Unit Roller I2Cドライバヘッダ
│   │   └── unit_rolleri2c.cpp      # Unit Roller I2Cドライバ実装
│   └── include/
├── CLAUDE.md                        # プロジェクトドキュメント
├── I2C_DRIVER_ANALYSIS.md           # I2C技術分析
├── CONTROL_REVIEW.md                # 制御理論レビュー
└── README.md                        # このファイル
```

---

## 安全機能

### 自動モーター停止

システムはピッチ角と角速度をリアルタイムで監視し、安全リミットを超えた場合は自動的にモーターを停止します：

```cpp
if (|pitch| > 30° || |pitch_rate| > 300 dps) {
    motorStopSafe();  // current=0, output=0に設定
    Display: "SAFETY STOP!"
}
```

### 安全リミット設定

`src/main.cpp`で編集：
```cpp
static constexpr float PITCH_LIMIT_DEG = 30.0f;   // 角度リミット [deg]
static constexpr float GYRO_LIMIT_DPS  = 300.0f;  // 角速度リミット [dps]
```

---

## 調整ガイド

### 初回テスト前

**⚠️ 重要: 制御軸方向の確認**

1. `src/main.cpp`で`ENABLE_CONTROL = false`に設定（モニタモード）

2. デバイスを傾けた際のピッチ角を表示で確認

3. 符号規則が期待される動作と一致することを確認

4. 制御有効化後に発散する場合は符号を反転：
   ```cpp
   float cmd_f = -(KP_PITCH * pitch + KD_PITCH * pitch_rate);
   ```

### PDゲイン調整

#### システムが振動する場合
→ **Kdを増やす**（減衰を追加）
```cpp
static constexpr float KD_PITCH = 10.0f;  // 5.0から増加
```

#### 応答が遅すぎる場合
→ **Kpを増やす**（応答を高速化）
```cpp
static constexpr float KP_PITCH = 150.0f;  // 100.0から増加
```

#### モーターが飽和する場合
→ **両方のゲインを減らす**（穏やかな制御）
```cpp
static constexpr float KP_PITCH = 50.0f;
static constexpr float KD_PITCH = 2.5f;
```

### システム同定

最適な制御性能のため、システムパラメータを測定：

- リアクションホイール慣性モーメント J_wheel [kg⋅m²]
- 振子質量 m [kg]
- 重心距離 l [m]
- 振子慣性モーメント J_pendulum [kg⋅m²]

極配置法とLQR設計手法については CONTROL_REVIEW.md を参照。

---

## トラブルシューティング

### ビルド問題

**エラー: "command not found: pio"**
```bash
pip install platformio
```

**エラー: "Platform espressif32 not found"**
```bash
pio platform install espressif32@6.7.0
```

### アップロード問題

**エラー: "No serial ports found"**
1. USB-Cケーブル接続を確認
2. 必要に応じてCP210x USBドライバをインストール
3. 別のUSBポートを試す

**エラー: "Permission denied on serial port"** (Linux/Mac)
```bash
sudo chmod 666 /dev/ttyUSB0  # ポート名を置き換え
```

### 実行時問題

**BNO055初期化失敗**
1. I2C配線確認（SDA=GPIO6, SCL=GPIO7）
2. BNO055 I2Cアドレス確認（0x28または0x29）
3. BNO055への電源供給確認

**制御がすぐに発散**
1. 制御軸方向を確認（調整ガイド参照）
2. 制御の符号を反転してみる
3. ENABLE_CONTROL = falseにしてセンサー読み取り値を監視

**"SAFETY STOP"が頻繁に表示**
1. デバイスが適切に取り付けられているか確認
2. 必要に応じて安全リミットを増やす
3. センサーノイズまたはキャリブレーション問題を確認

---

## ライセンス

このプロジェクトは以下のコードを組み込んでいます：

- **Adafruit_BNO055** ライブラリ（MITライセンス） - Copyright (c) K.Townsend (Adafruit Industries)
- **M5Unified** ライブラリ（MITライセンス） - Copyright (c) M5Stack

ESP-IDF I2Cドライバ実装：
- Copyright (c) 2026
- Unit Roller I2Cパターンに基づく

---

## お問い合わせ

質問、問題報告、貢献については：

- GitHubでissueを開く
- `CLAUDE.md`、`I2C_DRIVER_ANALYSIS.md`、`CONTROL_REVIEW.md`の既存ドキュメントを確認

---

## 謝辞

- Adafruit Industries - BNO055ライブラリとドキュメント
- M5Stack - CoreS3ハードウェアとM5Unifiedライブラリ
- ESP-IDFチーム - 堅牢なI2Cドライバ実装

---

**安定した倒立振子制御のために ❤️ を込めて開発**

---
---
---

<a id="english-version"></a>

# 📘 English Documentation

## 📋 Table of Contents

- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Quick Start](#quick-start)
  - [1. Environment Setup](#1-environment-setup)
  - [2. Build](#2-build)
  - [3. Flash](#3-flash)
- [Key Improvements](#key-improvements)
  - [I2C Driver Improvement](#i2c-driver-improvement)
  - [Control System Improvement](#control-system-improvement)
- [Documentation](#documentation)
- [Project Structure](#project-structure)
- [Safety Features](#safety-features)
- [Tuning Guide](#tuning-guide)
- [Troubleshooting](#troubleshooting)
- [License](#license)

---

## Features

- ✅ **Stable ESP-IDF I2C Driver** - Both BNO055 and Unit Roller use ESP-IDF low-level I2C driver (proven reliability)
- ✅ **PD Control Implementation** - Proper Proportional-Derivative control for inverted pendulum stability
- ✅ **Dual I2C Bus Architecture** - Separate I2C buses for sensor (I2C_NUM_1) and motor (I2C_NUM_0)
- ✅ **Safety Limits** - Automatic motor shutdown when tilt angle or angular velocity exceeds limits
- ✅ **Real-time Monitoring** - Display shows control status, sensor data, and motor feedback
- ✅ **Conditional Compilation** - Easy switching between ESP-IDF and Wire drivers for comparison

---

## Hardware Requirements

| Component | Description |
|-----------|-------------|
| **M5Stack CoreS3** | ESP32-S3 based controller with display |
| **BNO055 IMU** | 9-axis absolute orientation sensor |
| **Unit Roller (485)** | M5Stack motor driver unit (I2C address: 0x64) |
| **Reaction Wheel** | Flywheel for generating stabilizing torque |

### Pin Connections

| Device | I2C Port | SDA Pin | SCL Pin | I2C Address |
|--------|----------|---------|---------|-------------|
| Unit Roller | I2C_NUM_0 | GPIO 2 | GPIO 1 | 0x64 |
| BNO055 IMU | I2C_NUM_1 | GPIO 6 | GPIO 7 | 0x28 |

---

## Quick Start

### 1. Environment Setup

#### Prerequisites

- Python 3.10 or later
- Git

#### Install PlatformIO

```bash
# Install PlatformIO Core
pip install platformio

# Verify installation
pio --version
```

#### Clone Repository

```bash
git clone https://github.com/kouhei1970/simple_cores3_roller485_bno055.git
cd simple_cores3_roller485_bno055
```

---

### 2. Build

```bash
cd simple_cores3_roller485_bno055
pio run
```

**Expected Output:**
```
SUCCESS
RAM:   [=         ]   6.7% (used 22112 bytes from 327680 bytes)
Flash: [=         ]   6.9% (used 449589 bytes from 6553600 bytes)
```

#### Build Configuration

- **Platform**: `espressif32@6.7.0`
- **Board**: `m5stack-cores3`
- **Framework**: `arduino`

Dependencies are automatically installed on first build:
- M5Unified
- Adafruit BNO055
- Adafruit Unified Sensor
- Adafruit BusIO

---

### 3. Flash

#### Connect Device

1. Connect M5Stack CoreS3 to PC via USB-C cable

2. Check serial port
   ```bash
   pio device list
   ```

#### Upload Firmware

```bash
cd simple_cores3_roller485_bno055
pio run --target upload
```

#### Monitor Serial Output

```bash
pio device monitor --baud 115200
```

---

## Key Improvements

### I2C Driver Improvement

**Problem:**

Original implementation used Arduino Wire library for BNO055. Wire library proven unstable in Unit Roller testing (communication failures). Unit Roller successfully migrated to ESP-IDF I2C driver.

**Solution:**

Migrated BNO055 driver to ESP-IDF I2C (I2C_NUM_1). Created `BNO055_ESPIDF` class using ESP-IDF low-level I2C API. Reused proven Adafruit_BNO055 initialization logic (Thin Wrapper Pattern). Separate I2C buses prevent interference between sensor and motor.

**Benefits:**
- ✅ Stable I2C communication for both devices
- ✅ Precise timing control with ESP-IDF API
- ✅ No I2C bus contention
- ✅ Proven reliability pattern from Unit Roller

**📖 Details:** See [I2C_DRIVER_ANALYSIS.md](I2C_DRIVER_ANALYSIS.md) for complete technical analysis

---

### Control System Improvement

**Problem:**

Original implementation used P-only control: `cmd = pitch * 100`. No derivative term (damping) → unstable for inverted pendulum. Gyro unit comment error ([rad/s] vs actual [dps]) → potential 57x gain error. No safety limits → risk of motor damage or system damage.

**Solution:**

Implemented PD control: `cmd = Kp*pitch + Kd*pitch_rate`. Added control parameters with clear documentation. Corrected gyro unit to [dps]. Added safety limits for angle (±30°) and angular velocity (±300 dps). Real-time control status display.

**Control Parameters:**
```cpp
CONTROL_FREQ_HZ = 100.0 Hz    // Sampling frequency
KP_PITCH        = 100.0       // Proportional gain
KD_PITCH        = 5.0         // Derivative gain
PITCH_LIMIT_DEG = 30.0°       // Safety angle limit
GYRO_LIMIT_DPS  = 300.0 dps   // Safety rate limit
```

**📖 Details:** See [CONTROL_REVIEW.md](CONTROL_REVIEW.md) for complete control theory analysis

---

## Documentation

| Document | Description |
|----------|-------------|
| **[CLAUDE.md](CLAUDE.md)** | Project overview and architecture for Claude Code sessions |
| **[I2C_DRIVER_ANALYSIS.md](I2C_DRIVER_ANALYSIS.md)** | I2C driver conflict analysis and ESP-IDF migration guide |
| **[CONTROL_REVIEW.md](CONTROL_REVIEW.md)** | Control theory review and PD controller design |

---

## Project Structure

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

## Safety Features

### Automatic Motor Shutdown

The system monitors pitch angle and angular velocity in real-time and automatically stops the motor if safety limits are exceeded:

```cpp
if (|pitch| > 30° || |pitch_rate| > 300 dps) {
    motorStopSafe();  // Set current=0, output=0
    Display: "SAFETY STOP!"
}
```

### Safety Limit Configuration

Edit in `src/main.cpp`:
```cpp
static constexpr float PITCH_LIMIT_DEG = 30.0f;   // Angle limit [deg]
static constexpr float GYRO_LIMIT_DPS  = 300.0f;  // Rate limit [dps]
```

---

## Tuning Guide

### Before First Test

**⚠️ CRITICAL: Verify control axis direction**

1. Set `ENABLE_CONTROL = false` in `src/main.cpp` (monitor mode)

2. Observe pitch angle on display when tilting device

3. Verify sign convention matches expected behavior

4. If control diverges after enabling, reverse sign:
   ```cpp
   float cmd_f = -(KP_PITCH * pitch + KD_PITCH * pitch_rate);
   ```

### PD Gain Tuning

#### If System Oscillates
→ **Increase Kd** (add more damping)
```cpp
static constexpr float KD_PITCH = 10.0f;  // Increase from 5.0
```

#### If Response is Too Slow
→ **Increase Kp** (faster response)
```cpp
static constexpr float KP_PITCH = 150.0f;  // Increase from 100.0
```

#### If Motor Saturates
→ **Decrease both gains** (gentler control)
```cpp
static constexpr float KP_PITCH = 50.0f;
static constexpr float KD_PITCH = 2.5f;
```

### System Identification

For optimal control performance, measure system parameters:

- Reaction wheel inertia J_wheel [kg⋅m²]
- Pendulum mass m [kg]
- Center of mass distance l [m]
- Pendulum inertia J_pendulum [kg⋅m²]

See CONTROL_REVIEW.md for pole placement and LQR design methods.

---

## Troubleshooting

### Build Issues

**Error: "command not found: pio"**
```bash
pip install platformio
```

**Error: "Platform espressif32 not found"**
```bash
pio platform install espressif32@6.7.0
```

### Upload Issues

**Error: "No serial ports found"**
1. Check USB-C cable connection
2. Install CP210x USB driver if needed
3. Try different USB port

**Error: "Permission denied on serial port"** (Linux/Mac)
```bash
sudo chmod 666 /dev/ttyUSB0  # Replace with your port
```

### Runtime Issues

**BNO055 initialization fails**
1. Check I2C wiring (SDA=GPIO6, SCL=GPIO7)
2. Verify BNO055 I2C address (0x28 or 0x29)
3. Check power supply to BNO055

**Control diverges immediately**
1. Verify control axis direction (see Tuning Guide)
2. Try reversing control sign
3. Set ENABLE_CONTROL = false to monitor sensor readings

**"SAFETY STOP" appears frequently**
1. Check if device is properly mounted
2. Increase safety limits if appropriate
3. Check for sensor noise or calibration issues

---

## License

This project incorporates code from:

- **Adafruit_BNO055** library (MIT License) - Copyright (c) K.Townsend (Adafruit Industries)
- **M5Unified** library (MIT License) - Copyright (c) M5Stack

ESP-IDF I2C driver implementation:
- Copyright (c) 2026
- Based on proven Unit Roller I2C pattern

---

## Contact

For questions, issues, or contributions, please:

- Open an issue on GitHub
- Check existing documentation in `CLAUDE.md`, `I2C_DRIVER_ANALYSIS.md`, or `CONTROL_REVIEW.md`

---

## Acknowledgments

- Adafruit Industries for BNO055 library and documentation
- M5Stack for CoreS3 hardware and M5Unified library
- ESP-IDF team for robust I2C driver implementation

---

**Built with ❤️ for stable inverted pendulum control**
