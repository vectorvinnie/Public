// Renders PocketPet screens to PNG files on a desktop computer, using the
// same ui.cpp the device runs. Used to check the artwork without hardware.
//
//   make -C tools/host screenshots M5GFX=/path/to/M5GFX
#include <png.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "../../PocketPet/ui.h"
#include "../../PocketPet/config.h"

static const int SCALE = 2;

struct Shot {
  std::string name;
  std::vector<uint8_t> rgb;  // SCREEN_W*SCALE x SCREEN_H*SCALE
};

static Shot grab(lgfx::LGFX_Sprite& c, const char* name) {
  Shot s;
  s.name = name;
  int w = SCREEN_W * SCALE, h = SCREEN_H * SCALE;
  s.rgb.resize(w * h * 3);
  for (int y = 0; y < SCREEN_H; y++) {
    for (int x = 0; x < SCREEN_W; x++) {
      auto px = c.readPixelRGB(x, y);
      for (int dy = 0; dy < SCALE; dy++)
        for (int dx = 0; dx < SCALE; dx++) {
          uint8_t* o = &s.rgb[((y * SCALE + dy) * w + x * SCALE + dx) * 3];
          o[0] = px.R8(); o[1] = px.G8(); o[2] = px.B8();
        }
    }
  }
  return s;
}

static bool writePng(const char* path, const uint8_t* rgb, int w, int h) {
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  png_infop info = png_create_info_struct(png);
  if (setjmp(png_jmpbuf(png))) { fclose(f); return false; }
  png_init_io(png, f);
  png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);
  for (int y = 0; y < h; y++) png_write_row(png, (png_bytep)(rgb + y * w * 3));
  png_write_end(png, nullptr);
  png_destroy_write_struct(&png, &info);
  fclose(f);
  return true;
}

static UiState baseUi() {
  UiState u = {};
  u.screen = SCR_HOME;
  u.batteryPercent = 72;
  u.clock = {true, 16};
  u.minute = 5;
  u.soundOn = true;
  u.brightness = 1;
  u.autoOff = 1;
  u.gameGuess = -1;
  return u;
}

static PetState basePet(uint8_t color, uint8_t ears, uint32_t ageHours) {
  PetState p;
  petNewEgg(p);
  petHatch(p, "MOCHI", color, ears);
  p.ageSeconds = ageHours * 3600;
  p.food = 85; p.fun = 80; p.energy = 75;
  p.hearts = 42;
  return p;
}

int main(int argc, char** argv) {
  const char* outDir = argc > 1 ? argv[1] : ".";
  lgfx::LGFX_Sprite c;
  c.setColorDepth(16);
  if (!c.createSprite(SCREEN_W, SCREEN_H)) { fprintf(stderr, "no sprite\n"); return 1; }

  std::vector<Shot> shots;
  auto shot = [&](const char* name, const PetState& p, const UiState& u, uint32_t ms) {
    c.fillScreen(0);
    uiDraw(c, p, u, ms);
    shots.push_back(grab(c, name));
  };

  PetState egg;
  petNewEgg(egg);
  UiState u = baseUi();
  u.screen = SCR_EGG; u.eggTaps = 1;
  shot("01-egg", egg, u, 1000);

  u = baseUi(); u.screen = SCR_PICK_COLOR; u.pickColor = 0; u.pickEars = EARS_CAT;
  shot("02-pick-color", egg, u, 500);
  u.screen = SCR_PICK_EARS; u.pickColor = 2; u.pickEars = EARS_BUNNY;
  shot("03-pick-style", egg, u, 500);
  u.screen = SCR_PICK_NAME; strcpy(u.nameBuf, "LULU    "); u.nameCursor = 3;
  shot("04-name", egg, u, 100);
  u = baseUi(); u.screen = SCR_SET_CLOCK; u.clockHour = 16; u.clockMinute = 5;
  shot("05-clock", egg, u, 100);

  PetState p = basePet(0, EARS_CAT, 2);
  u = baseUi();
  shot("06-home-happy", p, u, 500);

  p = basePet(1, EARS_BEAR, 30);
  p.food = 18; p.poops = 2;
  u = baseUi(); u.cursor = ACT_FEED;
  shot("07-home-hungry", p, u, 500);

  u.screen = SCR_FEED; u.cursor = FEED_TREAT;
  shot("08-feed-menu", p, u, 300);
  u.screen = SCR_HOME; u.cursor = ACT_FEED; u.anim = ANIM_MEAL; u.animStart = 0;
  p.poops = 0;
  shot("09-eating", p, u, 300);

  p = basePet(3, EARS_BUNNY, 80);
  p.poops = 2;
  u = baseUi(); u.cursor = ACT_CLEAN; u.anim = ANIM_CLEAN; u.animStart = 0;
  shot("10-cleaning", p, u, 700);

  p = basePet(4, EARS_NONE, 10);
  u = baseUi(); u.cursor = ACT_CUDDLE; u.anim = ANIM_CUDDLE; u.animStart = 0;
  shot("11-cuddle", p, u, 400);

  p = basePet(2, EARS_CAT, 50);
  p.asleep = true; p.energy = 40;
  u = baseUi(); u.cursor = ACT_LIGHT; u.clock = {true, 21};
  shot("12-asleep", p, u, 1234);

  p = basePet(5, EARS_BEAR, 5);
  u = baseUi(); u.screen = SCR_GAME; u.gameRound = 3; u.gameScore = 2; u.gameGuess = 1;
  u.gameLook = 1; u.gameShownAt = 0;
  shot("13-game", p, u, 400);

  p = basePet(0, EARS_BUNNY, 100);
  u = baseUi(); u.screen = SCR_INFO;
  shot("14-info", p, u, 100);

  p = basePet(0, EARS_CAT, 3);
  u = baseUi(); u.screen = SCR_SETTINGS; u.cursor = SET_SCHOOL;
  shot("15-settings", p, u, 100);

  p.schoolMode = true;
  u = baseUi();
  shot("16-school-mode", p, u, 100);

  p = basePet(1, EARS_CAT, 3);
  u = baseUi(); u.anim = ANIM_NO; u.animStart = 0; strcpy(u.bubble, "I'm full!"); u.bubbleUntil = 2000;
  shot("17-full", p, u, 150);

  u = baseUi(); u.screen = SCR_NEW_EGG; u.holdMs = 1800;
  shot("18-new-egg", p, u, 100);
  u.screen = SCR_BYE;
  shot("19-bye", p, u, 100);

  // Everything side by side in one picture.
  int w = SCREEN_W * SCALE, h = SCREEN_H * SCALE, gap = 12;
  int cols = 4, rows = (shots.size() + cols - 1) / cols;
  int sw = cols * (w + gap) + gap, sh = rows * (h + gap) + gap;
  std::vector<uint8_t> sheet(sw * sh * 3, 70);
  for (size_t i = 0; i < shots.size(); i++) {
    int ox = gap + (i % cols) * (w + gap), oy = gap + (i / cols) * (h + gap);
    for (int y = 0; y < h; y++)
      memcpy(&sheet[((oy + y) * sw + ox) * 3], &shots[i].rgb[y * w * 3], w * 3);
  }
  std::string path = std::string(outDir) + "/all-screens.png";
  if (!writePng(path.c_str(), sheet.data(), sw, sh)) { fprintf(stderr, "write %s failed\n", path.c_str()); return 1; }
  printf("wrote %zu screens to %s\n", shots.size(), outDir);
  return 0;
}
