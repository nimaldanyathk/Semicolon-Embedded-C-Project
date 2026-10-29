// AI Thinker mapping from Espressif CameraWebServer examples.
// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
// SPDX-License-Identifier: Apache-2.0 (see LICENSES/Apache-2.0.txt)
#pragma once
#include <Arduino.h>
#include "esp_camera.h"

// AI Thinker ESP32-CAM / OV2640 only. No microSD or flash-LED control.
inline camera_config_t semicolonCameraConfig() {
    camera_config_t config = {};
    config.pin_pwdn = 32;
    config.pin_reset = -1;
    config.pin_xclk = 0;
    config.pin_sccb_sda = 26;
    config.pin_sccb_scl = 27;
    config.pin_d7 = 35;
    config.pin_d6 = 34;
    config.pin_d5 = 39;
    config.pin_d4 = 36;
    config.pin_d3 = 21;
    config.pin_d2 = 19;
    config.pin_d1 = 18;
    config.pin_d0 = 5;
    config.pin_vsync = 25;
    config.pin_href = 23;
    config.pin_pclk = 22;
    config.xclk_freq_hz = 20000000;
    config.ledc_timer = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    return config;
}
