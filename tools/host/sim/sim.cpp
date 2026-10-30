// Runs the real PocketPet.ino on a computer with pretend buttons, clock,
// screen and sleep, following a scripted play session, and checks what
// happens.
//
//   make -C tools/host sim LGFX=/path/to/LovyanGFX
#include <png.h>
#include <stdio.h>

#include <map>
#include <string>
#include <vector>

#include "Arduino.h"
#include "Preferences.h"
#include "../../../PocketPet/hardware.h"
#include "../../../PocketPet/pet.h"

uint32_t simMillis = 0;
std::map<std::string, std::vector<uint8_t>> simFlash;

// ---- Pretend hardware ----
static bool want[BTN_COUNT];
static bool clockSet = false;
static uint32_t clockBase = 0, clockBaseMillis = 0;  // local seconds since 2000
static int sleeps = 0, frames = 0, tones = 0;
static bool wokeFromSleep = false;
static uint8_t brightness = 0;

void hwBegin() {}
bool hwWokeFromSleep() { return wokeFromSleep; }
void hwShow(lgfx::LGFX_Sprite&) { frames++; }
void hwBrightness(uint8_t level) { brightness = level; }
bool hwButtonDown(Button b) { return want[b]; }
void hwTone(uint16_t freq) { if (freq) tones++; }
int hwBatteryPercent() { return 64; }
void hwSleep() { sleeps++; }

// Pretend WiFi: joins on the second poll; the script can then "upload".
static bool simWifiHasSecrets = true;
static int simWifiPolls = 0;
static WifiUpdateState simWifi = WIFI_OFF;
void hwWifiUpdateStart() { simWifi = simWifiHasSecrets ? WIFI_CONNECTING : WIFI_NOT_SET_UP; simWifiPolls = 0; }
void hwWifiUpdateStop() { simWifi = WIFI_OFF; }
WifiUpdateStatus hwWifiUpdatePoll() {
  if (simWifi == WIFI_CONNECTING && ++simWifiPolls >= 2) simWifi = WIFI_READY;
  WifiUpdateStatus s = {};
  s.state = simWifi;
  strcpy(s.network, "HomeWiFi");
  if (simWifi == WIFI_READY) strcpy(s.address, "pocketpet-3a7f at 192.168.1.23");
  return s;
}

static uint32_t clockNow() { return clockBase + (simMillis - clockBaseMillis) / 1000; }

ClockReading hwClock() {
  ClockReading r = {};
  if (!clockSet) return r;
  uint32_t t = clockNow(), days = t / 86400, secs = t % 86400;
  auto leap = [](int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); };
  int y = 2000;
  while (days >= (uint32_t)(leap(y) ? 366 : 365)) days -= leap(y) ? 366 : 365, y++;
  static const int ml[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int m = 0;
  while (days >= (uint32_t)(ml[m] + (m == 1 && leap(y)))) days -= ml[m] + (m == 1 && leap(y)), m++;
  r.valid = true;
  r.year = y;
  r.month = m + 1;
  r.day = days + 1;
  r.hour = secs / 3600;
  r.minute = secs / 60 % 60;
  r.second = secs % 60;
  return r;
}

void hwSetClock(int year, int month, int day, int hour, int minute) {
  clockBase = fromDateTime(year, month, day, hour, minute, 0);
  clockBaseMillis = simMillis;
  clockSet = true;
}

#include "../../../PocketPet/PocketPet.ino"

// ---- Driving it ----
static void runFor(uint32_t ms) {
  uint32_t end = simMillis + ms;
  while ((int32_t)(end - simMillis) > 0) {
    loop();
    simMillis += 5;  // drawing time
  }
}

static void tap(Button b, int times = 1) {
  for (int i = 0; i < times; i++) {
    want[b] = true;
    runFor(60);
    want[b] = false;
    runFor(100);
  }
}

static void holdFor(Button b, uint32_t ms) {
  want[b] = true;
  runFor(ms);
  want[b] = false;
  runFor(100);
}

// Wake up `hoursAsleep` hours after going to sleep (flash and clock kept).
static void wake(float hoursAsleep, bool coldStart = false) {
  simMillis += (uint32_t)(hoursAsleep * 3600 * 1000);
  if (coldStart) clockSet = false;  // power switch: the clock is lost
  wokeFromSleep = !coldStart;
  setup();
}

static int failures = 0;
#define CHECK(cond)                                                                    \
  do {                                                                                 \
    if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; }        \
  } while (0)

static const char* outDir = nullptr;
static int shotNo = 0;

