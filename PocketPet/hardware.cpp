// PocketPet hardware: LILYGO T-Display S3 on an EspBoy-style board
// (https://github.com/paixonite/EspBoy): 8 buttons wired to 3.3 V with
// pull-down resistors, a passive buzzer, and a LiPo battery.
#include "hardware.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <sys/time.h>
#include <time.h>

// ---- Pins (EspBoy wiring) ----
static const uint8_t BUTTON_PINS[BTN_COUNT] = {
  1,   // UP
  3,   // DOWN
  43,  // LEFT
  2,   // RIGHT
  17,  // A
  21,  // B
  44,  // START
  18,  // SELECT
};
static const uint8_t PIN_BUZZER = 16;
static const uint8_t PIN_POWER_ON = 15;    // keeps the screen powered on battery
static const uint8_t PIN_BATTERY = 4;      // battery voltage through a 1:2 divider
static const uint8_t PIN_BACKLIGHT = 38;
static const uint8_t BUZZER_CHANNEL = 0;   // the backlight uses LEDC channel 7

// Buttons that can wake the board (they must be RTC pins: GPIO 0-21).
static const uint8_t WAKE_PINS[] = {17, 21, 18, 1, 3, 2};

// ---- Screen: ST7789 170x320 on an 8-bit parallel bus ----
class LGFX_TDisplayS3 : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 panel_;
  lgfx::Bus_Parallel8 bus_;
  lgfx::Light_PWM light_;

 public:
  LGFX_TDisplayS3() {
    {
      auto cfg = bus_.config();
      cfg.freq_write = 20000000;
      cfg.pin_wr = 8;
      cfg.pin_rd = 9;
      cfg.pin_rs = 7;  // D/C
      cfg.pin_d0 = 39;
      cfg.pin_d1 = 40;
      cfg.pin_d2 = 41;
      cfg.pin_d3 = 42;
      cfg.pin_d4 = 45;
      cfg.pin_d5 = 46;
      cfg.pin_d6 = 47;
      cfg.pin_d7 = 48;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    {
      auto cfg = panel_.config();
      cfg.pin_cs = 6;
      cfg.pin_rst = 5;
      cfg.pin_busy = -1;
      cfg.memory_width = 240;
      cfg.memory_height = 320;
      cfg.panel_width = 170;
      cfg.panel_height = 320;
      cfg.offset_x = 35;
      cfg.offset_y = 0;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.bus_shared = false;
      panel_.config(cfg);
    }
    {
      auto cfg = light_.config();
      cfg.pin_bl = PIN_BACKLIGHT;
      cfg.invert = false;
      cfg.freq = 12000;
      cfg.pwm_channel = 7;
      light_.config(cfg);
      panel_.setLight(&light_);
    }
    setPanel(&panel_);
  }
};

static LGFX_TDisplayS3 lcd;

// If the picture is upside down in your case, change 1 to 3.
static const uint8_t SCREEN_ROTATION = 1;

void hwBegin() {
  gpio_hold_dis((gpio_num_t)PIN_POWER_ON);
  gpio_hold_dis((gpio_num_t)PIN_BACKLIGHT);
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  for (uint8_t pin : WAKE_PINS) rtc_gpio_deinit((gpio_num_t)pin);
  for (uint8_t pin : BUTTON_PINS) pinMode(pin, INPUT_PULLDOWN);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttachChannel(PIN_BUZZER, 2000, 10, BUZZER_CHANNEL);
#else
  ledcSetup(BUZZER_CHANNEL, 2000, 10);
  ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);
#endif
  hwTone(0);

  analogReadResolution(12);
  pinMode(PIN_BATTERY, INPUT);

  lcd.init();
  lcd.setRotation(SCREEN_ROTATION);
  lcd.fillScreen(0);
}

bool hwWokeFromSleep() { return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1; }

void hwShow(lgfx::LGFX_Sprite& frame) { frame.pushSprite(&lcd, 0, 0); }

void hwBrightness(uint8_t level) { lcd.setBrightness(level); }

bool hwButtonDown(Button b) { return digitalRead(BUTTON_PINS[b]) == HIGH; }

void hwTone(uint16_t freq) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(PIN_BUZZER, freq);
#else
  ledcWriteTone(BUZZER_CHANNEL, freq);
#endif
}

int hwBatteryPercent() {
  int mv = analogReadMilliVolts(PIN_BATTERY) * 2;
  // A LiPo cell from empty to full, roughly.
  static const int MV[] = {3300, 3600, 3700, 3800, 3900, 4000, 4100, 4200};
  static const int PCT[] = {0, 10, 30, 55, 70, 82, 93, 100};
  if (mv <= MV[0]) return 0;
  for (int i = 1; i < 8; i++) {
    if (mv < MV[i]) return PCT[i - 1] + (PCT[i] - PCT[i - 1]) * (mv - MV[i - 1]) / (MV[i] - MV[i - 1]);
  }
  return 100;
}

// The ESP32's own clock keeps running through deep sleep, but not when the
// power switch is turned off. No time zone is set, so "UTC" here simply means
// local time.
ClockReading hwClock() {
  ClockReading r = {};
  time_t now = time(nullptr);
  if (now < 1704067200) return r;  // before 2024: never set
  struct tm t;
  gmtime_r(&now, &t);
  r.valid = true;
  r.year = t.tm_year + 1900;
  r.month = t.tm_mon + 1;
  r.day = t.tm_mday;
  r.hour = t.tm_hour;
  r.minute = t.tm_min;
  r.second = t.tm_sec;
  return r;
}

void hwSetClock(int year, int month, int day, int hour, int minute) {
  struct tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = minute;
  struct timeval tv = {mktime(&t), 0};  // no TZ set, so mktime works in UTC
  settimeofday(&tv, nullptr);
}

void hwSleep() {
  hwTone(0);
  // Wait for the button that sent us to sleep to be let go, or we'd wake at once.
  uint32_t start = millis();
  bool anyDown = true;
  while (anyDown && millis() - start < 3000) {
    anyDown = false;
    for (uint8_t pin : WAKE_PINS) anyDown |= digitalRead(pin) == HIGH;
    delay(10);
  }

  lcd.setBrightness(0);
  lcd.sleep();
  digitalWrite(PIN_BACKLIGHT, LOW);
  digitalWrite(PIN_POWER_ON, LOW);
  gpio_hold_en((gpio_num_t)PIN_BACKLIGHT);
  gpio_hold_en((gpio_num_t)PIN_POWER_ON);
  gpio_deep_sleep_hold_en();

  uint64_t mask = 0;
  for (uint8_t pin : WAKE_PINS) {
    mask |= 1ULL << pin;
    rtc_gpio_pullup_dis((gpio_num_t)pin);
    rtc_gpio_pulldown_en((gpio_num_t)pin);
  }
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_HIGH);
  esp_deep_sleep_start();
}
