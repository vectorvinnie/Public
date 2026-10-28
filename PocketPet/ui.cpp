#include "ui.h"
#include "config.h"
#include "meadow.h"

#include <math.h>
#include <string.h>

using lgfx::LGFX_Sprite;

// ---------------------------------------------------------------------------
// Layout (portrait, 135 x 240, front button below the screen)
// ---------------------------------------------------------------------------
static const int TOP_H = 16;      // name, clock, battery
static const int STATS_H = 18;    // food / fun / energy bars
static const int SCENE_Y = TOP_H + STATS_H;
static const int BAR_Y = 196;     // action bar
static const int GROUND_Y = 176;  // where the pet's feet are
static const int CX = SCREEN_W / 2;

const uint8_t AUTO_OFF_MINUTES[3] = {1, 2, 5};

// ---------------------------------------------------------------------------
// Colors. Always build them with rgb() so they stay uint16_t (RGB565):
// LovyanGFX reads a 32-bit color argument as RGB888.
// ---------------------------------------------------------------------------
static inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return lgfx::color565(r, g, b); }

static const uint16_t INK = rgb(60, 40, 50);
static const uint16_t WHITE = rgb(255, 255, 255);
static const uint16_t CREAM = rgb(255, 248, 235);
static const uint16_t CREAM_DARK = rgb(235, 220, 200);
static const uint16_t BLUSH = rgb(255, 150, 170);
static const uint16_t INNER_EAR = rgb(255, 190, 200);
static const uint16_t MOUTH = rgb(170, 50, 70);
static const uint16_t NIGHT = rgb(28, 30, 70);
static const uint16_t NIGHT_GRASS = rgb(30, 70, 60);
static const uint16_t STAR_YELLOW = rgb(255, 220, 90);
static const uint16_t GOOD = rgb(110, 200, 110);
static const uint16_t MEH = rgb(250, 200, 70);
static const uint16_t BAD = rgb(240, 100, 100);
static const uint16_t POOP = rgb(150, 100, 60);
static const uint16_t POOP_LIGHT = rgb(185, 135, 90);
static const uint16_t HIGHLIGHT = rgb(255, 120, 170);

const char* const PET_COLOR_NAMES[PET_COLOR_COUNT] = {
  "Pink", "Mint", "Lavender", "Sky", "Peach", "Lemon"};
const char* const EAR_NAMES[EARS_COUNT] = {"Cat ears", "Bunny ears", "Bear ears", "Sprout"};

struct Palette { uint16_t body, light, outline; };

static Palette petPalette(uint8_t i) {
  static const uint8_t c[PET_COLOR_COUNT][3] = {
    {255, 170, 200}, {150, 225, 190}, {200, 170, 240},
    {140, 200, 250}, {255, 190, 140}, {250, 225, 110}};
  i %= PET_COLOR_COUNT;
  Palette p;
  p.body = rgb(c[i][0], c[i][1], c[i][2]);
  p.light = rgb(c[i][0] + (255 - c[i][0]) / 2, c[i][1] + (255 - c[i][1]) / 2,
                c[i][2] + (255 - c[i][2]) / 2);
  p.outline = rgb(c[i][0] * 5 / 10, c[i][1] * 4 / 10, c[i][2] * 5 / 10);
  return p;
}

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
static bool blinkOn(uint32_t ms, uint32_t period, uint32_t on) { return ms % period < on; }

static void text(LGFX_Sprite& c, const char* s, int x, int y, uint16_t color,
                 const lgfx::IFont* font = &fonts::Font2,
                 lgfx::textdatum_t datum = lgfx::middle_center) {
  c.setFont(font);
  c.setTextDatum(datum);
  c.setTextColor(color);
  c.drawString(s, x, y);
}

static void heart(LGFX_Sprite& c, int x, int y, int s, uint16_t color) {
  c.fillCircle(x - s / 2, y - s / 4, s / 2 + 1, color);
  c.fillCircle(x + s / 2, y - s / 4, s / 2 + 1, color);
  c.fillTriangle(x - s - 1, y - s / 6, x + s + 1, y - s / 6, x, y + s, color);
}

static void star(LGFX_Sprite& c, int x, int y, int r, uint16_t color) {
  float px[10], py[10];
  for (int i = 0; i < 10; i++) {
    float a = -M_PI / 2 + i * M_PI / 5;
    float rr = (i % 2) ? r * 0.45f : r;
    px[i] = x + cosf(a) * rr;
    py[i] = y + sinf(a) * rr;
  }
  for (int i = 0; i < 10; i += 2) {
    c.fillTriangle(x, y, px[i], py[i], px[(i + 1) % 10], py[(i + 1) % 10], color);
    c.fillTriangle(x, y, px[i], py[i], px[(i + 9) % 10], py[(i + 9) % 10], color);
  }
}

static void bubbleText(LGFX_Sprite& c, const char* s, int x, int y) {
  c.setFont(&fonts::Font2);
  int w = c.textWidth(s) + 14;
  if (w > SCREEN_W - 4) w = SCREEN_W - 4;
  int left = x - w / 2;
  if (left < 2) left = 2;
  if (left + w > SCREEN_W - 2) left = SCREEN_W - 2 - w;
  c.fillRoundRect(left - 1, y - 12, w + 2, 24, 9, INK);
  c.fillRoundRect(left, y - 11, w, 22, 8, WHITE);
  c.fillTriangle(x - 5, y + 10, x + 5, y + 10, x, y + 17, INK);
  c.fillTriangle(x - 4, y + 9, x + 4, y + 9, x, y + 15, WHITE);
  text(c, s, left + w / 2, y, INK);
}

// ---------------------------------------------------------------------------
// Icons (all centred on x, y and roughly `s` pixels tall)
// ---------------------------------------------------------------------------
enum Icon : uint8_t {
  ICON_RICEBALL, ICON_BALL, ICON_BROOM, ICON_MOON, ICON_HEART, ICON_CHART, ICON_GEAR,
  ICON_CUPCAKE, ICON_BACK, ICON_APPLE, ICON_STAR, ICON_BOLT, ICON_POOP,
};

