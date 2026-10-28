// Runs the real PocketPet.ino on a computer with pretend buttons, clock and
// power, following a scripted play session, and checks what happens.
//
//   make -C tools/host sim M5GFX=/path/to/M5GFX
#include <png.h>
#include <stdio.h>

#include <map>
#include <string>
#include <vector>

#include "M5Unified.h"
#include "Preferences.h"

uint32_t simMillis = 0;
std::map<std::string, std::vector<uint8_t>> simFlash;
m5::M5_Class M5;

// Pretend real-time clock: local seconds since 2000, ticking with millis().
static bool rtcSet = false;
static uint32_t rtcBase = 0, rtcBaseMillis = 0;
static int powerOffs = 0;

#include "../../../PocketPet/PocketPet.ino"

static uint32_t rtcNow() { return rtcBase + (simMillis - rtcBaseMillis) / 1000; }

bool m5::RTC_Class::getDateTime(rtc_datetime_t* dt) const {
  if (!rtcSet) { *dt = rtc_datetime_t(); dt->date.year = 2000; dt->time = {0, 0, 0}; return true; }
  uint32_t t = rtcNow();
  // Days since 2000-01-01 back to a date (only needs to round-trip with fromDateTime).
  uint32_t days = t / 86400, secs = t % 86400;
  int y = 2000;
  while (true) {
    int len = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
    if (days < (uint32_t)len) break;
    days -= len;
    y++;
  }
  static const int ml[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int m = 0;
  while (true) {
    int len = ml[m] + (m == 1 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)));
    if (days < (uint32_t)len) break;
    days -= len;
    m++;
  }
  dt->date.year = y;
  dt->date.month = m + 1;
  dt->date.date = days + 1;
  dt->time.hours = secs / 3600;
  dt->time.minutes = secs / 60 % 60;
  dt->time.seconds = secs % 60;
  return true;
}

void m5::RTC_Class::setDateTime(const rtc_datetime_t& dt) {
  rtcBase = fromDateTime(dt.date.year, dt.date.month, dt.date.date, dt.time.hours,
                         dt.time.minutes, dt.time.seconds);
  rtcBaseMillis = simMillis;
  rtcSet = true;
}

void m5::Power_Class::powerOff() { powerOffs++; }

// ---------------------------------------------------------------------------
static bool wantA = false, wantB = false, wantPwr = false;

void m5::M5_Class::update() {
  BtnA.sim(wantA);
  BtnB.sim(wantB);
  BtnPWR.sim(wantPwr);
}

static void runFor(uint32_t ms) {
  uint32_t end = simMillis + ms;
  while ((int32_t)(end - simMillis) > 0) {
    loop();
    simMillis += 5;  // drawing time
  }
}

static void tap(bool& btn, int times = 1) {
  for (int i = 0; i < times; i++) {
    btn = true;
    runFor(60);
    btn = false;
    runFor(120);
  }
}

static void holdFor(bool& btn, uint32_t ms) {
  btn = true;
  runFor(ms);
  btn = false;
  runFor(100);
}

// Switch on again after `hoursOff` hours, keeping flash and the clock.
static void reboot(float hoursOff) {
  simMillis += (uint32_t)(hoursOff * 3600 * 1000);
  screenOff = false;
  dimmed = false;
  firstRun = false;
  setup();
}

static int failures = 0;
#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } \
  } while (0)

static const char* outDir = nullptr;
static int shotNo = 0;

static void snap(const char* name) {
  if (!outDir) return;
  canvas.fillScreen(0);
  uiDraw(canvas, pet, ui, millis());
  int w = SCREEN_W * 2, h = SCREEN_H * 2;
  std::vector<uint8_t> rgb(w * h * 3);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      auto px = canvas.readPixelRGB(x / 2, y / 2);
      uint8_t* o = &rgb[(y * w + x) * 3];
      o[0] = px.R8(); o[1] = px.G8(); o[2] = px.B8();
    }
  char path[512];
  snprintf(path, sizeof(path), "%s/sim-%02d-%s.png", outDir, ++shotNo, name);
  FILE* f = fopen(path, "wb");
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  png_infop info = png_create_info_struct(png);
  png_init_io(png, f);
  png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_RGB, 0, 0, 0);
  png_write_info(png, info);
  for (int y = 0; y < h; y++) png_write_row(png, &rgb[y * w * 3]);
  png_write_end(png, nullptr);
  png_destroy_write_struct(&png, &info);
  fclose(f);
}

