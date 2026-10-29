// PocketPet - a gentle virtual pet for a Game Boy-style handheld.
//
// Controls:
//   D-pad   move around
//   A       do it / choose
//   B       back
//   START   settings
//   SELECT  sleep (switch off); A wakes it up again
//
// The pet never dies. If it's ignored it gets hungry, bored or sleepy, and
// it always cheers up again once someone looks after it.

#include <Preferences.h>

#include "config.h"
#include "hardware.h"
#include "pet.h"
#include "ui.h"

// Small types live up here, before any function, because the Arduino IDE
// adds function prototypes above the first function it finds.
struct Note { uint16_t freq; uint16_t ms; };
struct Now { bool valid; uint8_t hour, minute; uint32_t epoch; };
struct ButtonState { bool down; bool pressed; bool ignore; uint32_t since; uint32_t repeatAt; };

static lgfx::LGFX_Sprite frame;
static Preferences prefs;

static PetState pet;
static UiState ui;
static ButtonState buttons[BTN_COUNT];

static bool firstRun = false;     // going through egg -> color -> ears -> name -> clock
static bool dimmed = false;
static bool sleeping = false;     // only ever true on a computer (simulation, web)
static uint32_t sleepAtMs = 0;    // when "Bye bye!" ends and the real sleep starts
static uint32_t lastInputMs = 0;
static uint32_t lastTickMs = 0;
static uint32_t lastSaveMs = 0;
static uint32_t lastNeedBeepMs = 0;
static uint32_t lastBatteryMs = 0;
static uint32_t newEggPressAt = 0; // the A press that opened "New egg?" doesn't count

static const uint8_t BRIGHTNESS[3] = {50, 120, 220};
static const uint8_t DIM_BRIGHTNESS = 15;

static const char* const RANDOM_NAMES[] = {
  "MOCHI", "BOBA", "PIP", "LULU", "NOODLE", "BUN", "SUKI", "POPPY", "BEAN", "TOFU", "KIWI", "MILO"};
static const int RANDOM_NAME_COUNT = sizeof(RANDOM_NAMES) / sizeof(RANDOM_NAMES[0]);

// ---------------------------------------------------------------------------
// Sounds: tiny note sequences played without blocking (idea from TamaFi's
// retro sound engine).
// ---------------------------------------------------------------------------
static const Note SND_CLICK[] = {{2000, 20}, {0, 0}};
static const Note SND_YUM[] = {{600, 60}, {900, 60}, {1200, 60}, {1500, 90}, {0, 0}};
static const Note SND_HAPPY[] = {{1047, 80}, {1319, 80}, {1568, 120}, {0, 0}};
static const Note SND_NO[] = {{500, 90}, {350, 140}, {0, 0}};
static const Note SND_POOP[] = {{300, 80}, {200, 120}, {0, 0}};
static const Note SND_CALL[] = {{1568, 90}, {1, 60}, {1568, 90}, {1, 60}, {2093, 160}, {0, 0}};
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
  if (n.ms == 0) { sndPlaying = nullptr; hwTone(0); return; }
  hwTone(n.freq > 1 ? n.freq : 0);  // freq 1 = a short rest
  sndNextMs = now + n.ms;
  sndPlaying++;
}

// ---------------------------------------------------------------------------
// Clock
// ---------------------------------------------------------------------------
static Now readClock() {
  Now n = {false, 0, 0, 0};
  ClockReading r = hwClock();
  if (!r.valid) return n;
  n.valid = true;
  n.hour = r.hour;
  n.minute = r.minute;
  n.epoch = fromDateTime(r.year, r.month, r.day, r.hour, r.minute, r.second);
  return n;
}

