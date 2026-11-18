// Pull-based OTA update. The update is announced via the retained MQTT
// message on TOPIC_OTA:
//   {"version":"1.0.1","url":"https://.../firmware.bin","sha256":"<hex>"}
// The image is downloaded over HTTPS and only activated if its SHA-256
// matches the hash from the (TLS-authenticated, ACL-protected) MQTT message.
#pragma once

#include <Arduino.h>

// Progress 0..100 while an update is running, -1 otherwise. Read by the
// LED task to show a progress bar.
extern volatile int8_t gOtaProgress;

// Blocks until done. Restarts the device on success, returns false on error.
bool performOta(const String& url, const String& sha256Hex);