static void poop(LGFX_Sprite& c, int x, int y, int s, bool face) {
  // y is the bottom of the pile
  c.fillEllipse(x, y - s * 0.2f, s * 0.55f, s * 0.22f, POOP);
  c.fillEllipse(x, y - s * 0.48f, s * 0.4f, s * 0.2f, POOP);
  c.fillEllipse(x + 1, y - s * 0.75f, s * 0.24f, s * 0.16f, POOP);
  c.fillEllipse(x - s * 0.2f, y - s * 0.3f, s * 0.12f, s * 0.06f, POOP_LIGHT);
  if (face) {
    c.fillCircle(x - s * 0.15f, y - s * 0.38f, 1, INK);
    c.fillCircle(x + s * 0.15f, y - s * 0.38f, 1, INK);
  }
}

static void drawIcon(LGFX_Sprite& c, Icon icon, int x, int y, int s) {
  float h = s / 2.0f;
  switch (icon) {
    case ICON_RICEBALL:
      c.fillTriangle(x, y - h - 1, x - h - 2, y + h * 0.7f + 1, x + h + 2, y + h * 0.7f + 1, INK);
      c.fillTriangle(x, y - h + 1, x - h, y + h * 0.7f, x + h, y + h * 0.7f, WHITE);
      c.fillRect(x - h * 0.45f, y + h * 0.1f, h * 0.9f, h * 0.65f, rgb(40, 70, 50));
      break;
    case ICON_CUPCAKE:
      c.fillTriangle(x - h * 0.8f, y, x + h * 0.8f, y, x - h * 0.55f, y + h, rgb(120, 190, 240));
      c.fillTriangle(x + h * 0.8f, y, x + h * 0.55f, y + h, x - h * 0.55f, y + h, rgb(120, 190, 240));
      c.fillEllipse(x, y - h * 0.1f, h * 0.9f, h * 0.55f, rgb(255, 200, 220));
      c.fillCircle(x, y - h * 0.75f, h * 0.25f + 1, rgb(230, 40, 70));
      break;
    case ICON_BALL:
      c.fillCircle(x, y, h + 1, INK);
      c.fillCircle(x, y, h, rgb(255, 120, 120));
      c.fillRect(x - h, y - h * 0.2f, s, h * 0.4f + 1, WHITE);
      c.fillCircle(x, y, h * 0.3f, rgb(90, 160, 250));
      break;
    case ICON_BROOM:
      c.drawWideLine(x + h * 0.6f, y - h, x - h * 0.1f, y + h * 0.2f, 2, rgb(150, 100, 60));
      c.fillTriangle(x - h * 0.1f, y + h * 0.05f, x - h * 0.95f, y + h, x + h * 0.55f, y + h,
                     rgb(240, 200, 80));
      break;
    case ICON_MOON:
      c.fillCircle(x, y, h, STAR_YELLOW);
      c.fillCircle(x + h * 0.45f, y - h * 0.3f, h * 0.8f, CREAM);
      break;
    case ICON_HEART:
      heart(c, x, y - 1, h * 0.75f, HIGHLIGHT);
      break;
    case ICON_CHART:
      c.fillRect(x - h, y + h * 0.1f, h * 0.5f, h * 0.9f, BAD);
      c.fillRect(x - h * 0.25f, y - h * 0.4f, h * 0.5f, h * 1.4f, MEH);
      c.fillRect(x + h * 0.5f, y - h, h * 0.5f, h * 2, GOOD);
      break;
    case ICON_GEAR:
      for (int i = 0; i < 8; i++) {
        float a = i * M_PI / 4;
        c.fillCircle(x + cosf(a) * h * 0.8f, y + sinf(a) * h * 0.8f, h * 0.25f + 1, rgb(140, 140, 160));
      }
      c.fillCircle(x, y, h * 0.75f, rgb(140, 140, 160));
      c.fillCircle(x, y, h * 0.3f, CREAM);
      break;
    case ICON_BACK:
      c.fillTriangle(x - h, y, x, y - h * 0.8f, x, y + h * 0.8f, INK);
      c.fillRect(x, y - h * 0.3f, h, h * 0.6f + 1, INK);
      break;
    case ICON_APPLE:
      c.fillCircle(x - h * 0.3f, y + 1, h * 0.6f, rgb(235, 70, 70));
      c.fillCircle(x + h * 0.3f, y + 1, h * 0.6f, rgb(235, 70, 70));
      c.drawLine(x, y - h * 0.4f, x + 1, y - h, rgb(120, 80, 40));
      c.fillEllipse(x + h * 0.4f, y - h * 0.8f, h * 0.35f, h * 0.18f, GOOD);
      break;
    case ICON_STAR:
      star(c, x, y, h, STAR_YELLOW);
      break;
    case ICON_BOLT:
      c.fillTriangle(x + h * 0.3f, y - h, x - h * 0.6f, y + h * 0.15f, x + h * 0.05f, y + h * 0.15f,
                     rgb(90, 170, 250));
      c.fillTriangle(x - h * 0.3f, y + h, x + h * 0.6f, y - h * 0.15f, x - h * 0.05f, y - h * 0.15f,
                     rgb(90, 170, 250));
      break;
    case ICON_POOP:
      poop(c, x, y + h, s, false);
      break;
  }
}

// ---------------------------------------------------------------------------
// The pet
// ---------------------------------------------------------------------------
enum Face : uint8_t {
  FACE_NORMAL, FACE_HAPPY, FACE_SAD, FACE_SLEEP, FACE_TIRED, FACE_EAT_OPEN, FACE_EAT_SHUT,
  FACE_OOPS,
};

struct PetLook {
  uint8_t color;
  uint8_t ears;
  PetStage stage;
  Face face;
  int8_t look;     // -1 left, 0 ahead, 1 right
  bool blush;
};

static int petSize(PetStage s) {
  switch (s) {
    case STAGE_BABY: return 19;
    case STAGE_KID: return 24;
    default: return 28;
  }
}

static void fillTri(LGFX_Sprite& c, float x0, float y0, float x1, float y1, float x2, float y2,
                    uint16_t color) {
  c.fillTriangle((int)x0, (int)y0, (int)x1, (int)y1, (int)x2, (int)y2, color);
}

// Shrink a triangle toward its middle (for outlines and inner ears).
static void triInset(float* x, float* y, float k) {
  float mx = (x[0] + x[1] + x[2]) / 3, my = (y[0] + y[1] + y[2]) / 3;
  for (int i = 0; i < 3; i++) { x[i] = mx + (x[i] - mx) * k; y[i] = my + (y[i] - my) * k; }
}

