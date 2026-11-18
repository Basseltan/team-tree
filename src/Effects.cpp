#include "Effects.h"

namespace {

// Deterministic integer hash - identical on every tree, so "random" twinkles
// still happen on the same LEDs at the same time everywhere.
uint32_t hash32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

// speed 5 = base period, 10 = twice as fast, 1 = five times slower.
uint32_t scaledPeriod(uint32_t baseMs, uint8_t speed) {
  return baseMs * 5U / (speed == 0 ? 1U : speed);
}

// Position within the current period, 0..255.
uint8_t phase8(int64_t nowMs, uint32_t periodMs) {
  return static_cast<uint8_t>(static_cast<uint64_t>(nowMs % periodMs) * 256U / periodMs);
}

uint32_t cycleIndex(int64_t nowMs, uint32_t periodMs) {
  return static_cast<uint32_t>(nowMs / periodMs);
}

const CRGB& colorAt(const TreeState& s, uint32_t index) {
  return s.colors[index % s.colorCount];
}

void renderSolid(const TreeState& s, CRGB* leds, uint16_t count) {
  for (uint16_t i = 0; i < count; i++) leds[i] = colorAt(s, i);
}

void renderBreathe(const TreeState& s, CRGB* leds, uint16_t count, int64_t now) {
  uint32_t period = scaledPeriod(4000, s.speed);
  // Start at the dark point of the sine so the colour switches while dim.
  uint8_t level = 20 + scale8(sin8(phase8(now, period) - 64), 235);
  CRGB c = colorAt(s, cycleIndex(now, period));
  c.nscale8_video(level);
  fill_solid(leds, count, c);
}

void renderFade(const TreeState& s, CRGB* leds, uint16_t count, int64_t now) {
  uint32_t period = scaledPeriod(3000, s.speed);
  uint32_t idx = cycleIndex(now, period);
  CRGB c = blend(colorAt(s, idx), colorAt(s, idx + 1), ease8InOutCubic(phase8(now, period)));
  fill_solid(leds, count, c);
}

void renderRainbow(const TreeState& s, CRGB* leds, uint16_t count, int64_t now) {
  uint8_t base = phase8(now, scaledPeriod(6000, s.speed));
  for (uint16_t i = 0; i < count; i++) {
    leds[i] = CHSV(base + i * 256 / count, 255, 255);
  }
}

void renderChase(const TreeState& s, CRGB* leds, uint16_t count, int64_t now) {
  uint32_t step = scaledPeriod(150, s.speed);
  uint32_t pos = cycleIndex(now, step) % count;
  uint32_t lap = cycleIndex(now, step * count);
  // With >1 colour: first colour is the background, the head cycles through
  // the remaining colours lap by lap.
  CRGB background = s.colorCount > 1 ? s.colors[0] : CRGB::Black;
  background.nscale8_video(40);
  CRGB head = s.colorCount > 1 ? s.colors[1 + lap % (s.colorCount - 1)] : s.colors[0];
  constexpr uint8_t kTail[] = {255, 110, 40};
  for (uint16_t i = 0; i < count; i++) {
    uint32_t dist = (pos + count - i) % count;
    leds[i] = dist < sizeof(kTail) ? blend(background, head, kTail[dist]) : background;
  }
}

void renderTwinkle(const TreeState& s, CRGB* leds, uint16_t count, int64_t now) {
  uint32_t slot = scaledPeriod(1200, s.speed);
  for (uint16_t i = 0; i < count; i++) {
    // Stagger the slots per LED so not all LEDs change at once.
    int64_t local = now + hash32(i * 7919U) % slot;
    uint32_t slotIdx = cycleIndex(local, slot);
    uint32_t within = static_cast<uint32_t>(local % slot);
    uint32_t r = hash32(slotIdx * 2654435761U ^ (i + 1) * 40503U);

    CRGB base = colorAt(s, i);
    base.nscale8_video(70);
    if (r % 3 == 0) {
      // Triangle envelope 0 -> 255 -> 0 across the slot.
      uint32_t half = slot / 2;
      uint8_t env = within < half ? within * 255 / half : (slot - within) * 255 / half;
      CRGB sparkle = s.colorCount > 1 ? colorAt(s, r >> 8) : CRGB(0xFFF4E0);
      leds[i] = blend(base, sparkle, env);
    } else {
      leds[i] = base;
    }
  }
}

}  // namespace

void renderEffect(const TreeState& state, CRGB* leds, uint16_t count, int64_t nowMs) {
  if (state.colorCount == 0 && state.mode != Mode::Rainbow && state.mode != Mode::Off) {
    fill_solid(leds, count, CRGB::Black);
    return;
  }
  switch (state.mode) {
    case Mode::Off:
      fill_solid(leds, count, CRGB::Black);
      break;
    case Mode::Solid:
      renderSolid(state, leds, count);
      break;
    case Mode::Breathe:
      renderBreathe(state, leds, count, nowMs);
      break;
    case Mode::Fade:
      renderFade(state, leds, count, nowMs);
      break;
    case Mode::Rainbow:
      renderRainbow(state, leds, count, nowMs);
      break;
    case Mode::Chase:
      renderChase(state, leds, count, nowMs);
      break;
    case Mode::Twinkle:
      renderTwinkle(state, leds, count, nowMs);
      break;
  }
}
