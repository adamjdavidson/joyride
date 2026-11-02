#include "wifi_link.h"
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <sys/time.h>

static NetworkClientSecure client;
static String ssid, password, host, fallback, identifier, credential, ca, incoming;
static uint16_t port = 8791;
static bool configured = false, mdnsStarted = false, overflow = false;
static uint32_t nextAttempt = 0, lastData = 0, lastWifiAttempt = 0;
static const char *stage = "Wi-Fi setup needed";

static const char *field(cJSON *packet, const char *key) {
  cJSON *value = cJSON_GetObjectItemCaseSensitive(packet, key);
  return cJSON_IsString(value) ? value->valuestring : "";
}
static bool load(cJSON *packet) {
  cJSON *number = cJSON_GetObjectItemCaseSensitive(packet, "port");
  cJSON *epoch = cJSON_GetObjectItemCaseSensitive(packet, "time");
  if (!packet || !strlen(field(packet, "ssid")) || strlen(field(packet, "ssid")) > 32 || strlen(field(packet, "password")) > 64 ||
      strlen(field(packet, "token")) != 64 || !strlen(field(packet, "id")) || strlen(field(packet, "id")) > 64 ||
      !strlen(field(packet, "host")) || strlen(field(packet, "host")) > 128 || !strlen(field(packet, "ip")) ||
      strlen(field(packet, "ca")) < 100 || !cJSON_IsNumber(number) || number->valueint < 1024 || number->valueint > 65535 ||
      !cJSON_IsNumber(epoch) || epoch->valuedouble < 1700000000) return false;
  ssid = field(packet, "ssid"); password = field(packet, "password");
  host = field(packet, "host"); fallback = field(packet, "ip");
  identifier = field(packet, "id"); credential = field(packet, "token"); ca = field(packet, "ca"); port = number->valueint;
  // A trusted provisioning timestamp allows certificate checks before NTP arrives.
  if (time(nullptr) < (time_t)epoch->valuedouble) {
    timeval now = { .tv_sec = (time_t)epoch->valuedouble, .tv_usec = 0 }; settimeofday(&now, nullptr);
  }
  configured = true;
  return true;
}
static void connectWifi() {
  client.stop();
  if (mdnsStarted) { MDNS.end(); mdnsStarted = false; }
  WiFi.persistent(false);
  WiFi.setHostname(identifier.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(ssid.c_str(), password.c_str());
  lastWifiAttempt = millis(); stage = "Connecting to Wi-Fi";
  client.setConnectionTimeout(2000); client.setHandshakeTimeout(3);
  incoming.reserve(8192);
}
void wifiBegin() {
  Preferences prefs; prefs.begin("instrument", true); String saved = prefs.getString("wifi", ""); prefs.end();
  cJSON *packet = cJSON_Parse(saved.c_str());
  if (load(packet)) connectWifi();
  cJSON_Delete(packet);
}
bool wifiConfigure(cJSON *packet, const String &serialized) {
  if (!load(packet)) return false;
  Preferences prefs;
  if (!prefs.begin("instrument", false)) return false;
  size_t written = prefs.putString("wifi", serialized); prefs.end();
  if (!written) return false;
  WiFi.disconnect(true); connectWifi(); nextAttempt = 0;
  return true;
}
const char *wifiStatus() { return stage; }
void wifiAcknowledge(double sequence, bool connected) {
  if (client.connected()) client.printf("{\"type\":\"display_ack\",\"v\":1,\"seq\":%.0f,\"connected\":%s}\n", sequence, connected ? "true" : "false");
}
void wifiLoop(void (*receive)(const String &)) {
  if (!configured) return;
  uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    client.stop(); stage = "Connecting to Wi-Fi";
    if (now - lastWifiAttempt > 15000) { WiFi.reconnect(); lastWifiAttempt = now; }
    return;
  }
  if (!mdnsStarted) {
    mdnsStarted = MDNS.begin(identifier.c_str());
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
  }
  if (client.connected() && now - lastData > 5000) client.stop();
  if (!client.connected()) {
    stage = "Wi-Fi up / connecting to Pi";
    if ((int32_t)(now - nextAttempt) < 0) return;
    nextAttempt = now + 3000;
    String localName = host; if (localName.endsWith(".local")) localName.remove(localName.length() - 6);
    IPAddress address = mdnsStarted ? MDNS.queryHost(localName.c_str(), 750) : IPAddress();
    if (!address) address.fromString(fallback);
    if (!client.connect(address, port, host.c_str(), ca.c_str(), nullptr, nullptr)) {
      stage = "Wi-Fi up / Pi unavailable";
      Serial.println("{\"type\":\"wifi_status\",\"state\":\"server_unavailable\"}");
      return;
    }
    cJSON *hello = cJSON_CreateObject();
    cJSON_AddNumberToObject(hello, "v", 1); cJSON_AddStringToObject(hello, "type", "hello");
    cJSON_AddStringToObject(hello, "id", identifier.c_str()); cJSON_AddStringToObject(hello, "token", credential.c_str());
    char *data = cJSON_PrintUnformatted(hello); client.println(data); cJSON_free(data); cJSON_Delete(hello);
    incoming = ""; overflow = false; lastData = millis();
    stage = "Wi-Fi connected";
    Serial.println("{\"type\":\"wifi_status\",\"state\":\"tls_connected\"}");
  }
  int count = 0;
  while (client.available() && count++ < 8192) {
    char c = client.read();
    if (c == '\n') {
      if (!overflow) { receive(incoming); lastData = millis(); }
      incoming = ""; overflow = false;
    } else if (c != '\r') { if (incoming.length() < 8191) incoming += c; else overflow = true; }
  }
}
