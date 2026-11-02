#include <Arduino.h>
#include "lvgl.h"
#include "cJSON.h"
#include "bsp_lvgl_port.h"
#include "src/port_bsp/i2c_bsp.h"
#include "src/port_bsp/axp2101_bsp.h"
#include "wifi_link.h"
#include "mode_artwork.h"

// Wi-Fi display. USB is only for programming and private network provisioning.
I2cMasterBus displayBus(GPIO_NUM_7, GPIO_NUM_8, I2C_NUM_0);
static lv_obj_t *title, *artwork, *emptyCard;
static String visibleMode;
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
  visibleMode = "";
  lv_obj_add_flag(artwork, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(emptyCard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
  lv_label_set_text(title, "NO MODE SELECTED");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 350);
}
static void createUI() {
  lv_obj_t *screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(MODE_PAPER), 0);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  artwork = lv_image_create(screen);
  lv_obj_align(artwork, LV_ALIGN_TOP_MID, 0, 64);
  // Native vector illustration of an empty card; no extra bitmap or decoder.
  emptyCard = lv_obj_create(screen);
  lv_obj_set_size(emptyCard, 134, 176);
  lv_obj_align(emptyCard, LV_ALIGN_TOP_MID, 0, 104);
  lv_obj_set_style_bg_opa(emptyCard, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(emptyCard, lv_color_hex(MODE_INK), 0);
  lv_obj_set_style_border_width(emptyCard, 4, 0);
  lv_obj_set_style_radius(emptyCard, 18, 0);
  lv_obj_remove_flag(emptyCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *dash = lv_obj_create(emptyCard);
  lv_obj_set_size(dash, 48, 4);
  lv_obj_center(dash);
  lv_obj_set_style_bg_color(dash, lv_color_hex(MODE_INK), 0);
  lv_obj_set_style_border_width(dash, 0, 0);
  title = label(350, 400, &lv_font_montserrat_48, MODE_INK);
  showDisconnected();
}
static void applyState(cJSON *state) {
  const char *mode = text(state, "mode");
  const lv_image_dsc_t *image = modeArtwork(mode);
  // The dedicated mode screen keeps showing the mode during perspective visits.
  // Heartbeats and unrelated control changes do not redraw the image.
  if (connected && visibleMode == mode) return;
  if (!image) { showDisconnected(); connected = true; return; }
  lv_obj_add_flag(emptyCard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_48, 0);
  lv_image_set_src(artwork, image);
  lv_obj_remove_flag(artwork, LV_OBJ_FLAG_HIDDEN);
  lv_obj_align(artwork, LV_ALIGN_TOP_MID, 0, 64);
  String upper = mode; upper.toUpperCase();
  lv_label_set_text(title, upper.c_str());
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 350);
  visibleMode = mode;
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
  cJSON *mode = cJSON_GetObjectItemCaseSensitive(state, "mode");
  if (valid && (!online || (cJSON_IsObject(state) && (cJSON_IsNull(mode) || modeArtwork(text(state, "mode")))))) {
    lastPacket = millis();
    if (bsp_lvgl_lock(0)) {
      if (online) applyState(state); else showDisconnected();
      bsp_lvgl_unlock();
      // Acknowledge after updating UI objects; LVGL paints on its own task.
      Serial.printf("{\"type\":\"display_ack\",\"v\":1,\"seq\":%.0f,\"connected\":%s,\"heap\":%u,\"mode\":\"%s\"}\n", seq->valuedouble, online ? "true" : "false", ESP.getFreeHeap(), visibleMode.c_str());
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
  Serial.println("{\"type\":\"display_ready\",\"v\":1,\"role\":\"mode\",\"firmware\":\"3.0.1-navy\"}");
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
  delay(5);
}
