#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>
#include "wifi_link.h"

// Waveshare ESP32-S3-Touch-LCD-1.28. USB is programming/provisioning/power.
TFT_eSPI panel;
TFT_eSprite frame(&panel);
static bool online = false, stopped = false, factory = false, overflow = false;
static int speed = 3, baseline = 3;
static float shown = 3;
static uint32_t lastPacket = 0, lastDraw = 0;
static String input, engine = "", response = "";
static bool dirty = true;
static const uint16_t bg = 0xFFFF, ink = 0x0000, muted = 0x0000, blue = 0x12BB, red = 0xC8A7;
static const char *names[] = {"", "DEEPEST", "DELIBERATE", "CONVERSATIONAL", "QUICK", "IMMEDIATE"};

static const char *text(cJSON *object, const char *key) {
  cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
  return cJSON_IsString(value) ? value->valuestring : "";
}
static bool level(cJSON *object, const char *key, int &out) {
  cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
  if (!cJSON_IsNumber(value) || value->valuedouble != value->valueint || value->valueint < 1 || value->valueint > 5) return false;
  out = value->valueint; return true;
}
static float gaugeAngle(float value) { return (150 + (value - 1) * 60) * PI / 180; }
static int px(float a, int r) { return lroundf(120 + cosf(a) * r); }
static int py(float a, int r) { return lroundf(116 + sinf(a) * r); }
static void centered(const String &s, int y, int font, uint16_t color) {
  frame.setTextColor(color, bg); frame.drawCentreString(s, 120, y, font);
}
static void drawGauge() {
  frame.fillSprite(bg);
  uint16_t active = stopped ? red : blue;
  for (int i = 0; i <= 80; i++) {
    float v = 1 + i / 20.0f, a = gaugeAngle(v);
    frame.drawLine(px(a, 102), py(a, 102), px(a, 108), py(a, 108), online && v <= shown ? active : 0xB61A);
  }
  for (int i = 1; i <= 5; i++) {
    float a = gaugeAngle(i);
    frame.drawLine(px(a, 91), py(a, 91), px(a, 100), py(a, 100), muted);
    frame.setTextColor(ink, bg); frame.setTextDatum(MC_DATUM);
    frame.drawString(String(i), px(a, 80), py(a, 80), 2);
  }
  frame.setTextDatum(TL_DATUM);

  if (online) {
    float a = gaugeAngle(shown), normal = a + PI / 2;
    frame.fillTriangle(px(a, 67), py(a, 67), px(normal, 6), py(normal, 6), px(normal + PI, 6), py(normal + PI, 6), ink);
    frame.fillCircle(120, 116, 8, ink);
    frame.fillCircle(120, 116, 3, bg);
    float b = gaugeAngle(baseline);
    frame.fillCircle(px(b, 114), py(b, 114), 4, red);
    centered(String(speed), 135, 6, blue);
    centered(stopped ? "STOPPED" : names[speed], 186, 2, active);
    centered(factory ? "FACTORY DEFAULT" : String("DEFAULT ") + baseline, 206, 1, muted);
    centered(stopped ? "TAP GAS TO RESUME" : (engine == "live" ? "LIVE / WI-FI" : "DEMO / WI-FI"), 220, 1, muted);
  } else {
    centered("WAITING", 135, 4, ink);
    centered("Open instrument", 178, 2, muted);
    const char *link = wifiStatus();
    centered(!strcmp(link, "Wi-Fi connected") ? "WI-FI CONNECTED" : "CONNECTING WI-FI", 207, 1, muted);
  }
  centered("SPEED", 61, 2, ink);
  frame.pushSprite(0, 0);
}
static void receivePacket(const String &line) {
  cJSON *packet = cJSON_Parse(line.c_str());
  if (!packet) return;
  cJSON *version = cJSON_GetObjectItemCaseSensitive(packet, "v");
  cJSON *seq = cJSON_GetObjectItemCaseSensitive(packet, "seq");
  cJSON *connected = cJSON_GetObjectItemCaseSensitive(packet, "connected");
  cJSON *state = cJSON_GetObjectItemCaseSensitive(packet, "state");
  int next = speed, saved = baseline;
  bool valid = cJSON_IsNumber(version) && version->valueint == 1 && cJSON_IsNumber(seq) && cJSON_IsBool(connected);
  bool active = cJSON_IsTrue(connected);
  if (active) valid = valid && cJSON_IsObject(state) && level(cJSON_GetObjectItemCaseSensitive(state, "controls"), "speed", next)
    && level(cJSON_GetObjectItemCaseSensitive(state, "defaults"), "speed", saved);
  if (valid) {
    online = active; lastPacket = millis();
    if (active) {
      speed = next; baseline = saved; engine = text(state, "engine"); response = text(state, "status");
      stopped = response == "stopped"; factory = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(state, "factory"));
    }
    dirty = true;
    wifiAcknowledge(seq->valuedouble, active);
    Serial.printf("{\"type\":\"display_ack\",\"v\":1,\"role\":\"speed\",\"seq\":%.0f,\"connected\":%s,\"speed\":%d,\"baseline\":%d,\"stopped\":%s}\n", seq->valuedouble, active ? "true" : "false", speed, baseline, stopped ? "true" : "false");
  }
  cJSON_Delete(packet);
}
void setup() {
  Serial.setRxBufferSize(16384); Serial.begin(115200); delay(350);
  // Mounted face was 90 degrees counterclockwise; rotate the entire layout
  // clockwise so level 3 and the labels are upright together.
  panel.init(); panel.setRotation(1); panel.fillScreen(bg);
  frame.setColorDepth(16);
  if (!frame.createSprite(240, 240)) {
    panel.setTextColor(TFT_WHITE); panel.drawString("Memory error", 30, 110, 2);
    while (true) delay(1000);
  }
  input.reserve(8192); drawGauge(); wifiBegin();
  Serial.println("{\"type\":\"display_ready\",\"v\":1,\"role\":\"speed\",\"firmware\":\"1.1.1-upright\"}");
}
void loop() {
  int count = 0;
  while (Serial.available() && count++ < 2048) {
    char c = Serial.read();
    if (c == '\n') {
      if (!overflow) {
        cJSON *config = cJSON_Parse(input.c_str());
        if (config && !strcmp(text(config, "type"), "wifi_config") && wifiConfigure(config, input))
          Serial.println("{\"type\":\"wifi_configured\",\"v\":1}");
        cJSON_Delete(config);
      }
      input = ""; overflow = false;
    } else if (c != '\r') { if (input.length() < 8191) input += c; else overflow = true; }
  }
  wifiLoop(receivePacket);
  uint32_t now = millis();
  if (online && now - lastPacket > 5000) { online = false; dirty = true; }
  if (now - lastDraw >= 33) {
    float target = online ? speed : 3;
    if (fabsf(target - shown) > .005f) { shown += (target - shown) * .22f; dirty = true; }
    else shown = target;
    if (dirty || !online) { drawGauge(); dirty = false; }
    lastDraw = now;
  }
  delay(2);
}
