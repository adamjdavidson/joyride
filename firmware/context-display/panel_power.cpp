#include <Arduino.h>
#include <Wire.h>
#include "panel_power.h"

// The board's complete vendor ESP-IDF epaper_port powers the panel via ALDO3.
// TG28 uses the AXP2101 register interface; preserve all other power rails,
// battery/charging configuration and reserved bits.
static int readRegister(uint8_t reg) {
  Wire.beginTransmission(0x34); Wire.write(reg);
  if (Wire.endTransmission(false) || Wire.requestFrom(uint8_t(0x34), uint8_t(1)) != 1) return -1;
  return Wire.read();
}
static bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(0x34); Wire.write(reg); Wire.write(value);
  return Wire.endTransmission() == 0;
}
void panelPowerBegin() { Wire.begin(41, 42); }
bool panelPowerEnable() {
  if (readRegister(0x03) != 0x4A) return false;
  int enabled = readRegister(0x90), voltage = readRegister(0x94);
  if (enabled < 0 || voltage < 0) return false;
  // 500 mV + 28 * 100 mV = 3.3 V, matching the vendor ALDO3 setting.
  if (!writeRegister(0x94, (voltage & 0xE0) | 28) || !writeRegister(0x90, enabled | 4)) return false;
  delay(500);
  int result = readRegister(0x90), actualVoltage = readRegister(0x94);
  bool ok = result >= 0 && (result & 4) && actualVoltage >= 0 && (actualVoltage & 31) == 28;
  Serial.printf("{\"type\":\"panel_power\",\"ok\":%s,\"enabledBefore\":%d,\"enabledAfter\":%d,\"millivolts\":%d}\n",
    ok ? "true" : "false", enabled, result, actualVoltage < 0 ? -1 : 500 + (actualVoltage & 31) * 100);
  return ok;
}
