# 🤖 ESP32 Odometry Bot

> Real-time robot localization and path visualization using encoder dead reckoning + IMU sensor fusion, served over WebSocket to a browser dashboard.

---

![Demo GIF](assets/demo.gif)
> *Replace with your own demo image or GIF*

---

## 📖 Overview

The **ESP32 Odometry Bot** is a differential-drive robot that tracks its own position in real time using wheel encoders and a gyroscope. It streams live pose data (X, Y, heading θ) over WebSocket to a self-hosted browser dashboard, where the robot's path is drawn on an HTML5 canvas with a turtle-style directional pointer.

This is **Phase 1** of a larger robotics project. The goal of this phase was to build a solid, reliable odometry and communication foundation before adding more complexity.

**Inspired by:** [Bench Robotics — Differential Drive Odometry](https://youtu.be/Q_frEzHduPs?si=fmfbe_z3UwtfOlZt)

---

## ✨ Features

- 🕹️ **Browser-based D-pad control** over Wi-Fi (no app needed)
- 📡 **Real-time WebSocket telemetry** — X, Y, θ streamed at 150 ms intervals
- 🧭 **Sensor fusion** — encoder odometry + MPU6050 gyro for accurate heading
- 🗺️ **Live path visualization** on an HTML5 canvas
- ⚙️ **Gyro auto-calibration** at startup to remove resting drift
- 💾 **Dashboard served from flash** (PROGMEM) — no SD card or external server needed

---

## 🗂️ Project Structure

```
ESP32-Odometry-Bot/
├── src/
│   └── main.ino        # Main firmware (motor control, odometry, WebSocket server)
├── include/
│   └── index.h         # Dashboard HTML stored in PROGMEM
├── assets/
│   └── demo.gif        # Demo image / GIF (add your own)
└── README.md
```

---

## 🔧 Hardware Used

| Component | Purpose |
|---|---|
| ESP32 Dev Board | Main microcontroller |
| DRV8833 Motor Driver | Dual H-bridge PWM motor control |
| 2× DC Gear Motors | Differential drive wheels |
| 2× Quadrature Encoders | Wheel tick counting for odometry |
| MPU6050 IMU | Gyroscope Z-axis for heading |
| Power Supply | Battery pack for motors + ESP32 |

---

## 📌 Wiring Reference

### DRV8833 Motor Driver → ESP32

The DRV8833 uses two PWM input pins per motor channel. Speed and direction are both controlled via PWM — one pin gets the duty cycle, the other stays LOW (or vice versa for reverse).

| DRV8833 Pin | ESP32 GPIO | Description |
|---|---|---|
| AIN1 | GPIO 33 | Left Motor — Forward |
| AIN2 | GPIO 32 | Left Motor — Backward |
| BIN1 | GPIO 25 | Right Motor — Forward |
| BIN2 | GPIO 26 | Right Motor — Backward |
| VM | Battery + | Motor power supply (up to 10V) |
| GND | GND | Common ground (ESP32 + battery) |
| VCC | 3.3V or 5V | Logic power |
| STBY | 3.3V or VCC | Pull HIGH to enable driver (tie to VCC if not using sleep mode) |

> ⚠️ **Important:** The DRV8833 is a current-limited motor driver. Unlike the L298N, it does not have a separate enable pin — direction and speed are both controlled purely by PWM duty cycle on AIN1/AIN2 and BIN1/BIN2.

---

### Quadrature Encoders → ESP32

Each motor has a quadrature encoder with two output channels (A and B). The ESP32Encoder library uses both channels in full-quad mode for maximum resolution.

| Encoder | Channel | ESP32 GPIO | Description |
|---|---|---|---|
| Left Motor | A | GPIO 14 | Left encoder channel A |
| Left Motor | B | GPIO 13 | Left encoder channel B |
| Right Motor | A | GPIO 4 | Right encoder channel A |
| Right Motor | B | GPIO 5 | Right encoder channel B |
| Both | VCC | 3.3V | Encoder power |
| Both | GND | GND | Common ground |

> 📝 **Note:** If the encoder counts backwards when moving forward, swap the A and B pins for that encoder, or negate the tick delta in firmware.

---

### MPU6050 IMU → ESP32

The MPU6050 uses I2C. Custom pins are used here to avoid conflicts with other peripherals.

| MPU6050 Pin | ESP32 GPIO | Description |
|---|---|---|
| SDA | GPIO 18 | I2C Data |
| SCL | GPIO 19 | I2C Clock |
| INT | GPIO 15 | Interrupt (optional — not used in polling mode) |
| VCC | 3.3V | Power |
| GND | GND | Ground |
| AD0 | GND | I2C address select (LOW = 0x68) |

> ⚠️ **Critical:** Always call `Wire.begin(18, 19)` **before** `mpu.begin()` in your setup. The Adafruit MPU6050 library does not accept pin arguments — it relies on the Wire object being pre-configured.

---

### Full GPIO Summary

| GPIO | Role |
|---|---|
| 33 | Left Motor IN1 (Forward) |
| 32 | Left Motor IN2 (Backward) |
| 25 | Right Motor IN1 (Forward) |
| 26 | Right Motor IN2 (Backward) |
| 14 | Left Encoder A |
| 13 | Left Encoder B |
| 4 | Right Encoder A |
| 5 | Right Encoder B |
| 18 | MPU6050 SDA |
| 19 | MPU6050 SCL |
| 15 | MPU6050 INT (optional) |

---

## 📚 Software Dependencies

Install these libraries via Arduino Library Manager or PlatformIO:

| Library | Author | Purpose |
|---|---|---|
| `AsyncTCP` | me-no-dev | Async TCP layer for ESP32 |
| `ESPAsyncWebServer` | me-no-dev | Async HTTP + WebSocket server |
| `ArduinoJson` | Benoit Blanchon | JSON parsing for WebSocket messages |
| `ESP32Encoder` | madhephaestus | Quadrature encoder reading |
| `Adafruit MPU6050` | Adafruit | IMU sensor driver |
| `Adafruit Unified Sensor` | Adafruit | Sensor abstraction layer |

---

## 🚀 Getting Started

### 1. Clone the Repository

```bash
git clone https://github.com/YOUR_USERNAME/ESP32-Odometry-Bot.git
cd ESP32-Odometry-Bot
```

### 2. Configure Wi-Fi Credentials

Open `src/main.ino` and update the following line in `setup()`:

```cpp
WiFi.begin("YOUR_SSID", "YOUR_PASSWORD");
```

### 3. Calibrate for Your Hardware

At the top of `main.ino`, adjust these constants to match your motors and encoders:

```cpp
const float WHEEL_DIAMETER  = 0.065;   // meters — measure your wheel
const float TICKS_PER_REV   = 345.0;   // encoder ticks per full wheel revolution
const int   DRIVE_SPEED     = 100;     // PWM duty (0–255); below ~80 may not move
```

### 4. Flash the ESP32

- Open in Arduino IDE (select **ESP32 Dev Module** as the board)
- Or use PlatformIO with the provided config
- Upload and open Serial Monitor at **115200 baud**

### 5. Open the Dashboard

After boot, the Serial Monitor will print the ESP32's IP address:

```
Connected! Open this IP in your browser:
192.168.x.x
```

Navigate to that IP in any browser on the same Wi-Fi network. The dashboard will load and connect automatically.

> ⚠️ **Calibration window:** The robot runs a 2-second gyro calibration at startup. **Do not move the robot** during this time or the heading offset will be wrong.

---

## 🧠 How It Works

### Odometry Loop (every 20 ms)

```
1. Read encoder tick deltas  →  convert to wheel distances
2. Average both distances    →  forward displacement (d_center)
3. Read gyro Z angular rate  →  integrate over dt  →  update heading θ
4. Apply deadband filter     →  suppress drift when stationary
5. Update global X, Y pose   →  using cos(θ), sin(θ) projection
```

### Sensor Fusion

Encoders handle **distance**. The MPU6050 gyroscope handles **rotation**. This split avoids the heading error that builds up fast with encoder-only differential drive on real surfaces.

### WebSocket Flow

```
Browser  ──── JSON command {F/B/L/R/S} ────►  ESP32
Browser  ◄─── JSON telemetry {x, y, θ} ────  ESP32  (every 150 ms)
```

---

## ⚠️ Known Challenges

### I2C Bus Conflicts with MPU6050
The ESP32's default I2C pins (GPIO 21/22) conflicted with other signals in this build. The fix was remapping the I2C bus using `Wire.begin(18, 19)` before calling `mpu.begin()`. If you see `"Failed to find MPU6050"` on boot, this is the first thing to check.

### Wi-Fi Connectivity
The firmware will stall indefinitely if Wi-Fi credentials are wrong or the network is unavailable. A connection timeout with AP-mode fallback is planned for a future phase. During development, using a stable access point (not a mobile hotspot) resolved most connection issues.

---

## 🗺️ Roadmap

| Phase | Feature | Status |
|---|---|---|
| ✅ Phase 1 | Encoder odometry + IMU fusion + WebSocket dashboard | **Complete** |
| 🔲 Phase 2 | TOF sensor (VL53L0X) for obstacle detection | Planned |
| 🔲 Phase 3 | PID closed-loop motor speed control | Planned |
| 🔲 Phase 4 | SLAM — simultaneous localization and mapping | Planned |
| 🔲 Phase 5 | micro-ROS integration for ROS2 compatibility | Planned |

---

## 📄 License

MIT License — feel free to use, modify, and build on this.

---

## 🙏 Credits

- [Bench Robotics](https://youtu.be/Q_frEzHduPs?si=fmfbe_z3UwtfOlZt) — original differential drive odometry inspiration
- [me-no-dev](https://github.com/me-no-dev) — ESPAsyncWebServer & AsyncTCP
- [madhephaestus](https://github.com/madhephaestus) — ESP32Encoder
- [Adafruit](https://github.com/adafruit) — MPU6050 library
- [Benoit Blanchon](https://github.com/bblanchon) — ArduinoJson
