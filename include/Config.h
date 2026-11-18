// Compile-time configuration shared by all trees.
#pragma once

#include <stdint.h>

#define FW_VERSION "1.0.0"

constexpr uint16_t kNumLeds = 12;

// Hard current cap for the LED strip (USB powered). 12 LEDs at full white
// would draw ~720 mA; FastLED scales brightness down to stay below this.
constexpr uint32_t kMaxMilliamps = 400;

constexpr uint32_t kFrameIntervalMs = 20;  // 50 fps

// MQTT topics.
#define TOPIC_STATE "team-tree/state"
#define TOPIC_OTA "team-tree/ota"
#define TOPIC_STATUS_PREFIX "team-tree/status/"

constexpr uint16_t kMqttKeepAliveSec = 60;
constexpr uint32_t kMqttRetryIntervalMs = 10000;
constexpr uint32_t kStatusIntervalMs = 5UL * 60UL * 1000UL;

// Local time zone (used for logging; animations run on UTC epoch time).
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

// Button timings.
constexpr uint32_t kLongPressMs = 5000;  // reset WiFi + settings
constexpr uint32_t kDebounceMs = 40;
