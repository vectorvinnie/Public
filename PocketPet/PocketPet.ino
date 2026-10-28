// PocketPet - a gentle virtual pet for the M5StickC Plus2.
//
// Buttons (holding the stick upright, screen facing you):
//   A   = the big button under the screen: do / choose
//   B   = the button on the right side: next
//   PWR = the button on the left side: back, or switch off from the home screen
//
// The pet never dies. If it's ignored it gets hungry, bored or sleepy, and
// it always cheers up again once someone looks after it.

#include <M5Unified.h>
#include <Preferences.h>

#include "config.h"
#include "pet.h"
#include "ui.h"

static M5Canvas canvas(&M5.Display);
static Preferences prefs;

static PetState pet;
static UiState ui;

static bool firstRun = false;     // going through egg -> color -> name -> clock
static bool screenOff = false;    // only on USB power, when switching off didn't work
static bool dimmed = false;
static uint32_t lastInputMs = 0;
static uint32_t lastTickMs = 0;
static uint32_t lastSaveMs = 0;
static uint32_t lastNeedBeepMs = 0;
static uint32_t lastBatteryMs = 0;
static uint32_t repeatAtMs = 0;   // hold-to-repeat on B (and PWR in the name picker)
static uint32_t newEggPressAt = 0; // the A press that opened "New egg?" doesn't count

static const uint8_t BRIGHTNESS[3] = {40, 100, 180};
static const uint8_t DIM_BRIGHTNESS = 12;

static const char* const RANDOM_NAMES[] = {
  "MOCHI", "BOBA", "PIP", "LULU", "NOODLE", "BUN", "SUKI", "POPPY", "BEAN", "TOFU", "KIWI", "MILO"};

// ---------------------------------------------------------------------------
// Sounds: tiny note sequences played without blocking (idea from TamaFi's
// retro sound engine).
// ---------------------------------------------------------------------------
struct Note { uint16_t freq; uint16_t ms; };
// The clock as read from the RTC. (Declared up here, before any function,
// because the Arduino IDE adds function prototypes above the first one.)
struct Now { bool valid; uint8_t hour, minute; uint32_t epoch; };

static const Note SND_CLICK[] = {{2000, 25}, {0, 0}};
static const Note SND_YUM[] = {{600, 60}, {900, 60}, {1200, 60}, {1500, 90}, {0, 0}};
static const Note SND_HAPPY[] = {{1047, 80}, {1319, 80}, {1568, 120}, {0, 0}};
static const Note SND_NO[] = {{500, 90}, {350, 140}, {0, 0}};
static const Note SND_POOP[] = {{300, 80}, {200, 120}, {0, 0}};
static const Note SND_CALL[] = {{1568, 90}, {0, 60}, {1568, 90}, {0, 60}, {2093, 160}, {0, 0}};
static const Note SND_HATCH[] = {{523, 80}, {659, 80}, {784, 80}, {1047, 80}, {1319, 200}, {0, 0}};
static const Note SND_NIGHT[] = {{784, 120}, {659, 120}, {523, 200}, {0, 0}};
static const Note SND_BYE[] = {{1047, 100}, {784, 100}, {523, 180}, {0, 0}};

static const Note* sndPlaying = nullptr;
static uint32_t sndNextMs = 0;

static void play(const Note* s) {
  if (!ui.soundOn) return;
  sndPlaying = s;
  sndNextMs = 0;
}

static void soundUpdate(uint32_t now) {
  if (!sndPlaying || now < sndNextMs) return;
  const Note& n = *sndPlaying;
  if (n.ms == 0) { sndPlaying = nullptr; return; }
  if (n.freq) M5.Speaker.tone(n.freq, n.ms);
  sndNextMs = now + n.ms;
  sndPlaying++;
}

// ---------------------------------------------------------------------------
// Clock
// ---------------------------------------------------------------------------

