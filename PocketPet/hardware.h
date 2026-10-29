// PocketPet - the handheld's hardware, behind a few small functions.
//
// hardware.cpp has the real thing (LILYGO T-Display S3 on an EspBoy-style
// board). tools/host/sim has pretend versions for testing on a computer.
#pragma once
#include <stdint.h>

#include <LovyanGFX.hpp>

enum Button : uint8_t {
  BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B, BTN_START, BTN_SELECT, BTN_COUNT
};

// Local date and time from the clock. `valid` is false until it has been set.
struct ClockReading {
  bool valid;
  int year, month, day, hour, minute, second;
};

void hwBegin();
bool hwWokeFromSleep();                    // true when a button woke us from sleep
void hwShow(lgfx::LGFX_Sprite& frame);     // put a finished frame on the screen
void hwBrightness(uint8_t level);          // 0 = backlight off, 255 = brightest
bool hwButtonDown(Button b);               // raw: is it pressed right now?
void hwTone(uint16_t freq);                // 0 = quiet
int hwBatteryPercent();                    // -1 if unknown
ClockReading hwClock();
void hwSetClock(int year, int month, int day, int hour, int minute);
// Screen off and deep sleep until A, B, SELECT, UP, DOWN or RIGHT is pressed.
// The clock keeps running. On the device this never returns (waking up starts
// the program again from setup()).
void hwSleep();
