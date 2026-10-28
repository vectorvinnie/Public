// Tests for the pet rules in PocketPet/pet.cpp.  Run with: make -C tools/host test
#include <stdio.h>

#include "../../PocketPet/config.h"
#include "../../PocketPet/pet.h"

static int failures = 0;
#define CHECK(cond)                                                    \
  do {                                                                 \
    if (!(cond)) {                                                     \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
      failures++;                                                      \
    }                                                                  \
  } while (0)

static const PetClock NO_CLOCK = {false, 0};
static PetClock at(int hour) { return PetClock{true, (uint8_t)hour}; }

static PetState hatched() {
  PetState p;
  petNewEgg(p);
  petHatch(p, "MOCHI", 0, EARS_CAT);
  return p;
}

static void testEggDoesNothing() {
  PetState p;
  petNewEgg(p);
  CHECK(!p.hatched);
  petAdvance(p, 3600 * 10, NO_CLOCK, false);
  CHECK(p.food == 80 && p.ageSeconds == 0);
  CHECK(petStage(p) == STAGE_EGG);
}

static void testNeverDies() {
  PetState p = hatched();
  // A whole week with nobody looking after it, minute by minute.
  for (int i = 0; i < 7 * 24 * 60; i++) petAdvance(p, 60, NO_CLOCK, false);
  CHECK(p.hatched);
  CHECK(p.food >= 0 && p.fun >= 0 && p.energy >= 0);
  CHECK(p.poops == MAX_POOPS);
  // ...and it can always be cheered up again.
  if (p.asleep) petToggleSleep(p, NO_CLOCK);
  CHECK(petFeedMeal(p) == RESULT_OK);
  CHECK(petClean(p) == RESULT_OK);
  CHECK(p.poops == 0);
  CHECK(p.food > 0);
}

static void testDecayRates() {
  PetState p = hatched();
  p.food = p.fun = p.energy = 100;
  petAdvance(p, 3600, NO_CLOCK, false);
  CHECK(p.food > 100 - FOOD_DROP_PER_HOUR - 0.01f && p.food < 100 - FOOD_DROP_PER_HOUR + 0.01f);
  CHECK(p.fun > 100 - FUN_DROP_PER_HOUR - 0.01f && p.fun < 100 - FUN_DROP_PER_HOUR + 0.01f);
  CHECK(p.energy < 100);
}

static void testRunsOutOfEnergyNaps() {
  PetState p = hatched();
  p.energy = 1;
  petAdvance(p, 3600, NO_CLOCK, false);
  CHECK(p.asleep);
  CHECK(petMood(p) == MOOD_ASLEEP);
}

static void testOfflineFloor() {
  PetState p = hatched();
  uint32_t start = fromDateTime(2026, 10, 1, 9, 0, 0);
  p.lastSeen = start;
  petCatchUp(p, start + 3 * 86400);  // away for a long weekend
  CHECK(p.food >= OFFLINE_FLOOR - 0.01f);
  CHECK(p.fun >= OFFLINE_FLOOR - 0.01f);
  CHECK(p.ageSeconds == 3 * 86400);

  // Already below the floor: catching up doesn't push it up or further down.
  PetState q = hatched();
  q.food = 5;
  q.lastSeen = start;
  petCatchUp(q, start + 3600);
  CHECK(q.food == 5);
}

static void testOfflineIsSlower() {
  PetState online = hatched(), offline = hatched();
  petAdvance(online, 3600, at(12), false);
  petAdvance(offline, 3600, at(12), true);
  CHECK(offline.food > online.food);
}

static void testBogusClockJumpIgnored() {
  PetState p = hatched();
  uint32_t start = fromDateTime(2026, 10, 1, 9, 0, 0);
  p.lastSeen = start;
  petCatchUp(p, start + (OFFLINE_MAX_DAYS + 1) * 86400u);
  CHECK(p.food == 80 && p.ageSeconds == 0);
  petCatchUp(p, start - 100);  // clock went backwards
  CHECK(p.food == 80);
  p.lastSeen = 0;              // clock never set
  petCatchUp(p, start);
  CHECK(p.food == 80);
}

static void testSchoolModeFreezes() {
  PetState p = hatched();
  p.schoolMode = true;
  petAdvance(p, 8 * 3600, at(10), false);
  CHECK(p.food == 80 && p.fun == 80 && p.ageSeconds == 0 && p.poops == 0);
  CHECK(petMood(p) == MOOD_PAUSED);
  CHECK(petNeed(p) == NEED_NONE);
  CHECK(petFeedMeal(p) == RESULT_PAUSED);
  uint32_t start = fromDateTime(2026, 10, 1, 9, 0, 0);
  p.lastSeen = start;
  petCatchUp(p, start + 86400);
  CHECK(p.food == 80);
}

static void testBedtime() {
  PetState p = hatched();
  petAdvance(p, 60, at(19), false);
  CHECK(!p.asleep);
  petAdvance(p, 60, at(BEDTIME_HOUR), false);
  CHECK(p.asleep);
  // Stays asleep all night even once rested.
  p.energy = 100;
  petAdvance(p, 60, at(3), false);
  CHECK(p.asleep);
  // Wakes up in the morning.
  petAdvance(p, 60, at(WAKE_HOUR), false);
  CHECK(!p.asleep);

  // Without a clock there is no bedtime.
  PetState q = hatched();
  petAdvance(q, 60, PetClock{false, 22}, false);
  CHECK(!q.asleep);
  // Bedtime switched off.
  q.bedtimeOn = false;
  petAdvance(q, 60, at(22), false);
  CHECK(!q.asleep);
}

