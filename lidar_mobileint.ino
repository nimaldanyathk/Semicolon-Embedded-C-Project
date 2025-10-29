#include <WiFi.h>
#include "esp_http_server.h"

/*
 * ESP32 (no camera) — TF-Luna LiDAR + Mobile Web UI with Beep + Voice + Vibrate
 * - LiDAR on UART1 (GPIO14 RX, GPIO15 TX)
 * - Web dashboard on http://<IP>:82/
 * - Beeps and speaks under threshold, and also vibrates (if supported)
 */

// =========================== WiFi ===========================
const char *ssid     = "MINI";
const char *password = "mini1234567";

// =========================== LiDAR (TF-Luna) ===========================
// Wiring (ESP32-CAM headers):
//   LiDAR TX -> GPIO14 (ESP32 RX2)
//   LiDAR RX -> GPIO15 (ESP32 TX2)
//   VCC -> 5V, GND -> GND
#define LIDAR_RX 14
#define LIDAR_TX 15
HardwareSerial LidarSerial(1);

int readLidarDistanceCM() {
  // TF-Luna frame: 0x59 0x59 Dist_L Dist_H Strength_L Strength_H Temp_L Temp_H Checksum
  uint8_t buf[9];
  while (LidarSerial.available() >= 9) {
    if (LidarSerial.read() == 0x59 && LidarSerial.read() == 0x59) {
      buf[0] = 0x59; buf[1] = 0x59;
      for (int i=2; i<9; i++) buf[i] = LidarSerial.read();
      // Optimized checksum calculation - start with header bytes
      uint16_t checksum = 0x59 + 0x59;
      for (int i=2; i<8; i++) checksum += buf[i];
      if ((checksum & 0xFF) == buf[8]) {
        int distance = buf[2] | (buf[3] << 8);
        return distance; // cm
      }
    }
  }
  return -1; // no valid frame yet
}

// =========================== ESP32 temp (rough) ===========================
extern "C" uint8_t temprature_sens_read();
// Cache temperature readings to reduce sensor calls (100ms cache)
// Note: Not thread-safe. If called from multiple tasks, add synchronization.
static float cachedTemp = 0.0f;
static uint32_t lastTempReadMs = 0;
float getChipTemperatureC() {
  uint32_t now = millis();
  if (now - lastTempReadMs >= 100) {
    cachedTemp = (temprature_sens_read() - 32) / 1.8f; // F→C approx.
    lastTempReadMs = now;
  }
  return cachedTemp;
}

// =========================== Web UI (port 82) ===========================
httpd_handle_t web_httpd = nullptr;

static esp_err_t sensors_get_handler(httpd_req_t *req) {
  int   lidar = readLidarDistanceCM();
  float tempC = getChipTemperatureC();

  // Use snprintf instead of String concatenation for better performance
  char json[64];
  int len = snprintf(json, sizeof(json), "{\"lidar\":%d,\"temp\":%.1f}", lidar, tempC);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json, len);
}

