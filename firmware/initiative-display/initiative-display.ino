#include <Arduino.h>
#include "lvgl.h"
#include "panel.h"
#include "wifi_link.h"

static lv_obj_t *meter, *number, *nameLabel, *description, *defaultLabel, *statusLabel, *connectionLabel;
static lv_meter_indicator_t *needle, *defaultNeedle, *activeArc;
static bool online = false, factory = false, overflow = false;
static int initiative = 3, saved = 3;
static float shown = 3;
static String input, response = "ready", engine = "scripted";
static uint32_t lastPacket = 0, lastDraw = 0;
static const char *names[] = {"", "Minimal response", "Follow me", "Contribute", "Lead this activity", "Pursue what's needed", "Take charge"};
static const char *details[] = {"", "Very short answers.\nStay close to what I say.", "Respond to what I ask.\nLet me set the direction.", "Add useful connections.\nLeave the next move to me.", "Choose useful next moves\nwithin this activity.", "Choose the activities\nthat advance the work.", "Set priorities and approach.\nCarry the work forward."};
static const char *text(cJSON *object, const char *key) {
  cJSON *v = cJSON_GetObjectItemCaseSensitive(object, key); return cJSON_IsString(v) ? v->valuestring : "";
}
static bool level(cJSON *object, const char *key, int &out) {
  cJSON *v = cJSON_GetObjectItemCaseSensitive(object, key);
  if (!cJSON_IsNumber(v) || v->valuedouble != v->valueint || v->valueint < 1 || v->valueint > 6) return false;
  out = v->valueint; return true;
}
static lv_obj_t *label(int x, int y, int width, const lv_font_t *font, uint32_t color) {
  lv_obj_t *obj = lv_label_create(lv_scr_act()); lv_obj_set_pos(obj, x, y); lv_obj_set_width(obj, width);
  lv_obj_set_style_text_font(obj, font, 0); lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
  lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP); return obj;
}
static void tickLabels(lv_event_t *event) {
  auto *part = static_cast<lv_obj_draw_part_dsc_t *>(lv_event_get_param(event));
  if (part->part == LV_PART_TICKS && part->text) lv_snprintf(part->text, part->text_length, "%d", part->value / 200 + 1);
}
static void showState() {
  if (!online) {
    lv_obj_add_flag(meter, LV_OBJ_FLAG_HIDDEN); lv_label_set_text(number, "--");
    lv_label_set_text(nameLabel, "WAITING"); lv_label_set_text(description, "Open the instrument\non your computer.");
    lv_label_set_text(defaultLabel, ""); lv_label_set_text(statusLabel, "NO ACTIVE DESK");
    lv_obj_set_style_bg_color(statusLabel, lv_color_hex(0x090C12), 0);
    lv_label_set_text(connectionLabel, wifiStatus()); return;
  }
  lv_obj_clear_flag(meter, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text_fmt(number, "%d", initiative); lv_label_set_text(nameLabel, names[initiative]);
  lv_label_set_text(description, details[initiative]);
  lv_label_set_text_fmt(defaultLabel, factory ? "L3 / Original default: 3" : "Saved default: %d", factory ? 3 : saved);
  lv_meter_set_indicator_value(meter, defaultNeedle, ((factory ? 3 : saved) - 1) * 200);
  const char *status = response == "stopped" ? "STOPPED" : response == "thinking" ? "THINKING" : response == "responding" ? "RESPONDING" : response == "error" ? "CHECK CHAT" : "READY";
  lv_label_set_text(statusLabel, status);
  lv_obj_set_style_bg_color(statusLabel, lv_color_hex(response == "stopped" || response == "error" ? 0xCC1438 : 0x1457D9), 0);
  lv_label_set_text(connectionLabel, engine == "live" ? "LIVE AI / WI-FI" : "SCRIPTED DEMO / WI-FI");
}
static void createUI() {
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xFFFFFF), 0); lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
  auto *header = lv_obj_create(lv_scr_act()); lv_obj_set_pos(header, 0, 0); lv_obj_set_size(header, 536, 34);
  lv_obj_set_style_border_width(header, 0, 0); lv_obj_set_style_radius(header, 0, 0); lv_obj_set_style_bg_color(header, lv_color_hex(0x090C12), 0);
  lv_label_set_text(label(25, 7, 190, &lv_font_montserrat_18, 0xFFFFFF), "INITIATIVE");
  meter = lv_meter_create(lv_scr_act()); lv_obj_set_pos(meter, 10, 28); lv_obj_set_size(meter, 208, 208);
  lv_obj_set_style_bg_opa(meter, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(meter, 0, 0);
  lv_obj_set_style_text_font(meter, &lv_font_montserrat_18, LV_PART_TICKS);
  lv_obj_set_style_text_color(meter, lv_color_hex(0x090C12), LV_PART_TICKS);
  lv_obj_set_style_pad_all(meter, 12, 0);
  lv_meter_scale_t *scale = lv_meter_add_scale(meter);
  lv_meter_set_scale_ticks(meter, scale, 6, 3, 10, lv_color_hex(0x090C12));
  lv_meter_set_scale_major_ticks(meter, scale, 1, 3, 13, lv_color_hex(0x090C12), 9);
  lv_meter_set_scale_range(meter, scale, 0, 1000, 270, 135);
  lv_meter_indicator_t *arc = lv_meter_add_arc(meter, scale, 7, lv_color_hex(0xD7DEE8), 0);
  lv_meter_set_indicator_start_value(meter, arc, 0); lv_meter_set_indicator_end_value(meter, arc, 1000);
  activeArc = lv_meter_add_arc(meter, scale, 7, lv_color_hex(0x1457D9), 0);
  lv_meter_set_indicator_start_value(meter, activeArc, 0);
  lv_meter_set_indicator_end_value(meter, activeArc, 400);
  defaultNeedle = lv_meter_add_needle_line(meter, scale, 3, lv_color_hex(0xCC1438), 2);
  needle = lv_meter_add_needle_line(meter, scale, 7, lv_color_hex(0x090C12), -18);
  lv_meter_set_indicator_value(meter, needle, 400);
  lv_obj_add_event_cb(meter, tickLabels, LV_EVENT_DRAW_PART_BEGIN, nullptr);
  number = label(74, 151, 80, &lv_font_montserrat_48, 0x1457D9); lv_obj_set_style_text_align(number, LV_TEXT_ALIGN_CENTER, 0);
  auto *ofSix = label(79, 205, 70, &lv_font_montserrat_14, 0x090C12); lv_label_set_text(ofSix, "OF 6"); lv_obj_set_style_text_align(ofSix, LV_TEXT_ALIGN_CENTER, 0);
  auto *divider = lv_obj_create(lv_scr_act()); lv_obj_set_pos(divider, 230, 25); lv_obj_set_size(divider, 2, 190);
  lv_obj_set_style_border_width(divider, 0, 0); lv_obj_set_style_bg_color(divider, lv_color_hex(0x090C12), 0);
  connectionLabel = label(250, 9, 272, &lv_font_montserrat_14, 0xFFFFFF);
  nameLabel = label(250, 48, 270, &lv_font_montserrat_22, 0x1457D9);
  description = label(250, 85, 268, &lv_font_montserrat_18, 0x090C12);
  statusLabel = label(248, 142, 274, &lv_font_montserrat_24, 0xFFFFFF);
  lv_obj_set_height(statusLabel, 44); lv_obj_set_style_pad_left(statusLabel, 12, 0); lv_obj_set_style_pad_top(statusLabel, 7, 0);
  lv_obj_set_style_bg_opa(statusLabel, LV_OPA_COVER, 0); lv_obj_set_style_radius(statusLabel, 6, 0);
  defaultLabel = label(250, 200, 270, &lv_font_montserrat_18, 0xCC1438);
  showState();
}
static void receivePacket(const String &line) {
  cJSON *packet = cJSON_Parse(line.c_str()); if (!packet) return;
  cJSON *v = cJSON_GetObjectItemCaseSensitive(packet, "v"), *seq = cJSON_GetObjectItemCaseSensitive(packet, "seq");
  cJSON *connection = cJSON_GetObjectItemCaseSensitive(packet, "connected"), *state = cJSON_GetObjectItemCaseSensitive(packet, "state");
  bool active = cJSON_IsTrue(connection);
  bool valid = cJSON_IsNumber(v) && v->valueint == 1 && cJSON_IsNumber(seq) && cJSON_IsBool(connection);
  int next = initiative, baseline = saved;
  if (active) valid = valid && cJSON_IsObject(state) && level(cJSON_GetObjectItemCaseSensitive(state, "controls"), "initiative", next)
    && level(cJSON_GetObjectItemCaseSensitive(state, "defaults"), "initiative", baseline);
  if (valid) {
    online = active; lastPacket = millis();
    if (active) { initiative = next; saved = baseline; factory = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(state, "factory")); response = text(state, "status"); engine = text(state, "engine"); }
    showState(); wifiAcknowledge(seq->valuedouble, active);
    Serial.printf("{\"type\":\"display_ack\",\"v\":1,\"role\":\"initiative\",\"seq\":%.0f,\"connected\":%s,\"initiative\":%d,\"default\":%d,\"stopped\":%s}\n", seq->valuedouble, active ? "true" : "false", initiative, saved, response == "stopped" ? "true" : "false");
  }
  cJSON_Delete(packet);
}
void setup() {
  Serial.setRxBufferSize(16384); Serial.begin(115200); delay(350);
  panelBegin(); createUI(); input.reserve(8192); wifiBegin();
  Serial.println("{\"type\":\"display_ready\",\"v\":1,\"role\":\"initiative\",\"firmware\":\"1.1.0-camera\"}");
}
void loop() {
  int count = 0;
  while (Serial.available() && count++ < 2048) {
    char c = Serial.read();
    if (c == '\n') {
      if (!overflow) { cJSON *p = cJSON_Parse(input.c_str()); if (p && !strcmp(text(p, "type"), "wifi_config") && wifiConfigure(p, input)) Serial.println("{\"type\":\"wifi_configured\",\"v\":1}"); cJSON_Delete(p); }
      input = ""; overflow = false;
    } else if (c != '\r') { if (input.length() < 8191) input += c; else overflow = true; }
  }
  wifiLoop(receivePacket);
  uint32_t now = millis();
  if (online && now - lastPacket > 5000) { online = false; showState(); }
  if (!online) lv_label_set_text(connectionLabel, wifiStatus());
  if (now - lastDraw >= 25) {
    shown += ((online ? initiative : 3) - shown) * .2f;
    lv_meter_set_indicator_value(meter, needle, lroundf((shown - 1) * 200));
    lv_meter_set_indicator_end_value(meter, activeArc, lroundf((shown - 1) * 200)); lastDraw = now;
  }
  lv_timer_handler(); delay(3);
}