static Now readClock() {
  Now n = {false, 0, 0, 0};
  m5::rtc_datetime_t dt;
  if (!M5.Rtc.isEnabled() || !M5.Rtc.getDateTime(&dt)) return n;
  // A clock that lost power restarts in 2000; treat anything that old as unset.
  if (dt.date.year < 2024 || dt.date.year > 2099) return n;
  n.valid = true;
  n.hour = dt.time.hours;
  n.minute = dt.time.minutes;
  n.epoch = fromDateTime(dt.date.year, dt.date.month, dt.date.date, dt.time.hours,
                         dt.time.minutes, dt.time.seconds);
  return n;
}

static void setClock(uint8_t hour, uint8_t minute) {
  m5::rtc_datetime_t dt;
  bool haveDate = M5.Rtc.getDateTime(&dt) && dt.date.year >= 2024 && dt.date.year <= 2099;
  if (!haveDate) {
    // Only the time of day matters to the pet; start the calendar at the
    // build date so "time since last seen" works from here on.
    static const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char* d = __DATE__;  // "Mmm dd yyyy"
    dt.date.year = atoi(d + 7);
    dt.date.month = (strstr(months, String(d).substring(0, 3).c_str()) - months) / 3 + 1;
    dt.date.date = atoi(d + 4);
  }
  dt.time.hours = hour;
  dt.time.minutes = minute;
  dt.time.seconds = 0;
  M5.Rtc.setDateTime(dt);
}

