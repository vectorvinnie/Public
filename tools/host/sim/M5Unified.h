// Host stand-in for M5Unified: only what PocketPet.ino touches, with the
// same names and signatures as the real library. Buttons, clock and power
// are driven by the simulation script (sim.cpp).
#pragma once
#include "Arduino.h"
#include <LovyanGFX.hpp>

namespace m5 {
struct rtc_time_t { int8_t hours = -1, minutes = -1, seconds = -1; };
struct rtc_date_t { int16_t year = 2000; int8_t month = 1, date = -1, weekDay = -1; };
struct rtc_datetime_t { rtc_date_t date; rtc_time_t time; };

class Button_Class {
 public:
  bool wasClicked() const { return clicked; }
  bool isPressed() const { return press; }
  bool wasPressed() const { return press && !oldPress; }
  uint32_t lastChange() const { return changedAt; }
  uint32_t getUpdateMsec() const { return simMillis; }
  // simulation side
  void sim(bool down) {
    clicked = press && !down;  // a short press, just released
    oldPress = press;
    if (down != press) changedAt = simMillis;
    press = down;
  }
  bool press = false, oldPress = false, clicked = false;
  uint32_t changedAt = 0;
};

class RTC_Class {
 public:
  bool isEnabled() const { return true; }
  bool getDateTime(rtc_datetime_t* dt) const;
  void setDateTime(const rtc_datetime_t& dt);
};

class Power_Class {
 public:
  enum is_charging_t { is_discharging = 0, is_charging, charge_unknown };
  int32_t getBatteryLevel() { return 64; }
  is_charging_t isCharging() { return charge_unknown; }
  void powerOff();
};

class Speaker_Class {
 public:
  void setVolume(uint8_t) {}
  bool tone(float freq, uint32_t ms) { tones++; (void)freq; (void)ms; return true; }
  int tones = 0;
};

struct Display_Class {
  void setRotation(int) {}
  void setBrightness(uint8_t b) { brightness = b; }
  void sleep() { asleep = true; }
  void wakeup() { asleep = false; }
  uint8_t brightness = 0;
  bool asleep = false;
};

struct M5_Class {
  struct config_t {};
  config_t config() { return {}; }
  void begin(config_t) {}
  void update();
  Button_Class BtnA, BtnB, BtnPWR;
  RTC_Class Rtc;
  Power_Class Power;
  Speaker_Class Speaker;
  Display_Class Display;
};
}  // namespace m5

extern m5::M5_Class M5;

class M5Canvas : public lgfx::LGFX_Sprite {
 public:
  explicit M5Canvas(void*) {}
  // No real screen here: count frames instead of sending them anywhere.
  void pushSprite(int32_t, int32_t) { frames++; }
  int frames = 0;
};