static void snap(const char* name) {
  if (!outDir) return;
  uiDraw(frame, pet, ui, millis());
  int w = SCREEN_W * 2, h = SCREEN_H * 2;
  std::vector<uint8_t> rgb(w * h * 3);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      auto px = frame.readPixelRGB(x / 2, y / 2);
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
  tap(BTN_A, 3);
  runFor(1600);
  CHECK(ui.screen == SCR_PICK_COLOR);

  tap(BTN_RIGHT, 3);
  tap(BTN_LEFT);
  CHECK(ui.pickColor == 2);
  tap(BTN_A);
  CHECK(ui.screen == SCR_PICK_EARS);
  tap(BTN_B);  // back to colors and forward again
  CHECK(ui.screen == SCR_PICK_COLOR);
  tap(BTN_A);
  tap(BTN_RIGHT);
  CHECK(ui.pickEars == EARS_BUNNY);
  tap(BTN_A);
  CHECK(ui.screen == SCR_PICK_NAME);

  // Spell LULU arcade-style: Up from blank gives A; Down from blank gives Z.
  tap(BTN_UP, 12);                 // L
  tap(BTN_RIGHT);
  tap(BTN_DOWN, 6);                // Z, Y, X, W, V, U
  tap(BTN_RIGHT);
  tap(BTN_UP, 12);                 // L
  tap(BTN_RIGHT);
  tap(BTN_DOWN, 6);                // U
  tap(BTN_RIGHT);
  tap(BTN_UP, 3);                  // C... then erase it with B
  tap(BTN_B);
  char name[PET_NAME_MAX + 1];
  typedName(name);
  CHECK(strcmp(name, "LULU") == 0);
  tap(BTN_RIGHT);                  // can't move past an empty slot
  CHECK(ui.nameCursor == 4);
  snap("name");
  tap(BTN_A);
  CHECK(ui.screen == SCR_SET_CLOCK);

  // The clock was never set, so it starts at 12:00. Make it 16:05.
  tap(BTN_UP, 4);
  tap(BTN_RIGHT);
  tap(BTN_UP, 6);
  tap(BTN_DOWN);
  tap(BTN_A);
  CHECK(ui.screen == SCR_HOME);
  CHECK(pet.hatched);
  CHECK(strcmp(pet.name, "LULU") == 0);
  CHECK(pet.color == 2 && pet.ears == EARS_BUNNY);
  CHECK(ui.clock.valid && ui.clock.hour == 16 && ui.minute == 5);
  runFor(300);
  snap("hello");
  runFor(2500);

  // --- Play the guessing game with the D-pad ---
  tap(BTN_RIGHT);
  CHECK(ui.cursor == ACT_PLAY);
  tap(BTN_A);
  CHECK(ui.screen == SCR_GAME);
  for (int round = 1; round <= GAME_ROUNDS; round++) {
    CHECK(ui.gameRound == round);
    tap(round % 2 ? BTN_LEFT : BTN_RIGHT);
    CHECK(ui.gameGuess >= 0);
    if (round == 2) snap("game");
    runFor(1400);
  }
  runFor(3200);
  CHECK(ui.screen == SCR_HOME);
  runFor(1600);

  // --- Feed a meal, then a treat when too full for a meal ---
  pet.food = 40;
  tap(BTN_LEFT);
  CHECK(ui.cursor == ACT_FEED);
  tap(BTN_A);
  CHECK(ui.screen == SCR_FEED);
  tap(BTN_A);
  CHECK(ui.screen == SCR_HOME && ui.anim == ANIM_MEAL);
  CHECK(pet.food > 70);
  runFor(500);
  snap("eating");
  runFor(1200);
  pet.food = 95;
  tap(BTN_A); tap(BTN_A);
  CHECK(ui.anim == ANIM_NO);
  CHECK(strcmp(ui.bubble, "I'm full!") == 0);
  runFor(1500);
  float fun = pet.fun;
  tap(BTN_A); tap(BTN_RIGHT); tap(BTN_A);  // Treat
  CHECK(ui.anim == ANIM_TREAT && pet.fun > fun);
  runFor(1600);
  tap(BTN_A); tap(BTN_B);                  // open the menu, change our mind
  CHECK(ui.screen == SCR_HOME && ui.cursor == ACT_FEED);

  // --- Poop appears and gets cleaned ---
  pet.poopClock = POOP_EVERY_HOURS * 3600 - 2;
  runFor(3000);
  CHECK(pet.poops == 1);
  tap(BTN_RIGHT, 2);
  CHECK(ui.cursor == ACT_CLEAN);
  tap(BTN_A);
  CHECK(pet.poops == 0 && ui.anim == ANIM_CLEAN);
  runFor(1800);

  // --- START opens settings; school mode on and off ---
  tap(BTN_START);
  CHECK(ui.screen == SCR_SETTINGS && ui.cursor == SET_SCHOOL);
  tap(BTN_A);
  CHECK(pet.schoolMode);
  tap(BTN_DOWN, 2);
  CHECK(ui.cursor == SET_BRIGHTNESS);
  tap(BTN_RIGHT);
  CHECK(ui.brightness == 2);
  tap(BTN_LEFT);
  snap("settings");
  tap(BTN_B);
  CHECK(ui.screen == SCR_HOME);
  float food = pet.food;
  runFor(60000);
  CHECK(pet.food == food);  // frozen
  snap("school");
  tap(BTN_START);
  tap(BTN_A);
  CHECK(!pet.schoolMode);
  tap(BTN_START);
  CHECK(ui.screen == SCR_HOME);

  // --- Left alone, it dims and then goes to sleep by itself ---
  runFor((DIM_AFTER_S + 2) * 1000);
  CHECK(dimmed);
  runFor(AUTO_OFF_MINUTES[ui.autoOff] * 60000);
  CHECK(sleeps == 1);
  CHECK(ui.screen == SCR_BYE);

  // --- A wakes it six hours later (now about 22:30, past bedtime) ---
  food = pet.food;
  want[BTN_A] = true;
  wake(6);
  runFor(200);
  want[BTN_A] = false;
  runFor(200);
  CHECK(ui.screen == SCR_HOME);
  CHECK(ui.cursor == ACT_FEED);       // the wake-up press wasn't an action
  CHECK(pet.food < food);             // time passed while asleep...
  CHECK(pet.food >= OFFLINE_FLOOR);   // ...gently
  CHECK(pet.asleep);                  // bedtime happened meanwhile
  snap("asleep");

  // Waking it at night: it stays up a couple of minutes, then sleeps again.
  tap(BTN_LEFT, ACT_COUNT - ACT_LIGHT);
  CHECK(ui.cursor == ACT_LIGHT);
  tap(BTN_A);
  CHECK(!pet.asleep);
  runFor(60000);
  tap(BTN_RIGHT, ACT_COUNT);  // keep the screen on
  runFor((WOKEN_AT_NIGHT_SECONDS - 60 + 5) * 1000UL);
  CHECK(pet.asleep);

  // --- SELECT puts it to sleep; back after a long weekend ---
  tap(BTN_SELECT);
  CHECK(sleeps == 1 && ui.screen == SCR_BYE);  // says bye first...
  runFor(1600);
  CHECK(sleeps == 2);                          // ...then sleeps
  wake(60);
  CHECK(!pet.asleep);
  CHECK(pet.food >= OFFLINE_FLOOR - 0.01f && pet.fun >= OFFLINE_FLOOR - 0.01f);
  CHECK(strcmp(ui.bubble, "I missed you!") == 0);
  runFor(400);
  snap("missed-you");
  runFor(2000);

  // --- Power switch off and on: the clock is lost but the pet isn't ---
  tap(BTN_SELECT);
  runFor(1600);
  wake(1, true);
  CHECK(ui.screen == SCR_HOME && pet.hatched);
  CHECK(!ui.clock.valid);
  runFor(2000);
  tap(BTN_START);
  tap(BTN_UP, SET_COUNT - SET_CLOCK);
  CHECK(ui.cursor == SET_CLOCK);
  tap(BTN_A);
  CHECK(ui.screen == SCR_SET_CLOCK);
  tap(BTN_UP, 3);  // 15:00
  tap(BTN_A);
  CHECK(ui.screen == SCR_SETTINGS && ui.clock.valid && ui.clock.hour == 15);

  // --- Update over WiFi: connects, waits, saves the pet when an upload starts ---
  tap(BTN_DOWN);
  CHECK(ui.cursor == SET_WIFI);
  tap(BTN_A);
  CHECK(ui.screen == SCR_WIFI);
  runFor(200);
  CHECK(ui.wifi.state == WIFI_READY);
  snap("wifi-ready");
  runFor(5 * 60000);                     // waiting doesn't send it to sleep...
  CHECK(ui.screen == SCR_WIFI);
  pet.hearts = 777;
  simFlash.erase("pet");
  appUpdateProgress(0);                  // ...and an upload saves the pet first
  appUpdateProgress(40);
  CHECK(ui.wifiUpdating && ui.wifiProgress == 40 && simFlash.count("pet"));
  snap("wifi-updating");
  ui.wifiUpdating = false;
  tap(BTN_B);
  CHECK(ui.screen == SCR_SETTINGS && simWifi == WIFI_OFF);

  // --- Start over: needs a fresh 3-second hold on A ---
  tap(BTN_DOWN);
  CHECK(ui.cursor == SET_NEW_EGG);
  holdFor(BTN_A, 1500);  // the press that opens the screen doesn't count
  CHECK(ui.screen == SCR_NEW_EGG);
  holdFor(BTN_A, 1500);  // too short
  CHECK(ui.screen == SCR_NEW_EGG && pet.hatched);
  want[BTN_A] = true; runFor(1500);
  snap("new-egg");
  runFor(1700); want[BTN_A] = false; runFor(100);
  CHECK(ui.screen == SCR_EGG);
  CHECK(!pet.hatched);

  printf("%d frames, %d notes, %d sleeps\n", frames, tones, sleeps);
  if (failures) { printf("%d check(s) failed\n", failures); return 1; }
  printf("simulated play session passed\n");
  return 0;
}