static void refreshClock() {
  Now n = readClock();
  ui.clock.valid = n.valid;
  ui.clock.hour = n.hour;
  ui.minute = n.minute;
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------
static void saveAll() {
  Now n = readClock();
  pet.lastSeen = n.valid ? n.epoch : 0;
  prefs.putBytes("pet", &pet, sizeof(pet));
  prefs.putBool("sound", ui.soundOn);
  prefs.putUChar("bright", ui.brightness);
  prefs.putUChar("autooff", ui.autoOff);
  lastSaveMs = millis();
}

static void loadAll() {
  ui.soundOn = prefs.getBool("sound", true);
  ui.brightness = prefs.getUChar("bright", 1) % 3;
  ui.autoOff = prefs.getUChar("autooff", 1) % 3;
  if (prefs.getBytesLength("pet") == sizeof(pet)) {
    prefs.getBytes("pet", &pet, sizeof(pet));
    if (pet.version == PET_SAVE_VERSION) return;
  }
  petNewEgg(pet);
}

// ---------------------------------------------------------------------------
// Screen helpers
// ---------------------------------------------------------------------------
static void go(Screen s, uint8_t cursor = 0) {
  ui.screen = s;
  ui.cursor = cursor;
}

static void startAnim(Anim a) {
  ui.anim = a;
  ui.animStart = millis();
}

static void say(const char* text, uint32_t ms = 1800) {
  strncpy(ui.bubble, text, sizeof(ui.bubble) - 1);
  ui.bubble[sizeof(ui.bubble) - 1] = '\0';
  ui.bubbleUntil = millis() + ms;
}

static uint32_t animLength(Anim a) {
  switch (a) {
    case ANIM_HELLO: return 1500;
    case ANIM_MEAL:
    case ANIM_TREAT: return 1500;
    case ANIM_CLEAN: return 1700;
    case ANIM_CUDDLE: return 1500;
    case ANIM_NO: return 1200;
    case ANIM_HATCH: return 1400;
    default: return 0;
  }
}

static void applyBrightness(bool dim) {
  M5.Display.setBrightness(dim ? DIM_BRIGHTNESS : BRIGHTNESS[ui.brightness]);
}

// Tell the kids why an action didn't happen.
static void explain(ActionResult r) {
  switch (r) {
    case RESULT_FULL: say("I'm full!"); break;
    case RESULT_TOO_TIRED: say("Too sleepy..."); break;
    case RESULT_ASLEEP: say("Zzz..."); break;
    case RESULT_NOTHING: say("All clean!"); break;
    case RESULT_PAUSED: say("School mode!"); break;
    case RESULT_NOT_SLEEPY: say("Not sleepy!"); break;
    default: return;
  }
  if (r != RESULT_ASLEEP) startAnim(ANIM_NO);
  play(SND_NO);
}

static void finishSetup() {
  if (ui.nameBuf[0] == '\0') strcpy(ui.nameBuf, RANDOM_NAMES[random(12)]);
  petHatch(pet, ui.nameBuf, ui.pickColor, ui.pickEars);
  firstRun = false;
  saveAll();
  go(SCR_HOME);
  char hi[24];
  snprintf(hi, sizeof(hi), "Hi! I'm %s", pet.name);
  say(hi, 2500);
  startAnim(ANIM_HELLO);
  play(SND_HAPPY);
}

static void switchOff() {
  saveAll();
  go(SCR_BYE);
  play(SND_BYE);
  uint32_t until = millis() + 1500;
  while (millis() < until) {
    soundUpdate(millis());
    uiDraw(canvas, pet, ui, millis());
    canvas.pushSprite(0, 0);
    delay(30);
  }
  Screen back = firstRun ? SCR_EGG : SCR_HOME;
  M5.Power.powerOff();

  // Still running: we're on USB power and can't switch off, so just turn the
  // screen off until a button is pressed.
  delay(300);
  M5.Display.sleep();
  M5.Display.setBrightness(0);
  screenOff = true;
  if (back == SCR_EGG) ui.eggTaps = 0;
  go(back);
}

// ---------------------------------------------------------------------------
// Buttons
// ---------------------------------------------------------------------------
// B (and PWR on the name picker) repeat while held, for fast scrolling.
static bool repeating(m5::Button_Class& btn, uint32_t now) {
  if (btn.wasPressed()) { repeatAtMs = now + 450; return true; }
  if (btn.isPressed() && now >= repeatAtMs) { repeatAtMs = now + 110; return true; }
  return false;
}

static void handleHome(bool a, bool b, bool pwr) {
  if (pwr) { switchOff(); return; }
  if (b) { ui.cursor = (ui.cursor + 1) % ACT_COUNT; play(SND_CLICK); }
  if (!a) return;

  Now n = readClock();
  PetClock clock = {n.valid, n.hour};
  ActionResult r;
  switch (ui.cursor) {
    case ACT_FEED:
      if (pet.schoolMode || pet.asleep) { explain(pet.schoolMode ? RESULT_PAUSED : RESULT_ASLEEP); break; }
      go(SCR_FEED, FEED_MEAL);
      play(SND_CLICK);
      break;
    case ACT_PLAY:
      r = petCanPlay(pet);
      if (r != RESULT_OK) { explain(r); break; }
      go(SCR_GAME);
      ui.gameRound = 1;
      ui.gameScore = 0;
      ui.gameGuess = -1;
      play(SND_CLICK);
      break;
    case ACT_CLEAN:
      r = petClean(pet);
      if (r != RESULT_OK) { explain(r); break; }
      startAnim(ANIM_CLEAN);
      play(SND_HAPPY);
      break;
    case ACT_LIGHT:
      r = petToggleSleep(pet, clock);
      if (r != RESULT_OK) { explain(r); break; }
      if (pet.asleep) {
        play(SND_NIGHT);
        say("Night night!", 1500);
      } else {
        startAnim(ANIM_HELLO);
        say(pet.stayAwakeFor ? "Yawn... hi!" : "Good morning!");
        play(SND_HAPPY);
      }
      break;
    case ACT_CUDDLE:
      r = petCuddle(pet);
      if (r != RESULT_OK) { explain(r); break; }
      startAnim(ANIM_CUDDLE);
      play(SND_HAPPY);
      break;
    case ACT_INFO:
      go(SCR_INFO);
      play(SND_CLICK);
      break;
    case ACT_SETTINGS:
      go(SCR_SETTINGS, SET_SCHOOL);
      play(SND_CLICK);
      break;
  }
}

static void handleFeed(bool a, bool b, bool pwr) {
  if (pwr) { go(SCR_HOME, ACT_FEED); return; }
  if (b) { ui.cursor = (ui.cursor + 1) % FEED_COUNT; play(SND_CLICK); }
  if (!a) return;
  uint8_t choice = ui.cursor;
  go(SCR_HOME, ACT_FEED);
  if (choice == FEED_BACK) return;
  ActionResult r = choice == FEED_MEAL ? petFeedMeal(pet) : petFeedTreat(pet);
  if (r != RESULT_OK) { explain(r); return; }
  startAnim(choice == FEED_MEAL ? ANIM_MEAL : ANIM_TREAT);
  play(SND_YUM);
}

static void handleGame(bool a, bool b, bool pwr, uint32_t now) {
  if (pwr) { go(SCR_HOME, ACT_PLAY); return; }
  if (ui.gameGuess < 0) {
    if (!a && !b) return;
    ui.gameGuess = b ? 1 : 0;
    ui.gameLook = random(2);
    ui.gameShownAt = now;
    bool won = ui.gameGuess == ui.gameLook;
    if (won) ui.gameScore++;
    petGameRound(pet, won);
    play(won ? SND_HAPPY : SND_NO);
    return;
  }
  // Answer is showing: move on after a moment.
  uint32_t shown = now - ui.gameShownAt;
  if (ui.gameRound < GAME_ROUNDS) {
    if (shown > 1300) { ui.gameRound++; ui.gameGuess = -1; }
  } else if (shown > 3000 || (shown > 1200 && (a || b))) {
    go(SCR_HOME, ACT_PLAY);
    startAnim(ANIM_CUDDLE);
  }
}

static void handleSettings(bool a, bool b, bool pwr) {
  if (pwr) { go(SCR_HOME, ACT_SETTINGS); saveAll(); return; }
  if (b) { ui.cursor = (ui.cursor + 1) % SET_COUNT; play(SND_CLICK); }
  if (!a) return;
  play(SND_CLICK);
  switch (ui.cursor) {
    case SET_SCHOOL: pet.schoolMode = !pet.schoolMode; break;
    case SET_SOUND: ui.soundOn = !ui.soundOn; break;
    case SET_BRIGHTNESS: ui.brightness = (ui.brightness + 1) % 3; applyBrightness(false); break;
    case SET_AUTO_OFF: ui.autoOff = (ui.autoOff + 1) % 3; break;
    case SET_BEDTIME: pet.bedtimeOn = !pet.bedtimeOn; break;
    case SET_CLOCK: {
      Now n = readClock();
      ui.clockHour = n.valid ? n.hour : 12;
      ui.clockMinute = n.valid ? n.minute : 0;
      ui.clockField = 0;
      go(SCR_SET_CLOCK);
      return;
    }
    case SET_NEW_EGG:
      ui.holdMs = 0;
      newEggPressAt = M5.BtnA.lastChange();
      go(SCR_NEW_EGG);
      return;
    case SET_BACK: go(SCR_HOME, ACT_SETTINGS); break;
  }
  saveAll();
}

static void handleName(bool a, bool next, bool prev) {
  if (next) ui.letter = (ui.letter + 1) % NAME_ENTRIES;
  if (prev) ui.letter = (ui.letter + NAME_ENTRIES - 1) % NAME_ENTRIES;
  if (next || prev) play(SND_CLICK);
  if (!a) return;
  size_t len = strlen(ui.nameBuf);
  play(SND_CLICK);
  if (ui.letter < 26) {
    if (len < PET_NAME_MAX) { ui.nameBuf[len] = 'A' + ui.letter; ui.nameBuf[len + 1] = '\0'; }
  } else if (ui.letter == NAME_DEL) {
    if (len) ui.nameBuf[len - 1] = '\0';
  } else if (ui.letter == NAME_RANDOM) {
    strcpy(ui.nameBuf, RANDOM_NAMES[random(12)]);
  } else {  // NAME_DONE
    if (ui.nameBuf[0] == '\0') strcpy(ui.nameBuf, RANDOM_NAMES[random(12)]);
    Now n = readClock();
    ui.clockHour = n.valid ? n.hour : 12;
    ui.clockMinute = n.valid ? n.minute : 0;
    ui.clockField = 0;
    go(SCR_SET_CLOCK);
  }
}

static void handleClock(bool a, bool plus) {
  if (plus) {
    if (ui.clockField == 0) ui.clockHour = (ui.clockHour + 1) % 24;
    else ui.clockMinute = (ui.clockMinute + 1) % 60;
  }
  if (!a) return;
  play(SND_CLICK);
  if (ui.clockField == 0) { ui.clockField = 1; return; }
  setClock(ui.clockHour, ui.clockMinute);
  refreshClock();
  if (firstRun) { finishSetup(); return; }
  saveAll();  // also restamps lastSeen, so changing the clock isn't "time away"
  go(SCR_SETTINGS, SET_CLOCK);
}

static void handleButtons(uint32_t now) {
  bool a = M5.BtnA.wasPressed();
  bool b = M5.BtnB.wasPressed();
  bool pwr = M5.BtnPWR.wasClicked();
  bool anyDown = M5.BtnA.isPressed() || M5.BtnB.isPressed() || M5.BtnPWR.isPressed();
  if (a || b || pwr || anyDown) {
    lastInputMs = now;
    if (screenOff) {  // wake up and swallow this press
      screenOff = false;
      dimmed = false;
      M5.Display.wakeup();
      applyBrightness(false);
      return;
    }
    if (dimmed) { dimmed = false; applyBrightness(false); }
  }

  // Let animations finish before taking new actions.
  bool busy = ui.anim != ANIM_NONE;
  if (busy && ui.screen != SCR_EGG) return;

  switch (ui.screen) {
    case SCR_EGG:
      if (!a || ui.anim == ANIM_HATCH) break;
      ui.eggTaps++;
      play(ui.eggTaps >= 3 ? SND_HATCH : SND_CLICK);
      if (ui.eggTaps >= 3) startAnim(ANIM_HATCH);
      break;
    case SCR_PICK_COLOR:
      if (b) { ui.pickColor = (ui.pickColor + 1) % PET_COLOR_COUNT; play(SND_CLICK); }
      if (a) { go(SCR_PICK_EARS); play(SND_CLICK); }
      break;
    case SCR_PICK_EARS:
      if (b) { ui.pickEars = (ui.pickEars + 1) % EARS_COUNT; play(SND_CLICK); }
      if (a) { go(SCR_PICK_NAME); ui.letter = 0; ui.nameBuf[0] = '\0'; play(SND_CLICK); }
      break;
    case SCR_PICK_NAME:
      handleName(a, repeating(M5.BtnB, now), repeating(M5.BtnPWR, now));
      break;
    case SCR_SET_CLOCK:
      handleClock(a, repeating(M5.BtnB, now));
      break;
    case SCR_HOME: handleHome(a, b, pwr); break;
    case SCR_FEED: handleFeed(a, b, pwr); break;
    case SCR_GAME: handleGame(a, b, pwr, now); break;
    case SCR_INFO:
      if (a || b || pwr) { go(SCR_HOME, ACT_INFO); play(SND_CLICK); }
      break;
    case SCR_SETTINGS: handleSettings(a, b, pwr); break;
    case SCR_NEW_EGG:
      if (b || pwr) { go(SCR_SETTINGS, SET_NEW_EGG); break; }
      break;  // holding A is handled in loop()
    case SCR_BYE: break;
  }
}

// ---------------------------------------------------------------------------
// Time passing
// ---------------------------------------------------------------------------
static void tick(uint32_t now) {
  uint32_t seconds = (now - lastTickMs) / 1000;
  if (seconds == 0) return;
  lastTickMs += seconds * 1000;

  refreshClock();
  if (now - lastBatteryMs > 5000) {
    lastBatteryMs = now;
    ui.batteryPercent = M5.Power.getBatteryLevel();
    ui.charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  }
  if (firstRun) return;

  bool wasAsleep = pet.asleep;
  if (petAdvance(pet, seconds, ui.clock, false)) play(SND_POOP);
  if (wasAsleep && !pet.asleep && ui.screen == SCR_HOME) {
    startAnim(ANIM_HELLO);
    say("Good morning!");
  }

  // A little tune when the pet needs something (not too often).
  if (petNeed(pet) != NEED_NONE && now - lastNeedBeepMs > NEED_BEEP_EVERY_S * 1000UL) {
    lastNeedBeepMs = now;
    play(SND_CALL);
  }

  if (now - lastSaveMs > SAVE_EVERY_S * 1000UL) saveAll();
}

// ---------------------------------------------------------------------------
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(0);  // portrait, big button under the screen
  M5.Speaker.setVolume(140);
  randomSeed(esp_random());

  canvas.setColorDepth(16);
  canvas.createSprite(SCREEN_W, SCREEN_H);

  prefs.begin("pocketpet", false);
  memset(&ui, 0, sizeof(ui));
  loadAll();
  applyBrightness(false);
  ui.gameGuess = -1;
  ui.batteryPercent = M5.Power.getBatteryLevel();
  refreshClock();

  uint32_t now = millis();
  lastInputMs = lastTickMs = lastSaveMs = lastBatteryMs = now;
  lastNeedBeepMs = now;  // no reminder right at switch-on

  if (!pet.hatched) {
    firstRun = true;
    go(SCR_EGG);
    return;
  }

  // Catch up on the time the stick was switched off.
  Now n = readClock();
  uint32_t away = (n.valid && pet.lastSeen && n.epoch > pet.lastSeen) ? n.epoch - pet.lastSeen : 0;
  if (n.valid) petCatchUp(pet, n.epoch);
  saveAll();

  go(SCR_HOME);
  if (!n.valid) {  // the clock lost power: ask again
    ui.clockHour = 12;
    ui.clockMinute = 0;
    ui.clockField = 0;
    go(SCR_SET_CLOCK);
  } else if (!pet.asleep && !pet.schoolMode) {
    say(away > 3 * 3600 ? "I missed you!" : "Hi!");
    startAnim(ANIM_HELLO);
    play(SND_HAPPY);
  }
}

