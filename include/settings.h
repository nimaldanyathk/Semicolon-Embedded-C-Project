#pragma once
#include <stdint.h>
#if __has_include("config.h")
#include "config.h"
#endif
#ifndef SEMICOLON_WIFI_SSID
#define SEMICOLON_WIFI_SSID ""
#endif
#ifndef SEMICOLON_WIFI_PASSWORD
#define SEMICOLON_WIFI_PASSWORD ""
#endif
#ifndef SEMICOLON_AP_PASSWORD
#define SEMICOLON_AP_PASSWORD ""
#endif
#ifndef SEMICOLON_ULTRASONIC
#define SEMICOLON_ULTRASONIC 0
#endif
#ifndef SEMICOLON_CAMERA
#define SEMICOLON_CAMERA 0
#endif

namespace settings {
constexpr uint32_t sensorTimeoutMs = 1500;
constexpr uint32_t wifiTimeoutMs = 15000;
constexpr uint32_t ultrasonicIntervalMs = 80;
constexpr uint32_t ultrasonicTimeoutUs = 18000;
#if SEMICOLON_CAMERA
constexpr int lidarRx = 14;
constexpr int ultrasonicTrig = 13;
constexpr int ultrasonicEcho = 15;
#else
constexpr int lidarRx = 16;
constexpr int ultrasonicTrig = 26;
constexpr int ultrasonicEcho = 27;
#endif
// TF-Luna sends measurements without commands: its RX remains unconnected.
constexpr int lidarTx = -1;
}