static void drawEars(LGFX_Sprite& c, const PetLook& p, const Palette& pal, int cx, int top, int s) {
  for (int side = -1; side <= 1; side += 2) {
    switch (p.ears) {
      case EARS_CAT: {
        float x[3] = {cx + side * s * 0.95f, cx + side * s * 0.80f, cx + side * s * 0.15f};
        float y[3] = {top + s * 0.75f, top - s * 0.45f, top + s * 0.2f};
        fillTri(c, x[0] + side * 2, y[0] + 1, x[1] + side * 1, y[1] - 2, x[2] - side * 2, y[2], pal.outline);
        fillTri(c, x[0], y[0], x[1], y[1], x[2], y[2], pal.body);
        triInset(x, y, 0.5f);
        fillTri(c, x[0], y[0], x[1], y[1], x[2], y[2], INNER_EAR);
        break;
      }
      case EARS_BUNNY: {
        int ex = cx + side * s * 0.42f, ey = top - s * 0.35f;
        c.fillEllipse(ex, ey, s * 0.26f + 2, s * 0.75f + 2, pal.outline);
        c.fillEllipse(ex, ey, s * 0.26f, s * 0.75f, pal.body);
        c.fillEllipse(ex, ey + 2, s * 0.12f, s * 0.5f, INNER_EAR);
        break;
      }
      case EARS_BEAR: {
        int ex = cx + side * s * 0.72f, ey = top + s * 0.22f;
        c.fillCircle(ex, ey, s * 0.36f + 2, pal.outline);
        c.fillCircle(ex, ey, s * 0.36f, pal.body);
        c.fillCircle(ex, ey, s * 0.18f, INNER_EAR);
        break;
      }
      default:
        break;
    }
  }
  if (p.ears == EARS_NONE) {
    c.drawWideLine(cx, top + 2, cx + 2, top - s * 0.35f, 1.5f, rgb(70, 140, 70));
    c.fillEllipse(cx - s * 0.2f, top - s * 0.38f, s * 0.22f, s * 0.11f, GOOD);
    c.fillEllipse(cx + s * 0.25f, top - s * 0.45f, s * 0.22f, s * 0.11f, GOOD);
  }
}

static void drawEyes(LGFX_Sprite& c, const PetLook& p, const Palette& pal, int cx, int ey, int s,
                     uint32_t ms) {
  int dx = s * 0.42f;
  int rx = s * 0.13f + 1, ry = s * 0.17f + 1;
  int shift = p.look * s * 0.12f;
  Face f = p.face;
  if ((f == FACE_NORMAL || f == FACE_SAD) && blinkOn(ms, 3300, 140)) f = FACE_SLEEP;

  for (int side = -1; side <= 1; side += 2) {
    int x = cx + side * dx + shift;
    switch (f) {
      case FACE_HAPPY:
      case FACE_EAT_SHUT:
        c.fillArc(x, ey + ry / 2, rx + 1, rx - 1, 200, 340, INK);   // ^ ^
        break;
      case FACE_SLEEP:
        c.fillArc(x, ey - ry / 2, rx + 1, rx - 1, 20, 160, INK);    // u u
        break;
      case FACE_TIRED:
        c.fillEllipse(x, ey, rx, ry, INK);
        c.fillRect(x - rx - 1, ey - ry - 1, rx * 2 + 3, ry + 1, pal.body);
        c.drawFastHLine(x - rx - 1, ey, rx * 2 + 3, INK);
        break;
      case FACE_OOPS:
        c.fillCircle(x, ey, rx, INK);
        c.fillCircle(x, ey, rx - 2 > 0 ? rx - 2 : 1, WHITE);
        break;
      default:
        c.fillEllipse(x, ey, rx, ry, INK);
        c.fillCircle(x - rx / 3, ey - ry / 3, rx / 2 > 1 ? rx / 2 : 1, WHITE);
        break;
    }
    if (f == FACE_SAD) {  // worried eyebrows
      c.drawWideLine(x - side * rx * 1.0f, ey - ry * 2.3f, x + side * rx * 0.9f, ey - ry * 1.7f, 1, INK);
    }
  }
}

static void drawMouth(LGFX_Sprite& c, const PetLook& p, int cx, int my, int s) {
  int r = s * 0.16f + 1;
  switch (p.face) {
    case FACE_HAPPY:
      c.fillArc(cx, my - 1, r + 1, 0, 0, 180, MOUTH);
      c.fillEllipse(cx, my + r * 0.6f, r * 0.5f, r * 0.3f, BLUSH);
      break;
    case FACE_SAD:
    case FACE_TIRED:
      c.fillArc(cx, my + r, r, r - 2, 215, 325, INK);
      break;
    case FACE_EAT_OPEN:
      c.fillEllipse(cx, my + 1, r * 0.8f, r, MOUTH);
      break;
    case FACE_EAT_SHUT:
      c.fillArc(cx, my - 2, r, r - 2, 30, 150, INK);
      break;
    case FACE_OOPS:
      c.fillCircle(cx, my + 1, r * 0.55f, MOUTH);
      break;
    case FACE_SLEEP:
      c.fillEllipse(cx, my + 1, r * 0.35f, r * 0.25f, INK);
      break;
    default:
      c.fillArc(cx, my - 2, r, r - 2, 25, 155, INK);
      break;
  }
}

