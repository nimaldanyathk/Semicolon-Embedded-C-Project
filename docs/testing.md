# Verification and remaining hardware work

## Automated checks

```sh
cmake -S . -B build -DSEMICOLON_DESKTOP=ON -DSEMICOLON_SANITIZERS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
node --test tests/dashboard.test.js
python3 tools/check_repository.py
pio run -e navigation-esp32cam -e navigation-esp32dev
```

Omit `SEMICOLON_DESKTOP` if OpenCV is unavailable. Sanitizers require a supported
Clang/GCC toolchain. The dashboard tests require Node.js 18+ (CI uses 22).

| Area | Automated coverage |
| --- | --- |
| LiDAR parser | Checksum, little-endian decoding, partial frames, corrupt/truncated-frame resynchronization, noise, range/strength limits |
| Reading selection | LiDAR priority, ultrasonic fallback, invalid data, expiry boundary, timestamp wraparound |
| Dashboard | Threshold validation and range/type checks; unavailable data distinguished from valid readings |
| Desktop matching | Missing/empty datasets, invalid images, constant-image rejection, matching/nonmatching synthetic images, invalid dimensions, old-template clearing |
| CLI | Help and failure exit codes for missing datasets/invalid camera index |
| Repository | Tracked binary/config exclusion, credential macros, relative Markdown links |
| Firmware CI | ESP32-CAM and ESP32 Dev Module, each with optional ultrasonic disabled and enabled |

CI builds firmware but does not flash hardware. The currency firmware is excluded
because its trained model is missing. Desktop tests generate temporary synthetic
patterns; they measure software behavior, not banknote accuracy.

## Bench validation checklist

Record board revision, power supply, sensor firmware/mode, build commit, browser,
and measurements with each run. These checks require physical hardware and were
not performed as part of the software-only repository refresh.

- Verify voltage levels, common ground, bootstrapping behavior, and the revised
  ultrasonic ECHO pin before power-on.
- Compare LiDAR readings with measured distances across the intended operating
  range and varied surfaces/light levels.
- Disconnect LiDAR and confirm invalidation after 1.5 seconds; repeat while near
  an obstacle. Check invalid-strength frames and reconnect recovery.
- With ultrasonic enabled, verify fallback, echo timeout, and automatic return
  to LiDAR. Confirm that absent sensors never appear as a zero-distance success.
- Open two dashboards and a camera stream; confirm continuing JSON updates,
  capture busy responses, stream disconnection recovery, and bounded heap use.
- Disconnect Wi-Fi, check AP fallback and station reconnection, and verify that
  browser alerts do not continue displaying stale measurements.
- Exercise keyboard/screen-reader navigation, audio enable/disable, threshold
  changes, mute, browser backgrounding, phone sleep, and unsupported vibration.
- Run a sustained soak test while logging resets, free heap, latency, and power.
- Restore and evaluate the model before making currency-recognition claims.

Semicolon remains an educational assistive-device prototype. Software tests do
not establish navigation safety, accessibility effectiveness, or readiness to
replace an established mobility aid.
