<!-- PROJECT HEADER -->
<h1 align="center">👁️‍🗨️ Semicolon — <i>Where Syntax Meets Sight</i></h1>


<p align="center">
  An assistive vision & navigation device built using <b>ESP32-CAM</b>, <b>TF-Luna LiDAR</b>, and <b>HC-SR04 Ultrasonic</b> sensors.<br>
  Runs an on-device <b>Edge Impulse</b> model for real-time <b>Indian currency detection</b> with haptics and buzzer feedback.  
</p>

<div align="center">

![Build](https://img.shields.io/badge/build-passing-brightgreen)
![Platform](https://img.shields.io/badge/platform-ESP32--CAM-blue)
![Lang](https://img.shields.io/badge/language-C%2FC%2B%2B-orange)
![License](https://img.shields.io/badge/license-MIT-lightgrey)
![AI Model](https://img.shields.io/badge/AI-EdgeImpulse-yellow)

</div>

---

## ✨ Features

- **👣 Navigation:**  
  LiDAR as primary + ultrasonic as backup → vibration intensity scales with distance.  
  Uses **SSE-based web hosting** for low-latency updates.

- **💵 Currency Detection:**  
  On-device **Edge Impulse** model detects and identifies Indian banknotes via the ESP32-CAM.  
  Provides **voice output** feedback for blind or visually impaired users.

- **🔀 Modes:**  
  - Short press: toggle between **Navigation ↔ Currency**  

- **🌐 Camera Server:**  
  Built-in web interface for **live streaming**, capture, and remote configuration.

---

## 🗂️ Repository Layout
📦 Semicolon-Embedded-C-Project
```

├── 📂 currencydetection_c
│   ├── currency_detector_wrapper.cpp      # Wrapper for currency detection logic
│   ├── currency_detector_wrapper.o        # Compiled object file
│   ├── main.c                             # Main application entry for currency detection
│
├── 📂 lidar
│   ├── lidar_realtime_working_appupdated.c  # Real-time LiDAR data acquisition and processing
│   ├── lidar_with_ui.c                      # LiDAR integration with web/UI interface
│
├── 📂 CameraWebServer1
│   ├── CameraWebServer1.ino               # Main ESP32-CAM web server sketch
│   ├── app_httpd.cpp                      # HTTP server application for streaming
│   ├── board_config.h                     # Board configuration and pin definitions
│   ├── camera_index.h                     # HTML page template for camera streaming
│   ├── camera_pins.h                      # Camera pin mapping for various ESP32 boards
│   ├── cl.json                            # Configuration or class label file
│   ├── partitions.csv                     # ESP32 partition layout file
│
├── EdgeImpulseModel.c                     # Embedded Edge Impulse model for ML inference
│
├── README.md                              # Documentation for setup and usage
└── LICENSE                                # License file (if applicable)
```
---

---

## ⚙️ Hardware Setup

| Component | Description / Notes |
|------------|--------------------|
| **ESP32-CAM (AI-Thinker)** | PSRAM enabled, main controller |
| **TF-Luna LiDAR (UART)** | For distance sensing |
| **HC-SR04 Ultrasonic** | Backup sensor (TRIG/ECHO; ECHO via 5V→3.3V divider) |
| **Vibration Motor (ERM)** | Controlled via NPN transistor (2N2222) with 1 kΩ base resistor |
| **Active Buzzer / I²S Speaker** | Audio or vibration feedback |
| **Li-ion / LiPo (3.7 V)** | Power source with TP4056 + boost converter |
| **Momentary Push Button** | Mode selector (short = toggle / long = mute) |

---

### 🧩 Pin Configuration

| Signal | ESP32-CAM Pin | Function / Notes |
|--------|----------------|------------------|
| TF-Luna TX/RX | GPIO14 (UART) / GPIO15 (RX2) | `Serial2` for LiDAR |
| HC-SR04 TRIG/ECHO | GPIO14 / GPIO15 | ECHO via voltage divider | We didnt use HC-SR04
| Vibration Motor | GPIO12 → transistor | 1 kΩ base resistor, diode across motor |
| Buzzer | GPIO13 | Active buzzer output |
| Button | GPIO2 | `INPUT_PULLUP` |
| Power | 5 V / GND | From boost converter, common ground |

---

## 🧰 Build & Flash Instructions

### 🔹 Arduino IDE
1. Install **ESP32 board support** via Board Manager.  
2. Select board: **AI Thinker ESP32-CAM**  
   - PSRAM: **Enabled**  
   - Partition: **Huge APP** or `partitions.csv`  
3. Open and upload `CameraWebServer1/CameraWebServer1.ino`  
4. Monitor serial output (`115200 baud`) to find the device IP.  
5. Access web UI via:  

---

### 🔹 PlatformIO (Arduino IDE)
```ini
[env:esp32cam]
platform = espressif32
board = esp32cam
framework = arduino
board_build.partitions = partitions.csv
build_flags =
-DEI_CLASSIFIER_TFLITE_ENABLE_ESP_NN=1
-DEI_CLASSIFIER_ALLOCATION_STATIC=1
monitor_speed = 115200
```
<p align="center">
  <img src="https://readme-typing-svg.herokuapp.com?font=Fira+Code&duration=2500&pause=500&color=00C8FF&center=true&vCenter=true&width=600&lines=👁️‍🗨️+Semicolon+—+Where+Syntax+Meets+Sight;Built+with+ESP32-CAM+and+Edge+Impulse+💡" alt="Typing SVG" />
</p>


