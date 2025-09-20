#include "esp_camera.h"
#include <WiFi.h>

// ===========================
// Select camera model in board_config.h
// ===========================
#include "board_config.h"

// ===========================
// WiFi credentials
// ===========================
const char *ssid = "S23";
const char *password = "mobilehotspot";

void startCameraServer();
void setupLedFlash();

// ===========================
// Ultrasonic Setup (HC-SR04)
// ===========================
#define ULTRA_TRIG 13
#define ULTRA_ECHO 12
const uint32_t PULSE_TIMEOUT_US = 6000UL; // ~1m max
const int SAMPLES = 3;
const int SAMPLE_DELAY_MS = 10;

float readUltrasonicOnceCM() {
  digitalWrite(ULTRA_TRIG, LOW); delayMicroseconds(3);
  digitalWrite(ULTRA_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(ULTRA_TRIG, LOW);
  unsigned long us = pulseIn(ULTRA_ECHO, HIGH, PULSE_TIMEOUT_US);
  if (us == 0) return -1.0f;
  return (us * 0.0343f) / 2.0f;
}

float readUltrasonicAvgCM() {
  float sum = 0; int good = 0;
  for (int i=0; i<SAMPLES; i++) {
    float d = readUltrasonicOnceCM();
    if (d > 0) { sum += d; good++; }
    delay(SAMPLE_DELAY_MS);
  }
  return good ? (sum/good) : -1.0f;
}

// ===========================
// TF-Luna LiDAR Setup (UART)
// ===========================
#define LIDAR_RX 14  // ESP32-CAM RX pin
#define LIDAR_TX 15  // ESP32-CAM TX pin
HardwareSerial LidarSerial(1);

int readLidarDistanceCM() {
  uint8_t buf[9];
  if (LidarSerial.available() >= 9) {
    if (LidarSerial.read() == 0x59 && LidarSerial.read() == 0x59) {
      buf[0] = 0x59; buf[1] = 0x59;
      for (int i=2; i<9; i++) buf[i] = LidarSerial.read();
      uint16_t checksum = 0;
      for (int i=0; i<8; i++) checksum += buf[i];
      if ((checksum & 0xFF) == buf[8]) {
        int distance = buf[2] + (buf[3] << 8);
        return distance;
      }
    }
  }
  return -1;
}

// ===========================
// ESP32 Internal Temperature
// ===========================
// ESP-IDF provides a raw internal sensor
extern "C" uint8_t temprature_sens_read();
float getChipTemperatureC() {
  return (temprature_sens_read() - 32) / 1.8; // convert F → C
}

// ===========================
// Setup
// ===========================
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // Ultrasonic pins
  pinMode(ULTRA_TRIG, OUTPUT);
  pinMode(ULTRA_ECHO, INPUT);
  digitalWrite(ULTRA_TRIG, LOW);
  Serial.println("Ultrasonic ready (TRIG=13, ECHO=12)");

  // Lidar Serial Init
  LidarSerial.begin(115200, SERIAL_8N1, LIDAR_RX, LIDAR_TX);
  Serial.println("TF-Luna LiDAR ready (TX=15, RX=14)");

  // Camera setup
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(LED_GPIO_NUM)
  setupLedFlash();
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

// ===========================
// Loop
// ===========================
void loop() {
  int lidar = readLidarDistanceCM();
  float ultra = readUltrasonicAvgCM();
  float tempC = getChipTemperatureC();

  if (lidar > 0) Serial.printf("LiDAR Distance: %d cm | ", lidar);
  else Serial.print("LiDAR: no data | ");

  if (ultra > 0) Serial.printf("Ultrasonic Distance: %.1f cm | ", ultra);
  else Serial.print("Ultrasonic: no echo | ");

  Serial.printf("ESP32 Temp: %.1f °C\n", tempC);

  delay(200);
}