# Contributing

Start with the [README](README.md), [architecture](docs/architecture.md), and
[hardware map](docs/hardware.md). Keep changes focused and describe the behavior
being fixed, the board/environment involved, and how you verified it.

## Development

1. Create a branch from `main`.
2. Keep Wi-Fi credentials in ignored `include/config.h`, and datasets/model
   exports outside the tracked source. Do not commit generated firmware or objects.
3. Put portable sensor logic in `lib/semicolon_core/` and add regression coverage
   for behavioral fixes. Keep Arduino/networking dependencies in `firmware/`.
4. Run the applicable checks in [testing.md](docs/testing.md). Report hardware
   tests separately from host tests and compilation.
5. Update wiring, build instructions, and migration notes for changes to pins,
   endpoints, or target behavior.

Use C11 for the sensor core/C entry points and C++17 for the desktop adapter.
Do not rename Arduino/C++ source to `.c`. Follow `.editorconfig`, check all error
paths, keep loops and network waits bounded, and preserve third-party licenses.

## Reporting a problem

Include the commit, build target, complete compiler error (if relevant), board
and sensor models, wiring differences, serial log with secrets removed, and
steps to reproduce. For browser issues, include device/browser versions and
whether the tab was visible. Avoid unsupported performance or accuracy claims.
