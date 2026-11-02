#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <math.h>
#include "wifi_link.h"

// Pinout, offsets and register sequence from the vendor's touch-board example.
Arduino_DataBus *bus = new Arduino_ESP32SPI(45, 21, 38, 39);
Arduino_GFX *panel = new Arduino_ST7789(bus, 47, 0, false, 172, 320, 34, 0, 34, 0);
Arduino_Canvas frame(320, 172, panel);
#include "panel_init.h"

static bool online = false, stopped = false, factory = false, overflow = false, dirty = true;
static int offset = 0, savedNormal = 0;
static float shown = 0;
static uint32_t lastPacket = 0, lastDraw = 0;
static String input, engine;
static const uint16_t bg = 0xFFFF, ink = 0x0000, muted = 0x0000, blue = 0x12BB, defaultColor = 0xC8A7, red = 0xC8A7;
static const char *names[] = {"OBVIOUS", "CONVENTIONAL", "RESTRAINED", "CURRENT NORMAL", "FRESH", "OUTLANDISH", "RIDICULOUS"};

static const char *text(cJSON *object, const char *key) {
  cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
  return cJSON_IsString(value) ? value->valuestring : "";
}
static bool integer(cJSON *object, const char *key, int low, int high, int &out) {
  cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
  if (!cJSON_IsNumber(value) || value->valuedouble != value->valueint || value->valueint < low || value->valueint > high) return false;
  out = value->valueint; return true;
}
static String signedValue(int value) { return (value > 0 ? String("+") : String("")) + value; }
static float gaugeAngle(float value) { return (180 + (value + 3) * 30) * PI / 180; }
static int px(float a, int rx) { return lroundf(160 + cosf(a) * rx); }
static int py(float a, int ry) { return lroundf(118 + sinf(a) * ry); }
static void label(const String &s, int x, int y, int size, uint16_t color) {
  frame.setTextSize(size); frame.setTextColor(color); frame.setTextWrap(false);
  frame.setCursor(x - s.length() * 6 * size / 2, y); frame.print(s);
}
static void drawGauge() {
  frame.fillScreen(bg);
  frame.fillRect(0, 0, 320, 24, ink);
  label("CREATIVE REACH", 160, 5, 2, bg);
  frame.fillCircle(306, 13, 3, online ? blue : muted);
  if (online) label(factory ? "L3 / ORIGINAL NORMAL" : String("SAVED NORMAL ") + signedValue(savedNormal), 160, 26, 1, defaultColor);
  else label(!strcmp(wifiStatus(), "Wi-Fi connected") ? "WI-FI CONNECTED" : wifiStatus(), 160, 26, 1, muted);
  for (int i = 0; i <= 180; i++) {
    float v = -3 + i / 30.0f, a = gaugeAngle(v);
    uint16_t color = online && ((shown >= 0 && v >= 0 && v <= shown) || (shown < 0 && v <= 0 && v >= shown)) ? blue : 0xB61A;
    frame.drawLine(px(a, 118), py(a, 59), px(a, 125), py(a, 66), color);
  }
  for (int i = -3; i <= 3; i++) {
    float a = gaugeAngle(i);
    frame.drawLine(px(a, 112), py(a, 56), px(a, 118), py(a, 60), muted);
    label(signedValue(i), px(a, 139), py(a, 77) - 6, 2, ink);
  }
  frame.fillTriangle(156, 48, 164, 48, 160, 55, defaultColor);
  label("CONVENTIONAL", 47, 134, 1, muted);
  label("WILD", 278, 134, 1, muted);
  if (online) {
    float a = gaugeAngle(shown);
    frame.fillTriangle(px(a, 106), py(a, 52), 155, 122, 165, 122, ink);
    frame.fillCircle(160, 118, 7, ink); frame.fillCircle(160, 118, 2, bg);
    label(signedValue(offset), 160, 127, 2, blue);
    frame.fillRect(0, 147, 320, 25, stopped ? red : blue);
    label(names[offset + 3], 160, 151, 2, bg);
    label(stopped ? "STOPPED" : (engine == "live" ? "LIVE" : "DEMO"), 291, 28, 1, stopped ? red : muted);
  } else {
    label("WAITING", 160, 88, 2, ink);
    label("Open the instrument on your computer", 160, 151, 1, muted);
  }
  frame.flush();
}
static void receivePacket(const String &line) {
  cJSON *packet = cJSON_Parse(line.c_str()); if (!packet) return;
  cJSON *version = cJSON_GetObjectItemCaseSensitive(packet, "v");
  cJSON *seq = cJSON_GetObjectItemCaseSensitive(packet, "seq");
  cJSON *connection = cJSON_GetObjectItemCaseSensitive(packet, "connected");
  cJSON *state = cJSON_GetObjectItemCaseSensitive(packet, "state");
  bool active = cJSON_IsTrue(connection);
  bool valid = cJSON_IsNumber(version) && version->valueint == 1 && cJSON_IsNumber(seq) && cJSON_IsBool(connection);
  int next = offset, saved = savedNormal;
  if (active) valid = valid && cJSON_IsObject(state)
    && integer(cJSON_GetObjectItemCaseSensitive(state, "controls"), "offset", -3, 3, next)
    && integer(cJSON_GetObjectItemCaseSensitive(state, "defaults"), "wild", -1000, 1000, saved);
  if (valid) {
    online = active; lastPacket = millis();
    if (active) {
      offset = next; savedNormal = saved; factory = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(state, "factory"));
      stopped = !strcmp(text(state, "status"), "stopped"); engine = text(state, "engine");
    }
    dirty = true; wifiAcknowledge(seq->valuedouble, active);
    Serial.printf("{\"type\":\"display_ack\",\"v\":1,\"role\":\"reach\",\"seq\":%.0f,\"connected\":%s,\"offset\":%d,\"savedNormal\":%d,\"factory\":%s}\n", seq->valuedouble, active ? "true" : "false", offset, savedNormal, factory ? "true" : "false");
  }
  cJSON_Delete(packet);
}
void setup() {
  Serial.setRxBufferSize(16384); Serial.begin(115200); delay(350);
  pinMode(40, OUTPUT); digitalWrite(40, LOW); delay(10); digitalWrite(40, HIGH);
  if (!frame.begin(40000000)) { Serial.println("display_init_failed"); while (true) delay(1000); }
  lcd_reg_init(); panel->setRotation(1);
  pinMode(46, OUTPUT); digitalWrite(46, HIGH);
  input.reserve(8192); drawGauge(); wifiBegin();
  Serial.println("{\"type\":\"display_ready\",\"v\":1,\"role\":\"reach\",\"firmware\":\"1.1.0-camera\"}");
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
    float target = online ? offset : 0;
    if (fabsf(target - shown) > .005f) { shown += (target - shown) * .22f; dirty = true; }
    else shown = target;
    if (dirty || !online) { drawGauge(); dirty = false; }
    lastDraw = now;
  }
  delay(2);
}
