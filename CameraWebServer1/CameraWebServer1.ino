#include <WiFi.h>
#include "esp_http_server.h"

/*
 * ESP32 + TF-Luna LiDAR + Mobile Web Dashboard (SSE realtime + vibration)
 * -----------------------------------------------------------------------
 * - LiDAR on UART1 (GPIO14 RX, GPIO15 TX)
 * - Web dashboard on http://<IP>:82/
 * - Realtime push via Server-Sent Events (/events), fallback /sensors kept
 * - Beep, voice & vibration alerts (tap to arm; iOS tone fallback)
 */

// =========================== WiFi ===========================
const char *ssid     = "S23";
const char *password = "mobilehotspot";

// =========================== LiDAR (TF-Luna) ===========================
#define LIDAR_RX 14
#define LIDAR_TX 15
HardwareSerial LidarSerial(1);

// updated in loop(), read by HTTP thread
volatile int latestDistance = -1;
volatile uint32_t lastLidarMs = 0;

// =========================== LiDAR non-blocking frame parser ===========================
// TF-Luna frame: 0x59 0x59 | Dist L | Dist H | Strength L | Strength H | Temp L | Temp H | Checksum
void readLidarOnce() {
  static uint8_t frame[9];
  static int idx = 0;

  // Process just enough to decode ONE frame per call
  while (LidarSerial.available()) {
    uint8_t b = (uint8_t)LidarSerial.read();

    if (idx == 0) {                  // looking for first header
      if (b == 0x59) { frame[idx++] = b; }
      continue;
    }
    if (idx == 1) {                  // looking for second header
      if (b == 0x59) { frame[idx++] = b; }
      else { idx = 0; }              // restart
      continue;
    }

    // accumulate remaining bytes
    frame[idx++] = b;

    if (idx == 9) {
      // verify checksum
      uint16_t sum = 0;
      for (int i=0;i<8;i++) sum += frame[i];
      if ((sum & 0xFF) == frame[8]) {
        int d = frame[2] | (frame[3] << 8);
        latestDistance = d;
        lastLidarMs = millis();

        // Throttled Serial print (~5/sec)
        static uint32_t lastPrintMs = 0;
        uint32_t now = millis();
        if (now - lastPrintMs >= 200) {
          Serial.printf("[LiDAR] %d cm\n", d);
          lastPrintMs = now;
        }
      }
      idx = 0;                        // ready for next frame
      break;                          // only handle ONE frame per loop pass
    }
  }

  // If no valid frame for a while, show -1
  if (millis() - lastLidarMs > 1500) {
    latestDistance = -1;
  }
}

// =========================== ESP32 temp (approx) ===========================
extern "C" uint8_t temprature_sens_read();
// Cache temperature readings to reduce sensor calls (100ms cache)
// Note: Not thread-safe. If called from multiple tasks, add synchronization.
static float cachedTemp = 0.0f;
static uint32_t lastTempReadMs = 0;
float getChipTemperatureC() {
  uint32_t now = millis();
  if (now - lastTempReadMs >= 100) {
    cachedTemp = (temprature_sens_read() - 32) / 1.8f; // F->C approx.
    lastTempReadMs = now;
  }
  return cachedTemp;
}

// =========================== Web UI (port 82) ===========================
httpd_handle_t web_httpd = nullptr;

// ---------- /sensors JSON (kept for fallback) ----------
static esp_err_t sensors_get_handler(httpd_req_t *req) {
  float tempC = getChipTemperatureC();

  // Use snprintf instead of String concatenation for better performance
  char json[64];
  int len = snprintf(json, sizeof(json), "{\"lidar\":%d,\"temp\":%.1f}", (int)latestDistance, tempC);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate");
  httpd_resp_set_hdr(req, "Pragma", "no-cache");
  httpd_resp_set_hdr(req, "Connection", "keep-alive");
  return httpd_resp_send(req, json, len);
}

// ---------- /events SSE: pushes latest data ~100ms ----------
static esp_err_t events_get_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/event-stream");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");  // per SSE spec
  httpd_resp_set_hdr(req, "Connection", "keep-alive");
  httpd_resp_sendstr_chunk(req, ": connected\n\n");

  const TickType_t tickDelay = pdMS_TO_TICKS(100); // ~10 fps (stable)
  uint32_t lastPing = xTaskGetTickCount();
  
  // Pre-allocate buffer for SSE messages to avoid repeated allocations
  char line[80];

  for (;;) {
    float tempC = getChipTemperatureC();
    // Use snprintf instead of String concatenation for better performance
    int len = snprintf(line, sizeof(line), "data:{\"lidar\":%d,\"temp\":%.1f}\n\n", (int)latestDistance, tempC);

    if (httpd_resp_send_chunk(req, line, len) != ESP_OK) break;

    if ((xTaskGetTickCount() - lastPing) > pdMS_TO_TICKS(10000)) {
      if (httpd_resp_sendstr_chunk(req, ": ping\n\n") != ESP_OK) break;
      lastPing = xTaskGetTickCount();
    }

    vTaskDelay(tickDelay);
  }

  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

