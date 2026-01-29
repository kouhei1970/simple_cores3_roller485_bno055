# リアクションホイール倒立振子制御コードレビュー
# Reaction Wheel Inverted Pendulum Control Code Review

## 🚨 Critical Issues / 重大な問題

### 1. **制御則が不十分 (P制御のみ)**

**現在の実装** (main.cpp:123):
```cpp
float cmd_f = eul.y() * 100.0f;  // 比例制御のみ
int32_t cmd = (int32_t)cmd_f;
```

**問題点**:
- **角度（pitch）のみ**を使用した単純なP制御
- **角速度（gyro）フィードバックがない**
- 倒立振子には**最低限PD制御が必要**（比例＋微分）

**影響**:
- システムが不安定または振動的になる
- 外乱への応答が悪い
- ダンピングがなく発散しやすい

**数学的背景**:
倒立振子の運動方程式（線形化）:
```
J * θ̈ = m*g*l*sin(θ) - T_reaction
≈ m*g*l*θ - T_reaction  (小角度近似)
```

極配置法による安定化には：
```
T_reaction = Kp * θ + Kd * θ̇
```

現在の実装は `Kd = 0` であり、不安定です。

---

### 2. **Gyroデータの単位不一致（コメントミス）**

**Wire版のコメント** (main.cpp:120):
```cpp
imu::Vector<3> gyro = bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);   // [rad/s] ← 間違い
```

**実際の出力単位**:
- **Adafruit_BNO055**: dps (degrees per second)
- **BNO055_ESPIDF**: dps (degrees per second)

**証拠** (Adafruit_BNO055.cpp:428-432):
```cpp
case VECTOR_GYROSCOPE:
    /* 1dps = 16 LSB */
    xyz[0] = ((double)x) / 16.0;
    xyz[1] = ((double)y) / 16.0;
    xyz[2] = ((double)z) / 16.0;
    break;
```

**BNO055データシート 3.6.4節**:
- Gyroscope: 1 degree/s = 16 LSB (default)
- Unit selection register (0x3B) で rad/s に変更可能だが、デフォルトは dps

**問題点**:
- コメントが誤っているため、制御ゲイン設計時に混乱する
- rad/s と勘違いすると、ゲイン計算が約**57倍**ずれる（180/π ≈ 57.3）

---

### 3. **サンプリング時間の考慮不足**

**現在の実装** (main.cpp:168):
```cpp
delay(10);  // 10ms = 100Hz
```

**問題点**:
- サンプリング時間 Ts = 10ms は記録されていない
- デジタル制御理論では Ts が制御ゲインに影響
- ジッター（タイミングのばらつき）の管理がない

**推奨**:
```cpp
static constexpr float CONTROL_FREQ_HZ = 100.0f;
static constexpr float CONTROL_DT_SEC  = 1.0f / CONTROL_FREQ_HZ;  // 0.01秒
```

---

### 4. **安全機構の未実装**

**定義されているが使われていない** (main.cpp:54-57):
```cpp
static void motorStopSafe() {
  Roller.setCurrent(0);
  Roller.setOutput(0);
}
// ← どこからも呼ばれていない
```

**必要な安全機構**:
1. **角度リミット**: |pitch| > 30° で停止
2. **角速度リミット**: |gyro_y| > 300 dps で停止
3. **通信エラー検出**: BNO055読み取り失敗時の処理
4. **電流リミット**: 既に実装済み（±100000）だが、保護動作が不明確

---

### 5. **制御ゲインが未調整**

**現在の比例ゲイン** (main.cpp:123):
```cpp
Kp = 100.0  // [モーター電流単位 / degree]
```

**問題点**:
- **経験的な値で根拠がない**
- システム同定（パラメータ推定）なしで設定
- 微分ゲイン Kd = 0

**倒立振子の典型的なゲイン範囲**:
```
Kp: 10 ~ 500   [制御入力/deg]
Kd: 0.5 ~ 50   [制御入力/(deg/s)]
```

実際の値はシステムパラメータに依存:
- リアクションホイールの慣性モーメント J_wheel
- 振子の質量 m、長さ l
- 振子の慣性モーメント J_pendulum

---

### 6. **制御軸の未確認**

**Pitch軸を使用** (main.cpp:123):
```cpp
float cmd_f = eul.y() * 100.0f;  // eul.y() = pitch (Y軸周り回転)
```

**確認すべき点**:
1. **座標系の定義**:
   - BNO055の X, Y, Z 軸方向
   - M5Stack CoreS3 の取り付け方向
   - Pitch (eul.y) が実際に倒立振子の倒れ方向と一致するか

2. **制御方向の確認**:
   - pitch > 0 のとき、どちらに傾いている？
   - モーター電流 > 0 のとき、ホイールはどちらに回転？
   - **符号が逆だとシステムが発散する**

