#include <Arduino.h>
#include "panel.h"
#include "GUI_Paint.h"
#include "wifi_link.h"
#include "portraits.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

struct View { bool online; char person[32]; };
static View current = {};
static QueueHandle_t views;
static uint8_t pixels[5000];
static String input;
static bool overflow = false;
static uint32_t lastPacket = 0;
static const uint16_t ink = 0x00, paper = 0xff;

static const char *field(cJSON *object, const char *key) {
  cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
  return cJSON_IsString(value) ? value->valuestring : "";
}
static void label(const char *text, int y, sFONT *font = &Font16) {
  int x = max(0, (200 - int(strlen(text)) * font->Width) / 2);
  Paint_DrawString_EN(x, y, text, font, ink, paper);
}
static void draw(const View &view) {
  Paint_Clear(paper);
  if (!view.online) {
    label("PERSPECTIVE", 18); label("WAITING", 82, &Font20);
    label("Open the instrument", 136, &Font12);
    label("on your computer", 152, &Font12);
    return;
  }
  if (!view.person[0]) {
    label("PERSPECTIVE", 18); label("NO VISITOR", 82);
    label("Invite a perspective", 136, &Font12);
    label("to step aside together", 152, &Font12);
    return;
  }
  const Portrait *portrait = nullptr;
  for (const auto &p : portraits) if (!strcmp(view.person, p.id)) { portrait = &p; break; }
  if (!portrait) {
    label("PERSPECTIVE", 18); label("NOT LOADED", 82);
    label("Update this display", 140, &Font12); return;
  }
  if (portrait->pixels) {
    for (int i = 0; i < 5000; ++i) pixels[i] = ~portrait->pixels[i];
  } else {
    label("PERSPECTIVE", 18); label("PHOTO PENDING", 78);
    label("Portrait not loaded", 112, &Font12);
  }
  if (!portrait->pixels) {
    label(portrait->first, 151, &Font16); label(portrait->last, 175, &Font20);
  }
}
static void renderTask(void *) {
  DEV_Module_Init(); // This board's screen power switch is active LOW, GPIO6.
  Paint_NewImage(pixels, 200, 200, ROTATE_0, paper); Paint_SetScale(2);
  View next = {}, painted = {};
  bool havePainted = false;
  for (;;) {
    xQueueReceive(views, &next, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(750)); // Keep the latest visitor during a burst.
    xQueueReceive(views, &next, 0);
    if (havePainted && !memcmp(&next, &painted, sizeof(next))) continue;
    draw(next);
    uint32_t start = millis();
    panelInit();
    if (panelHealthy()) panelDisplay(pixels);
    bool healthy = panelHealthy();
    panelSleep();
    healthy = healthy && panelHealthy();
    if (healthy) { painted = next; havePainted = true; }
    Serial.printf("{\"type\":\"perspective_rendered\",\"ok\":%s,\"connected\":%s,\"person\":\"%s\",\"ms\":%lu}\n",
      healthy ? "true" : "false", next.online ? "true" : "false", next.person, (unsigned long)(millis() - start));
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}
static bool validPerson(const char *name) {
  if (!name[0]) return true;
  for (const auto &p : portraits) if (!strcmp(name, p.id)) return true;
  return false;
}
static void receivePacket(const String &line) {
  cJSON *packet = cJSON_Parse(line.c_str()); if (!packet) return;
  cJSON *version = cJSON_GetObjectItemCaseSensitive(packet, "v");
  cJSON *seq = cJSON_GetObjectItemCaseSensitive(packet, "seq");
  cJSON *connection = cJSON_GetObjectItemCaseSensitive(packet, "connected");
  cJSON *state = cJSON_GetObjectItemCaseSensitive(packet, "state");
  cJSON *person = cJSON_GetObjectItemCaseSensitive(state, "person");
  bool active = cJSON_IsTrue(connection);
  bool valid = cJSON_IsNumber(version) && version->valuedouble == 1 && cJSON_IsNumber(seq) && cJSON_IsBool(connection);
  if (active) valid = valid && cJSON_IsObject(state) && (cJSON_IsNull(person) ||
    (cJSON_IsString(person) && validPerson(person->valuestring)));
  if (valid) {
    memset(&current, 0, sizeof(current)); current.online = active;
    if (active) strlcpy(current.person, field(state, "person"), sizeof(current.person));
    lastPacket = millis(); xQueueOverwrite(views, &current);
    wifiAcknowledge(seq->valuedouble, active);
    Serial.printf("{\"type\":\"display_ack\",\"role\":\"perspective\",\"seq\":%.0f,\"connected\":%s,\"person\":\"%s\"}\n",
      seq->valuedouble, active ? "true" : "false", current.person);
  }
  cJSON_Delete(packet);
}
void setup() {
  Serial.setRxBufferSize(16384); Serial.begin(115200); delay(350);
  views = xQueueCreate(1, sizeof(View));
  if (!views) { Serial.println("perspective_queue_failed"); while (true) delay(1000); }
  xQueueOverwrite(views, &current);
  if (xTaskCreatePinnedToCore(renderTask, "portrait-panel", 8192, nullptr, 1, nullptr, 1) != pdPASS) {
    Serial.println("perspective_task_failed"); while (true) delay(1000);
  }
  input.reserve(8192); wifiBegin();
  Serial.println("{\"type\":\"display_ready\",\"role\":\"perspective\",\"firmware\":\"1.1.1-first-names\"}");
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
    memset(&current, 0, sizeof(current)); xQueueOverwrite(views, &current);
  }
  delay(2);
}
