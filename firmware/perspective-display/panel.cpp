// Monochrome 1.54-inch panel sequence adapted from Waveshare's official
// ESP32-S3-ePaper-1.54 epaper_driver_bsp.cpp, aaf395c1bbd86f7c5b7a2f9f0401241ff5c44956.
#include "panel.h"
#include "panel_lut.h"
static bool healthy = true;
bool panelHealthy() { return healthy; }
static void command(uint8_t value) {
  digitalWrite(EPD_DC_PIN, LOW); DEV_SPI_WriteByte(value);
}
static void data(uint8_t value) {
  digitalWrite(EPD_DC_PIN, HIGH); DEV_SPI_WriteByte(value);
}
static void waitReady(bool refreshing = false) {
  if (!healthy) return;
  uint32_t start = millis();
  // This monochrome controller uses HIGH=busy, unlike the four-color G panel.
  if (refreshing) {
    while (!digitalRead(EPD_BUSY_PIN) && millis()-start < 1000) delay(1);
    if (!digitalRead(EPD_BUSY_PIN)) { healthy=false; Serial.println("epaper_no_refresh_response"); return; }
  } else delay(10);
  while (digitalRead(EPD_BUSY_PIN)) {
    if (millis()-start > 15000) { healthy=false; Serial.println("epaper_busy_timeout"); return; }
    delay(5);
  }
}
void panelInit() {
  healthy=true;
  digitalWrite(EPD_RST_PIN,HIGH); delay(50);
  digitalWrite(EPD_RST_PIN,LOW); delay(20);
  digitalWrite(EPD_RST_PIN,HIGH); delay(50);
  waitReady(); if (!healthy) return;
  command(0x12); waitReady(); if (!healthy) return;
  command(0x01); data(0xc7); data(0); data(1);
  command(0x11); data(1);
  command(0x44); data(0); data(24);
  command(0x45); data(199); data(0); data(0); data(0);
  command(0x3c); data(1);
  command(0x18); data(0x80);
  command(0x22); data(0xb1); command(0x20);
  command(0x4e); data(0);
  command(0x4f); data(199); data(0);
  waitReady(); if (!healthy) return;
  command(0x32); for(int i=0;i<153;++i) data(WF_Full_1IN54[i]);
  waitReady();
  command(0x3f); data(WF_Full_1IN54[153]);
  command(0x03); data(WF_Full_1IN54[154]);
  command(0x04); for(int i=155;i<158;++i) data(WF_Full_1IN54[i]);
  command(0x2c); data(WF_Full_1IN54[158]);
}
void panelDisplay(const uint8_t *pixels) {
  if (!healthy) return;
  command(0x24); for(int i=0;i<5000;++i) data(pixels[i]);
  command(0x22); data(0xc7); command(0x20); waitReady(true);
}
void panelSleep() {
  if (!healthy) return;
  command(0x10); data(0x01); delay(10);
}
