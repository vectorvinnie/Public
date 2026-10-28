#include "pet.h"
#include "config.h"

#include <string.h>

static float clamp100(float v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

static uint32_t subSat(uint32_t a, uint32_t b) { return a > b ? a - b : 0; }

// Lower a need, but never below `floor`. A need that is already under the
// floor is left alone rather than pushed up.
static void drop(float& v, float amount, float floor) {
  if (v <= floor) return;
  v = v - amount < floor ? floor : v - amount;
}

bool isNightHour(uint8_t hour) {
  return hour >= BEDTIME_HOUR || hour < WAKE_HOUR;
}

static bool bedtimeNow(const PetState& p, PetClock clock) {
  return p.bedtimeOn && clock.valid && isNightHour(clock.hour);
}

void petNewEgg(PetState& p) {
  memset(&p, 0, sizeof(p));
  p.version = PET_SAVE_VERSION;
  p.food = p.fun = p.energy = 80;
  p.bedtimeOn = true;
}

void petHatch(PetState& p, const char* name, uint8_t color, uint8_t ears) {
  strncpy(p.name, name, PET_NAME_MAX);
  p.name[PET_NAME_MAX] = '\0';
  p.color = color;
  p.ears = ears;
  p.hatched = true;
  p.food = p.fun = p.energy = 80;
  p.poops = 0;
  p.poopClock = 0;
  p.asleep = false;
  p.ageSeconds = 0;
  p.hearts = 0;
}

bool petAdvance(PetState& p, uint32_t seconds, PetClock clock, bool offline) {
  if (!p.hatched || p.schoolMode || seconds == 0) return false;

  const float rate = offline ? OFFLINE_RATE : 1.0f;
  const float floor = offline ? OFFLINE_FLOOR : 0.0f;
  const float hours = seconds / 3600.0f * rate;
  bool pooped = false;

  if (p.asleep) {
    drop(p.food, FOOD_DROP_ASLEEP * hours, floor);
    drop(p.fun, FUN_DROP_ASLEEP * hours, floor);
    p.energy = clamp100(p.energy + ENERGY_GAIN_ASLEEP * seconds / 3600.0f);
    if (p.energy >= 100 && !bedtimeNow(p, clock)) p.asleep = false;
  } else {
    drop(p.food, FOOD_DROP_PER_HOUR * hours, floor);
    drop(p.fun, (FUN_DROP_PER_HOUR + FUN_DROP_PER_POOP * p.poops) * hours, floor);
    drop(p.energy, ENERGY_DROP_PER_HOUR * hours, floor);

    p.poopClock += seconds * rate;
    if (p.poopClock >= POOP_EVERY_HOURS * 3600) {
      p.poopClock -= POOP_EVERY_HOURS * 3600;
      if (p.poops < MAX_POOPS) {
        p.poops++;
        pooped = true;
      }
    }

    p.stayAwakeFor = subSat(p.stayAwakeFor, seconds);
    // Running out of energy just means a nap, never anything worse.
    if (p.energy <= 0) p.asleep = true;
    if (bedtimeNow(p, clock) && p.stayAwakeFor == 0) p.asleep = true;
  }

  p.cuddleCooldown = subSat(p.cuddleCooldown, seconds);
  p.ageSeconds += seconds;
  return pooped;
}

void petCatchUp(PetState& p, uint32_t nowEpoch) {
  if (!p.hatched || p.schoolMode || p.lastSeen == 0 || nowEpoch <= p.lastSeen) return;
  if (nowEpoch - p.lastSeen > (uint32_t)OFFLINE_MAX_DAYS * 86400) return;

  // Ten-minute steps keep bedtime and waking roughly on time.
  for (uint32_t t = p.lastSeen; t < nowEpoch;) {
    uint32_t step = nowEpoch - t < 600 ? nowEpoch - t : 600;
    PetClock clock = {true, (uint8_t)((t / 3600) % 24)};
    petAdvance(p, step, clock, true);
    t += step;
  }
}

PetStage petStage(const PetState& p) {
  if (!p.hatched) return STAGE_EGG;
  if (p.ageSeconds < (uint32_t)KID_AFTER_HOURS * 3600) return STAGE_BABY;
  if (p.ageSeconds < (uint32_t)GROWN_AFTER_HOURS * 3600) return STAGE_KID;
  return STAGE_GROWN;
}

PetMood petMood(const PetState& p) {
  if (!p.hatched) return MOOD_OK;
  if (p.schoolMode) return MOOD_PAUSED;
  if (p.asleep) return MOOD_ASLEEP;
  if (p.food < NEED_LOW) return MOOD_HUNGRY;
  if (p.energy < NEED_LOW) return MOOD_TIRED;
  if (p.fun < NEED_LOW || p.poops >= 2) return MOOD_BORED;
  if (p.food > 60 && p.fun > 60 && p.energy > 40 && p.poops == 0) return MOOD_HAPPY;
  return MOOD_OK;
}

PetNeed petNeed(const PetState& p) {
  if (!p.hatched || p.schoolMode || p.asleep) return NEED_NONE;
  PetNeed need = NEED_NONE;
  float lowest = NEED_LOW;
  if (p.food < lowest) { lowest = p.food; need = NEED_FOOD; }
  if (p.fun < lowest) { lowest = p.fun; need = NEED_FUN; }
  if (p.energy < lowest) { lowest = p.energy; need = NEED_SLEEP; }
  if (need == NEED_NONE && p.poops >= 2) need = NEED_CLEAN;
  return need;
}

ActionResult petFeedMeal(PetState& p) {
  if (p.schoolMode) return RESULT_PAUSED;
  if (p.asleep) return RESULT_ASLEEP;
  if (p.food >= FULL_AT) return RESULT_FULL;
  p.food = clamp100(p.food + MEAL_FOOD);
  p.energy = clamp100(p.energy + MEAL_ENERGY);
  p.poopClock += 1800;  // what goes in...
  p.hearts++;
  return RESULT_OK;
}

ActionResult petFeedTreat(PetState& p) {
  if (p.schoolMode) return RESULT_PAUSED;
  if (p.asleep) return RESULT_ASLEEP;
  if (p.food >= 98 && p.fun >= 98) return RESULT_FULL;
  p.food = clamp100(p.food + TREAT_FOOD);
  p.fun = clamp100(p.fun + TREAT_FUN);
  p.hearts++;
  return RESULT_OK;
}

ActionResult petCanPlay(const PetState& p) {
  if (p.schoolMode) return RESULT_PAUSED;
  if (p.asleep) return RESULT_ASLEEP;
  if (p.energy < TOO_TIRED_TO_PLAY) return RESULT_TOO_TIRED;
  return RESULT_OK;
}

void petGameRound(PetState& p, bool won) {
  p.fun = clamp100(p.fun + (won ? GAME_WIN_FUN : GAME_LOSE_FUN));
  p.food = clamp100(p.food - GAME_ROUND_FOOD);
  p.energy = clamp100(p.energy - GAME_ROUND_ENERGY);
  if (won) p.hearts++;
}

ActionResult petClean(PetState& p) {
  if (p.schoolMode) return RESULT_PAUSED;
  if (p.poops == 0) return RESULT_NOTHING;
  p.poops = 0;
  p.fun = clamp100(p.fun + CLEAN_FUN);
  p.hearts++;
  return RESULT_OK;
}

ActionResult petCuddle(PetState& p) {
  if (p.schoolMode) return RESULT_PAUSED;
  if (p.asleep) return RESULT_ASLEEP;
  if (p.cuddleCooldown == 0) {
    p.fun = clamp100(p.fun + CUDDLE_FUN);
    p.cuddleCooldown = CUDDLE_COOLDOWN_S;
    p.hearts++;
  }
  return RESULT_OK;
}

ActionResult petToggleSleep(PetState& p, PetClock clock) {
  if (p.schoolMode) return RESULT_PAUSED;
  bool night = p.bedtimeOn && clock.valid && isNightHour(clock.hour);
  if (p.asleep) {
    p.asleep = false;
    p.stayAwakeFor = night ? WOKEN_AT_NIGHT_SECONDS : 0;
    return RESULT_OK;
  }
  if (p.energy >= 90 && !night) return RESULT_NOT_SLEEPY;
  p.asleep = true;
  p.stayAwakeFor = 0;
  return RESULT_OK;
}

uint32_t fromDateTime(int year, int month, int day, int hour, int minute, int second) {
  // Days since 2000-01-01 using the "days from civil" method.
  int y = year - (month <= 2);
  int era = y / 400;
  int yoe = y - era * 400;
  int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  long days = (long)era * 146097 + doe - 730425;  // 730425 = days to 2000-01-01
  if (days < 0) return 0;
  return (uint32_t)days * 86400u + hour * 3600u + minute * 60u + second;
}