static void setClock(uint8_t hour, uint8_t minute) {
  ClockReading r = hwClock();
  if (!r.valid) {
    // Only the time of day matters to the pet; start the calendar at the
    // build date so "time since last seen" works from here on.
    static const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char* d = __DATE__;  // "Mmm dd yyyy"
    r.year = atoi(d + 7);
    r.day = atoi(d + 4);
    r.month = 1;
    for (int m = 0; m < 12; m++)
      if (strncmp(months + m * 3, d, 3) == 0) r.month = m + 1;
  }
  hwSetClock(r.year, r.month, r.day, hour, minute);
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
// Buttons
// ---------------------------------------------------------------------------
static void readButtons(uint32_t now) {
  for (int i = 0; i < BTN_COUNT; i++) {
    ButtonState& b = buttons[i];
    bool down = hwButtonDown((Button)i);
    b.pressed = false;
    if (down == b.down || now - b.since < 25) continue;  // 25 ms debounce
    b.down = down;
    b.since = now;
    if (!down) { b.ignore = false; continue; }
    if (!b.ignore) { b.pressed = true; b.repeatAt = now + 400; }
  }
}

static bool pressed(Button b) { return buttons[b].pressed; }
static bool held(Button b) { return buttons[b].down && !buttons[b].ignore; }

// For the D-pad in lists: a press, then repeats while held.
static bool repeating(Button b, uint32_t now) {
  ButtonState& s = buttons[b];
  if (s.pressed) return true;
  if (s.down && !s.ignore && now >= s.repeatAt) { s.repeatAt = now + 120; return true; }
  return false;
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
  hwBrightness(dim ? DIM_BRIGHTNESS : BRIGHTNESS[ui.brightness]);
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

static void openClock() {
  Now n = readClock();
  ui.clockHour = n.valid ? n.hour : 12;
  ui.clockMinute = n.valid ? n.minute : 0;
  ui.clockField = 0;
  go(SCR_SET_CLOCK);
}

// The name as typed: slots trimmed of spaces at both ends.
static void typedName(char* out) {
  int start = 0, end = PET_NAME_MAX;
  while (start < end && ui.nameBuf[start] == ' ') start++;
  while (end > start && ui.nameBuf[end - 1] == ' ') end--;
  memcpy(out, ui.nameBuf + start, end - start);
  out[end - start] = '\0';
}

static void finishSetup() {
  char name[PET_NAME_MAX + 1];
  typedName(name);
  if (!name[0]) strcpy(name, RANDOM_NAMES[random(RANDOM_NAME_COUNT)]);
  petHatch(pet, name, ui.pickColor, ui.pickEars);
  firstRun = false;
  saveAll();
  go(SCR_HOME);
  char hi[24];
  snprintf(hi, sizeof(hi), "Hi! I'm %s", pet.name);
  say(hi, 2500);
  startAnim(ANIM_HELLO);
  play(SND_HAPPY);
}

// Says "Bye bye!" for a moment; loop() then puts the handheld to sleep.
static void goToSleep() {
  saveAll();
  go(SCR_BYE);
  play(SND_BYE);
  sleepAtMs = millis() + 1500;
}

// ---------------------------------------------------------------------------
// What each screen does with the buttons
// ---------------------------------------------------------------------------
static void handleHome(uint32_t now) {
  if (pressed(BTN_SELECT)) { goToSleep(); return; }
  if (pressed(BTN_START)) { go(SCR_SETTINGS, SET_SCHOOL); play(SND_CLICK); return; }
  if (repeating(BTN_RIGHT, now)) { ui.cursor = (ui.cursor + 1) % ACT_COUNT; play(SND_CLICK); }
  if (repeating(BTN_LEFT, now)) { ui.cursor = (ui.cursor + ACT_COUNT - 1) % ACT_COUNT; play(SND_CLICK); }
  if (!pressed(BTN_A)) return;

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

static void handleFeed() {
  if (pressed(BTN_B)) { go(SCR_HOME, ACT_FEED); play(SND_CLICK); return; }
  if (pressed(BTN_LEFT) || pressed(BTN_RIGHT)) { ui.cursor = 1 - ui.cursor; play(SND_CLICK); }
  if (!pressed(BTN_A)) return;
  uint8_t choice = ui.cursor;
  go(SCR_HOME, ACT_FEED);
  ActionResult r = choice == FEED_MEAL ? petFeedMeal(pet) : petFeedTreat(pet);
  if (r != RESULT_OK) { explain(r); return; }
  startAnim(choice == FEED_MEAL ? ANIM_MEAL : ANIM_TREAT);
  play(SND_YUM);
}

static void handleGame(uint32_t now) {
  if (pressed(BTN_B)) { go(SCR_HOME, ACT_PLAY); return; }
  if (ui.gameGuess < 0) {
    if (!pressed(BTN_LEFT) && !pressed(BTN_RIGHT)) return;
    ui.gameGuess = pressed(BTN_RIGHT) ? 1 : 0;
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
  } else if (shown > 3000 || (shown > 1200 && pressed(BTN_A))) {
    go(SCR_HOME, ACT_PLAY);
    startAnim(ANIM_CUDDLE);
  }
}

static void changeSetting(int dir) {
  switch (ui.cursor) {
    case SET_SCHOOL: pet.schoolMode = !pet.schoolMode; break;
    case SET_SOUND: ui.soundOn = !ui.soundOn; if (!ui.soundOn) hwTone(0); break;
    case SET_BRIGHTNESS: ui.brightness = (ui.brightness + 3 + dir) % 3; applyBrightness(false); break;
    case SET_AUTO_OFF: ui.autoOff = (ui.autoOff + 3 + dir) % 3; break;
    case SET_BEDTIME: pet.bedtimeOn = !pet.bedtimeOn; break;
    default: return;
  }
  play(SND_CLICK);
  saveAll();
}

static void handleSettings(uint32_t now) {
  if (pressed(BTN_B) || pressed(BTN_START)) { go(SCR_HOME, ACT_SETTINGS); saveAll(); play(SND_CLICK); return; }
  if (repeating(BTN_DOWN, now)) { ui.cursor = (ui.cursor + 1) % SET_COUNT; play(SND_CLICK); }
  if (repeating(BTN_UP, now)) { ui.cursor = (ui.cursor + SET_COUNT - 1) % SET_COUNT; play(SND_CLICK); }
  if (pressed(BTN_RIGHT)) changeSetting(1);
  if (pressed(BTN_LEFT)) changeSetting(-1);
  if (!pressed(BTN_A)) return;
  switch (ui.cursor) {
    case SET_CLOCK: play(SND_CLICK); openClock(); break;
    case SET_NEW_EGG:
      play(SND_CLICK);
      ui.holdMs = 0;
      newEggPressAt = buttons[BTN_A].since;
      go(SCR_NEW_EGG);
      break;
    case SET_BACK: play(SND_CLICK); go(SCR_HOME, ACT_SETTINGS); break;
    default: changeSetting(1); break;
  }
}

static const char NAME_LETTERS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static void handleName(uint32_t now) {
  char& slot = ui.nameBuf[ui.nameCursor];
  int letter = strchr(NAME_LETTERS, slot) ? strchr(NAME_LETTERS, slot) - NAME_LETTERS : 0;
  const int count = sizeof(NAME_LETTERS) - 1;

  if (repeating(BTN_UP, now)) { slot = NAME_LETTERS[letter % (count - 1) + 1]; play(SND_CLICK); }
  if (repeating(BTN_DOWN, now)) { slot = NAME_LETTERS[letter <= 1 ? count - 1 : letter - 1]; play(SND_CLICK); }
  if (pressed(BTN_RIGHT) && ui.nameCursor < PET_NAME_MAX - 1 && slot != ' ') {
    ui.nameCursor++;
    play(SND_CLICK);
  }
  if (pressed(BTN_LEFT) && ui.nameCursor > 0) { ui.nameCursor--; play(SND_CLICK); }
  if (pressed(BTN_B)) {
    play(SND_CLICK);
    if (slot != ' ') slot = ' ';
    else if (ui.nameCursor > 0) ui.nameBuf[--ui.nameCursor] = ' ';
    else go(SCR_PICK_EARS);
  }
  if (pressed(BTN_SELECT)) {
    const char* n = RANDOM_NAMES[random(RANDOM_NAME_COUNT)];
    memset(ui.nameBuf, ' ', PET_NAME_MAX);
    memcpy(ui.nameBuf, n, strlen(n));
    ui.nameCursor = strlen(n) - 1;
    play(SND_HAPPY);
  }
  if (pressed(BTN_A) || pressed(BTN_START)) { play(SND_CLICK); openClock(); }
}

static void handleClock(uint32_t now) {
  int dir = repeating(BTN_UP, now) ? 1 : repeating(BTN_DOWN, now) ? -1 : 0;
  if (dir) {
    if (ui.clockField == 0) ui.clockHour = (ui.clockHour + 24 + dir) % 24;
    else ui.clockMinute = (ui.clockMinute + 60 + dir) % 60;
  }
  if (pressed(BTN_LEFT) || pressed(BTN_RIGHT)) { ui.clockField = 1 - ui.clockField; play(SND_CLICK); }
  if (pressed(BTN_B)) {
    play(SND_CLICK);
    if (firstRun) go(SCR_PICK_NAME);
    else go(SCR_SETTINGS, SET_CLOCK);
    return;
  }
  if (!pressed(BTN_A) && !pressed(BTN_START)) return;
  play(SND_CLICK);
  setClock(ui.clockHour, ui.clockMinute);
  refreshClock();
  if (firstRun) { finishSetup(); return; }
  saveAll();  // also restamps lastSeen, so changing the clock isn't "time away"
  go(SCR_SETTINGS, SET_CLOCK);
}

static void handleButtons(uint32_t now) {
  readButtons(now);
  bool any = false;
  for (auto& b : buttons) any |= b.pressed || (b.down && !b.ignore);
  if (any) {
    lastInputMs = now;
    if (dimmed) { dimmed = false; applyBrightness(false); }
  }

  // Let animations finish before taking new actions.
  if (ui.anim != ANIM_NONE && ui.screen != SCR_EGG) return;

  switch (ui.screen) {
    case SCR_EGG:
      if (!pressed(BTN_A) || ui.anim == ANIM_HATCH) break;
      ui.eggTaps++;
      play(ui.eggTaps >= 3 ? SND_HATCH : SND_CLICK);
      if (ui.eggTaps >= 3) startAnim(ANIM_HATCH);
      break;
    case SCR_PICK_COLOR:
      if (pressed(BTN_RIGHT)) { ui.pickColor = (ui.pickColor + 1) % PET_COLOR_COUNT; play(SND_CLICK); }
      if (pressed(BTN_LEFT)) { ui.pickColor = (ui.pickColor + PET_COLOR_COUNT - 1) % PET_COLOR_COUNT; play(SND_CLICK); }
      if (pressed(BTN_A)) { go(SCR_PICK_EARS); play(SND_CLICK); }
      break;
    case SCR_PICK_EARS:
      if (pressed(BTN_RIGHT)) { ui.pickEars = (ui.pickEars + 1) % EARS_COUNT; play(SND_CLICK); }
      if (pressed(BTN_LEFT)) { ui.pickEars = (ui.pickEars + EARS_COUNT - 1) % EARS_COUNT; play(SND_CLICK); }
      if (pressed(BTN_B)) { go(SCR_PICK_COLOR); play(SND_CLICK); }
      if (pressed(BTN_A)) {
        go(SCR_PICK_NAME);
        memset(ui.nameBuf, ' ', PET_NAME_MAX);
        ui.nameBuf[PET_NAME_MAX] = '\0';
        ui.nameCursor = 0;
        play(SND_CLICK);
      }
      break;
    case SCR_PICK_NAME: handleName(now); break;
    case SCR_SET_CLOCK: handleClock(now); break;
    case SCR_HOME: handleHome(now); break;
    case SCR_FEED: handleFeed(); break;
    case SCR_GAME: handleGame(now); break;
    case SCR_INFO:
      if (pressed(BTN_A) || pressed(BTN_B) || pressed(BTN_START)) { go(SCR_HOME, ACT_INFO); play(SND_CLICK); }
      break;
    case SCR_SETTINGS: handleSettings(now); break;
    case SCR_NEW_EGG:
      if (pressed(BTN_B)) go(SCR_SETTINGS, SET_NEW_EGG);
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
    ui.batteryPercent = hwBatteryPercent();
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
  hwBegin();
  randomSeed(micros());

  frame.setColorDepth(16);
  frame.createSprite(SCREEN_W, SCREEN_H);

  prefs.begin("pocketpet", false);
  memset(&ui, 0, sizeof(ui));
  memset(buttons, 0, sizeof(buttons));
  loadAll();
  applyBrightness(false);
  ui.gameGuess = -1;
  ui.batteryPercent = hwBatteryPercent();
  refreshClock();

  uint32_t now = millis();
  lastInputMs = lastTickMs = lastSaveMs = lastBatteryMs = now;
  lastNeedBeepMs = now;  // no reminder right at switch-on
  dimmed = false;
  sleeping = false;
  // The button that woke us (or is held at power-on) doesn't count as a press.
  for (int i = 0; i < BTN_COUNT; i++) {
    buttons[i].down = buttons[i].ignore = hwButtonDown((Button)i);
    buttons[i].since = now;
  }

  if (!pet.hatched) {
    firstRun = true;
    go(SCR_EGG);
    return;
  }
  firstRun = false;

  // Catch up on the time it was asleep. (After the power switch was turned
  // off the clock is unknown, so nothing is caught up; set it in Settings.)
  Now n = readClock();
  uint32_t away = (n.valid && pet.lastSeen && n.epoch > pet.lastSeen) ? n.epoch - pet.lastSeen : 0;
  if (n.valid) petCatchUp(pet, n.epoch);
  saveAll();

  go(SCR_HOME);
  if (!pet.asleep && !pet.schoolMode) {
    say(away > 3 * 3600 ? "I missed you!" : "Hi!");
    startAnim(ANIM_HELLO);
    play(SND_HAPPY);
  }
}

void loop() {
  if (sleeping) return;
  uint32_t now = millis();

  if (ui.screen == SCR_BYE) {
    soundUpdate(now);
    if ((int32_t)(now - sleepAtMs) >= 0) {
      hwSleep();
      sleeping = true;  // (only reached on a computer)
      return;
    }
    uiDraw(frame, pet, ui, now);
    hwShow(frame);
    delay(10);
    return;
  }

  soundUpdate(now);
  handleButtons(now);

  if (ui.anim != ANIM_NONE && now - ui.animStart > animLength(ui.anim)) {
    if (ui.anim == ANIM_HATCH) go(SCR_PICK_COLOR);
    ui.anim = ANIM_NONE;
  }

  if (ui.screen == SCR_NEW_EGG) {
    bool fresh = held(BTN_A) && buttons[BTN_A].since != newEggPressAt;
    uint32_t heldMs = now - buttons[BTN_A].since;
    ui.holdMs = fresh ? (uint16_t)(heldMs < 3000 ? heldMs : 3000) : 0;
    if (ui.holdMs >= 3000) {
      petNewEgg(pet);
      saveAll();
      ui.eggTaps = 0;
      firstRun = true;
      go(SCR_EGG);
    }
  }

  tick(now);

  uint32_t idle = now - lastInputMs;
  if (idle > AUTO_OFF_MINUTES[ui.autoOff] * 60000UL) { goToSleep(); return; }
  if (!dimmed && idle > DIM_AFTER_S * 1000UL) { dimmed = true; applyBrightness(true); }

  uiDraw(frame, pet, ui, now);
  hwShow(frame);
  delay(10);
}