// cx = middle, gy = ground under the feet. `bob` squashes the body a little.
static void drawPet(LGFX_Sprite& c, const PetLook& p, int cx, int gy, float bob, uint32_t ms) {
  Palette pal = petPalette(p.color);
  int s = petSize(p.stage);
  int w = s + (int)bob;                // half width
  int h = (int)(s * 0.86f) - (int)bob; // half height
  int cy = gy - h - 2;
  int top = cy - h;

  // shadow
  c.fillEllipse(cx, gy, s * 0.8f, 3, rgb(120, 150, 70));

  drawEars(c, p, pal, cx, top, s);

  // feet
  c.fillEllipse(cx - s * 0.45f, gy - 2, s * 0.25f + 1, 4, pal.outline);
  c.fillEllipse(cx + s * 0.45f, gy - 2, s * 0.25f + 1, 4, pal.outline);

  // body
  c.fillEllipse(cx, cy, w + 2, h + 2, pal.outline);
  c.fillEllipse(cx, cy, w, h, pal.body);
  c.fillEllipse(cx, cy + h * 0.45f, w * 0.55f, h * 0.38f, pal.light);
  c.fillEllipse(cx - w * 0.45f, cy - h * 0.5f, w * 0.18f, h * 0.12f, pal.light);  // shine

  int eyeY = cy - h * 0.12f;
  drawEyes(c, p, pal, cx, eyeY, s, ms);
  if (p.blush || p.face == FACE_HAPPY) {
    c.fillEllipse(cx - s * 0.68f, eyeY + s * 0.3f, s * 0.15f, s * 0.08f + 1, BLUSH);
    c.fillEllipse(cx + s * 0.68f, eyeY + s * 0.3f, s * 0.15f, s * 0.08f + 1, BLUSH);
  }
  drawMouth(c, p, cx + p.look * s * 0.1f, eyeY + s * 0.36f, s);

  if (p.stage == STAGE_GROWN) {  // a bow for the grown-up
    int bx = cx + s * 0.55f, by = top + s * 0.25f;
    c.fillTriangle(bx, by, bx - 9, by - 6, bx - 9, by + 6, HIGHLIGHT);
    c.fillTriangle(bx, by, bx + 9, by - 6, bx + 9, by + 6, HIGHLIGHT);
    c.fillCircle(bx, by, 3, rgb(230, 80, 140));
  }
}

// ---------------------------------------------------------------------------
// Scene pieces
// ---------------------------------------------------------------------------
static void drawMeadow(LGFX_Sprite& c, int y) {
  c.pushImage(0, y, MEADOW_W, MEADOW_H, MEADOW);
}

static void drawNight(LGFX_Sprite& c, uint32_t ms) {
  c.fillRect(0, SCENE_Y, SCREEN_W, BAR_Y - SCENE_Y, NIGHT);
  c.fillRect(0, GROUND_Y - 20, SCREEN_W, BAR_Y - GROUND_Y + 20, NIGHT_GRASS);
  static const uint8_t sx[] = {12, 40, 70, 25, 100, 120, 58, 88, 8, 112};
  static const uint8_t sy[] = {48, 60, 44, 90, 80, 52, 104, 120, 118, 128};
  for (int i = 0; i < 10; i++) {
    bool twinkle = ((ms / 400) + i * 3) % 7 == 0;
    c.fillCircle(sx[i], sy[i], twinkle ? 2 : 1, STAR_YELLOW);
  }
  c.fillCircle(108, 62, 12, STAR_YELLOW);
  c.fillCircle(115, 56, 11, NIGHT);
}

static void drawPoops(LGFX_Sprite& c, int count, int hideRightOf, uint32_t ms) {
  static const int px[] = {20, 116, 40};
  static const int py[] = {GROUND_Y - 4, GROUND_Y + 6, GROUND_Y + 14};
  for (int i = 0; i < count && i < 3; i++) {
    if (px[i] > hideRightOf) continue;
    poop(c, px[i], py[i], 16, true);
    int wob = (ms / 250 + i) % 2 ? 1 : -1;  // stink lines
    for (int k = -1; k <= 1; k += 2) {
      int lx = px[i] + k * 5;
      c.drawLine(lx, py[i] - 16, lx + wob, py[i] - 20, rgb(140, 160, 90));
      c.drawLine(lx + wob, py[i] - 20, lx, py[i] - 24, rgb(140, 160, 90));
    }
  }
}

static void drawThought(LGFX_Sprite& c, PetNeed need, int x, int y) {
  Icon icon = ICON_APPLE;
  switch (need) {
    case NEED_FOOD: icon = ICON_APPLE; break;
    case NEED_FUN: icon = ICON_BALL; break;
    case NEED_SLEEP: icon = ICON_MOON; break;
    case NEED_CLEAN: icon = ICON_BROOM; break;
    default: return;
  }
  c.fillCircle(x - 14, y + 18, 2, WHITE);
  c.fillCircle(x - 9, y + 12, 3, WHITE);
  c.fillCircle(x, y, 13, INK);
  c.fillCircle(x, y, 12, WHITE);
  if (icon == ICON_MOON) {
    c.fillCircle(x, y, 7, STAR_YELLOW);
    c.fillCircle(x + 4, y - 3, 6, WHITE);
  } else {
    drawIcon(c, icon, x, y, 14);
  }
}

static void drawZzz(LGFX_Sprite& c, int x, int y, uint32_t ms) {
  for (int i = 0; i < 3; i++) {
    int t = (ms / 30 + i * 33) % 100;  // 0..99 rise
    int zx = x + t / 6 + i * 2;
    int zy = y - t / 3;
    text(c, "z", zx, zy, WHITE, i == 1 ? &fonts::Font4 : &fonts::Font2);
  }
}

static void drawTopBars(LGFX_Sprite& c, const PetState& pet, const UiState& ui, uint32_t ms) {
  c.fillRect(0, 0, SCREEN_W, SCENE_Y, CREAM);
  c.drawFastHLine(0, SCENE_Y - 1, SCREEN_W, CREAM_DARK);

  text(c, pet.name, 4, 8, INK, &fonts::Font2, lgfx::middle_left);

  // battery
  int bx = SCREEN_W - 22, by = 3;
  int pct = ui.batteryPercent;
  bool low = pct >= 0 && pct < 15;
  if (!low || blinkOn(ms, 1000, 600)) {
    uint16_t edge = low ? BAD : INK;
    c.drawRoundRect(bx, by, 17, 9, 2, edge);
    c.fillRect(bx + 17, by + 3, 2, 3, edge);
    if (pct >= 0) c.fillRect(bx + 2, by + 2, 13 * pct / 100 + 1, 5, low ? BAD : GOOD);
    if (ui.charging) {
      c.fillTriangle(bx + 9, by + 1, bx + 5, by + 5, bx + 9, by + 5, MEH);
      c.fillTriangle(bx + 8, by + 8, bx + 12, by + 4, bx + 8, by + 4, MEH);
    }
  }

  if (ui.clock.valid) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d:%02d", ui.clock.hour, ui.minute);
    text(c, buf, bx - 4, 8, rgb(140, 120, 120), &fonts::Font0, lgfx::middle_right);
  }

  // food / fun / energy
  const float vals[3] = {pet.food, pet.fun, pet.energy};
  const Icon icons[3] = {ICON_APPLE, ICON_STAR, ICON_BOLT};
  for (int i = 0; i < 3; i++) {
    int x = 2 + i * 45;
    int y = TOP_H + STATS_H / 2 - 1;
    drawIcon(c, icons[i], x + 6, y, 11);
    int v = (int)(vals[i] + 0.5f);
    uint16_t col = v > 50 ? GOOD : (v > NEED_LOW ? MEH : BAD);
    c.drawRoundRect(x + 13, y - 4, 28, 9, 3, CREAM_DARK);
    if (v > 0) c.fillRoundRect(x + 14, y - 3, 26 * v / 100 + 1, 7, 2, col);
  }
}

