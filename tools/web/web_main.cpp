// PocketPet in a web browser: the real PocketPet.ino, with the handheld's
// hardware (screen, buttons, buzzer, clock, sleep) provided by play.html.
#include "Arduino.h"
#include "Preferences.h"
#include "../../PocketPet/hardware.h"
#include "../../PocketPet/pet.h"

extern "C" {
JS_IMPORT(js_show) void js_show(const void* pixels, int w, int h);
JS_IMPORT(js_tone) void js_tone(int freq);
JS_IMPORT(js_sleep) void js_sleep();
JS_IMPORT(js_local_seconds) double js_local_seconds();  // local time, seconds since 2000
}

static bool keys[BTN_COUNT];
static bool woke = false;
static int32_t clockOffset = 0;  // seconds the player moved the clock by

void hwBegin() {
  if (js_prefs_get("clockofs", &clockOffset, sizeof(clockOffset)) != sizeof(clockOffset)) clockOffset = 0;
}
bool hwWokeFromSleep() { return woke; }
void hwShow(lgfx::LGFX_Sprite& frame) { js_show(frame.getBuffer(), frame.width(), frame.height()); }
void hwBrightness(uint8_t) {}
bool hwButtonDown(Button b) { return keys[b]; }
void hwTone(uint16_t freq) { js_tone(freq); }
int hwBatteryPercent() { return 100; }
void hwSleep() { js_sleep(); }

ClockReading hwClock() {
  ClockReading r = {};
  uint32_t t = (uint32_t)(js_local_seconds() + clockOffset), days = t / 86400, secs = t % 86400;
  auto leap = [](int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); };
  int y = 2000;
  while (days >= (uint32_t)(leap(y) ? 366 : 365)) days -= leap(y) ? 366 : 365, y++;
  static const int ml[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int m = 0;
  while (days >= (uint32_t)(ml[m] + (m == 1 && leap(y)))) days -= ml[m] + (m == 1 && leap(y)), m++;
  r.valid = true;  // a browser always knows the time
  r.year = y;
  r.month = m + 1;
  r.day = days + 1;
  r.hour = secs / 3600;
  r.minute = secs / 60 % 60;
  r.second = secs % 60;
  return r;
}

void hwSetClock(int year, int month, int day, int hour, int minute) {
  // Keep the date the browser has; only move the time of day.
  ClockReading now = hwClock();
  uint32_t want = fromDateTime(now.year, now.month, now.day, hour, minute, 0);
  uint32_t have = fromDateTime(now.year, now.month, now.day, now.hour, now.minute, 0);
  clockOffset += (int32_t)(want - have);
  js_prefs_set("clockofs", &clockOffset, sizeof(clockOffset));
  (void)year; (void)month; (void)day;
}

#include "../../PocketPet/PocketPet.ino"

#define EXPORT(name) extern "C" __attribute__((export_name(#name)))
EXPORT(pp_start) void pp_start() { setup(); }
EXPORT(pp_loop) void pp_loop() { loop(); }
EXPORT(pp_button) void pp_button(int b, int down) { if (b >= 0 && b < BTN_COUNT) keys[b] = down; }
EXPORT(pp_wake) void pp_wake() { woke = true; setup(); }