// =========================== HTML ===========================
static const char PROGMEM INDEX_HTML[] = R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 LiDAR Dashboard</title>
<style>
body {
  font-family: 'Segoe UI', system-ui, Arial;
  background: radial-gradient(circle at top, #0b0f19, #050812);
  color: #fff;
  margin: 0; padding: 20px;
}
h2 { margin: 0 0 15px; font-size: 22px; text-align: center; }
.card {
  background: #151b31;
  padding: 15px 18px;
  border-radius: 14px;
  margin-bottom: 14px;
  border: 1px solid #1d2547;
  box-shadow: 0 4px 14px rgba(0,0,0,0.4);
}
.row { display: flex; flex-wrap: wrap; gap: 12px; align-items: center; }
.k { opacity: .8 }
.v { font-weight: 700; font-size: 20px; }
.bad { color: #ff6b6b }
button {
  padding: 10px 14px; border: 0; border-radius: 10px;
  background: #2d4cf0; color: #fff; font-weight: 600; cursor: pointer; transition: .2s;
}
button:hover { background:#4966ff; }
label { display:inline-flex; align-items:center; gap:6px; font-size:14px; }
input[type="number"] { width:70px; border-radius:6px; border:0; padding:4px; text-align:center; }
.pill {
  display:inline-block; padding:5px 12px; border-radius:999px;
  background:#0f1a3b; border:1px solid #223a7a; font-size:13px; min-width:65px; text-align:center;
}
.pill.bad { background:#380a0a; border-color:#ff4444; color:#ff8888; }
.fade { animation: blink .8s infinite alternate; }
@keyframes blink { from{opacity:1;} to{opacity:.55;} }
footer { text-align:center; opacity:.6; font-size:13px; margin-top:10px; }
</style></head><body>
<h2>ESP32 LiDAR Dashboard</h2>

<div class="card">
  <div class="row">
    <div><span class="k">LiDAR:</span> <span id="lidar" class="v">--</span> cm</div>
    <div><span class="k">Temp:</span> <span id="temp" class="v">--</span> °C</div>
    <div><span id="status" class="pill">Idle</span></div>
  </div>
</div>

<div class="card">
  <div class="row">
    <label><input type="checkbox" id="beep" checked> Beep</label>
    <label><input type="checkbox" id="speak" checked> Voice</label>
    <label><input type="checkbox" id="vibrate" checked> Vibrate</label>
    <label>Threshold <input id="thresh" type="number" value="80" min="10" max="300"> cm</label>
  </div>
  <div class="row" style="margin-top:10px">
    <button id="enableAudio">Enable Audio + Haptics</button>
    <button id="testSpeak">Test Voice</button>
    <button id="testBuzz">Test Beep</button>
    <button id="testVib">Test Vibrate</button>
  </div>
</div>

<div class="card" style="opacity:.85;font-size:13px">
  <div id="log"></div>
</div>

<footer>ESP32 LiDAR | Live Monitor v3.1 (SSE)</footer>

<script>
const log = (...a)=>{ const el=document.getElementById('log'); el.textContent=[new Date().toLocaleTimeString(),...a].join(' ')+"\\n"+el.textContent; };

// ===== Audio + Haptics =====
let audioCtx; let osc=null;
let hapticsArmed = false;
const isIOS = /iPad|iPhone|iPod/.test(navigator.userAgent) || (navigator.platform==='MacIntel' && navigator.maxTouchPoints>1);

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
function vibOnce(pattern=[220,120,220]){
  if(!document.getElementById('vibrate').checked) return false;
  if(!hapticsArmed) return false; // require gesture
  if('vibrate' in navigator){
    return navigator.vibrate(pattern);
  }
  return false;
}
// iOS fallback “tap” tone (since iOS blocks web vibration)
function hapticTone(){
  if(!document.getElementById('vibrate').checked) return;
  ensureAudio();
  const o=audioCtx.createOscillator(), g=audioCtx.createGain();
  o.frequency.value=120; g.gain.value=0.28;
  o.connect(g).connect(audioCtx.destination);
  o.start();
  setTimeout(()=>{g.gain.exponentialRampToValueAtTime(0.0001, audioCtx.currentTime+0.04);}, 10);
  setTimeout(()=>{o.stop(); o.disconnect(); g.disconnect();}, 70);
}

document.getElementById('enableAudio').onclick=()=>{
  ensureAudio();
  hapticsArmed = true;
  if('vibrate' in navigator){ navigator.vibrate([60]); }
  speak('Audio and haptics enabled');
};
window.addEventListener('touchstart', ()=>{ if(!hapticsArmed){ hapticsArmed=true; } }, {once:true});

document.getElementById('testSpeak').onclick=()=>speak('Voice alert working.');
document.getElementById('testBuzz').onclick=()=>{ ensureAudio(); buzz(true); setTimeout(()=>buzz(false), 350); };
document.getElementById('testVib').onclick=()=>{
  const ok = vibOnce([200,100,200,100,200]);
  if(!ok && isIOS){ hapticTone(); }
};

// ===== Realtime via SSE (no fetch, no AbortError) =====
let lastNear=false;
function updateUI(ld, tc){
  document.getElementById('lidar').textContent = ld>0?ld:'--';
  document.getElementById('temp').textContent  = Number(tc).toFixed(1);

  const th = parseFloat(document.getElementById('thresh').value)||80;
  const near = (ld>0 && ld<th);

  const st = document.getElementById('status');
  st.textContent = near ? 'ALERT' : 'Idle';
  st.className = 'pill' + (near ? ' bad fade' : '');

  buzz(near);
  if(near && !lastNear){
    speak('Warning. Object too close.');
    const ok = vibOnce([250,100,250,100,300]);
    if(!ok && isIOS){ hapticTone(); }
  }
  lastNear = near;
}

(function startSSE(){
  if (!('EventSource' in window)) {
    log('SSE not supported; falling back to /sensors polling');
    (async function poll(){
      for(;;){
        try{
          const r=await fetch('/sensors', {cache:'no-store'});
          if(!r.ok) throw new Error('HTTP '+r.status);
          const j=await r.json();
          updateUI(j.lidar, j.temp);
          await new Promise(res=>setTimeout(res, 120));
        }catch(e){ log('poll error '+e); await new Promise(res=>setTimeout(res, 400)); }
      }
    })();
    return;
  }

  const es = new EventSource('/events');
  es.onmessage = e=>{
    try {
      const j = JSON.parse(e.data);
      updateUI(j.lidar, j.temp);
    } catch(_){}
  };
  es.onerror = e=>{
    log('SSE error; reconnecting...');
  };
  log('SSE started');
})();
</script>
</body></html>
)HTML";

// ---------- HTTP handlers ----------
static esp_err_t index_get_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate");
  httpd_resp_set_hdr(req, "Pragma", "no-cache");
  httpd_resp_set_hdr(req, "Connection", "keep-alive");
  httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
  return ESP_OK;
}

void startSensorServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 82;
  config.lru_purge_enable = true;
  // Give the HTTP task a bit more breathing room
  config.stack_size = 8192;   // default is smaller; this helps under load

  if (httpd_start(&web_httpd, &config) == ESP_OK) {
    httpd_uri_t sensors = { .uri="/sensors", .method=HTTP_GET, .handler=sensors_get_handler, .user_ctx=NULL };
    httpd_uri_t events  = { .uri="/events",  .method=HTTP_GET, .handler=events_get_handler,  .user_ctx=NULL };
    httpd_uri_t index   = { .uri="/",        .method=HTTP_GET, .handler=index_get_handler,   .user_ctx=NULL };
    httpd_register_uri_handler(web_httpd, &sensors);
    httpd_register_uri_handler(web_httpd, &events);
    httpd_register_uri_handler(web_httpd, &index);
    Serial.println("[HTTP] Dashboard at :82 (SSE enabled)");
  } else {
    Serial.println("[HTTP] start failed");
  }
}

// =========================== Setup / Loop ===========================
void setup() {
  Serial.begin(115200);
  WiFi.setSleep(false); // cuts WiFi latency spikes

  LidarSerial.begin(115200, SERIAL_8N1, LIDAR_RX, LIDAR_TX);

  WiFi.begin(ssid, password);
  Serial.print("WiFi connecting");
  uint32_t t0=millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t0<15000) {
    delay(250); Serial.print(".");
  }
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

void loop() {
  readLidarOnce();        // non-blocking: at most 1 frame per pass
  delay(1);               // yield to Wi-Fi/HTTP tasks for smooth serving
}