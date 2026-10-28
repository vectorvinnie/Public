// PocketPet - the pet itself: its saved state and all the rules.
//
// Plain C++ with no hardware calls, so the rules can be tested on a computer
// (see tools/host).
#pragma once
#include <stdint.h>

#define PET_SAVE_VERSION 1
#define PET_NAME_MAX 8

enum PetStage : uint8_t { STAGE_EGG, STAGE_BABY, STAGE_KID, STAGE_GROWN };

enum PetMood : uint8_t {
  MOOD_HAPPY,
  MOOD_OK,
  MOOD_HUNGRY,
  MOOD_BORED,
  MOOD_TIRED,
  MOOD_ASLEEP,
  MOOD_PAUSED,
};

// What the pet would like most right now (shown as a thought bubble).
enum PetNeed : uint8_t { NEED_NONE, NEED_FOOD, NEED_FUN, NEED_SLEEP, NEED_CLEAN };

enum EarStyle : uint8_t { EARS_CAT, EARS_BUNNY, EARS_BEAR, EARS_NONE, EARS_COUNT };

// Everything here is saved to flash as one blob, so keep it plain data.
struct PetState {
  uint16_t version;
  bool hatched;
  char name[PET_NAME_MAX + 1];
  uint8_t color;      // index into PET_COLORS (ui.cpp)
  uint8_t ears;       // EarStyle

  float food;         // 0..100, 100 = full
  float fun;          // 0..100
  float energy;       // 0..100
  uint8_t poops;
  float poopClock;    // awake seconds since the last poop

  bool asleep;
  bool schoolMode;    // everything frozen
  bool bedtimeOn;     // auto sleep / wake by the clock

  uint32_t ageSeconds;      // pet time lived (paused in school mode)
  uint32_t hearts;          // times it was looked after
  uint32_t cuddleCooldown;  // seconds until a cuddle counts again
  uint32_t stayAwakeFor;    // seconds left of being kept up at night
  uint32_t lastSeen;        // clock time of the last save, 0 = unknown
};

// The clock as the rules see it. `valid` is false until someone sets the time.
struct PetClock {
  bool valid;
  uint8_t hour;   // 0..23, local time
};

void petNewEgg(PetState& p);
void petHatch(PetState& p, const char* name, uint8_t color, uint8_t ears);

// Move time forward. `offline` is used when catching up after the device was
// off (slower and never below OFFLINE_FLOOR). Returns true if a poop appeared.
bool petAdvance(PetState& p, uint32_t seconds, PetClock clock, bool offline);

// Catch up the time the device was switched off, in small steps so bedtime
// still happens at the right moment. `nowEpoch` and `p.lastSeen` are local
// seconds from fromDateTime(); nothing happens if either is unknown.
void petCatchUp(PetState& p, uint32_t nowEpoch);

PetStage petStage(const PetState& p);
PetMood petMood(const PetState& p);
PetNeed petNeed(const PetState& p);
bool isNightHour(uint8_t hour);

// ---- Things the kids can do ----
enum ActionResult : uint8_t {
  RESULT_OK,
  RESULT_FULL,        // not hungry
  RESULT_TOO_TIRED,   // too sleepy to play
  RESULT_ASLEEP,      // it's asleep; wake it first
  RESULT_NOTHING,     // nothing to clean
  RESULT_PAUSED,      // school mode is on
  RESULT_NOT_SLEEPY,  // too awake for a nap
};

ActionResult petFeedMeal(PetState& p);
ActionResult petFeedTreat(PetState& p);
ActionResult petCanPlay(const PetState& p);
void petGameRound(PetState& p, bool won);
ActionResult petClean(PetState& p);
ActionResult petCuddle(PetState& p);
// Lights off / on. At night, waking keeps the pet up for a little while.
ActionResult petToggleSleep(PetState& p, PetClock clock);

// Seconds since 2000-01-01 for a local date and time (no time zones).
uint32_t fromDateTime(int year, int month, int day, int hour, int minute, int second);
