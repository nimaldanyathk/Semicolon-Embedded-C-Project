
# 👁️‍🗨️ Semicolon — *Where syntax meets Sight*
An assistive device built on **ESP32-CAM + TF-Luna LiDAR + HC-SR04**, running an **Edge Impulse** model for on-device currency detection, with haptics and buzzer feedback.

<p align="center">
  <img src="assets/hero.jpg" width="80%" alt="Device hero image"/>
</p>

<div align="center">

![Build](https://img.shields.io/badge/build-passing-brightgreen)
![Platform](https://img.shields.io/badge/platform-ESP32--CAM-blue)
![Lang](https://img.shields.io/badge/lang-C%2FC%2B%2B-orange)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

</div>

---

## ✨ Features
- **Navigation:** LiDAR primary + ultrasonic backup → vibration intensity scales with distance.  
- **Currency detection:** Edge Impulse model runs fully on ESP32-CAM.  
- **Modes:** Button toggles NAV ↔ CURRENCY; long-press = MUTE.  
- **Camera server:** Built-in web UI for live stream and capture.  

---

## 📸 Images to upload
Upload these files to `assets/` in the repo root:

- `assets/hero.jpg` → Hero banner/device render  
- `assets/build.jpg` → Breadboard build photo  
- `assets/wiring.png` → Wiring diagram/schematic  
- `assets/webui.png` → Screenshot of web UI  
- `assets/coverage.png` → LiDAR + ultrasonic coverage diagram  
- `assets/currency-demo.gif` → Short demo (currency detection)  

---

## 🗂️ Repo layout

.
├─ firmware/
│  ├─ src/
│  │  ├─ main.cpp
│  │  ├─ drivers/ (tf_luna., ultrasonic., haptics., audio., button., camera.)
│  │  └─ inference/edge_impulse/  # drop exported EI library here
│  └─ platformio.ini
├─ mechanical/ (enclosure.stl, faceplate.stl)
├─ assets/ (images listed above)
└─ README.md

---

## 🛠️ Hardware
- **ESP32-CAM (AI-Thinker)** with **PSRAM enabled**  
- **TF-Luna LiDAR** (UART)  
- **HC-SR04 Ultrasonic** (TRIG/ECHO; ECHO via 5V→3.3V divider)  
- **Vibration motor** (ERM) + NPN transistor (2N2222) + 1 kΩ resistor + diode  
- **Active buzzer** (or I2S amp + speaker)  
- **3.7 V Li-ion/LiPo** + TP4056 charger (with protection) + 5 V boost  
- **Momentary push button**  
- *(Optional)* OLED I²C screen  

---

## 🔌 Wiring (example)
<p align="center">
  <img src="assets/wiring.png" width="80%" alt="Wiring diagram"/>
</p>

| Signal | ESP32-CAM Pin | Notes |
|---|---|---|
| TF-Luna TX/RX | GPIO16 (RX2) / GPIO17 (TX2) | `Serial2` |
| HC-SR04 TRIG/ECHO | GPIO14 / GPIO15 | ECHO via divider |
| Vibration motor | GPIO12 → transistor | base 1kΩ, diode across motor |
| Buzzer | GPIO13 | Active buzzer |
| Button | GPIO2 | `INPUT_PULLUP` |
| Power | 5 V / GND | From boost module; common GND |

---

## ⚙️ Build & Flash

**Arduino IDE**
1. Install ESP32 board support.  
2. Board: **AI Thinker ESP32-CAM**; PSRAM: **Enabled**; Partition: **Huge APP** or `partitions.csv`.  
3. Upload `esp32_c.ino` (or `main.cpp` converted to `.ino`).  
4. Serial Monitor @ 115200 → get device IP.  

**PlatformIO (VS Code)**  
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

cd firmware
pio run -t upload
pio device monitor


⸻

🧠 Edge Impulse (currency)
	1.	In Edge Impulse → Deploy → Arduino Library (EON + int8).
	2.	Place library in firmware/src/inference/edge_impulse/.
	3.	Use grayscale, QQVGA/QVGA frames to save RAM.
	4.	Inference result → beep patterns.

⸻

🌐 Web Endpoints
	•	/ → control page
	•	/stream → MJPEG stream
	•	/capture → single JPEG
	•	/status → JSON camera status
	•	/control?var=<name>&val=<int> → set camera param

<p align="center">
  <img src="assets/webui.png" width="80%" alt="Web UI"/>
</p>



⸻

🧭 Modes
	•	NAV: haptics scale with distance.
	•	CURRENCY: run inference; beep per denomination.
	•	MUTE: hold button ~1.5s.

⸻

🧪 Quick test
	1.	Power up device.
	2.	Visit http://<esp32-ip>/capture → see still image.
	3.	Visit http://<esp32-ip>/stream → live video works.
	4.	Press button to switch NAV/CURRENCY; listen for feedback.

⸻

🧯 Troubleshooting
	•	Camera init fail / resets: Power sag → use boost module >700 mA; disable Wi-Fi if unused:

#include <WiFi.h>
void setup(){ WiFi.mode(WIFI_OFF); btStop(); }


	•	PSRAM allocation error: Lower frame size; use grayscale; ensure Huge APP/custom partition.
	•	Ultrasonic noisy: Keep wires short; use voltage divider; common ground.

⸻

🗺️ Roadmap
	•	Tie EI inference tightly to camera ring buffer.
	•	I2S amp + mini speaker for clearer voice prompts.
	•	OLED status screen.
	•	Perfboard/PCB + 3D-printed enclosure.

⸻

🙌 Credits
	•	Edge Impulse for TinyML tooling.
	•	ESP32 + open hardware communities.

⸻

📄 License

MIT

Do you want me to also give you the **GitHub commands** (from `git init` → `commit` → `push`) so you can upload this `README.md` file into a fresh repo directly?
