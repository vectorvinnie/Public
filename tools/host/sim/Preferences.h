// In-memory Preferences, so "flash" survives a simulated reboot.
#pragma once
#include <map>
#include <string>
#include <vector>
#include <stdint.h>
#include <string.h>

extern std::map<std::string, std::vector<uint8_t>> simFlash;

class Preferences {
 public:
  bool begin(const char*, bool) { return true; }
  size_t putBytes(const char* k, const void* v, size_t n) {
    simFlash[k].assign((const uint8_t*)v, (const uint8_t*)v + n); return n; }
  size_t getBytesLength(const char* k) { return simFlash.count(k) ? simFlash[k].size() : 0; }
  size_t getBytes(const char* k, void* v, size_t n) {
    if (!simFlash.count(k)) return 0; memcpy(v, simFlash[k].data(), n); return n; }
  void putBool(const char* k, bool v) { putBytes(k, &v, 1); }
  bool getBool(const char* k, bool d) { bool v = d; if (getBytesLength(k)) getBytes(k, &v, 1); return v; }
  void putUChar(const char* k, uint8_t v) { putBytes(k, &v, 1); }
  uint8_t getUChar(const char* k, uint8_t d) { uint8_t v = d; if (getBytesLength(k)) getBytes(k, &v, 1); return v; }
};
