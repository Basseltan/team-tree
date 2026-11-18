// LED effects. Every effect is a pure function of the state and the current
// UTC epoch time in milliseconds - no internal frame counters. Since all
// trees sync their clock via NTP, they render the very same frame at the
// same moment without having to exchange any per-frame data.
#pragma once

#include <FastLED.h>

#include "TreeState.h"

void renderEffect(const TreeState& state, CRGB* leds, uint16_t count, int64_t nowMs);
