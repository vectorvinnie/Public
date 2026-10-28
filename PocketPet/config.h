// PocketPet - tuning knobs.
//
// Everything that decides how "hard" the pet is to look after lives here.
// Rates are in points per hour on a 0-100 scale.
#pragma once

// ---- How fast needs go down while the pet is awake ----
#define FOOD_DROP_PER_HOUR      10.0f   // full -> empty in about 10 hours
#define FUN_DROP_PER_HOUR       12.0f   // full -> empty in about 8 hours
#define ENERGY_DROP_PER_HOUR     7.0f   // a full day awake before it's exhausted
#define FUN_DROP_PER_POOP        3.0f   // extra fun lost per hour for each poop

// ---- While asleep ----
#define FOOD_DROP_ASLEEP         2.0f
#define FUN_DROP_ASLEEP          1.0f
#define ENERGY_GAIN_ASLEEP      20.0f   // empty -> full in 5 hours

// ---- Poop ----
#define POOP_EVERY_HOURS         3.0f   // awake hours between poops
#define MAX_POOPS                3

// ---- While the device is switched off ----
// Time still passes (using the real-time clock), but slower, and needs never
// drop below OFFLINE_FLOOR. Coming back after a weekend gives a hungry pet,
// never a miserable one.
#define OFFLINE_RATE             0.5f
#define OFFLINE_FLOOR           20.0f
#define OFFLINE_MAX_DAYS        30      // ignore clock jumps bigger than this

// ---- Bedtime (only used once the clock has been set) ----
#define BEDTIME_HOUR            20      // 8 pm: pet falls asleep by itself
#define WAKE_HOUR                7      // 7 am: pet wakes up by itself
#define WOKEN_AT_NIGHT_SECONDS 120      // stays awake this long if woken at night

// ---- Actions ----
#define MEAL_FOOD               35.0f
#define MEAL_ENERGY              5.0f
#define TREAT_FOOD              10.0f
#define TREAT_FUN               15.0f
#define FULL_AT                 92.0f   // refuses a meal above this
#define CUDDLE_FUN               4.0f
#define CUDDLE_COOLDOWN_S      300      // cuddles only raise fun every 5 minutes
#define CLEAN_FUN                5.0f
#define GAME_ROUNDS              5
#define GAME_WIN_FUN             8.0f
#define GAME_LOSE_FUN            3.0f
#define GAME_ROUND_FOOD          2.0f
#define GAME_ROUND_ENERGY        2.0f
#define TOO_TIRED_TO_PLAY       10.0f

// ---- Growing up (counts pet time, not school-mode time) ----
#define KID_AFTER_HOURS         24
#define GROWN_AFTER_HOURS       72

// ---- "I need something" ----
#define NEED_LOW                25.0f   // below this the pet asks for help
#define NEED_BEEP_EVERY_S     1200      // at most one reminder beep per 20 min

// ---- Screen & battery ----
#define DIM_AFTER_S             20
#define SAVE_EVERY_S            30