int main(int argc, char** argv) {
  if (argc > 1) outDir = argv[1];
  simMillis = 1000;

  // --- First switch-on: an egg ---
  setup();
  runFor(500);
  CHECK(ui.screen == SCR_EGG);
  CHECK(!pet.hatched);
  snap("egg");
  tap(wantA, 3);
  runFor(1600);
  CHECK(ui.screen == SCR_PICK_COLOR);

  tap(wantB, 2);
  CHECK(ui.pickColor == 2);
  tap(wantA);
  CHECK(ui.screen == SCR_PICK_EARS);
  tap(wantB);
  CHECK(ui.pickEars == EARS_BUNNY);
  tap(wantA);
  CHECK(ui.screen == SCR_PICK_NAME);

  // Spell LULU: L=11, U=20; PWR goes back a letter.
  tap(wantB, 11); tap(wantA);
  tap(wantB, 9); tap(wantA);
  tap(wantPwr, 9); tap(wantA);
  tap(wantB, 9); tap(wantA);
  CHECK(strcmp(ui.nameBuf, "LULU") == 0);
  snap("name");
  tap(wantB, NAME_DONE - 20);
  tap(wantA);
  CHECK(ui.screen == SCR_SET_CLOCK);

  // The clock was never set, so it starts at 12:00. Make it 16:05.
  tap(wantB, 4); tap(wantA);
  tap(wantB, 5); tap(wantA);
  CHECK(ui.screen == SCR_HOME);
  CHECK(pet.hatched);
  CHECK(strcmp(pet.name, "LULU") == 0);
  CHECK(pet.color == 2 && pet.ears == EARS_BUNNY);
  CHECK(ui.clock.valid && ui.clock.hour == 16 && ui.minute == 5);
  runFor(300);
  snap("hello");
  runFor(2500);

  // --- Play the guessing game ---
  tap(wantB);
  CHECK(ui.cursor == ACT_PLAY);
  tap(wantA);
  CHECK(ui.screen == SCR_GAME);
  for (int round = 1; round <= GAME_ROUNDS; round++) {
    CHECK(ui.gameRound == round);
    tap(round % 2 ? wantA : wantB);
    CHECK(ui.gameGuess >= 0);
    if (round == 2) snap("game");
    runFor(1400);
  }
  runFor(3200);
  CHECK(ui.screen == SCR_HOME);
  runFor(1600);

  // --- Feed a meal ---
  pet.food = 40;
  tap(wantB, ACT_COUNT - 1);  // wrap around to Feed
  CHECK(ui.cursor == ACT_FEED);
  tap(wantA);
  CHECK(ui.screen == SCR_FEED);
  tap(wantA);
  CHECK(ui.screen == SCR_HOME && ui.anim == ANIM_MEAL);
  CHECK(pet.food > 70);
  runFor(500);
  snap("eating");
  runFor(1200);
  // Too full for another meal.
  pet.food = 95;
  tap(wantA); tap(wantA);
  CHECK(ui.anim == ANIM_NO);
  CHECK(strcmp(ui.bubble, "I'm full!") == 0);
  runFor(1500);

  // --- Poop appears and gets cleaned ---
  pet.poopClock = POOP_EVERY_HOURS * 3600 - 2;
  runFor(3000);
  CHECK(pet.poops == 1);
  tap(wantB, ACT_CLEAN - ACT_FEED);
  CHECK(ui.cursor == ACT_CLEAN);
  tap(wantA);
  CHECK(pet.poops == 0 && ui.anim == ANIM_CLEAN);
  runFor(1800);

  // --- Settings: school mode on and off ---
  tap(wantB, ACT_SETTINGS - ACT_CLEAN);
  tap(wantA);
  CHECK(ui.screen == SCR_SETTINGS);
  tap(wantA);
  CHECK(pet.schoolMode);
  snap("settings");
  wantPwr = true; runFor(60); wantPwr = false; runFor(200);
  CHECK(ui.screen == SCR_HOME);
  float food = pet.food;
  runFor(60000);
  CHECK(pet.food == food);  // frozen
  snap("school");
  tap(wantA);  // back into settings (cursor is still on Settings)
  tap(wantA);
  CHECK(!pet.schoolMode);
  wantPwr = true; runFor(60); wantPwr = false; runFor(200);

  // --- Left alone, it dims and then switches itself off ---
  runFor((DIM_AFTER_S + 2) * 1000);
  CHECK(dimmed);
  runFor(AUTO_OFF_MINUTES[ui.autoOff] * 60000);
  CHECK(powerOffs == 1);

  // --- Switched back on six hours later (now 22:xx, past bedtime) ---
  food = pet.food;
  reboot(6);
  CHECK(ui.screen == SCR_HOME);
  CHECK(pet.food < food);             // time passed while off...
  CHECK(pet.food >= OFFLINE_FLOOR);   // ...gently
  CHECK(pet.asleep);                  // bedtime happened while off
  runFor(500);
  snap("asleep");

  // Waking it at night: it stays up a couple of minutes, then sleeps again.
  tap(wantB, ACT_LIGHT);  // switching on starts at Feed
  CHECK(ui.cursor == ACT_LIGHT);
  tap(wantA);
  CHECK(!pet.asleep);
  runFor((WOKEN_AT_NIGHT_SECONDS + 5) * 1000UL - 30000);
  // (pressing buttons keeps the screen on but doesn't keep it awake)
  tap(wantB, ACT_COUNT);
  runFor(31000);
  CHECK(pet.asleep);

  // --- Next morning after a long weekend away ---
  reboot(60);
  CHECK(!pet.asleep);
  CHECK(pet.food >= OFFLINE_FLOOR - 0.01f && pet.fun >= OFFLINE_FLOOR - 0.01f);
  CHECK(strcmp(ui.bubble, "I missed you!") == 0);
  runFor(400);
  snap("missed-you");
  runFor(2000);

  // --- Start over: needs a fresh 3-second hold on A ---
  tap(wantB, ACT_SETTINGS - ui.cursor);
  tap(wantA);
  tap(wantB, SET_NEW_EGG);
  CHECK(ui.cursor == SET_NEW_EGG);
  holdFor(wantA, 1500);  // the press that opens the screen doesn't count
  CHECK(ui.screen == SCR_NEW_EGG);
  holdFor(wantA, 1500);  // too short
  CHECK(ui.screen == SCR_NEW_EGG && pet.hatched);
  wantA = true; runFor(1500);
  snap("new-egg");
  runFor(1700); wantA = false; runFor(100);
  CHECK(ui.screen == SCR_EGG);
  CHECK(!pet.hatched);

  printf("%d sounds played, %d power-offs\n", M5.Speaker.tones, powerOffs);
  if (failures) { printf("%d check(s) failed\n", failures); return 1; }
  printf("simulated play session passed\n");
  return 0;
}
