#include <stdio.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "esp32-hal-ledc.h"
#include "sdkconfig.h"
#include "camera_index.h"   // Your HTML UI page

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* TAG = "esp32_camera";

// HTTP Response headers
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// LED control
static int led_duty = 0;

// Capture handler
static esp_err_t capture_handler(httpd_req_t *req) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return res;
}

// Stream handler
static esp_err_t stream_handler(httpd_req_t *req) {
    esp_err_t res = ESP_OK;
    char part_buf[64];
    httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);

    while (true) {
        camera_fb_t * fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }

        size_t hlen = snprintf(part_buf, 64, _STREAM_PART, fb->len);
        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        if (res == ESP_OK) res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);

        esp_camera_fb_return(fb);
        if (res != ESP_OK) break;
    }

    return res;
}

// Index page handler
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)index_html, strlen((char *)index_html));
}

// LED control handler
static esp_err_t led_handler(httpd_req_t *req) {
    char buf[32];
    int ret = httpd_req_get_url_query_str(req, buf, sizeof(buf));
    if (ret == ESP_OK) {
        char param[8];
        if (httpd_query_key_value(buf, "duty", param, sizeof(param)) == ESP_OK) {
            led_duty = atoi(param);
            ledcWrite(7, led_duty); // Channel 7 for flash LED
        }
    }
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, "OK");
}

// Start web server
httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t index_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = index_handler,
            .user_ctx  = NULL
        };
        httpd_uri_t capture_uri = {
            .uri       = "/capture",
            .method    = HTTP_GET,
            .handler   = capture_handler,
            .user_ctx  = NULL
        };
        httpd_uri_t stream_uri = {
            .uri       = "/stream",
            .method    = HTTP_GET,
            .handler   = stream_handler,
            .user_ctx  = NULL
        };
        httpd_uri_t led_uri = {
            .uri       = "/led",
            .method    = HTTP_GET,
            .handler   = led_handler,
            .user_ctx  = NULL
        };

        httpd_register_uri_handler(server, &index_uri);
        httpd_register_uri_handler(server, &capture_uri);
        httpd_register_uri_handler(server, &stream_uri);
        httpd_register_uri_handler(server, &led_uri);
    }

    return server;
}

// Main application
void app_main(void) {
    // Camera config (set pins according to your board)
    camera_config_t config = {
        .pin_pwdn  = -1,
        .pin_reset = -1,
        .pin_xclk = 0,
        .pin_sscb_sda = 26,
        .pin_sscb_scl = 27,
        .pin_d7 = 35,
        .pin_d6 = 34,
        .pin_d5 = 39,
        .pin_d4 = 36,
        .pin_d3 = 21,
        .pin_d2 = 19,
        .pin_d1 = 18,
        .pin_d0 = 5,
        .pin_vsync = 25,
        .pin_href = 23,
        .pin_pclk = 22,
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_QVGA,
        .jpeg_quality = 12,
        .fb_count = 2
    };

    // Init camera
    if (esp_camera_init(&config) != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed");
        return;
    }

    // Init LED (flash)
    ledcSetup(7, 5000, 8);
    ledcAttachPin(4, 7); // GPIO 4 is flash LED on most ESP32-CAM
    ledcWrite(7, 0);

    // Start HTTP server
    start_webserver();
}
