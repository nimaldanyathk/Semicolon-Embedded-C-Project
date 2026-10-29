# Repository migration

The refresh consolidates overlapping experiments into explicit build targets.
Older source remains recoverable in Git history; duplicated sketches, generated
objects, compressed copies, and unused vendor example UIs are removed from the
current tree.

| Previous path | Current home or disposition |
| --- | --- |
| `CameraWebServer1/CameraWebServer1.ino` | LiDAR/dashboard behavior consolidated into `firmware/navigation/` |
| `lidar/*.c`, `lidar_mobileint.ino` | Shared parser in `lib/semicolon_core/`; firmware/UI in `firmware/navigation/` |
| `esp32_c/`, `esp32_c.c` | Camera, LiDAR, and optional ultrasonic consolidated into the navigation target |
| `Semicolon_camera_esp32cam/`, `cam_config_nr.c` | Camera initialization and capture/stream handlers consolidated |
| `EdgeImpulseModel.c` | `firmware/currency/main.cpp`; still requires the missing model export |
| `CurrencyDetection_C/` | `desktop/currency_detection/` with a C ABI and CMake build |
| `currency_detector_wrapper.o`, `CameraWebServer1.zip` | Removed generated/duplicate artifacts |
| `OPTIMIZATION_SUMMARY.md`, `PERFORMANCE_OPTIMIZATIONS.md` | Replaced by architecture and testing documentation; unmeasured speedup claims removed |

## Deliberate behavior changes

- Dashboard: port **80** instead of 82. `/sensors` now has explicit centimeter
  fields, selected source, availability, and camera readiness.
- Telemetry: bounded polling replaces synchronous infinite SSE. `/events` is
  removed; clients must use the documented JSON API.
- ESP32-CAM ultrasonic ECHO: **GPIO15**, replacing GPIO12. TF-Luna RX is left
  unconnected; the ESP32 only receives measurement data. Rewire old builds.
- Credentials: local ignored configuration replaces hard-coded credentials and
  a shared AP password. No network opens by default.
- Camera: a fixed QVGA preview and capture replace the broad Espressif example
  register/clock/face-detection control interface. Arbitrary camera register
  writes, flash control, and unused board maps are removed.
- Temperature: the undocumented approximate chip-temperature endpoint is removed.
- Alerts: require an explicit button press and are cleared on stale data,
  disconnection, disable, or page hiding. Browser support is stated in the UI.
- Desktop: explicit dataset and camera arguments, error exit codes, bounded camera
  failure handling, and optional macOS speech replace hard-coded paths and a
  shell command.

The maintained firmware does not claim to combine navigation and currency
recognition in one binary. Physical mode switching and external haptic/audio
actuators need a future hardware/firmware integration design.
