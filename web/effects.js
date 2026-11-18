// Port of src/Effects.cpp - must stay in sync with the firmware so the
// preview shows exactly what the trees show (same formulas, same epoch time).
'use strict';

const TreeEffects = (() => {
  const U32 = 4294967296;
  const u32 = (n) => ((n % U32) + U32) % U32;  // for large Numbers (int64 -> uint32 cast)

  const scale8 = (i, s) => (i * (s + 1)) >> 8;
  const sin8 = (t) => {
    const v = Math.round(128 + 127.5 * Math.sin(((t & 255) / 256) * 2 * Math.PI));
    return Math.max(0, Math.min(255, v));
  };
  const ease8InOutCubic = (i) => {
    const ii = scale8(i, i);
    const iii = scale8(ii, i);
    return Math.max(0, Math.min(255, 3 * ii - 2 * iii));
  };

  function hash32(x) {
    x >>>= 0;
    x ^= x >>> 16;
    x = Math.imul(x, 0x7feb352d) >>> 0;
    x ^= x >>> 15;
    x = Math.imul(x, 0x846ca68b) >>> 0;
    x ^= x >>> 16;
    return x >>> 0;
  }

  const scaledPeriod = (baseMs, speed) => Math.floor((baseMs * 5) / (speed || 1));
  const phase8 = (now, period) => Math.floor(((now % period) * 256) / period);
  const cycleIndex = (now, period) => u32(Math.floor(now / period));

  const nscale8Video = (c, s) => c.map((x) => ((x * s) >> 8) + (x && s ? 1 : 0));
  const blend = (a, b, amt) => a.map((x, k) => Math.round(x + ((b[k] - x) * amt) / 255));

  function hsv(h8) {
    const h = (h8 / 256) * 6;
    const i = Math.floor(h);
    const f = h - i;
    const q = Math.round(255 * (1 - f));
    const t = Math.round(255 * f);
    return [[255, t, 0], [q, 255, 0], [0, 255, t], [0, q, 255], [t, 0, 255], [255, 0, q]][i % 6];
  }

  const BLACK = [0, 0, 0];
  const WARM_SPARKLE = [0xff, 0xf4, 0xe0];

  function hexToRgb(hex) {
    const v = parseInt(String(hex).replace('#', ''), 16);
    return [(v >> 16) & 255, (v >> 8) & 255, v & 255];
  }

  // state: {mode, colors: ['#rrggbb', ...], speed, brightness}
  function render(state, count, now) {
    const colors = (state.colors || []).map(hexToRgb);
    const n = colors.length;
    const speed = state.speed || 5;
    const at = (idx) => colors[idx % n];
    const leds = new Array(count).fill(BLACK);

    if (state.mode === 'off') return leds;
    if (n === 0 && state.mode !== 'rainbow') return leds;

    switch (state.mode) {
      case 'solid':
        for (let i = 0; i < count; i++) leds[i] = at(i);
        break;
      case 'breathe': {
        const period = scaledPeriod(4000, speed);
        const level = 20 + scale8(sin8(phase8(now, period) - 64), 235);
        leds.fill(nscale8Video(at(cycleIndex(now, period)), level));
        break;
      }
      case 'fade': {
        const period = scaledPeriod(3000, speed);
        const idx = cycleIndex(now, period);
        leds.fill(blend(at(idx), at(idx + 1), ease8InOutCubic(phase8(now, period))));
        break;
      }
      case 'rainbow': {
        const base = phase8(now, scaledPeriod(6000, speed));
        for (let i = 0; i < count; i++) leds[i] = hsv((base + Math.floor((i * 256) / count)) & 255);
        break;
      }
      case 'chase': {
        const step = scaledPeriod(150, speed);
        const pos = cycleIndex(now, step) % count;
        const lap = cycleIndex(now, step * count);
        const background = nscale8Video(n > 1 ? colors[0] : BLACK, 40);
        const head = n > 1 ? colors[1 + (lap % (n - 1))] : colors[0];
        const tail = [255, 110, 40];
        for (let i = 0; i < count; i++) {
          const dist = (pos + count - i) % count;
          leds[i] = dist < tail.length ? blend(background, head, tail[dist]) : background;
        }
        break;
      }
      case 'twinkle':
      default: {
        const slot = scaledPeriod(1200, speed);
        const half = Math.floor(slot / 2);
        for (let i = 0; i < count; i++) {
          const local = now + (hash32(Math.imul(i, 7919)) % slot);
          const slotIdx = cycleIndex(local, slot);
          const within = local % slot;
          const r = hash32(Math.imul(slotIdx, 2654435761) ^ Math.imul(i + 1, 40503));
          const base = nscale8Video(at(i), 70);
          if (r % 3 === 0) {
            const env = Math.min(255, Math.floor(((within < half ? within : slot - within) * 255) / half));
            const sparkle = n > 1 ? at(r >>> 8) : WARM_SPARKLE;
            leds[i] = blend(base, sparkle, env);
          } else {
            leds[i] = base;
          }
        }
      }
    }
    return leds;
  }

  return { render };
})();
