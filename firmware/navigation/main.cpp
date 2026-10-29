#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include "esp_http_server.h"
#include "settings.h"
#include "semicolon_core.h"
#include "camera_server.h"

extern const char indexHtml[] asm("_binary_firmware_navigation_web_index_html_start");
extern const char appJs[] asm("_binary_firmware_navigation_web_app_js_start");
extern const char styleCss[] asm("_binary_firmware_navigation_web_style_css_start");

static HardwareSerial lidarSerial(1);
static sc_lidar_parser parser = {};
static sc_reading lidar = {}, ultrasonic = {};
static portMUX_TYPE readingsMux = portMUX_INITIALIZER_UNLOCKED;
static std::atomic<bool> cameraReady{false};
static bool apStarted = false;
static uint32_t lastNetworkOkMs = 0;
static httpd_handle_t dashboard;

static void setReading(sc_reading &target, int distance, bool valid) {
    const uint32_t now = millis();
    portENTER_CRITICAL(&readingsMux);
    target = {distance, now, valid};
    portEXIT_CRITICAL(&readingsMux);
}

static sc_distance snapshot(sc_reading &lidarCopy, sc_reading &ultrasonicCopy) {
    portENTER_CRITICAL(&readingsMux);
    lidarCopy = lidar;
    ultrasonicCopy = ultrasonic;
    portEXIT_CRITICAL(&readingsMux);
    return sc_select_distance(&lidarCopy, &ultrasonicCopy, millis(), settings::sensorTimeoutMs);
}

static void readSensors() {
    // Drain bursts, with a budget so continuous input cannot starve other work.
    for (unsigned count = 0; count < 512 && lidarSerial.available(); ++count) {
        const int value = lidarSerial.read();
        sc_lidar_frame frame = {};
        if (value >= 0 && sc_lidar_feed(&parser, static_cast<uint8_t>(value), &frame)) {
            setReading(lidar, frame.distance_cm, frame.valid);
        }
    }
#if SEMICOLON_ULTRASONIC
    static uint32_t lastPing = 0;
    if (millis() - lastPing >= settings::ultrasonicIntervalMs) {
        lastPing = millis();
        digitalWrite(settings::ultrasonicTrig, LOW);
        delayMicroseconds(3);
        digitalWrite(settings::ultrasonicTrig, HIGH);
        delayMicroseconds(10);
        digitalWrite(settings::ultrasonicTrig, LOW);
        const unsigned long duration = pulseIn(settings::ultrasonicEcho, HIGH, settings::ultrasonicTimeoutUs);
        const int distance = static_cast<int>(duration * 343UL / 20000UL);
        setReading(ultrasonic, distance, duration > 0 && distance >= 2 && distance <= 300);
    }
#endif
}

static esp_err_t sensors(httpd_req_t *req) {
    sc_reading lidarCopy, ultrasonicCopy;
    const auto selected = snapshot(lidarCopy, ultrasonicCopy);
    const uint32_t now = millis();
    const int ld = sc_reading_fresh(&lidarCopy, now, settings::sensorTimeoutMs) ? lidarCopy.distance_cm : -1;
    const int ud = sc_reading_fresh(&ultrasonicCopy, now, settings::sensorTimeoutMs) ? ultrasonicCopy.distance_cm : -1;
    char json[224];
    const int length = snprintf(json, sizeof(json),
        "{\"distance_cm\":%d,\"source\":\"%s\",\"lidar_cm\":%d,\"ultrasonic_cm\":%d,"
        "\"camera\":%s,\"uptime_ms\":%lu}", selected.distance_cm, sc_source_name(selected.source),
        ld, ud, cameraReady ? "true" : "false", static_cast<unsigned long>(now));
    if (length < 0 || static_cast<size_t>(length) >= sizeof(json)) return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, json, length);
}

static esp_err_t asset(httpd_req_t *req) {
    const char *body = indexHtml;
    const char *type = "text/html; charset=utf-8";
    if (strcmp(req->uri, "/app.js") == 0) { body = appJs; type = "text/javascript; charset=utf-8"; }
    if (strcmp(req->uri, "/style.css") == 0) { body = styleCss; type = "text/css; charset=utf-8"; }
    httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    return httpd_resp_sendstr(req, body);
}

static bool startDashboard() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;
    config.send_wait_timeout = 2;
    config.recv_wait_timeout = 2;
    if (httpd_start(&dashboard, &config) != ESP_OK) return false;
    const char *paths[] = {"/", "/app.js", "/style.css", "/sensors"};
    for (const char *path : paths) {
        httpd_uri_t route = {};
        route.uri = path;
        route.method = HTTP_GET;
        route.handler = strcmp(path, "/sensors") == 0 ? sensors : asset;
        if (httpd_register_uri_handler(dashboard, &route) != ESP_OK) {
            httpd_stop(dashboard);
            dashboard = nullptr;
            return false;
        }
    }
    const bool ready = startCamera(dashboard);
    // HTTP may already be handling requests while the camera initializes.
    cameraReady = ready;
    Serial.printf("Camera: %s\n", cameraReady ? "ready (:81/stream, /capture)" : "unavailable/disabled");
    return true;
}

static void startAccessPoint() {
    const size_t length = strlen(SEMICOLON_AP_PASSWORD);
    if (apStarted || length < 8 || length > 63) return;
    if (WiFi.softAP("Semicolon", SEMICOLON_AP_PASSWORD)) {
        apStarted = true;
        Serial.print("Access point: Semicolon — http://");
        Serial.println(WiFi.softAPIP());
    }
}

void setup() {
    Serial.begin(115200);
    lidarSerial.setRxBufferSize(1024);
    lidarSerial.begin(115200, SERIAL_8N1, settings::lidarRx, settings::lidarTx);
#if SEMICOLON_ULTRASONIC
    pinMode(settings::ultrasonicTrig, OUTPUT);
    digitalWrite(settings::ultrasonicTrig, LOW);
    pinMode(settings::ultrasonicEcho, INPUT);
#endif
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    if (strlen(SEMICOLON_WIFI_SSID)) {
        WiFi.begin(SEMICOLON_WIFI_SSID, SEMICOLON_WIFI_PASSWORD);
        const uint32_t started = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - started < settings::wifiTimeoutMs) {
            readSensors();
            delay(5);
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Dashboard: http://");
        Serial.println(WiFi.localIP());
    } else {
        startAccessPoint();
        if (!apStarted) Serial.println("Network unavailable. Set credentials in include/config.h; serial sensing continues.");
    }
    lastNetworkOkMs = millis();
    if (!startDashboard()) Serial.println("HTTP server failed; serial sensing continues.");
}

void loop() {
    readSensors();
    const uint32_t now = millis();
    if (WiFi.status() == WL_CONNECTED) lastNetworkOkMs = now;
    else if (now - lastNetworkOkMs >= settings::wifiTimeoutMs) startAccessPoint();
    static uint32_t lastPrint = 0;
    if (now - lastPrint >= 1000) {
        lastPrint = now;
        sc_reading lidarCopy, ultrasonicCopy;
        const auto selected = snapshot(lidarCopy, ultrasonicCopy);
        Serial.printf("Distance: %d cm | source: %s\n", selected.distance_cm, sc_source_name(selected.source));
    }
    delay(1);
}