3. **軸マッピング**:
   - BNO055_ESPIDF では `setAxisRemap()` が実装済み
   - 必要に応じて座標軸を再マッピング可能

---

## ✅ 推奨される改善策

### A. PD制御の実装（最優先）

```cpp
// ============================================================================
// Control Parameters (adjust based on system identification)
// 制御パラメータ（システム同定に基づいて調整）
// ============================================================================
static constexpr float CONTROL_FREQ_HZ = 100.0f;       // [Hz] サンプリング周波数
static constexpr float CONTROL_DT_SEC  = 0.01f;        // [s] サンプリング時間
static constexpr float KP_PITCH        = 100.0f;       // [current/deg] 比例ゲイン
static constexpr float KD_PITCH        = 5.0f;         // [current/(deg/s)] 微分ゲイン
static constexpr float MAX_CURRENT     = 100000.0f;    // 最大電流
static constexpr float PITCH_LIMIT_DEG = 30.0f;        // 角度リミット
static constexpr float GYRO_LIMIT_DPS  = 300.0f;       // 角速度リミット

void loop() {
  M5.update();

  // Read sensor data
  // センサーデータ読み取り
  imu::Vector<3> eul  = bno.getVector(VECTOR_EULER);      // [deg]
  imu::Vector<3> gyro = bno.getVector(VECTOR_GYROSCOPE);  // [dps]

  float pitch      = eul.y();      // [deg] ピッチ角
  float pitch_rate = gyro.y();     // [dps] ピッチ角速度

  // Safety check: stop if tilted too much
  // 安全チェック：傾きすぎたら停止
  if (abs(pitch) > PITCH_LIMIT_DEG || abs(pitch_rate) > GYRO_LIMIT_DPS) {
    motorStopSafe();
    M5.Display.printf("SAFETY STOP!\n");
    while(1) { delay(100); }
  }

  // PD Control Law
  // PD制御則
  float cmd_f = KP_PITCH * pitch + KD_PITCH * pitch_rate;

  // Saturate command
  // コマンド飽和
  int32_t cmd = (int32_t)constrain(cmd_f, -MAX_CURRENT, MAX_CURRENT);

  Roller.setCurrent(cmd);

  // ... (display code)
}
```

---

### B. 単位の統一と明確化

**ヘッダーに定数を追加**:
```cpp
// Unit conversion constants
// 単位変換定数
static constexpr float DEG_TO_RAD = M_PI / 180.0f;
static constexpr float RAD_TO_DEG = 180.0f / M_PI;
```

**コメント修正**:
```cpp
// Both drivers output degrees per second (dps)
// 両ドライバは degrees per second (dps) を出力
imu::Vector<3> gyro = bno.getVector(VECTOR_GYROSCOPE);  // [dps] NOT [rad/s]
```

---

### C. システム同定の実施

制御ゲインを科学的に決定するため、以下のパラメータを測定：

1. **リアクションホイール**:
   - 慣性モーメント J_wheel [kg⋅m²]
   - 最大トルク T_max [N⋅m]
   - 電流-トルク特性 K_t [N⋅m/A]

2. **振子本体**:
   - 質量 m [kg]
   - 重心までの距離 l [m]
   - 慣性モーメント J_pendulum [kg⋅m²]

3. **開ループ応答実験**:
   - ステップ入力（一定電流）を与える
   - pitch角の時間応答を記録
   - 固有周波数 ω_n と減衰比 ζ を推定

4. **ゲイン調整**:
   - Ziegler-Nichols法
   - 極配置法
   - LQR（Linear Quadratic Regulator）

---

### D. ディスプレイ表示の改善

制御デバッグ用の情報を追加：

```cpp
M5.Display.printf("P:%+6.2f Pd:%+7.2f cmd:%ld\n",
                  pitch, pitch_rate, (long)cmd);
M5.Display.printf("Kp:%+7.0f Kd:%+6.2f\n", KP_PITCH, KD_PITCH);
```

---

### E. 制御軸の検証手順

**ステップ1: 静止状態でBNO055の軸を確認**
```cpp
void setup() {
  // ... (初期化コード)

  // Display axis orientation
  // 軸方向を表示
  M5.Display.printf("Tilt device and observe:\n");
  M5.Display.printf("Pitch(Y): forward/back\n");
  M5.Display.printf("Roll(X): left/right\n");
  delay(5000);
}
```

**ステップ2: 開ループテスト**
```cpp
// Open-loop test: apply constant current and observe
// 開ループテスト：一定電流を与えて観察
void testOpenLoop() {
  Roller.setCurrent(10000);  // Small positive current / 小さな正電流
  delay(2000);

  // Does the wheel rotate in the expected direction?
  // ホイールは期待した方向に回転する？
  // Does the pendulum tilt in the stabilizing direction?
  // 振子は安定化する方向に傾く？

  Roller.setCurrent(0);
}
```