// A bar with the selected item big in the middle and its neighbours small.
static void drawChoiceBar(LGFX_Sprite& c, const Icon* icons, const char* const* labels, int count,
                          int sel, bool attention) {
  c.fillRect(0, BAR_Y, SCREEN_W, SCREEN_H - BAR_Y, CREAM);
  c.drawFastHLine(0, BAR_Y, SCREEN_W, CREAM_DARK);
  int prev = (sel + count - 1) % count, next = (sel + 1) % count;

  c.fillRoundRect(CX - 17, BAR_Y + 3, 34, 24, 8, attention ? rgb(255, 225, 235) : WHITE);
  c.drawRoundRect(CX - 17, BAR_Y + 3, 34, 24, 8, HIGHLIGHT);
  drawIcon(c, icons[sel], CX, BAR_Y + 15, 18);
  drawIcon(c, icons[prev], 18, BAR_Y + 15, 12);
  drawIcon(c, icons[next], SCREEN_W - 18, BAR_Y + 15, 12);
  c.fillTriangle(SCREEN_W - 4, BAR_Y + 15, SCREEN_W - 8, BAR_Y + 11, SCREEN_W - 8, BAR_Y + 19,
                 CREAM_DARK);
  text(c, labels[sel], CX, BAR_Y + 35, INK);
}

static const Icon HOME_ICONS[ACT_COUNT] = {
  ICON_RICEBALL, ICON_BALL, ICON_BROOM, ICON_MOON, ICON_HEART, ICON_CHART, ICON_GEAR};
static const char* const HOME_LABELS[ACT_COUNT] = {
  "Feed", "Play", "Clean", "Lights", "Cuddle", "How am I?", "Settings"};
static const Icon FEED_ICONS[FEED_COUNT] = {ICON_RICEBALL, ICON_CUPCAKE, ICON_BACK};
static const char* const FEED_LABELS[FEED_COUNT] = {"Meal", "Treat", "Back"};

static Face faceForMood(PetMood m) {
  switch (m) {
    case MOOD_HAPPY: return FACE_HAPPY;
    case MOOD_HUNGRY:
    case MOOD_BORED: return FACE_SAD;
    case MOOD_TIRED: return FACE_TIRED;
    case MOOD_ASLEEP:
    case MOOD_PAUSED: return FACE_SLEEP;
    default: return FACE_NORMAL;
  }
}

static PetLook lookFor(const PetState& pet) {
  PetLook p = {};
  p.color = pet.color;
  p.ears = pet.ears;
  p.stage = petStage(pet);
  p.face = faceForMood(petMood(pet));
  return p;
}

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------
static void screenHome(LGFX_Sprite& c, const PetState& pet, const UiState& ui, uint32_t ms) {
  PetMood mood = petMood(pet);
  bool night = pet.asleep && ui.anim != ANIM_LIGHTS;
  uint32_t t = ms - ui.animStart;
  Anim anim = ui.anim;

  if (night) drawNight(c, ms);
  else drawMeadow(c, SCENE_Y - 26);

  PetLook look = lookFor(pet);
  int x = CX;
  float bob = mood == MOOD_ASLEEP ? sinf(ms / 700.0f) * 1.5f : sinf(ms / 260.0f) * 1.2f;
  int hop = 0;

  int broomX = SCREEN_W + 40;
  switch (anim) {
    case ANIM_HELLO:
      look.face = FACE_HAPPY;
      hop = (int)(fabsf(sinf(t / 130.0f)) * 8);
      break;
    case ANIM_MEAL:
    case ANIM_TREAT:
      look.face = (t / 220) % 2 ? FACE_EAT_SHUT : FACE_EAT_OPEN;
      break;
    case ANIM_CLEAN:
      broomX = SCREEN_W + 10 - (int)(t * (SCREEN_W + 30) / 1400);
      look.face = FACE_OOPS;
      break;
    case ANIM_CUDDLE:
      look.face = FACE_HAPPY;
      look.blush = true;
      bob = sinf(t / 90.0f) * 2.5f;
      break;
    case ANIM_NO:
      look.face = FACE_SAD;
      x += (int)(sinf(t / 60.0f) * 4);
      break;
    case ANIM_LIGHTS:
      look.face = FACE_SLEEP;
      break;
    default:
      break;
  }

  drawPoops(c, pet.poops, broomX, ms);
  drawPet(c, look, x, GROUND_Y - hop, bob, ms);

  int s = petSize(look.stage);
  int headY = GROUND_Y - s * 2 - 6;

  if (anim == ANIM_MEAL || anim == ANIM_TREAT) {
    int bite = t / 450;  // 0..3
    if (bite < 3) {
      int size = 20 - bite * 5;
      drawIcon(c, anim == ANIM_MEAL ? ICON_RICEBALL : ICON_CUPCAKE, x + s + 12, GROUND_Y - s,
               size);
    }
  }
  if (anim == ANIM_CLEAN && broomX < SCREEN_W + 10) {
    c.drawWideLine(broomX + 14, GROUND_Y - 34, broomX + 2, GROUND_Y - 6, 2, rgb(150, 100, 60));
    c.fillTriangle(broomX + 3, GROUND_Y - 8, broomX - 10, GROUND_Y + 10, broomX + 14, GROUND_Y + 10,
                   rgb(240, 200, 80));
  }
  if (anim == ANIM_CLEAN && t > 1200) {
    for (int i = 0; i < 5; i++) star(c, 15 + i * 26, GROUND_Y - 6 - (i % 2) * 14, 4, WHITE);
  }
  if (anim == ANIM_CUDDLE) {
    for (int i = 0; i < 3; i++) {
      int rise = (int)((t + i * 300) % 900) / 12;
      heart(c, x - 22 + i * 22, headY + 6 - rise, 5, HIGHLIGHT);
    }
  }

  if (mood == MOOD_ASLEEP && anim == ANIM_NONE) drawZzz(c, x + s - 2, headY + 4, ms);
  if (mood == MOOD_PAUSED) {
    c.fillRoundRect(10, SCENE_Y + 6, SCREEN_W - 20, 38, 8, WHITE);
    c.drawRoundRect(10, SCENE_Y + 6, SCREEN_W - 20, 38, 8, HIGHLIGHT);
    text(c, "School mode", CX, SCENE_Y + 17, INK);
    text(c, "see you later!", CX, SCENE_Y + 33, rgb(140, 120, 120));
  }

  if (ui.bubble[0] && (int32_t)(ui.bubbleUntil - ms) > 0) {
    bubbleText(c, ui.bubble, x, headY - 16);
  } else if (anim == ANIM_NONE) {
    PetNeed need = petNeed(pet);
    if (need != NEED_NONE && blinkOn(ms, 3000, 2000)) drawThought(c, need, x + s + 14, headY - 6);
  }

  drawTopBars(c, pet, ui, ms);
  if (ui.screen == SCR_FEED) {
    drawChoiceBar(c, FEED_ICONS, FEED_LABELS, FEED_COUNT, ui.cursor, false);
  } else {
    PetNeed need = petNeed(pet);
    bool attention = (need == NEED_FOOD && ui.cursor == ACT_FEED) ||
                     (need == NEED_FUN && ui.cursor == ACT_PLAY) ||
                     (need == NEED_SLEEP && ui.cursor == ACT_LIGHT) ||
                     (need == NEED_CLEAN && ui.cursor == ACT_CLEAN);
    drawChoiceBar(c, HOME_ICONS, HOME_LABELS, ACT_COUNT, ui.cursor, attention);
  }
}

