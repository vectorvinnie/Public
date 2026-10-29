// Browser stand-ins for the few Arduino calls PocketPet.ino uses.
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define JS_IMPORT(name) __attribute__((import_module("env"), import_name(#name)))
extern "C" {
JS_IMPORT(js_now_ms) double js_now_ms();
}

inline uint32_t millis() { return (uint32_t)js_now_ms(); }
inline uint32_t micros() { return (uint32_t)(js_now_ms() * 1000); }
inline void delay(uint32_t) {}  // the browser paces frames itself
inline long random(long n) { return rand() % n; }
inline void randomSeed(unsigned long s) { srand(s); }
