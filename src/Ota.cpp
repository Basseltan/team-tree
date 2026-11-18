#include "Ota.h"

#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <mbedtls/md.h>

volatile int8_t gOtaProgress = -1;

namespace {

constexpr uint32_t kStallTimeoutMs = 15000;

String toHex(const uint8_t* data, size_t len) {
  static const char kDigits[] = "0123456789abcdef";
  String out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; i++) {
    out += kDigits[data[i] >> 4];
    out += kDigits[data[i] & 0x0F];
  }
  return out;
}

bool fail(const char* reason) {
  Serial.printf("[ota] failed: %s\n", reason);
  gOtaProgress = -1;
  return false;
}

}  // namespace

bool performOta(const String& url, const String& sha256Hex) {
  if (sha256Hex.length() != 64) return fail("missing/invalid sha256");
  Serial.printf("[ota] downloading %s\n", url.c_str());
  gOtaProgress = 0;

  // The download host (e.g. GitHub release CDN) is not pinned; integrity is
  // guaranteed by the SHA-256 check below instead.
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(kStallTimeoutMs);
  if (!http.begin(client, url)) return fail("http begin");

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    Serial.printf("[ota] http status %d\n", code);
    return fail("http status");
  }

  int total = http.getSize();
  if (total <= 0) {
    http.end();
    return fail("unknown content length");
  }
  if (!Update.begin(total)) {
    http.end();
    return fail("not enough space");
  }

  mbedtls_md_context_t md;
  mbedtls_md_init(&md);
  mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
  mbedtls_md_starts(&md);

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[1024];
  int written = 0;
  uint32_t lastData = millis();
  bool ok = true;

  while (written < total) {
    size_t avail = stream->available();
    if (avail == 0) {
      if (!http.connected() || millis() - lastData > kStallTimeoutMs) {
        ok = false;
        break;
      }
      delay(2);
      continue;
    }
    size_t toRead = min(avail, sizeof(buf));
    int n = stream->readBytes(buf, toRead);
    if (n <= 0) continue;
    mbedtls_md_update(&md, buf, n);
    if (Update.write(buf, n) != static_cast<size_t>(n)) {
      ok = false;
      break;
    }
    written += n;
    lastData = millis();
    gOtaProgress = static_cast<int8_t>(written * 100LL / total);
  }
  http.end();

  uint8_t digest[32];
  mbedtls_md_finish(&md, digest);
  mbedtls_md_free(&md);

  if (!ok || written != total) {
    Update.abort();
    return fail("download incomplete");
  }
  if (!toHex(digest, sizeof(digest)).equalsIgnoreCase(sha256Hex)) {
    Update.abort();
    return fail("sha256 mismatch");
  }
  if (!Update.end(true)) return fail("update end");

  Serial.println(F("[ota] success, restarting"));
  delay(500);
  ESP.restart();
  return true;  // not reached
}
