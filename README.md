# Team-Tree

3D-printed Christmas tree with 15 WS2812B LEDs (144 LEDs/m strip) driven by an
ESP32-C3 Super Mini.
All trees subscribe to the same retained MQTT message (EMQX Cloud Serverless),
so they always show the same colours and animations - in lock-step, thanks to
NTP-synced, time-based effects.

## Wiring

| ESP32-C3 Super Mini | WS2812B strip |
|---|---|
| 5V | 5V (+ 470-1000 µF capacitor across 5V/GND) |
| GND | GND |
| GPIO10 | DIN via ~330 Ω resistor |

The data line runs at 3.3 V, which is usually fine for short wires. If the
first LED flickers, power only that LED through a 1N4148 diode (~4.3 V) so
3.3 V is a valid "high" level for it.

## Setup

1. `cp include/secrets.example.h include/secrets.h` and fill in the broker
   address and the `tree` user credentials.
2. `pio run -t upload`
3. Connect to the WiFi `Team-Tree-xxxx` and configure WiFi (and optionally the
   tree's name / MQTT settings) in the captive portal. The last LED blinks
   blue while the portal is open.

## BOOT button

| Press | Action |
|---|---|
| short | LEDs on/off (local only) |
| hold 2-5 s | open config portal (e.g. to change WiFi) |
| hold >= 5 s | factory reset (WiFi + settings) |

## MQTT topics

| Topic | Direction | Retained | Payload |
|---|---|---|---|
| `team-tree/state` | app -> trees | yes | see below |
| `team-tree/status/<id>` | tree -> | yes (LWT `online:false`) | `{"online":true,"name":"...","fw":"1.0.0","rssi":-60,...}` |
| `team-tree/ota` | admin -> trees | yes | `{"version":"1.0.1","url":"https://.../firmware.bin","sha256":"..."}` |

State payload:

```json
{"mode":"twinkle","colors":["#ff0000","#ffffff"],"speed":5,"brightness":80,"by":"Anna"}
```

- `mode`: `off`, `solid`, `breathe`, `fade`, `rainbow`, `chase`, `twinkle`
- `colors`: 1-6 hex colours (ignored by `rainbow`)
- `speed`: 1-10, `brightness`: 0-100

## Web app (smartphone remote)

[web/](web/) is a dependency-free PWA (plain HTML/JS, mqtt.js from unpkg). It
connects as user `remote` via MQTT over WebSocket (`wss://...:8084/mqtt`),
publishes the retained state and lists all trees from their status topics.
The preview uses a JS port of the effects ([web/effects.js](web/effects.js)) and
the same epoch time, so it runs in sync with the real trees - keep it in sync
with [src/Effects.cpp](src/Effects.cpp).

- The password is not in the code; each user enters it once (stored in the
  browser's localStorage).
- The app disconnects 15 s after going to the background to save session
  minutes on the free broker tier.
- Local test: open `web/index.html` directly in a browser.
- Hosting: any static HTTPS host, e.g. GitHub Pages (publish the `web/` folder).
  HTTPS is required for "Add to home screen" and the offline cache.

## OTA update

1. Bump `FW_VERSION` in [include/Config.h](include/Config.h), build.
2. Upload `.pio/build/team-tree/firmware.bin` somewhere reachable via HTTPS
   (e.g. a GitHub release asset).
3. `sha256sum .pio/build/team-tree/firmware.bin`
4. Publish the retained OTA message. Trees download the image, verify the
   SHA-256 and restart. The LEDs show a green progress bar meanwhile.
