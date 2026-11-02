#pragma once
#include <Arduino.h>
#include "cJSON.h"

void wifiBegin();
bool wifiConfigure(cJSON *packet, const String &serialized);
void wifiLoop(void (*receive)(const String &));
void wifiAcknowledge(double sequence, bool connected);
const char *wifiStatus();