**ステップ3: 符号確認**
```
期待される動作:
1. pitch > 0 (前に傾く) → motor current > 0 (ホイール正回転) → 振子が後ろに押される
2. pitch < 0 (後ろに傾く) → motor current < 0 (ホイール逆回転) → 振子が前に押される

もし逆なら:
- モーター電流の符号を反転: `cmd = -cmd;`
- または制御ゲインの符号を反転: `KP_PITCH = -100.0f;`
```

---

### F. データロギング（推奨）

制御性能の評価とデバッグのため：

```cpp
// Log data to Serial for plotting
// プロット用にシリアルにデータ出力
if (millis() % 100 == 0) {  // 10Hz logging / 10Hzでログ
  Serial.printf("%lu,%.3f,%.3f,%ld\n",
                millis(), pitch, pitch_rate, (long)cmd);
}
```

Pythonやmatplotlibでプロット可能:
```python
import matplotlib.pyplot as plt
import pandas as pd

data = pd.read_csv('log.csv', names=['time_ms', 'pitch', 'pitch_rate', 'cmd'])
plt.plot(data['time_ms'], data['pitch'])
plt.xlabel('Time [ms]')
plt.ylabel('Pitch [deg]')
plt.show()
```

---

## 📊 倒立振子制御の理論背景

### 線形化モデル

小角度近似 (θ ≈ sin(θ), cos(θ) ≈ 1) のもとで：

```
状態変数: x = [θ, θ̇]ᵀ  (角度、角速度)
入力:     u = T_reaction  (リアクションホイールのトルク)

状態方程式:
ẋ = A*x + B*u

A = [0,           1        ]
    [m*g*l/J,     0        ]

B = [0     ]
    [-1/J  ]

ここで:
- m: 振子の質量 [kg]
- g: 重力加速度 9.81 [m/s²]
- l: 重心までの距離 [m]
- J: 振子の慣性モーメント [kg⋅m²]
```

### 極配置法によるゲイン設計

閉ループ極を s = -ζω_n ± jω_n√(1-ζ²) に配置:

```
u = -K*x = -[Kp, Kd]*[θ, θ̇]ᵀ

典型的な設計仕様:
- 固有周波数 ω_n = 5 ~ 10 [rad/s]
- 減衰比 ζ = 0.7 (臨界制動に近い)

結果として:
Kp = (J*ω_n² - m*g*l)
Kd = 2*ζ*ω_n*J
```

### 現在の実装の問題

```
現在: u = -100.0 * θ  (Kp = 100, Kd = 0)

閉ループ極:
s² - (m*g*l/J)*s + (100/J) = 0

Kd = 0 のため、減衰がない。
もし m*g*l/J > 0 なら、一方の極が正の実部を持ち、不安定。
```

---

## 🎯 次のステップ

### 優先度1: PD制御の実装（即座に）
- [ ] KP_PITCH, KD_PITCH 定数を定義
- [ ] pitch_rate (gyro.y) を制御則に追加
- [ ] 安全リミットの実装

### 優先度2: システム検証（初回テスト前に）
- [ ] 座標軸の確認（pitch軸が倒立方向と一致するか）
- [ ] 制御方向の確認（符号が正しいか）
- [ ] 開ループテストの実施

### 優先度3: ゲイン調整（運転開始後）
- [ ] 初期ゲイン（Kp=100, Kd=5）でテスト
- [ ] 応答を観察し、Kd を増やして振動を抑制
- [ ] Kp を調整して応答速度を最適化

### 優先度4: 高度な制御（安定化後）
- [ ] 積分項の追加（PID制御）
- [ ] カルマンフィルタによるノイズ除去
- [ ] LQRによる最適制御
- [ ] 非線形制御（大角度での動作）

---

## 📚 参考資料

### BNO055関連
- **BNO055 Datasheet**: Section 3.6.4 (Data Output Formats)
- **Unit Selection Register**: 0x3B, bit 1 (Gyro: 0=dps, 1=rad/s)

### 倒立振子制御理論
- **Ogata "Modern Control Engineering"**: Chapter 7 (State-Space Design)
- **Åström & Murray "Feedback Systems"**: Chapter 6 (State Feedback)

### ESP32 リアルタイム制御
- **ESP-IDF Timer API**: High-precision timing for control loops
- **FreeRTOS**: Task scheduling with fixed sampling rate

---

**作成日**: 2026-01-29
**対象プロジェクト**: simple_cores3_roller485_bno055
**制御対象**: リアクションホイール倒立振子
