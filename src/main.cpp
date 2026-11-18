// Team-Tree firmware.
//
// - WiFiManager (non-blocking) captive portal for WiFi + MQTT settings
// - MQTT over TLS to the shared broker; the retained message on TOPIC_STATE
//   defines what all trees show
// - NTP-synced clock so animations run in lock-step on every tree
// - Separate FreeRTOS task renders the LEDs, so animations stay smooth
//   during blocking network operations (TLS handshake, portal)
// - Last state is persisted, so the tree keeps glowing without network
// - Pull-based OTA via TOPIC_OTA
//
// BOOT button: short press = LEDs on/off, hold 2-5 s = open config portal,
// hold >= 5 s = factory reset (WiFi + settings).

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <sys/time.h>
#include <time.h>

#include <string>

#include "BoardPins.h"
#include "Config.h"
#include "Effects.h"
#include "Ota.h"
#include "TreeState.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy include/secrets.example.h to include/secrets.h and fill in the broker credentials"
#endif

// Embedded via board_build.embed_txtfiles (NUL-terminated).
extern const char kCaCert[] asm("_binary_certs_emqxsl_ca_crt_start");

namespace {

constexpr time_t kValidTimeThreshold = 1700000000;  // 2023-11, "clock is set"
constexpr uint32_t kWifiRetryIntervalMs = 30000;
constexpr uint32_t kPortalTimeoutSec = 180;
constexpr uint32_t kPortalHoldMs = 2000;

// ---- Shared between loop task and LED task -------------------------------
CRGB gLeds[kNumLeds];
TreeState gState;
SemaphoreHandle_t gStateMutex = nullptr;
volatile bool gLocalOff = false;
volatile bool gPortalActive = false;

// ---- Loop task only ------------------------------------------------------
Preferences gPrefs;
WiFiManager gWm;
WiFiClientSecure gNet;
PubSubClient gMqtt(gNet);

struct Settings {
  String treeName;
  String host;
  uint16_t port = MQTT_DEFAULT_PORT;
  String user;
  String pass;
} gSettings;

WiFiManagerParameter gParamName("tree_name", "Name des Baums", "", 32);
WiFiManagerParameter gParamHost("mqtt_host", "MQTT Host", "", 80);
WiFiManagerParameter gParamPort("mqtt_port", "MQTT Port", "", 6);
WiFiManagerParameter gParamUser("mqtt_user", "MQTT User", "", 32);
WiFiManagerParameter gParamPass("mqtt_pass", "MQTT Passwort (leer = unver&auml;ndert)", "", 64,
                                "type='password'");

String gDeviceId;     // "tree-<mac>"
String gStatusTopic;  // TOPIC_STATUS_PREFIX + gDeviceId
std::string gLastStateJson;

bool gSntpStarted = false;
bool gTimeValid = false;
bool gMqttConfigChanged = false;
uint32_t gLastMqttAttempt = 0;
uint32_t gLastStatus = 0;
uint32_t gLastWifiRetry = 0;

bool gOtaPending = false;
String gOtaUrl;
String gOtaSha;
String gOtaVersion;
String gOtaFailedVersion;

// ---------------------------------------------------------------------------

int64_t epochMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

TreeState getState() {
  xSemaphoreTake(gStateMutex, portMAX_DELAY);
  TreeState copy = gState;
  xSemaphoreGive(gStateMutex);
  return copy;
}

void setState(const TreeState& s) {
  xSemaphoreTake(gStateMutex, portMAX_DELAY);
  gState = s;
  xSemaphoreGive(gStateMutex);
}

void ledTask(void*) {
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    int8_t ota = gOtaProgress;
    uint8_t brightness;
    if (ota >= 0) {
      uint16_t lit = kNumLeds * ota / 100;
      for (uint16_t i = 0; i < kNumLeds; i++) gLeds[i] = i < lit ? CRGB::Green : CRGB::Black;
      brightness = 80;
    } else if (gLocalOff) {
      fill_solid(gLeds, kNumLeds, CRGB::Black);
      brightness = 0;
    } else {
      TreeState s = getState();
      renderEffect(s, gLeds, kNumLeds, epochMs());
      brightness = map(s.brightness, 0, 100, 0, 255);
    }
    if (gPortalActive && ota < 0) {
      // Blink the last LED blue while the config portal is open.
      gLeds[kNumLeds - 1] = (millis() / 500) % 2 ? CRGB::Blue : CRGB::Black;
      brightness = max<uint8_t>(brightness, 60);
    }
    FastLED.setBrightness(brightness);
    FastLED.show();
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(kFrameIntervalMs));
  }
}