static void drawHint(LGFX_Sprite& c, const char* line1, const char* line2) {
  c.fillRect(0, BAR_Y, SCREEN_W, SCREEN_H - BAR_Y, CREAM);
  c.drawFastHLine(0, BAR_Y, SCREEN_W, CREAM_DARK);
  text(c, line1, CX, BAR_Y + 13, INK);
  if (line2) text(c, line2, CX, BAR_Y + 31, rgb(140, 120, 120));
}

static void drawTitle(LGFX_Sprite& c, const char* title) {
  c.fillRect(0, 0, SCREEN_W, 26, CREAM);
  c.drawFastHLine(0, 26, SCREEN_W, CREAM_DARK);
  text(c, title, CX, 13, INK);
}

static void screenEgg(LGFX_Sprite& c, const UiState& ui, uint32_t ms) {
  drawMeadow(c, 0);
  uint32_t t = ms - ui.animStart;
  int wob = 0;
  if (ui.anim == ANIM_HATCH || (ms % 2000) < 400) wob = (int)(sinf(ms / 50.0f) * 3);
  int ex = CX + wob, ey = 140;

  if (ui.anim == ANIM_HATCH) {
    // white flash that fades into the colour picker
    if (t > 300) {
      c.fillRect(0, 0, SCREEN_W, SCREEN_H, WHITE);
      star(c, CX, ey, 10 + t / 20, STAR_YELLOW);
      return;
    }
  }

  c.fillEllipse(CX, ey + 34, 26, 4, rgb(120, 150, 70));
  c.fillEllipse(ex, ey, 28, 36, INK);
  c.fillEllipse(ex, ey, 26, 34, rgb(255, 250, 235));
  c.fillCircle(ex - 10, ey - 12, 5, rgb(255, 180, 200));
  c.fillCircle(ex + 12, ey + 2, 6, rgb(150, 210, 250));
  c.fillCircle(ex - 6, ey + 18, 4, rgb(200, 180, 240));
  c.fillEllipse(ex - 12, ey - 20, 5, 3, WHITE);

  int taps = ui.anim == ANIM_HATCH ? 3 : ui.eggTaps;
  if (taps >= 1) {  // cracks
    c.drawLine(ex - 20, ey - 4, ex - 10, ey + 4, INK);
    c.drawLine(ex - 10, ey + 4, ex - 2, ey - 6, INK);
  }
  if (taps >= 2) {
    c.drawLine(ex - 2, ey - 6, ex + 8, ey + 4, INK);
    c.drawLine(ex + 8, ey + 4, ex + 20, ey - 5, INK);
  }

  bubbleText(c, taps == 0 ? "An egg!" : (taps == 1 ? "It moved!" : "Almost..."), CX, 80);
  drawHint(c, "Press A to hatch", "(the big button)");
}

static void screenPick(LGFX_Sprite& c, const UiState& ui, uint32_t ms, bool colors) {
  drawMeadow(c, 0);
  drawTitle(c, colors ? "Pick a color" : "Pick a style");
  PetLook p = {};
  p.color = ui.pickColor;
  p.ears = ui.pickEars;
  p.stage = STAGE_KID;
  p.face = FACE_HAPPY;
  drawPet(c, p, CX, GROUND_Y - 10, sinf(ms / 260.0f) * 1.2f, ms);
  bubbleText(c, colors ? PET_COLOR_NAMES[ui.pickColor] : EAR_NAMES[ui.pickEars], CX, 66);
  drawHint(c, "B: next one", "A: choose");
}