static const char PROGMEM INDEX_HTML[] = R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 LiDAR</title>
<style>
 body { font-family: system-ui, Arial; background:#0b0f19; color:#fff; margin:0; padding:20px; }
 h2 { margin:0 0 12px 0; }
 .card { background:#151b31; padding:15px; border-radius:12px; margin-bottom:12px; border:1px solid #1d2547; }
 .row { display:flex; gap:14px; flex-wrap:wrap; }
 .k { opacity:.75 } .v { font-weight:700 }
 .bad { color:#ff6b6b }
 button { padding:10px 14px; border:0; border-radius:10px; background:#2d4cf0; color:#fff; }
 label { display:inline-flex; align-items:center; gap:8px; }
 input[type="number"] { width:80px; }
 .pill { display:inline-block; padding:4px 8px; border-radius:999px; background:#0f1a3b; border:1px solid #223a7a; }
</style></head><body>
<h2>ESP32 LiDAR</h2>

<div class="card">
  <div class="row">
    <div><span class="k">LiDAR:</span> <span id="lidar" class="v">--</span> cm</div>
    <div><span class="k">Temp:</span> <span id="temp" class="v">--</span> °C</div>
    <div><span id="status" class="pill">idle</span></div>
  </div>
</div>

<div class="card">
  <div class="row">
    <label><input type="checkbox" id="beep" checked> Beep alarm</label>
    <label><input type="checkbox" id="speak" checked> Voice alert</label>
    <label><input type="checkbox" id="vibrate" checked> Vibrate</label>
    <label>Threshold (cm): <input id="thresh" type="number" value="80" min="10" max="300"></label>
    <button id="enableAudio">Enable Sound</button>
    <button id="testSpeak">Test Voice</button>
    <button id="testBuzz">Test Beep</button>
    <button id="testVib">Test Vibrate</button>
  </div>
</div>

<div class="card" style="opacity:.85;font-size:13px">
  <div id="log"></div>
</div>

<script>
const log = (...a)=>{ const el=document.getElementById('log'); el.textContent=[new Date().toLocaleTimeString(),...a].join(' ')+"\\n"+el.textContent; };

let audioCtx;
let osc=null;
function ensureAudio(){
  if(!audioCtx){
    audioCtx = new (window.AudioContext||window.webkitAudioContext)();
  } else if(audioCtx.state === 'suspended'){
    audioCtx.resume();
  }
}
function buzz(on){
  const enable = document.getElementById('beep').checked;
  if(!enable){ on=false; }
  if(on && !osc){
    ensureAudio();
    osc=audioCtx.createOscillator();
    const g=audioCtx.createGain();
    osc.frequency.value=850; g.gain.value=0.12;
    osc.connect(g).connect(audioCtx.destination);
    osc.start();
  } else if(!on && osc){
    osc.stop(); osc.disconnect(); osc=null;
  }
}
function speak(txt){
  if(!document.getElementById('speak').checked) return;
  try{
    speechSynthesis.cancel();
    const u=new SpeechSynthesisUtterance(txt);
    u.rate=1; u.pitch=1; u.lang='en-US';
    speechSynthesis.speak(u);
  }catch(e){log('TTS error', e);}
}
function vibrate(pattern=[200,100,200]){
  if(!document.getElementById('vibrate').checked) return;
  if('vibrate' in navigator){
    navigator.vibrate(pattern);
  }
}

document.getElementById('enableAudio').onclick=()=>{ ensureAudio(); speak('Audio enabled'); };
document.getElementById('testSpeak').onclick=()=>speak('Hello! Voice alert is working.');
document.getElementById('testBuzz').onclick=()=>{ ensureAudio(); buzz(true); setTimeout(()=>buzz(false), 400); };
document.getElementById('testVib').onclick=()=>vibrate();

let lastNear=false;
async function poll(){
  try{
    const r=await fetch('/sensors'); if(!r.ok) throw new Error('HTTP '+r.status);
    const j=await r.json();
    const ld = j.lidar;
    const tc = j.temp;

    document.getElementById('lidar').textContent = ld;
    document.getElementById('temp').textContent  = tc.toFixed(1);

    const th = parseFloat(document.getElementById('thresh').value)||80;
    const near = (ld>0 && ld<th);

    const st = document.getElementById('status');
    st.textContent = near ? 'ALERT' : 'idle';
    st.className = 'pill' + (near ? ' bad' : '');

    // Alerts
    buzz(near);
    if(near && !lastNear){
      speak('Warning. Obstacle detected.');
      vibrate([250,120,250,120,250]);
    }
    lastNear = near;

  }catch(e){ log(e); }
}
setInterval(poll, 800);
</script>
</body></html>
)HTML";

static esp_err_t index_get_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
  return ESP_OK;
}

void startSensorServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 82;
  if (httpd_start(&web_httpd, &config) == ESP_OK) {
    httpd_uri_t sensors = { .uri="/sensors", .method=HTTP_GET, .handler=sensors_get_handler, .user_ctx=NULL };
    httpd_uri_t index   = { .uri="/",       .method=HTTP_GET, .handler=index_get_handler, .user_ctx=NULL };
    httpd_register_uri_handler(web_httpd, &sensors);
    httpd_register_uri_handler(web_httpd, &index);
    Serial.println("[HTTP] Dashboard at :82");
  } else {
    Serial.println("[HTTP] start failed");
  }
}

// =========================== Setup ===========================
void setup() {
  Serial.begin(115200);

  // LiDAR UART
  LidarSerial.begin(115200, SERIAL_8N1, LIDAR_RX, LIDAR_TX);

  // WiFi STA with AP fallback
  WiFi.begin(ssid, password);
  Serial.print("WiFi connecting");
  uint32_t t0=millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t0<15000) { delay(500); Serial.print("."); }
  Serial.println(WiFi.status()==WL_CONNECTED? "\nWiFi connected" : "\nWiFi fail -> starting AP");

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-LiDAR","12345678");
    Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  }

  startSensorServer();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Open http://"); Serial.print(WiFi.localIP()); Serial.println(":82/");
  } else {
    Serial.print("Open http://"); Serial.print(WiFi.softAPIP()); Serial.println(":82/");
  }
}

// =========================== Loop ===========================
void loop() { delay(100); }
