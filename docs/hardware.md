# Hardware and wiring

The supported navigation targets are **AI Thinker ESP32-CAM (OV2640)** and a
standard **ESP32 Dev Module**. They share the same C sensor core; the Dev Module
build does not include camera support. Do not select an ESP32-S2/S3/C3 board for
these pin assignments.

## Parts

| Part | Use | Required |
| --- | --- | --- |
| ESP32-CAM with OV2640, or ESP32 Dev Module | Controller and Wi-Fi | Yes |
| Benewake TF-Luna configured for UART, 115200 baud, 9-byte/cm output | Primary distance sensor | Yes |
| Regulated 5 V supply with adequate current for the board and sensors | Power | Yes |
| USB-to-UART adapter with **3.3 V logic** | Flashing ESP32-CAM | For ESP32-CAM |
| HC-SR04 and echo level shifting/divider | Optional backup distance sensor | No |
| Phone/computer with a browser on the same network | Dashboard and browser alerts | For web interface |

Start with bench power and verify supply voltage under camera/Wi-Fi load. Battery
charging, boost conversion, enclosure, and battery protection are hardware design
tasks; this repository does not include a validated power schematic.

## Navigation connections

| Sensor signal | ESP32-CAM | ESP32 Dev Module | Notes |
| --- | --- | --- | --- |
| TF-Luna TX | GPIO14 | GPIO16 | ESP32 receives on UART1 |
| TF-Luna RX | Unconnected | Unconnected | Firmware sends no configuration commands |
| TF-Luna VCC / GND | 5 V / GND | 5 V / GND | Common ground; follow sensor supply specification |
| HC-SR04 TRIG | GPIO13 | GPIO26 | Optional; disabled by default |
| HC-SR04 ECHO | **GPIO15** | GPIO27 | **Level-shift 5 V echo to 3.3 V** |
| HC-SR04 VCC / GND | 5 V / GND | 5 V / GND | Optional |

**Migration:** older sketches used GPIO12 for ultrasonic ECHO and GPIO15 for
LiDAR TX. The consolidated firmware uses receive-only LiDAR and moves ECHO to
GPIO15. Rewire before enabling `SEMICOLON_ULTRASONIC`. GPIO12 is a flash-voltage
strapping pin; externally driving it high during boot can prevent startup.
GPIO15 is also a strapping pin (boot-log configuration), so verify reset behavior
with the complete circuit attached. See Espressif's
[strapping guidance](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32/schematic-checklist.html).

For a resistor divider, connect HC-SR04 ECHO through **1 kΩ** to the ESP32 input,
and connect **2 kΩ** from that input to ground (approximately 3.33 V from 5 V).
Verify the actual voltage; never connect a 5 V echo directly to an ESP32 input.
Do not connect sensors to the camera data pins. The ESP32-CAM microSD slot is
not available with this sensor wiring.

## Camera pin map

The fixed AI Thinker map is centralized in
[`camera_board.h`](../firmware/shared/camera_board.h):

| Signal | GPIO |
| --- | --- |
| PWDN / RESET | 32 / unconnected |
| XCLK / PCLK | 0 / 22 |
| SCCB SDA / SCL | 26 / 27 |
| VSYNC / HREF | 25 / 23 |
| D0–D7 | 5, 18, 19, 21, 36, 39, 34, 35 |

Navigation uses QVGA JPEG capture. Currency inference additionally requires
working PSRAM. The flash LED and microSD are not driven by this firmware.

## What provides feedback?

Beeps, speech, and vibration come from the **browser/phone**. No external motor,
buzzer, I²S speaker, or mode button is driven by the maintained targets. Those
were described in earlier project plans but were not implemented as a complete
integrated control path. Do not wire an actuator based on the old README.

## Sensor limits

The parser accepts the TF-Luna's 9-byte UART frame, checks the first eight bytes
against its checksum byte, and rejects unreliable strength and range values.
The configured range is 20–800 cm, with strength at least 100 and not 65535, per
the [Benewake TF-Luna manual](https://ai.benewake.com/en/resources/technical-documents/tf-luna-user-manual/).
Reflectivity, angle, ambient light, and target size affect measurements. A
missing/invalid reading cannot establish that a path is clear.

Optional ultrasonic sampling is spaced at least 80 ms apart, with an 18 ms echo
timeout and a 2–300 cm accepted range. The firmware prefers fresh LiDAR readings;
ultrasonic is a fallback, not sensor fusion or a minimum-of-both safety decision.
