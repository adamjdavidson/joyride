#include <Arduino.h>
#include "lvgl.h"
#include "cJSON.h"
#include "bsp_lvgl_port.h"
#include "src/port_bsp/i2c_bsp.h"
#include "src/port_bsp/axp2101_bsp.h"
#include "wifi_link.h"

// Wi-Fi display. USB is only for programming and private network provisioning.
I2cMasterBus displayBus(GPIO_NUM_7, GPIO_NUM_8, I2C_NUM_0);
static lv_obj_t *heading, *title, *description, *returnLine, *statusLine, *connectionLine, *accent;
static uint32_t lastPacket = 0;
static bool connected = false;
static String input;
static bool overflow = false;

static const char *text(cJSON *object, const char *key, const char *fallback = "") {
  cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
  return cJSON_IsString(value) ? value->valuestring : fallback;
}
static lv_obj_t *label(int y, int width, const lv_font_t *font, uint32_t color) {
  lv_obj_t *object = lv_label_create(lv_screen_active());
  lv_obj_set_width(object, width);
  lv_obj_set_style_text_font(object, font, 0);
  lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(object, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(object, LV_LABEL_LONG_WRAP);
  lv_obj_align(object, LV_ALIGN_TOP_MID, 0, y);
  return object;
}
static void showDisconnected() {
  connected = false;
  lv_label_set_text(heading, "THINKING INSTRUMENT");
  lv_label_set_text(title, "WAITING");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_48, 0);
  lv_label_set_text(description, "Open the instrument\non your computer.");
  lv_label_set_text(returnLine, "");
  lv_label_set_text(statusLine, "No current desk connected");
  lv_obj_set_style_bg_color(statusLine, lv_color_hex(0x090C12), 0);
  lv_label_set_text(connectionLine, wifiStatus());
  lv_obj_set_style_bg_color(accent, lv_color_hex(0x090C12), 0);
}
static void createUI() {
  lv_obj_t *screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0xFFFFFF), 0);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  heading = label(62, 350, &lv_font_montserrat_18, 0xFFFFFF);
  lv_obj_set_height(heading, 38); lv_obj_set_style_pad_top(heading, 8, 0);
  lv_obj_set_style_bg_opa(heading, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(heading, lv_color_hex(0x090C12), 0);
  lv_obj_set_style_radius(heading, 5, 0);
  accent = lv_obj_create(screen);
  lv_obj_set_size(accent, 112, 6);
  lv_obj_align(accent, LV_ALIGN_TOP_MID, 0, 112);
  lv_obj_set_style_border_width(accent, 0, 0);
  lv_obj_set_style_radius(accent, 2, 0);
  title = label(144, 424, &lv_font_montserrat_48, 0x1457D9);
  description = label(222, 370, &lv_font_montserrat_24, 0x090C12);
  returnLine = label(308, 356, &lv_font_montserrat_22, 0x1457D9);
  statusLine = label(365, 340, &lv_font_montserrat_18, 0xFFFFFF);
  lv_obj_set_height(statusLine, 40); lv_obj_set_style_pad_top(statusLine, 8, 0);
  lv_obj_set_style_bg_opa(statusLine, LV_OPA_COVER, 0); lv_obj_set_style_radius(statusLine, 5, 0);
  connectionLine = label(416, 300, &lv_font_montserrat_18, 0x090C12);
  showDisconnected();
}
static void applyState(cJSON *state) {
  const char *mode = text(state, "mode", "Download");
  const char *person = text(state, "person");
  const char *name = mode, *detail = "Stay with the current activity.";
  if (!strcmp(mode, "Download")) detail = "Articulate your thoughts.\nKeep your own direction.";
  else if (!strcmp(mode, "Expand")) detail = "Broaden the idea.\nExplore what it could mean.";
  else if (!strcmp(mode, "Challenge")) detail = "Find weak points, tensions\nand untested assumptions.";
  else if (!strcmp(mode, "Structure")) detail = "Find the order\nand the missing elements.";
  else if (!strcmp(mode, "Implement")) detail = "Turn the direction\ninto concrete work.";
  if (*person) {
    if (!strcmp(person, "freud")) { name = "SIGMUND FREUD"; detail = "Explore motives, desires\nand inner tensions."; }
    else if (!strcmp(person, "machiavelli")) { name = "MACHIAVELLI"; detail = "Look at power, incentives\nand strategic consequences."; }
    else if (!strcmp(person, "leonardo")) { name = "DA VINCI"; detail = "Observe the mechanism.\nInvent another possibility."; }
  }
  String upper = name; upper.toUpperCase();
  lv_label_set_text(heading, *person ? "PERSPECTIVE VISIT" : "CURRENT ACTIVITY");
  lv_obj_set_style_text_font(title, *person ? &lv_font_montserrat_32 : &lv_font_montserrat_48, 0);
  lv_label_set_text(title, upper.c_str());
  lv_label_set_text(description, detail);
  String returning = *person ? String("Return to ") + mode : "";
  lv_label_set_text(returnLine, returning.c_str());
  const char *status = text(state, "status", "ready");
  const char *statusText = "Ready when you are";
  if (!strcmp(status, "stopped")) statusText = "STOPPED - tap gas to resume";
  else if (!strcmp(status, "thinking")) statusText = "Thinking";
  else if (!strcmp(status, "responding")) statusText = "Responding";
  else if (!strcmp(status, "error")) statusText = "Check the conversation";
  lv_label_set_text(statusLine, statusText);
  lv_obj_set_style_bg_color(statusLine, lv_color_hex(!strcmp(status, "stopped") ? 0xCC1438 : 0x1457D9), 0);
  lv_label_set_text(connectionLine, !strcmp(text(state, "engine"), "live") ? "LIVE AI  /  WI-FI CONNECTED" : "SCRIPTED DEMO  /  WI-FI CONNECTED");
  lv_obj_set_style_bg_color(accent, lv_color_hex(!strcmp(status, "stopped") ? 0xCC1438 : 0x1457D9), 0);
  connected = true;
}
static void receivePacket(const String &line) {
  cJSON *packet = cJSON_Parse(line.c_str());
  if (!packet) return;
  cJSON *version = cJSON_GetObjectItemCaseSensitive(packet, "v");
  cJSON *seq = cJSON_GetObjectItemCaseSensitive(packet, "seq");
  cJSON *state = cJSON_GetObjectItemCaseSensitive(packet, "state");
  bool valid = cJSON_IsNumber(version) && version->valueint == 1 && cJSON_IsNumber(seq);
  bool online = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(packet, "connected"));
  if (valid && (!online || cJSON_IsObject(state))) {
    lastPacket = millis();
    if (bsp_lvgl_lock(0)) {
      if (online) applyState(state); else showDisconnected();
      bsp_lvgl_unlock();
      // Acknowledge after updating UI objects; LVGL paints on its own task.
      Serial.printf("{\"type\":\"display_ack\",\"v\":1,\"seq\":%.0f,\"connected\":%s,\"heap\":%u}\n", seq->valuedouble, online ? "true" : "false", ESP.getFreeHeap());
      wifiAcknowledge(seq->valuedouble, online);
    }
  }
  cJSON_Delete(packet);
}
void setup() {
  // Full metadata frames exceed the native USB CDC default of 256 bytes.
  Serial.setRxBufferSize(16384);
  Serial.begin(115200);
  delay(350);
  Custom_PmicPortInit(&displayBus, 0x34);
  bsp_lvgl_init(displayBus);
  if (bsp_lvgl_lock(0)) { Lcd_SetBacklight(90); createUI(); bsp_lvgl_unlock(); }
  input.reserve(8192);
  wifiBegin();
  Serial.println("{\"type\":\"display_ready\",\"v\":1,\"role\":\"mode\",\"firmware\":\"2.1.0-camera\"}");
}
void loop() {
  int processed = 0;
  while (Serial.available() && processed++ < 2048) {
    char c = Serial.read();
    if (c == '\n') {
      if (!overflow) {
        cJSON *config = cJSON_Parse(input.c_str());
        if (config && !strcmp(text(config, "type"), "wifi_config") && wifiConfigure(config, input))
          Serial.println("{\"type\":\"wifi_configured\",\"v\":1}");
        cJSON_Delete(config);
      }
      input = ""; overflow = false;
    }
    else if (c != '\r') { if (input.length() < 8191) input += c; else overflow = true; }
  }
  if (connected && millis() - lastPacket > 5000 && bsp_lvgl_lock(100)) { showDisconnected(); bsp_lvgl_unlock(); }
  wifiLoop(receivePacket);
  if (!connected && bsp_lvgl_lock(100)) { lv_label_set_text(connectionLine, wifiStatus()); bsp_lvgl_unlock(); }
  delay(5);
}
