// The shared tree state that is distributed via the retained MQTT message
// on TOPIC_STATE. Example payload:
//   {"mode":"twinkle","colors":["#ff0000","#ffffff"],"speed":5,
//    "brightness":80,"by":"Anna"}
#pragma once

#include <FastLED.h>

enum class Mode : uint8_t { Off, Solid, Breathe, Fade, Rainbow, Chase, Twinkle };

constexpr uint8_t kMaxColors = 6;

struct TreeState {
  Mode mode = Mode::Twinkle;
  CRGB colors[kMaxColors];
  uint8_t colorCount = 0;
  uint8_t speed = 5;         // 1 (slow) .. 10 (fast)
  uint8_t brightness = 60;   // 0 .. 100 %
};

TreeState defaultTreeState();

// Parses a JSON payload. Unknown/missing fields fall back to defaults.
// Returns false if the payload is not valid JSON.
bool parseTreeState(const char* json, size_t len, TreeState& out);

const char* modeName(Mode mode);