void loop() {
  M5.update();
  uint32_t now = millis();

  soundUpdate(now);
  handleButtons(now);

  if (ui.anim != ANIM_NONE && now - ui.animStart > animLength(ui.anim)) {
    if (ui.anim == ANIM_HATCH) go(SCR_PICK_COLOR);
    ui.anim = ANIM_NONE;
  }

  if (ui.screen == SCR_NEW_EGG) {
    bool fresh = M5.BtnA.isPressed() && M5.BtnA.lastChange() != newEggPressAt;
    uint32_t held = M5.BtnA.getUpdateMsec() - M5.BtnA.lastChange();
    ui.holdMs = fresh ? (uint16_t)(held < 3000 ? held : 3000) : 0;
    if (ui.holdMs >= 3000) {
      petNewEgg(pet);
      saveAll();
      memset(ui.nameBuf, 0, sizeof(ui.nameBuf));
      ui.eggTaps = 0;
      firstRun = true;
      go(SCR_EGG);
    }
  }

  tick(now);

  if (screenOff) { delay(50); return; }

  uint32_t idle = now - lastInputMs;
  if (idle > AUTO_OFF_MINUTES[ui.autoOff] * 60000UL) {
    switchOff();
    lastInputMs = millis();
    return;
  }
  if (!dimmed && idle > DIM_AFTER_S * 1000UL) { dimmed = true; applyBrightness(true); }

  uiDraw(canvas, pet, ui, now);
  canvas.pushSprite(0, 0);
  delay(15);
}