// ---- Settings -------------------------------------------------------------

// Preferences::getString() logs an error for missing keys, so check first.
String prefString(const char* key) { return gPrefs.isKey(key) ? gPrefs.getString(key, "") : String(); }

String prefOrDefault(const char* key, const char* fallback) {
  String value = prefString(key);
  return value.length() > 0 ? value : String(fallback);
}

void loadSettings() {
  gSettings.treeName = prefString("tree_name");
  gSettings.host = prefOrDefault("mqtt_host", MQTT_DEFAULT_HOST);
  gSettings.port = gPrefs.getUShort("mqtt_port", MQTT_DEFAULT_PORT);
  gSettings.user = prefOrDefault("mqtt_user", MQTT_DEFAULT_USER);
  gSettings.pass = prefOrDefault("mqtt_pass", MQTT_DEFAULT_PASS);

  static char portText[6];
  snprintf(portText, sizeof(portText), "%u", gSettings.port);
  gParamName.setValue(gSettings.treeName.c_str(), 32);
  gParamHost.setValue(gSettings.host.c_str(), 80);
  gParamPort.setValue(portText, 6);
  gParamUser.setValue(gSettings.user.c_str(), 32);
}

void saveParams() {
  gPrefs.putString("tree_name", gParamName.getValue());
  gPrefs.putString("mqtt_host", gParamHost.getValue());
  int port = atoi(gParamPort.getValue());
  if (port > 0 && port < 65536) gPrefs.putUShort("mqtt_port", port);
  gPrefs.putString("mqtt_user", gParamUser.getValue());
  if (strlen(gParamPass.getValue()) > 0) gPrefs.putString("mqtt_pass", gParamPass.getValue());
  loadSettings();
  gMqttConfigChanged = true;
  Serial.println(F("[cfg] settings saved"));
}

void loadPersistedState() {
  String json = prefString("state");
  TreeState s = defaultTreeState();
  if (json.length() > 0 && parseTreeState(json.c_str(), json.length(), s)) {
    gLastStateJson = json.c_str();
    Serial.printf("[state] restored: %s\n", json.c_str());
  }
  setState(s);
}

// ---- MQTT -----------------------------------------------------------------

void publishStatus() {
  JsonDocument doc;
  doc["online"] = true;
  doc["name"] = gSettings.treeName;
  doc["fw"] = FW_VERSION;
  doc["rssi"] = WiFi.RSSI();
  doc["ip"] = WiFi.localIP().toString();
  doc["uptime"] = millis() / 1000;
  char buf[256];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  gMqtt.publish(gStatusTopic.c_str(), reinterpret_cast<const uint8_t*>(buf), n, true);
  gLastStatus = millis();
}

void handleStateMessage(const char* payload, size_t len) {
  TreeState s;
  if (!parseTreeState(payload, len, s)) {
    Serial.println(F("[state] invalid JSON, ignored"));
    return;
  }
  setState(s);
  std::string json(payload, len);
  Serial.printf("[state] %s\n", json.c_str());
  if (json != gLastStateJson) {
    gLastStateJson = json;
    gPrefs.putString("state", json.c_str());
  }
}

