#include "TreeState.h"

#include <ArduinoJson.h>

namespace {

struct ModeEntry {
  const char* name;
  Mode mode;
};

constexpr ModeEntry kModes[] = {
    {"off", Mode::Off},         {"solid", Mode::Solid},     {"breathe", Mode::Breathe},
    {"fade", Mode::Fade},       {"rainbow", Mode::Rainbow}, {"chase", Mode::Chase},
    {"twinkle", Mode::Twinkle},
};

bool parseHexColor(const char* text, CRGB& out) {
  if (text == nullptr) return false;
  if (*text == '#') text++;
  if (strlen(text) != 6) return false;
  char* end = nullptr;
  uint32_t value = strtoul(text, &end, 16);
  if (end == nullptr || *end != '\0') return false;
  out = CRGB(value);
  return true;
}

}  // namespace

TreeState defaultTreeState() {
  TreeState s;
  s.mode = Mode::Twinkle;
  s.colors[0] = CRGB(0xFFB060);  // warm white
  s.colors[1] = CRGB(0xFF0000);
  s.colors[2] = CRGB(0x00FF20);
  s.colorCount = 3;
  s.speed = 5;
  s.brightness = 60;
  return s;
}

const char* modeName(Mode mode) {
  for (const auto& entry : kModes) {
    if (entry.mode == mode) return entry.name;
  }
  return "?";
}

bool parseTreeState(const char* json, size_t len, TreeState& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len) != DeserializationError::Ok) return false;

  TreeState s = defaultTreeState();

  const char* mode = doc["mode"] | "twinkle";
  for (const auto& entry : kModes) {
    if (strcmp(entry.name, mode) == 0) s.mode = entry.mode;
  }

  JsonArrayConst colors = doc["colors"].as<JsonArrayConst>();
  if (!colors.isNull()) {
    uint8_t count = 0;
    for (JsonVariantConst c : colors) {
      if (count >= kMaxColors) break;
      if (parseHexColor(c.as<const char*>(), s.colors[count])) count++;
    }
    if (count > 0) s.colorCount = count;
  }

  s.speed = constrain(doc["speed"] | 5, 1, 10);
  s.brightness = constrain(doc["brightness"] | 60, 0, 100);

  out = s;
  return true;
}