static void testWokenAtNight() {
  PetState p = hatched();
  petAdvance(p, 60, at(22), false);
  CHECK(p.asleep);
  CHECK(petToggleSleep(p, at(22)) == RESULT_OK);
  CHECK(!p.asleep);
  petAdvance(p, WOKEN_AT_NIGHT_SECONDS - 10, at(22), false);
  CHECK(!p.asleep);
  petAdvance(p, 20, at(22), false);
  CHECK(p.asleep);
}

static void testCatchUpCrossesBedtime() {
  PetState p = hatched();
  uint32_t start = fromDateTime(2026, 10, 1, 18, 0, 0);
  p.lastSeen = start;
  petCatchUp(p, fromDateTime(2026, 10, 1, 23, 0, 0));
  CHECK(p.asleep);
  petCatchUp(p, fromDateTime(2026, 10, 2, 9, 0, 0));
  CHECK(!p.asleep);
}

static void testFeeding() {
  PetState p = hatched();
  p.food = 50;
  CHECK(petFeedMeal(p) == RESULT_OK);
  CHECK(p.food > 84 && p.food < 86);
  p.food = 95;
  CHECK(petFeedMeal(p) == RESULT_FULL);
  CHECK(p.food == 95);
  CHECK(petFeedTreat(p) == RESULT_OK);
  CHECK(p.food == 100);
  p.asleep = true;
  CHECK(petFeedMeal(p) == RESULT_ASLEEP);
}

static void testPoopAndClean() {
  PetState p = hatched();
  p.food = p.fun = p.energy = 100;
  CHECK(petClean(p) == RESULT_NOTHING);
  int pooped = 0;
  for (int i = 0; i < 3 * 60; i++) pooped += petAdvance(p, 60, NO_CLOCK, false);
  CHECK(pooped == 1 && p.poops == 1);
  for (int i = 0; i < 12 * 60; i++) petAdvance(p, 60, NO_CLOCK, false);
  CHECK(p.poops <= MAX_POOPS);
  CHECK(petClean(p) == RESULT_OK);
  CHECK(p.poops == 0);
}

static void testSleepChoices() {
  PetState p = hatched();
  p.energy = 95;
  CHECK(petToggleSleep(p, at(12)) == RESULT_NOT_SLEEPY);
  CHECK(!p.asleep);
  p.energy = 50;
  CHECK(petToggleSleep(p, at(12)) == RESULT_OK);
  CHECK(p.asleep);
  // A daytime nap ends by itself once rested.
  for (int i = 0; i < 4 * 60; i++) petAdvance(p, 60, at(13), false);
  CHECK(!p.asleep);
}

static void testPlayAndCuddle() {
  PetState p = hatched();
  p.fun = 10;
  petGameRound(p, true);
  CHECK(p.fun == 10 + GAME_WIN_FUN);
  p.energy = 5;
  CHECK(petCanPlay(p) == RESULT_TOO_TIRED);
  float before = p.fun;
  CHECK(petCuddle(p) == RESULT_OK);
  CHECK(p.fun == before + CUDDLE_FUN);
  CHECK(petCuddle(p) == RESULT_OK);  // cooldown: no extra fun
  CHECK(p.fun == before + CUDDLE_FUN);
}

static void testMoodAndNeed() {
  PetState p = hatched();
  p.food = p.fun = p.energy = 90;
  CHECK(petMood(p) == MOOD_HAPPY);
  CHECK(petNeed(p) == NEED_NONE);
  p.food = 10;
  CHECK(petMood(p) == MOOD_HUNGRY);
  CHECK(petNeed(p) == NEED_FOOD);
  p.food = 90;
  p.poops = 2;
  CHECK(petNeed(p) == NEED_CLEAN);
  p.poops = 0;
  p.energy = 5;
  p.fun = 20;
  CHECK(petNeed(p) == NEED_SLEEP);  // the lowest need wins
}

static void testStages() {
  PetState p = hatched();
  CHECK(petStage(p) == STAGE_BABY);
  p.ageSeconds = KID_AFTER_HOURS * 3600;
  CHECK(petStage(p) == STAGE_KID);
  p.ageSeconds = GROWN_AFTER_HOURS * 3600;
  CHECK(petStage(p) == STAGE_GROWN);
}

static void testDates() {
  CHECK(fromDateTime(2000, 1, 1, 0, 0, 0) == 0);
  CHECK(fromDateTime(2026, 10, 28, 12, 0, 0) == 846504000u);
  CHECK(fromDateTime(2024, 2, 29, 23, 59, 59) == 762566399u);
}

static void testLongNames() {
  PetState p;
  petNewEgg(p);
  petHatch(p, "VERYLONGNAME", 1, EARS_BEAR);
  CHECK(p.name[PET_NAME_MAX] == '\0');
  CHECK(p.name[PET_NAME_MAX - 1] == 'G');  // "VERYLONG"
}

int main() {
  testEggDoesNothing();
  testNeverDies();
  testDecayRates();
  testRunsOutOfEnergyNaps();
  testOfflineFloor();
  testOfflineIsSlower();
  testBogusClockJumpIgnored();
  testSchoolModeFreezes();
  testBedtime();
  testWokenAtNight();
  testCatchUpCrossesBedtime();
  testFeeding();
  testPoopAndClean();
  testSleepChoices();
  testPlayAndCuddle();
  testMoodAndNeed();
  testStages();
  testDates();
  testLongNames();
  if (failures) {
    printf("%d check(s) failed\n", failures);
    return 1;
  }
  printf("all pet tests passed\n");
  return 0;
}