static void screenName(LGFX_Sprite& c, const PetState& pet, const UiState& ui, uint32_t ms) {
  c.fillRect(0, 0, SCREEN_W, SCREEN_H, CREAM);
  drawTitle(c, "Name your pet");

  PetLook p = {};
  p.color = ui.pickColor;
  p.ears = ui.pickEars;
  p.stage = STAGE_BABY;
  p.face = FACE_NORMAL;
  drawPet(c, p, CX, 86, 0, ms);

  // the name so far, with a blinking cursor
  char shown[PET_NAME_MAX + 2];
  snprintf(shown, sizeof(shown), "%s%s", ui.nameBuf,
           strlen(ui.nameBuf) < PET_NAME_MAX && blinkOn(ms, 800, 400) ? "_" : "");
  c.fillRoundRect(8, 96, SCREEN_W - 16, 28, 6, WHITE);
  text(c, shown, CX, 110, INK, &fonts::Font4);

  // letter wheel
  auto label = [](int i, char* buf) -> const char* {
    if (i < 26) { buf[0] = 'A' + i; buf[1] = 0; return buf; }
    if (i == NAME_DONE) return "DONE";
    if (i == NAME_DEL) return "DEL";
    return "RANDOM";
  };
  char b0[2], b1[2], b2[2];
  int cur = ui.letter, prev = (cur + NAME_ENTRIES - 1) % NAME_ENTRIES, next = (cur + 1) % NAME_ENTRIES;
  auto font = [](int i) -> const lgfx::IFont* {
    return i < 26 ? (const lgfx::IFont*)&fonts::Font4 : (const lgfx::IFont*)&fonts::Font0;
  };
  text(c, label(prev, b0), 22, 158, CREAM_DARK, font(prev));
  text(c, label(next, b2), SCREEN_W - 22, 158, CREAM_DARK, font(next));
  c.fillRoundRect(CX - 26, 140, 52, 36, 8, WHITE);
  c.drawRoundRect(CX - 26, 140, 52, 36, 8, HIGHLIGHT);
  text(c, label(cur, b1), CX, 158, INK, cur < 26 ? font(cur) : &fonts::Font2);

  (void)pet;
  drawHint(c, "B: next  PWR: back", "A: pick this");
}

static void screenClock(LGFX_Sprite& c, const UiState& ui, uint32_t ms) {
  c.fillRect(0, 0, SCREEN_W, SCREEN_H, CREAM);
  drawTitle(c, "What time is it?");
  drawIcon(c, ICON_MOON, CX, 62, 26);

  char hh[4], mm[4];
  snprintf(hh, sizeof(hh), "%02d", ui.clockHour);
  snprintf(mm, sizeof(mm), "%02d", ui.clockMinute);
  int y = 120;
  for (int f = 0; f < 2; f++) {
    int x = f == 0 ? CX - 30 : CX + 30;
    bool active = ui.clockField == f;
    c.fillRoundRect(x - 24, y - 20, 48, 40, 8, active ? WHITE : CREAM);
    if (active) c.drawRoundRect(x - 24, y - 20, 48, 40, 8, HIGHLIGHT);
    if (!active || blinkOn(ms, 1000, 750)) text(c, f == 0 ? hh : mm, x, y, INK, &fonts::Font4);
  }
  text(c, ":", CX, y - 2, INK, &fonts::Font4);
  text(c, "24-hour clock", CX, 156, rgb(140, 120, 120));
  drawHint(c, "B: +1 (hold = fast)", ui.clockField == 0 ? "A: next" : "A: done");
}

static void screenGame(LGFX_Sprite& c, const PetState& pet, const UiState& ui, uint32_t ms) {
  drawMeadow(c, SCENE_Y - 26);
  drawTopBars(c, pet, ui, ms);

  PetLook look = lookFor(pet);
  look.face = FACE_NORMAL;
  int x = CX;
  bool revealed = ui.gameGuess >= 0;
  bool won = revealed && ui.gameGuess == ui.gameLook;
  bool finished = revealed && ui.gameRound >= GAME_ROUNDS;
  if (revealed) {
    look.look = ui.gameLook ? 1 : -1;
    look.face = won ? FACE_HAPPY : FACE_OOPS;
    x += look.look * 10;
  } else {
    look.look = (ms / 300) % 3 - 1;  // looking around, thinking
  }
  drawPet(c, look, x, GROUND_Y, sinf(ms / 260.0f) * 1.2f, ms);

  int headY = GROUND_Y - petSize(look.stage) * 2 - 6;
  if (finished && ms - ui.gameShownAt > 900) {
    char buf[24];
    snprintf(buf, sizeof(buf), "You got %d!", ui.gameScore);
    bubbleText(c, buf, CX, headY - 16);
  } else if (revealed) {
    bubbleText(c, won ? "Yay! Again!" : "Hehe, nope!", x, headY - 16);
  } else {
    bubbleText(c, "Which way?", CX, headY - 16);
  }

  // round and score
  for (int i = 0; i < GAME_ROUNDS; i++) {
    int sx = CX - (GAME_ROUNDS - 1) * 9 + i * 18;
    if (i < ui.gameScore) star(c, sx, SCENE_Y + 12, 7, STAR_YELLOW);
    else c.drawCircle(sx, SCENE_Y + 12, 4, WHITE);
  }

  c.fillRect(0, BAR_Y, SCREEN_W, SCREEN_H - BAR_Y, CREAM);
  c.drawFastHLine(0, BAR_Y, SCREEN_W, CREAM_DARK);
  c.fillTriangle(10, BAR_Y + 18, 22, BAR_Y + 8, 22, BAR_Y + 28, HIGHLIGHT);
  c.fillTriangle(SCREEN_W - 10, BAR_Y + 18, SCREEN_W - 22, BAR_Y + 8, SCREEN_W - 22, BAR_Y + 28,
                 HIGHLIGHT);
  text(c, "A = left", 26, BAR_Y + 12, INK, &fonts::Font2, lgfx::middle_left);
  text(c, "right = B", SCREEN_W - 26, BAR_Y + 30, INK, &fonts::Font2, lgfx::middle_right);
  char round[16];
  snprintf(round, sizeof(round), "Round %d of %d", ui.gameRound, GAME_ROUNDS);
  text(c, round, CX, SCENE_Y + 26, WHITE, &fonts::Font0);
}

static void statRow(LGFX_Sprite& c, Icon icon, const char* name, float value, int y) {
  drawIcon(c, icon, 14, y, 14);
  text(c, name, 26, y - 1, INK, &fonts::Font2, lgfx::middle_left);
  int v = (int)(value + 0.5f);
  uint16_t col = v > 50 ? GOOD : (v > NEED_LOW ? MEH : BAD);
  c.drawRoundRect(72, y - 6, 56, 12, 4, CREAM_DARK);
  if (v > 0) c.fillRoundRect(73, y - 5, 54 * v / 100 + 1, 10, 3, col);
}

