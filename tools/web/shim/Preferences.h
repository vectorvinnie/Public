// Preferences kept in the browser's localStorage (see play.html).
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "Arduino.h"

extern "C" {
// Returns the stored length (or -1 if missing) and copies up to `max` bytes.
JS_IMPORT(js_prefs_get) int js_prefs_get(const char* key, void* buf, int max);
JS_IMPORT(js_prefs_set) void js_prefs_set(const char* key, const void* buf, int len);
}

class Preferences {
 public:
  bool begin(const char*, bool) { return true; }
  size_t putBytes(const char* k, const void* v, size_t n) { js_prefs_set(k, v, n); return n; }
  size_t getBytesLength(const char* k) { int n = js_prefs_get(k, nullptr, 0); return n < 0 ? 0 : n; }
  size_t getBytes(const char* k, void* v, size_t n) { int r = js_prefs_get(k, v, n); return r < 0 ? 0 : r; }
  void putBool(const char* k, bool v) { putBytes(k, &v, 1); }
  bool getBool(const char* k, bool d) { bool v = d; getBytes(k, &v, 1); return v; }
  void putUChar(const char* k, uint8_t v) { putBytes(k, &v, 1); }
  uint8_t getUChar(const char* k, uint8_t d) { uint8_t v = d; getBytes(k, &v, 1); return v; }
};
