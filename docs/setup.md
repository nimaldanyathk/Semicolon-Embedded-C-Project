# Build, flash, and use

## Requirements

- Python 3.10+ and Git.
- PlatformIO Core **6.1.19** (CLI or the VS Code extension's compatible Core).
- One supported board and the wiring in [hardware.md](hardware.md).

The project pins `espressif32@6.12.0`, which uses Arduino-ESP32 **2.0.17**.
This avoids mixing the Arduino 2.x and 3.x LEDC/camera APIs found in the original
sketches. The [PlatformIO platform manifest](https://github.com/platformio/platform-espressif32/blob/v6.12.0/platform.json)
records the underlying framework/toolchain versions.

## Set up the project

```sh
git clone https://github.com/nimaldanyathk/Semicolon-Embedded-C-Project.git
cd Semicolon-Embedded-C-Project
python3 -m venv .venv
source .venv/bin/activate
python -m pip install platformio==6.1.19
cp include/config.example.h include/config.h
```

On Windows, activate with `.venv\Scripts\activate` and copy the example using
your shell's file-copy command. Edit `include/config.h`:

- Set `SEMICOLON_WIFI_SSID` and `SEMICOLON_WIFI_PASSWORD` for a 2.4 GHz network.
- Optionally set `SEMICOLON_AP_PASSWORD` to an **8–63 character** password. This
  enables a fallback access point named `Semicolon` after connection failure.
- To use only the access point, leave the Wi-Fi SSID empty and configure its
  password.
- Leave `SEMICOLON_ULTRASONIC` at `0` unless the optional sensor is correctly
  connected. Change to `1` to enable it.

The local configuration is ignored by Git. With no network configuration the
firmware still builds and prints sensor data over serial, but does not create
an open access point. Credentials are embedded in your flashed firmware: do
not share your configured binaries.

## Build and upload

| Board | Environment | Camera |
| --- | --- | --- |
| AI Thinker ESP32-CAM | `navigation-esp32cam` | Yes |
| ESP32 Dev Module | `navigation-esp32dev` | No |

```sh
pio run -e navigation-esp32cam
pio run -e navigation-esp32cam -t upload
pio device monitor -b 115200
```

For a Dev Module, replace the environment name. Use `pio device list` and
`--upload-port PORT` if automatic port selection is ambiguous. Builds embed the
web assets automatically; there is no filesystem upload step.

For a bare ESP32-CAM, use 3.3 V UART signaling, cross adapter TX to board RX and
adapter RX to board TX, and connect ground. Power the board from an adequate
5 V supply. Hold GPIO0 to ground while resetting to enter the bootloader, upload,
then disconnect GPIO0 from ground and reset to run the application. Do not assume
a USB-UART adapter's 3.3 V output can power the board and camera.

## Use the dashboard

1. Read the URL printed at 115200 baud. With fallback AP mode, join `Semicolon`
   using your configured password and use the AP IP printed in the log.
2. Open `http://DEVICE_IP/` (port **80**).
3. Select **Enable alerts**. Choose beep, spoken warning, and/or phone vibration.
4. Adjust the warning distance from 20 to 300 cm. The default is 80 cm.
5. On ESP32-CAM, select **Start preview** for the optional MJPEG stream, or
   **Open still image** for a JPEG capture.

Keep the tab visible and the device awake. Browser speech, vibration, and audio
permissions vary; iOS browsers generally do not provide web vibration. The
interface reports support and requires an explicit gesture for alerts. A browser
dashboard is not an always-on hardware alarm.

## Network behavior

The initial station connection has a 15-second deadline while sensor sampling
continues. If configured, the password-protected fallback AP starts on failure;
it also starts after a later 15-second disconnection. Station auto-reconnect
remains enabled. An AP already started remains available until reboot.

The camera and telemetry endpoints use local HTTP without authentication or TLS.
Use a trusted isolated network; do not port-forward them to the Internet.
Changing/removing a credential in this checkout does not remove it from old Git
commits. Rotate the Wi-Fi password that was previously committed.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| No upload port | USB cable/driver, adapter wiring, port selection, bootloader mode |
| Brownout or resets | Supply stability, cable losses, sensor load, common ground |
| No dashboard | Serial IP, matching network, local network permissions, guest-network isolation, configured AP password |
| Sensor unavailable | TF-Luna UART mode, TX→RX wiring, baud rate, common ground, valid target and signal strength |
| Camera unavailable | Board selection, ribbon cable, power, PSRAM; distance sampling continues |
| Preview fails | Close another stream; the device supports one streaming client |
| No sound/vibration | Press Enable alerts; check browser support, permissions, mute state, and foreground tab |
| Missing `c_currency_inferencing.h` | Currency model is not included; see [currency.md](currency.md) |

The Arduino IDE is not the maintained build path: this layout uses explicit
C/C++ compilation, shared libraries, and embedded assets through PlatformIO.
