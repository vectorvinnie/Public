// PocketPet - everything that is drawn on the screen.
//
// Drawing only: these functions read the pet and the UI state and paint a
// frame. They never change anything, so the same code can also render
// screenshots on a computer (see tools/host).
#pragma once
#include <stdint.h>
#include "pet.h"

#if defined(ARDUINO)
#include <M5GFX.h>
#else
#include <LovyanGFX.hpp>
#endif

#define SCREEN_W 135
#define SCREEN_H 240

#define PET_COLOR_COUNT 6
extern const char* const PET_COLOR_NAMES[PET_COLOR_COUNT];
extern const char* const EAR_NAMES[EARS_COUNT];

enum Screen : uint8_t {
  SCR_EGG,          // first run: tap the egg to hatch it
  SCR_PICK_COLOR,
  SCR_PICK_EARS,
  SCR_PICK_NAME,
  SCR_SET_CLOCK,
  SCR_HOME,
  SCR_FEED,         // home scene with the Meal / Treat bar
  SCR_GAME,         // "Which way?" guessing game
  SCR_INFO,
  SCR_SETTINGS,
  SCR_NEW_EGG,      // hold-to-confirm start over
  SCR_BYE,          // shown just before switching off
};

enum HomeAction : uint8_t {
  ACT_FEED, ACT_PLAY, ACT_CLEAN, ACT_LIGHT, ACT_CUDDLE, ACT_INFO, ACT_SETTINGS,
  ACT_COUNT
};

enum FeedChoice : uint8_t { FEED_MEAL, FEED_TREAT, FEED_BACK, FEED_COUNT };

enum SettingsItem : uint8_t {
  SET_SCHOOL, SET_SOUND, SET_BRIGHTNESS, SET_AUTO_OFF, SET_BEDTIME, SET_CLOCK,
  SET_NEW_EGG, SET_BACK, SET_COUNT
};

// Short animations played on the home screen after an action.
enum Anim : uint8_t {
  ANIM_NONE, ANIM_HELLO, ANIM_MEAL, ANIM_TREAT, ANIM_CLEAN, ANIM_CUDDLE,
  ANIM_NO, ANIM_LIGHTS, ANIM_HATCH,
};

// Name picker entries after the 26 letters.
#define NAME_DONE   26
#define NAME_DEL    27
#define NAME_RANDOM 28
#define NAME_ENTRIES 29

struct UiState {
  Screen screen;
  uint8_t cursor;          // selected item on the current screen

  Anim anim;
  uint32_t animStart;      // ms
  char bubble[24];         // speech bubble text ("" = none)
  uint32_t bubbleUntil;    // ms

  // First-run setup
  uint8_t eggTaps;
  uint8_t pickColor;
  uint8_t pickEars;
  char nameBuf[PET_NAME_MAX + 1];
  uint8_t letter;          // 0..NAME_ENTRIES-1
  uint8_t clockHour;
  uint8_t clockMinute;
  uint8_t clockField;      // 0 = hour, 1 = minute

  // "Which way?" game
  uint8_t gameRound;       // 1..GAME_ROUNDS
  uint8_t gameScore;
  int8_t gameGuess;        // -1 waiting, 0 left, 1 right
  uint8_t gameLook;        // 0 left, 1 right
  uint32_t gameShownAt;    // ms the answer was revealed

  uint16_t holdMs;         // progress of "hold A" on SCR_NEW_EGG

  // Device status for the top bar and settings
  int batteryPercent;      // -1 unknown
  bool charging;
  PetClock clock;
  uint8_t minute;
  bool soundOn;
  uint8_t brightness;      // 0..2
  uint8_t autoOff;         // index into AUTO_OFF_MINUTES
};

extern const uint8_t AUTO_OFF_MINUTES[3];

void uiDraw(lgfx::LGFX_Sprite& c, const PetState& pet, const UiState& ui, uint32_t ms);
