#pragma once

// Copy to config.h (ignored by Git). Empty SSID disables station mode.
#define SEMICOLON_WIFI_SSID ""
#define SEMICOLON_WIFI_PASSWORD ""
// Set an 8–63 character password to enable the fallback access point.
// An empty password disables the AP; there is no default public password.
#define SEMICOLON_AP_PASSWORD ""

// Optional HC-SR04 backup. Read docs/hardware.md BEFORE enabling/wiring.
#define SEMICOLON_ULTRASONIC 0
