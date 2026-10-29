// Based on Espressif CameraWebServer examples.
// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
// SPDX-License-Identifier: Apache-2.0 (see LICENSES/Apache-2.0.txt)
#include "camera_server.h"
#include "settings.h"
#include <Arduino.h>
#if SEMICOLON_CAMERA
#include "camera_board.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t cameraMutex;
static httpd_handle_t streamServer;

static esp_err_t capture(httpd_req_t *req) {
    if (xSemaphoreTake(cameraMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return httpd_resp_send_err(req, HTTPD_503_SERVICE_UNAVAILABLE, "Camera busy");
    }
    camera_fb_t *frame = esp_camera_fb_get();
    esp_err_t result;
    if (!frame) {
        result = httpd_resp_send_500(req);
    } else {
        httpd_resp_set_type(req, "image/jpeg");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        result = httpd_resp_send(req, reinterpret_cast<const char *>(frame->buf), frame->len);
        esp_camera_fb_return(frame);
    }
    xSemaphoreGive(cameraMutex);
    return result;
}

static esp_err_t stream(httpd_req_t *req) {
    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=semicolon-frame");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    for (;;) {
        if (xSemaphoreTake(cameraMutex, pdMS_TO_TICKS(200)) != pdTRUE) return ESP_FAIL;
        camera_fb_t *frame = esp_camera_fb_get();
        if (!frame) {
            xSemaphoreGive(cameraMutex);
            return ESP_FAIL;
        }
        char header[128];
        int length = snprintf(header, sizeof(header),
            "\r\n--semicolon-frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
            static_cast<unsigned>(frame->len));
        esp_err_t result = ESP_FAIL;
        if (length > 0 && static_cast<size_t>(length) < sizeof(header)) {
            result = httpd_resp_send_chunk(req, header, length);
            if (result == ESP_OK) {
                result = httpd_resp_send_chunk(req, reinterpret_cast<const char *>(frame->buf), frame->len);
            }
        }
        esp_camera_fb_return(frame);
        xSemaphoreGive(cameraMutex);
        if (result != ESP_OK) return result;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

bool startCamera(httpd_handle_t dashboard) {
    auto config = semicolonCameraConfig();
    if (esp_camera_init(&config) != ESP_OK) return false;
    cameraMutex = xSemaphoreCreateMutex();
    if (!cameraMutex) {
        esp_camera_deinit();
        return false;
    }
    httpd_config_t http = HTTPD_DEFAULT_CONFIG();
    http.server_port = 81;
    http.ctrl_port += 1; // A separate task keeps streaming off the sensor server.
    http.max_open_sockets = 1;
    http.send_wait_timeout = 2;
    httpd_uri_t streamRoute = {};
    streamRoute.uri = "/stream";
    streamRoute.method = HTTP_GET;
    streamRoute.handler = stream;
    httpd_uri_t captureRoute = {};
    captureRoute.uri = "/capture";
    captureRoute.method = HTTP_GET;
    captureRoute.handler = capture;
    if (httpd_start(&streamServer, &http) != ESP_OK ||
        httpd_register_uri_handler(streamServer, &streamRoute) != ESP_OK ||
        httpd_register_uri_handler(dashboard, &captureRoute) != ESP_OK) {
        if (streamServer) httpd_stop(streamServer);
        streamServer = nullptr;
        vSemaphoreDelete(cameraMutex);
        esp_camera_deinit();
        return false;
    }
    return true;
}
#else
bool startCamera(httpd_handle_t) { return false; }
#endif
