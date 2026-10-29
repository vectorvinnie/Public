// Host stand-ins for the few Arduino calls PocketPet.ino uses.
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern uint32_t simMillis;
inline uint32_t millis() { return simMillis; }
inline uint32_t micros() { return simMillis * 1000; }
inline void delay(uint32_t ms) { simMillis += ms; }
inline long random(long n) { return rand() % n; }
inline void randomSeed(unsigned long s) { srand(s); }
