#pragma once
#include <stdint.h>
struct Portrait { const char *id, *first, *last; const uint8_t *pixels; };
static const Portrait portraits[] = {
  {"freud", "Sigmund", "Freud", nullptr},
  {"machiavelli", "Niccolo", "Machiavelli", nullptr},
  {"leonardo", "Leonardo", "da Vinci", nullptr},
};