void handleOtaMessage(const char* payload, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, len) != DeserializationError::Ok) return;
  String version = doc["version"] | "";
  String url = doc["url"] | "";
  String sha = doc["sha256"] | "";
  if (version.isEmpty() || url.isEmpty() || version == FW_VERSION || version == gOtaFailedVersion) {
    return;
  }
  Serial.printf("[ota] update %s -> %s announced\n", FW_VERSION, version.c_str());
  gOtaVersion = version;
  gOtaUrl = url;
  gOtaSha = sha;
  gOtaPending = true;  // handled in loop(), not inside the MQTT callback
}

void onMqttMessage(char* topic, uint8_t* payload, unsigned int len) {
  const char* text = reinterpret_cast<const char*>(payload);
  if (strcmp(topic, TOPIC_STATE) == 0) {
    handleStateMessage(text, len);
  } else if (strcmp(topic, TOPIC_OTA) == 0) {
    handleOtaMessage(text, len);
  }
}

void connectMqtt() {
  gMqtt.setServer(gSettings.host.c_str(), gSettings.port);

  JsonDocument will;
  will["online"] = false;
  will["name"] = gSettings.treeName;
  char willBuf[96];
  serializeJson(will, willBuf, sizeof(willBuf));

  Serial.printf("[mqtt] connecting to %s:%u as %s\n", gSettings.host.c_str(), gSettings.port,
                gDeviceId.c_str());
  if (!gMqtt.connect(gDeviceId.c_str(), gSettings.user.c_str(), gSettings.pass.c_str(),
                     gStatusTopic.c_str(), 1, true, willBuf)) {
    Serial.printf("[mqtt] connect failed, state %d\n", gMqtt.state());
    return;
  }
  Serial.println(F("[mqtt] connected"));
  publishStatus();
  gMqtt.subscribe(TOPIC_STATE, 1);
  gMqtt.subscribe(TOPIC_OTA, 1);
}

// ---- WiFi -----------------------------------------------------------------

String apName() { return "Team-Tree-" + gDeviceId.substring(gDeviceId.length() - 4); }

// Many ESP32-C3 Super Mini boards have a poorly matched antenna; at full TX
// power clients fail to associate. A reduced TX power makes WiFi reliable.
void applyTxPower() { WiFi.setTxPower(WIFI_POWER_8_5dBm); }