static void screenInfo(LGFX_Sprite& c, const PetState& pet, const UiState& ui, uint32_t ms) {
  c.fillRect(0, 0, SCREEN_W, SCREEN_H, CREAM);
  PetLook look = lookFor(pet);
  drawPet(c, look, CX, 100, sinf(ms / 260.0f) * 1.2f, ms);
  drawTitle(c, pet.name);  // after the pet, so tall ears tuck under it

  statRow(c, ICON_APPLE, "Food", pet.food, 112);
  statRow(c, ICON_STAR, "Fun", pet.fun, 132);
  statRow(c, ICON_BOLT, "Energy", pet.energy, 152);

  static const char* const STAGES[] = {"Egg", "Baby", "Kid", "Grown-up"};
  char buf[32];
  uint32_t days = pet.ageSeconds / 86400, hours = pet.ageSeconds / 3600 % 24;
  snprintf(buf, sizeof(buf), "%s, %lu d %lu h", STAGES[petStage(pet)], (unsigned long)days,
           (unsigned long)hours);
  text(c, buf, CX, 174, INK);
  heart(c, CX - 30, 191, 5, HIGHLIGHT);
  snprintf(buf, sizeof(buf), "%lu hearts", (unsigned long)pet.hearts);
  text(c, buf, CX - 20, 191, INK, &fonts::Font2, lgfx::middle_left);

  (void)ui;
  drawHint(c, "Any button: back", nullptr);
}

static void screenSettings(LGFX_Sprite& c, const PetState& pet, const UiState& ui) {
  c.fillRect(0, 0, SCREEN_W, SCREEN_H, CREAM);
  drawTitle(c, "Settings");

  static const char* const NAMES[SET_COUNT] = {
    "School mode", "Sound", "Brightness", "Auto off", "Bedtime", "Set clock", "New egg", "Back"};
  static const char* const BRIGHT[3] = {"Low", "Mid", "High"};
  const int rowH = 20, top = 30;
  for (int i = 0; i < SET_COUNT; i++) {
    int y = top + i * rowH + rowH / 2;
    if (i == ui.cursor) {
      c.fillRoundRect(3, y - rowH / 2 + 1, SCREEN_W - 6, rowH - 2, 6, WHITE);
      c.drawRoundRect(3, y - rowH / 2 + 1, SCREEN_W - 6, rowH - 2, 6, HIGHLIGHT);
    }
    text(c, NAMES[i], 8, y, INK, &fonts::Font2, lgfx::middle_left);
    char val[12] = "";
    switch (i) {
      case SET_SCHOOL: strcpy(val, pet.schoolMode ? "ON" : "off"); break;
      case SET_SOUND: strcpy(val, ui.soundOn ? "on" : "off"); break;
      case SET_BRIGHTNESS: strcpy(val, BRIGHT[ui.brightness % 3]); break;
      case SET_AUTO_OFF: snprintf(val, sizeof(val), "%d min", AUTO_OFF_MINUTES[ui.autoOff % 3]); break;
      case SET_BEDTIME: strcpy(val, pet.bedtimeOn ? "8pm" : "off"); break;
      default: break;
    }
    uint16_t col = (i == SET_SCHOOL && pet.schoolMode) ? HIGHLIGHT : rgb(140, 120, 120);
    text(c, val, SCREEN_W - 8, y, col, &fonts::Font2, lgfx::middle_right);
  }
  drawHint(c, "B: move  A: change", "PWR: back");
}

static void screenNewEgg(LGFX_Sprite& c, const PetState& pet, const UiState& ui, uint32_t ms) {
  c.fillRect(0, 0, SCREEN_W, SCREEN_H, CREAM);
  drawTitle(c, "New egg?");
  PetLook look = lookFor(pet);
  look.face = FACE_OOPS;
  drawPet(c, look, CX, 110, 0, ms);
  text(c, "This replaces", CX, 132, INK);
  char buf[32];
  snprintf(buf, sizeof(buf), "%s forever.", pet.name);
  text(c, buf, CX, 150, INK);

  c.drawRoundRect(12, 170, SCREEN_W - 24, 14, 5, INK);
  int w = (SCREEN_W - 28) * ui.holdMs / 3000;
  if (w > 0) c.fillRoundRect(14, 172, w, 10, 4, HIGHLIGHT);
  drawHint(c, "Hold A for 3 sec", "B or PWR: no!");
}

static void screenBye(LGFX_Sprite& c, const PetState& pet, uint32_t ms) {
  drawNight(c, ms);
  c.fillRect(0, 0, SCREEN_W, SCENE_Y, NIGHT);
  c.fillRect(0, BAR_Y, SCREEN_W, SCREEN_H - BAR_Y, NIGHT_GRASS);
  PetLook look = lookFor(pet);
  look.face = pet.hatched ? FACE_HAPPY : FACE_SLEEP;
  if (pet.hatched) drawPet(c, look, CX, GROUND_Y, 0, ms);
  bubbleText(c, "Bye bye!", CX, 80);
  text(c, "Press PWR to wake", CX, 220, WHITE);
}

void uiDraw(LGFX_Sprite& c, const PetState& pet, const UiState& ui, uint32_t ms) {
  c.setSwapBytes(true);  // the meadow image is stored as plain RGB565
  switch (ui.screen) {
    case SCR_EGG: screenEgg(c, ui, ms); break;
    case SCR_PICK_COLOR: screenPick(c, ui, ms, true); break;
    case SCR_PICK_EARS: screenPick(c, ui, ms, false); break;
    case SCR_PICK_NAME: screenName(c, pet, ui, ms); break;
    case SCR_SET_CLOCK: screenClock(c, ui, ms); break;
    case SCR_HOME:
    case SCR_FEED: screenHome(c, pet, ui, ms); break;
    case SCR_GAME: screenGame(c, pet, ui, ms); break;
    case SCR_INFO: screenInfo(c, pet, ui, ms); break;
    case SCR_SETTINGS: screenSettings(c, pet, ui); break;
    case SCR_NEW_EGG: screenNewEgg(c, pet, ui, ms); break;
    case SCR_BYE: screenBye(c, pet, ms); break;
  }
}
