#include <Arduino.h>
#include "panel_power.h"
#include "EPD_3in97.h"
#include "GUI_Paint.h"
#include "wifi_link.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Only this task touches the panel. The main loop keeps Wi-Fi and ACKs moving
// during multi-second refreshes. The one-slot mailbox always keeps the latest view.
struct ContextView {
  bool online, removeNext;
  uint8_t count, page;
  char names[50][321];
};
static ContextView current = {}, pending = {}, painted = {};
static QueueHandle_t views;
static uint8_t *pixels;
static uint32_t lastPacket = 0;
static String input;
static bool overflow = false;
static const int perPage = 6;

static const char *field(cJSON *object, const char *key) {
  cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
  return cJSON_IsString(value) ? value->valuestring : "";
}

// Vendor fonts contain printable ASCII only. Collapse each unsupported Unicode
// codepoint to one '?', never index the bitmap outside its glyph table.
static String displayText(const char *s) {
  String out;
  for (size_t i = 0; s[i]; ++i) {
    uint8_t c = s[i];
    if (c >= 32 && c <= 126) out += char(c);
    else if (c < 128) out += ' ';
    else if ((c & 0xC0) != 0x80) out += '?';
  }
  return out;
}
static void label(int x, int y, const String &s, sFONT *font = &Font24,
                  uint16_t foreground = BLACK, uint16_t background = WHITE) {
  Paint_DrawString_EN(x, y, s.c_str(), font, foreground, background);
}
static void bigLabel(int x, int y, const String &s, uint16_t color) {
  const int stride = (Font24.Width + 7) / 8;
  for (size_t i = 0; i < s.length(); ++i) {
    uint8_t c = s[i]; if (c < 32 || c > 126) c = '?';
    const uint8_t *glyph = Font24.table + (c - 32) * Font24.Height * stride;
    for (int row = 0; row < Font24.Height; ++row)
      for (int col = 0; col < Font24.Width; ++col)
        if (glyph[row * stride + col / 8] & (0x80 >> (col % 8)))
          for (int yy = 0; yy < 2; ++yy) for (int xx = 0; xx < 2; ++xx)
            Paint_SetPixel(x + i * 34 + col * 2 + xx, y + row * 2 + yy, color);
  }
}
static void draw(const ContextView &view) {
  Paint_Clear(WHITE);
  Paint_DrawRectangle(0, 0, 479, 71, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  bigLabel(24, 12, "CONTEXT", WHITE);
  if (view.online) {
    String count = String(view.count) + " ACTIVE";
    label(456 - count.length() * 17, 28, count, &Font24, WHITE, BLACK);
  }
  if (!view.online) {
    bigLabel(24, 180, "WAITING", BLACK);
    label(24, 280, "Open the instrument");
    label(24, 316, "on your computer.");
    label(24, 396, "Context cards arrive");
    label(24, 432, "over Wi-Fi.");
  } else if (!view.count) {
    bigLabel(24, 180, "YOUR CONTEXT", BLACK);
    bigLabel(24, 236, "SHELF", BLACK);
    label(24, 340, "No additional context");
    label(24, 376, "cards are active.");
    label(24, 456, "Tap a card to add it.");
  } else {
    label(24, 80, "ADDED CONTEXT CARDS", &Font16);
    for (int row = 0; row < perPage; ++row) {
      int item = view.page * perPage + row; if (item >= view.count) break;
      int y = 104 + row * 104;
      label(20, y + 3, String(item + 1), &Font20);
      String rest = displayText(view.names[item]);
      if (!rest.length()) rest = "(Unnamed context)";
      // Four 24-character lines fit any supported 80-character name. Word
      // breaks consume at least 19 characters so the fourth line cannot spill.
      for (int line = 0; line < 4 && rest.length(); ++line) {
        int split = min(24, int(rest.length()));
        if (rest.length() > 24) {
          int space = rest.lastIndexOf(' ', 24);
          if (space >= 19) split = space;
        }
        label(58, y + line * 24, rest.substring(0, split));
        rest = rest.substring(split); rest.trim();
      }
      Paint_DrawLine(20, y + 100, 460, y + 100, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    }
  }
  if (view.online && view.removeNext) {
    Paint_DrawRectangle(0, 734, 479, 765, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    label(20, 742, "REMOVE NEXT: tap a card to remove it", &Font16, WHITE, BLACK);
  } else label(20, 740, view.online ? "Tap cards to add context" : "Waiting for an active desk", &Font16);
  int pages = max(1, (view.count + perPage - 1) / perPage);
  label(20, 778, "SNAPSHOT", &Font16);
  if (pages > 1) {
    label(175, 778, "Rocker: pages", &Font16);
    label(390, 778, String(view.page + 1) + "/" + pages, &Font16);
  } else label(260, 778, "Context names only", &Font16);
}

static void renderTask(void *) {
  panelPowerBegin();
  DEV_Module_Init();
  Paint_NewImage(pixels, 800, 480, ROTATE_270, WHITE);
  Paint_SetScale(2);
  bool havePainted = false;
  unsigned refreshes = 0;
  for (;;) {
    xQueueReceive(views, &pending, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(350)); // Coalesce card bursts.
    xQueueReceive(views, &pending, 0);
    if (havePainted && !memcmp(&pending, &painted, sizeof(pending))) continue;
    draw(pending);
    bool full = refreshes % 10 == 0;
    uint32_t started = millis();
    bool healthy = panelPowerEnable();
    if (healthy) {
      if (full) { EPD_3IN97_Init(); EPD_3IN97_Display(pixels); }
      else { EPD_3IN97_Init_Fast(); EPD_3IN97_Display_Fast(pixels); }
      healthy = EPD_3IN97_Healthy();
      EPD_3IN97_Sleep(); // Panel sleeps; the ESP32 keeps its Wi-Fi link alive.
    }
    if (healthy) { painted = pending; havePainted = true; ++refreshes; }
    Serial.printf("{\"type\":\"context_rendered\",\"ok\":%s,\"connected\":%s,\"count\":%u,\"page\":%u,\"removeNext\":%s,\"full\":%s,\"ms\":%lu}\n",
      healthy ? "true" : "false", pending.online ? "true" : "false", pending.count, pending.page + 1,
      pending.removeNext ? "true" : "false", full ? "true" : "false", (unsigned long)(millis() - started));
    vTaskDelay(pdMS_TO_TICKS(2000)); // Minimum rest after putting panel to sleep.
  }
}
static void enqueue() { xQueueOverwrite(views, &current); }
static void receivePacket(const String &line) {
  cJSON *packet = cJSON_Parse(line.c_str()); if (!packet) return;
  cJSON *version = cJSON_GetObjectItemCaseSensitive(packet, "v");
  cJSON *seq = cJSON_GetObjectItemCaseSensitive(packet, "seq");
  cJSON *connection = cJSON_GetObjectItemCaseSensitive(packet, "connected");
  cJSON *state = cJSON_GetObjectItemCaseSensitive(packet, "state");
  cJSON *names = cJSON_GetObjectItemCaseSensitive(state, "contexts");
  bool active = cJSON_IsTrue(connection);
  bool valid = cJSON_IsNumber(version) && version->valuedouble == 1 && cJSON_IsNumber(seq) && cJSON_IsBool(connection);
  if (active) {
    valid = valid && cJSON_IsArray(names) && cJSON_GetArraySize(names) <= 50;
    cJSON *item;
    cJSON_ArrayForEach(item, names) valid = valid && cJSON_IsString(item) && strlen(item->valuestring) <= 320;
  }
  if (valid) {
    uint8_t oldPage = current.page;
    memset(&current, 0, sizeof(current));
    current.online = active;
    if (active) {
      current.count = cJSON_GetArraySize(names);
      current.page = min(int(oldPage), max(0, (current.count - 1) / perPage));
      current.removeNext = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(state, "removeNext"));
      for (int i = 0; i < current.count; ++i)
        strlcpy(current.names[i], cJSON_GetArrayItem(names, i)->valuestring, sizeof(current.names[i]));
    }
    lastPacket = millis(); enqueue();
    wifiAcknowledge(seq->valuedouble, active);
    Serial.printf("{\"type\":\"display_ack\",\"role\":\"context\",\"seq\":%.0f,\"connected\":%s,\"count\":%u,\"removeNext\":%s}\n",
      seq->valuedouble, active ? "true" : "false", current.count, current.removeNext ? "true" : "false");
  }
  cJSON_Delete(packet);
}
static void pageButtons() {
  // Official board: up=4, center=5, down=6. Local viewing only, no app writes.
  static int previous = 0, stable = 0;
  static uint32_t since = 0;
  int pressed = !digitalRead(4) ? -1 : !digitalRead(6) ? 1 : !digitalRead(5) ? 2 : 0;
  if (pressed != previous) { previous = pressed; since = millis(); }
  if (millis() - since < 40 || stable == pressed) return;
  stable = pressed;
  if (!pressed || !current.online || current.count <= perPage) return;
  int pages = (current.count + perPage - 1) / perPage;
  current.page = pressed == 2 ? 0 : (current.page + pressed + pages) % pages;
  enqueue();
}
void setup() {
  Serial.setRxBufferSize(16384); Serial.begin(115200); delay(350);
  pixels = (uint8_t *)ps_malloc(48000);
  views = xQueueCreate(1, sizeof(ContextView));
  if (!pixels || !views) { Serial.println("context_allocation_failed"); while (true) delay(1000); }
  for (int pin : {4, 5, 6}) pinMode(pin, INPUT_PULLUP);
  enqueue();
  if (xTaskCreatePinnedToCore(renderTask, "context-panel", 8192, nullptr, 1, nullptr, 1) != pdPASS) {
    Serial.println("context_task_failed"); while (true) delay(1000);
  }
  input.reserve(8192); wifiBegin();
  Serial.println("{\"type\":\"display_ready\",\"role\":\"context\",\"firmware\":\"1.0.0-wifi\"}");
}
void loop() {
  int count = 0;
  while (Serial.available() && count++ < 2048) {
    char c = Serial.read();
    if (c == '\n') {
      if (!overflow) {
        cJSON *config = cJSON_Parse(input.c_str());
        if (config && !strcmp(field(config, "type"), "wifi_config") && wifiConfigure(config, input))
          Serial.println("{\"type\":\"wifi_configured\",\"v\":1}");
        cJSON_Delete(config);
      }
      input = ""; overflow = false;
    } else if (c != '\r') { if (input.length() < 8191) input += c; else overflow = true; }
  }
  wifiLoop(receivePacket);
  if (current.online && millis() - lastPacket > 5000) {
    memset(&current, 0, sizeof(current)); enqueue();
  }
  pageButtons();
  delay(2);
}
