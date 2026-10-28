// Host stand-ins for the few Arduino calls PocketPet.ino uses.
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <algorithm>

extern uint32_t simMillis;
inline uint32_t millis() { return simMillis; }
inline void delay(uint32_t ms) { simMillis += ms; }
inline long random(long n) { return rand() % n; }
inline void randomSeed(unsigned long s) { srand(s); }
inline uint32_t esp_random() { return 1234; }
using std::min;

class String {
 public:
  String(const char* s) : s_(s) {}
  String(const std::string& s) : s_(s) {}
  String substring(int a, int b) const { return String(s_.substr(a, b - a)); }
  const char* c_str() const { return s_.c_str(); }
 private:
  std::string s_;
};