void startPortal() {
  Serial.printf("[wifi] config portal '%s' opened\n", apName().c_str());
  gWm.setConfigPortalTimeout(gWm.getWiFiIsSaved() ? kPortalTimeoutSec : 0);
  gWm.startConfigPortal(apName().c_str());
  applyTxPower();
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  applyTxPower();
  WiFi.setHostname(gDeviceId.c_str());

  gWm.setConfigPortalBlocking(false);
  gWm.setConnectTimeout(20);
  gWm.setTitle("Team-Tree");
  gWm.addParameter(&gParamName);
  gWm.addParameter(&gParamHost);
  gWm.addParameter(&gParamPort);
  gWm.addParameter(&gParamUser);
  gWm.addParameter(&gParamPass);
  gWm.setSaveParamsCallback(saveParams);
  // Without saved WiFi the portal must stay open until someone configures it.
  gWm.setConfigPortalTimeout(gWm.getWiFiIsSaved() ? kPortalTimeoutSec : 0);

  bool connected = gWm.autoConnect(apName().c_str());
  applyTxPower();  // the AP may have been (re)started inside autoConnect()
  if (connected) {
    Serial.printf("[wifi] connected, IP %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.printf("[wifi] not connected, portal '%s' running\n", apName().c_str());
  }
}

void maintainWifi() {
  gWm.process();
  gPortalActive = gWm.getConfigPortalActive();

  if (WiFi.status() == WL_CONNECTED) return;
  if (gPortalActive || !gWm.getWiFiIsSaved()) return;
  if (millis() - gLastWifiRetry < kWifiRetryIntervalMs) return;
  gLastWifiRetry = millis();
  Serial.println(F("[wifi] reconnecting..."));
  WiFi.begin();  // uses the stored credentials
}

// ---- Button ---------------------------------------------------------------

void factoryReset() {
  Serial.println(F("[btn] factory reset"));
  gWm.resetSettings();
  gPrefs.clear();
  delay(300);
  ESP.restart();
}

void handleButton() {
  static bool lastRaw = HIGH;
  static bool stable = HIGH;
  static uint32_t lastChange = 0;
  static uint32_t pressedAt = 0;

  bool raw = digitalRead(PIN_BUTTON_BOOT);
  if (raw != lastRaw) {
    lastRaw = raw;
    lastChange = millis();
  }
  if (millis() - lastChange < kDebounceMs || raw == stable) return;
  stable = raw;

  if (stable == LOW) {
    pressedAt = millis();
    digitalWrite(PIN_ONBOARD_LED, LOW);
    return;
  }
  digitalWrite(PIN_ONBOARD_LED, HIGH);
  uint32_t held = millis() - pressedAt;
  if (held >= kLongPressMs) {
    factoryReset();
  } else if (held >= kPortalHoldMs) {
    if (!gWm.getConfigPortalActive()) startPortal();
  } else {
    gLocalOff = !gLocalOff;
    Serial.printf("[btn] LEDs %s\n", gLocalOff ? "off" : "on");
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);  // let the native USB CDC connection come up

  // getEfuseMac() packs the MAC little-endian; print the bytes in the usual
  // order so the ID matches the MAC shown by routers and esptool.
  uint64_t efuseMac = ESP.getEfuseMac();
  uint8_t mac[6];
  memcpy(mac, &efuseMac, sizeof(mac));
  char id[20];
  snprintf(id, sizeof(id), "tree-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4],
           mac[5]);
  gDeviceId = id;
  gStatusTopic = String(TOPIC_STATUS_PREFIX) + gDeviceId;
  Serial.printf("=== Team-Tree %s (%s) ===\n", FW_VERSION, gDeviceId.c_str());

  pinMode(PIN_BUTTON_BOOT, INPUT_PULLUP);
  pinMode(PIN_ONBOARD_LED, OUTPUT);
  digitalWrite(PIN_ONBOARD_LED, HIGH);  // active-low, off

  gPrefs.begin("tree", false);
  loadSettings();

  gStateMutex = xSemaphoreCreateMutex();
  loadPersistedState();

  FastLED.addLeds<WS2812B, PIN_LED_DATA, GRB>(gLeds, kNumLeds);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, kMaxMilliamps);
  FastLED.clear(true);
  xTaskCreate(ledTask, "leds", 4096, nullptr, 2, nullptr);

  gNet.setCACert(kCaCert);
  gMqtt.setBufferSize(1024);
  gMqtt.setKeepAlive(kMqttKeepAliveSec);
  gMqtt.setCallback(onMqttMessage);

  setupWifi();
}

void loop() {
  maintainWifi();
  handleButton();

  if (WiFi.status() == WL_CONNECTED) {
    if (!gSntpStarted) {
      configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
      gSntpStarted = true;
    }
    if (!gTimeValid && time(nullptr) > kValidTimeThreshold) {
      gTimeValid = true;
      Serial.println(F("[time] synced"));
    }
    if (gMqttConfigChanged) {
      gMqttConfigChanged = false;
      gMqtt.disconnect();
      gLastMqttAttempt = 0;
    }

    // TLS certificate validation needs a valid clock.
    if (gTimeValid) {
      if (!gMqtt.connected()) {
        if (gLastMqttAttempt == 0 || millis() - gLastMqttAttempt > kMqttRetryIntervalMs) {
          gLastMqttAttempt = millis();
          connectMqtt();
        }
      } else {
        gMqtt.loop();
        if (millis() - gLastStatus > kStatusIntervalMs) publishStatus();
      }
    }

    if (gOtaPending) {
      gOtaPending = false;
      gMqtt.disconnect();  // free the TLS session memory for the download
      if (!performOta(gOtaUrl, gOtaSha)) gOtaFailedVersion = gOtaVersion;
    }
  }

  delay(5);
}
