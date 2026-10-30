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

// ---- Updating the game over WiFi ----
// While on, the handheld shows up in the Arduino IDE as a network port
// (Tools > Port), so a new version can be uploaded without opening the case.
enum WifiUpdateState : uint8_t {
  WIFI_OFF,
  WIFI_NOT_SET_UP,   // no network name in wifi_secrets.h (or no WiFi here)
  WIFI_CONNECTING,
  WIFI_READY,        // waiting for an upload
  WIFI_FAILED,       // couldn't join the network
  WIFI_UNAVAILABLE,  // this build has no WiFi (the browser version)
};
struct WifiUpdateStatus {
  WifiUpdateState state;
  char network[33];  // WiFi name
  char address[48];  // how it appears in the Arduino IDE, e.g. "pocketpet-3a7f at 192.168.1.23"
};
void hwWifiUpdateStart();
void hwWifiUpdateStop();
WifiUpdateStatus hwWifiUpdatePoll();  // call often while on; an upload runs inside this call
// The hardware layer calls this while an upload is coming in (0..100). The
// main loop is paused meanwhile, so this is where the screen gets redrawn.
// The handheld restarts by itself when the upload finishes.
void appUpdateProgress(uint8_t percent);

// Screen off and deep sleep until A, B, SELECT, UP, DOWN or RIGHT is pressed.
// The clock keeps running. On the device this never returns (waking up starts
// the program again from setup()).
void hwSleep();
