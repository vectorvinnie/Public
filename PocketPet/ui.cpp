#include "ui.h"
#include "config.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

using lgfx::LGFX_Sprite;

// ---------------------------------------------------------------------------
// Layout (landscape 320 x 170)
// ---------------------------------------------------------------------------
static const int TOP_H = 20;      // name, needs, clock, battery
static const int BAR_Y = 130;     // action bar along the bottom
static const int GROUND_Y = 122;  // where the pet's feet are
static const int GRASS_Y = 96;    // where the meadow starts
static const int CX = SCREEN_W / 2;

const uint8_t AUTO_OFF_MINUTES[3] = {1, 2, 5};

// ---------------------------------------------------------------------------
// Colors. Always build them with rgb() so they stay uint16_t (RGB565):
// LovyanGFX reads a 32-bit color argument as RGB888.
// ---------------------------------------------------------------------------
static inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return lgfx::color565(r, g, b); }

static const uint16_t INK = rgb(60, 40, 50);
static const uint16_t SOFT_INK = rgb(140, 120, 120);
static const uint16_t WHITE = rgb(255, 255, 255);
static const uint16_t CREAM = rgb(255, 248, 235);
static const uint16_t CREAM_DARK = rgb(235, 220, 200);
static const uint16_t BLUSH = rgb(255, 150, 170);
static const uint16_t INNER_EAR = rgb(255, 190, 200);
static const uint16_t MOUTH = rgb(170, 50, 70);
static const uint16_t NIGHT = rgb(28, 30, 70);
static const uint16_t NIGHT_HILL = rgb(36, 60, 80);
static const uint16_t NIGHT_GRASS = rgb(30, 70, 60);
static const uint16_t STAR_YELLOW = rgb(255, 220, 90);
static const uint16_t GOOD = rgb(110, 200, 110);
static const uint16_t MEH = rgb(250, 200, 70);
static const uint16_t BAD = rgb(240, 100, 100);
static const uint16_t POOP = rgb(150, 100, 60);
static const uint16_t POOP_LIGHT = rgb(185, 135, 90);
static const uint16_t HIGHLIGHT = rgb(255, 120, 170);
static const uint16_t GRASS = rgb(170, 220, 100);
static const uint16_t GRASS_DARK = rgb(130, 190, 80);
static const uint16_t SHADOW = rgb(130, 180, 80);

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

// Little arrow heads for "use the D-pad here". dir: 0 up, 1 right, 2 down, 3 left.
static void arrow(LGFX_Sprite& c, int x, int y, int s, int dir, uint16_t color) {
  switch (dir) {
    case 0: c.fillTriangle(x, y - s, x - s, y + s / 2, x + s, y + s / 2, color); break;
    case 1: c.fillTriangle(x + s, y, x - s / 2, y - s, x - s / 2, y + s, color); break;
    case 2: c.fillTriangle(x, y + s, x - s, y - s / 2, x + s, y - s / 2, color); break;
    default: c.fillTriangle(x - s, y, x + s / 2, y - s, x + s / 2, y + s, color); break;
  }
}

static void bubbleText(LGFX_Sprite& c, const char* s, int x, int y) {
  c.setFont(&fonts::Font2);
  int w = c.textWidth(s) + 16;
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
  ICON_CUPCAKE, ICON_APPLE, ICON_STAR, ICON_BOLT,
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

static void drawIcon(LGFX_Sprite& c, Icon icon, int x, int y, int s, uint16_t bg = CREAM) {
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
      c.fillCircle(x + h * 0.45f, y - h * 0.3f, h * 0.8f, bg);
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
      c.fillCircle(x, y, h * 0.3f, bg);
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
    case STAGE_BABY: return 21;
    case STAGE_KID: return 26;
    default: return 30;
  }
}

static void fillTri(LGFX_Sprite& c, float x0, float y0, float x1, float y1, float x2, float y2,
                    uint16_t color) {
  c.fillTriangle((int)x0, (int)y0, (int)x1, (int)y1, (int)x2, (int)y2, color);
}

// Shrink a triangle toward its middle (for inner ears).
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
    if (f == FACE_SAD) {  // worried eyebrows, inner ends up
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
static void drawPet(LGFX_Sprite& c, const PetLook& p, int cx, int gy, float bob, uint32_t ms,
                    uint16_t shadow = SHADOW) {
  Palette pal = petPalette(p.color);
  int s = petSize(p.stage);
  int w = s + (int)bob;                // half width
  int h = (int)(s * 0.86f) - (int)bob; // half height
  int cy = gy - h - 2;
  int top = cy - h;

  c.fillEllipse(cx, gy, s * 0.8f, 3, shadow);
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
// The meadow (drawn from shapes, so it fits any screen)
// ---------------------------------------------------------------------------
static void cloud(LGFX_Sprite& c, int x, int y) {
  c.fillCircle(x, y, 9, WHITE);
  c.fillCircle(x + 11, y - 5, 11, WHITE);
  c.fillCircle(x + 24, y, 9, WHITE);
  c.fillRect(x, y, 25, 9, WHITE);
}

static void tree(LGFX_Sprite& c, int x, int y, bool night) {
  c.fillRect(x - 3, y - 16, 7, 18, night ? rgb(60, 50, 60) : rgb(150, 100, 60));
  uint16_t leaf = night ? rgb(30, 80, 70) : rgb(80, 170, 90);
  uint16_t leafLight = night ? rgb(40, 95, 80) : rgb(110, 195, 110);
  c.fillCircle(x - 9, y - 22, 11, leaf);
  c.fillCircle(x + 9, y - 22, 11, leaf);
  c.fillCircle(x, y - 34, 13, leaf);
  c.fillCircle(x - 3, y - 36, 6, leafLight);
}

static void drawScene(LGFX_Sprite& c, int top, int bottom, bool night, uint32_t ms) {
  // sky
  for (int y = top; y < GRASS_Y; y += 4) {
    float t = (float)(y - top) / (GRASS_Y - top);
    uint16_t col = night ? rgb(20 + 20 * t, 22 + 20 * t, 60 + 25 * t)
                         : rgb(140 + 60 * t, 205 + 30 * t, 250);
    c.fillRect(0, y, SCREEN_W, 4, col);
  }
  if (night) {
    static const uint16_t sx[] = {20, 60, 105, 140, 190, 230, 262, 300, 85, 210, 35, 165};
    static const uint8_t sy[] = {30, 52, 34, 60, 28, 50, 70, 40, 72, 76, 66, 44};
    for (int i = 0; i < 12; i++) {
      if (sy[i] < top + 2) continue;
      bool twinkle = ((ms / 400) + i * 3) % 7 == 0;
      c.fillCircle(sx[i], sy[i], twinkle ? 2 : 1, STAR_YELLOW);
    }
    c.fillCircle(280, top + 22, 12, STAR_YELLOW);
    c.fillCircle(287, top + 16, 11, rgb(24, 26, 64));
  } else {
    c.fillCircle(285, top + 22, 13, rgb(255, 235, 140));
    c.fillCircle(285, top + 22, 10, rgb(255, 215, 90));
    int drift = ms / 250;
    cloud(c, (40 + drift) % (SCREEN_W + 60) - 40, top + 22);
    cloud(c, (190 + drift / 2) % (SCREEN_W + 60) - 40, top + 40);
  }

  // hills and trees
  c.fillEllipse(70, GRASS_Y + 10, 120, 30, night ? NIGHT_HILL : rgb(150, 210, 120));
  c.fillEllipse(270, GRASS_Y + 14, 130, 34, night ? rgb(32, 55, 74) : rgb(135, 200, 110));
  tree(c, 34, GRASS_Y + 4, night);
  tree(c, 290, GRASS_Y + 6, night);

  // grass with a few tufts
  c.fillRect(0, GRASS_Y + 4, SCREEN_W, bottom - GRASS_Y - 4, night ? NIGHT_GRASS : GRASS);
  static const uint16_t tx[] = {14, 52, 96, 128, 200, 236, 272, 306, 70, 250};
  static const uint8_t ty[] = {110, 124, 106, 132, 112, 128, 116, 104, 120, 106};
  uint16_t tuft = night ? rgb(40, 90, 70) : GRASS_DARK;
  for (int i = 0; i < 10; i++) {
    if (ty[i] >= bottom - 2) continue;
    c.drawLine(tx[i], ty[i], tx[i] - 3, ty[i] - 5, tuft);
    c.drawLine(tx[i], ty[i], tx[i], ty[i] - 6, tuft);
    c.drawLine(tx[i], ty[i], tx[i] + 3, ty[i] - 5, tuft);
  }
}

static void drawPoops(LGFX_Sprite& c, int count, int hideRightOf, uint32_t ms) {
  static const int px[] = {84, 236, 108};
  static const int py[] = {GROUND_Y - 2, GROUND_Y + 2, GROUND_Y + 6};
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
  Icon icon;
  switch (need) {
    case NEED_FOOD: icon = ICON_APPLE; break;
    case NEED_FUN: icon = ICON_BALL; break;
    case NEED_SLEEP: icon = ICON_MOON; break;
    case NEED_CLEAN: icon = ICON_BROOM; break;
    default: return;
  }
  c.fillCircle(x - 16, y + 18, 2, WHITE);
  c.fillCircle(x - 10, y + 12, 3, WHITE);
  c.fillCircle(x, y, 14, INK);
  c.fillCircle(x, y, 13, WHITE);
  drawIcon(c, icon, x, y, 15, WHITE);
}

static void drawZzz(LGFX_Sprite& c, int x, int y, uint32_t ms) {
  for (int i = 0; i < 3; i++) {
    int t = (ms / 30 + i * 33) % 100;  // 0..99 rise
    text(c, "z", x + t / 5 + i * 2, y - t / 3, WHITE, i == 1 ? &fonts::Font4 : &fonts::Font2);
  }
}

static void drawBattery(LGFX_Sprite& c, int pct, int bx, int by, uint32_t ms) {
  bool low = pct >= 0 && pct < 15;
  if (low && !blinkOn(ms, 1000, 600)) return;
  uint16_t edge = low ? BAD : INK;
  c.drawRoundRect(bx, by, 19, 10, 2, edge);
  c.fillRect(bx + 19, by + 3, 2, 4, edge);
  if (pct >= 0) c.fillRect(bx + 2, by + 2, 15 * pct / 100 + 1, 6, low ? BAD : GOOD);
}

static void drawTopBar(LGFX_Sprite& c, const PetState& pet, const UiState& ui, uint32_t ms) {
  c.fillRect(0, 0, SCREEN_W, TOP_H, CREAM);
  c.drawFastHLine(0, TOP_H - 1, SCREEN_W, CREAM_DARK);
  text(c, pet.name, 6, TOP_H / 2, INK, &fonts::Font2, lgfx::middle_left);

  // food / fun / energy
  const float vals[3] = {pet.food, pet.fun, pet.energy};
  const Icon icons[3] = {ICON_APPLE, ICON_STAR, ICON_BOLT};
  for (int i = 0; i < 3; i++) {
    int x = 86 + i * 54;
    int y = TOP_H / 2;
    drawIcon(c, icons[i], x, y, 12);
    int v = (int)(vals[i] + 0.5f);
    uint16_t col = v > 50 ? GOOD : (v > NEED_LOW ? MEH : BAD);
    c.drawRoundRect(x + 9, y - 5, 34, 10, 3, CREAM_DARK);
    if (v > 0) c.fillRoundRect(x + 10, y - 4, 32 * v / 100 + 1, 8, 2, col);
  }

  char buf[8];
  if (ui.clock.valid) snprintf(buf, sizeof(buf), "%d:%02d", ui.clock.hour, ui.minute);
  else strcpy(buf, "--:--");
  text(c, buf, SCREEN_W - 30, TOP_H / 2, SOFT_INK, &fonts::Font2, lgfx::middle_right);
  drawBattery(c, ui.batteryPercent, SCREEN_W - 25, 5, ms);
}

static void drawBarBackground(LGFX_Sprite& c) {
  c.fillRect(0, BAR_Y, SCREEN_W, SCREEN_H - BAR_Y, CREAM);
  c.drawFastHLine(0, BAR_Y, SCREEN_W, CREAM_DARK);
}

// A row of icons; the selected one is boxed and labelled.
static void drawIconBar(LGFX_Sprite& c, const Icon* icons, const char* const* labels, int count,
                        int sel, int attention) {
  drawBarBackground(c);
  float slot = (float)SCREEN_W / count;
  for (int i = 0; i < count; i++) {
    int x = (int)(slot * i + slot / 2);
    if (i == sel) {
      c.fillRoundRect(x - slot / 2 + 3, BAR_Y + 3, slot - 6, SCREEN_H - BAR_Y - 6, 8,
                      i == attention ? rgb(255, 225, 235) : WHITE);
      c.drawRoundRect(x - slot / 2 + 3, BAR_Y + 3, slot - 6, SCREEN_H - BAR_Y - 6, 8, HIGHLIGHT);
      drawIcon(c, icons[i], x, BAR_Y + 15, 17, i == attention ? rgb(255, 225, 235) : WHITE);
      text(c, labels[i], x, BAR_Y + 31, INK, &fonts::Font0);
    } else {
      drawIcon(c, icons[i], x, BAR_Y + 20, 17);
      if (i == attention) c.fillCircle(x + 10, BAR_Y + 8, 3, HIGHLIGHT);
    }
  }
}

static void drawHint(LGFX_Sprite& c, const char* line1, const char* line2) {
  drawBarBackground(c);
  if (line2) {
    text(c, line1, CX, BAR_Y + 12, INK);
    text(c, line2, CX, BAR_Y + 28, SOFT_INK);
  } else {
    text(c, line1, CX, BAR_Y + 20, INK);
  }
}

static void drawTitle(LGFX_Sprite& c, const char* title, const char* hint = nullptr) {
  c.fillRect(0, 0, SCREEN_W, TOP_H + 2, CREAM);
  c.drawFastHLine(0, TOP_H + 2, SCREEN_W, CREAM_DARK);
  text(c, title, 8, TOP_H / 2 + 1, INK, &fonts::Font2, lgfx::middle_left);
  if (hint) text(c, hint, SCREEN_W - 8, TOP_H / 2 + 1, SOFT_INK, &fonts::Font2, lgfx::middle_right);
}

static const Icon HOME_ICONS[ACT_COUNT] = {
  ICON_RICEBALL, ICON_BALL, ICON_BROOM, ICON_MOON, ICON_HEART, ICON_CHART, ICON_GEAR};
static const char* const HOME_LABELS[ACT_COUNT] = {
  "Feed", "Play", "Clean", "Lights", "Cuddle", "Stats", "Setup"};
static const Icon FEED_ICONS[FEED_COUNT] = {ICON_RICEBALL, ICON_CUPCAKE};
static const char* const FEED_LABELS[FEED_COUNT] = {"Meal", "Treat"};

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

static int headY(PetStage stage) { return GROUND_Y - petSize(stage) * 2 - 6; }

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------
static void screenHome(LGFX_Sprite& c, const PetState& pet, const UiState& ui, uint32_t ms) {
  PetMood mood = petMood(pet);
  uint32_t t = ms - ui.animStart;
  Anim anim = ui.anim;

  drawScene(c, TOP_H, BAR_Y, pet.asleep, ms);

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
    default:
      break;
  }

  drawPoops(c, pet.poops, broomX, ms);
  drawPet(c, look, x, GROUND_Y - hop, bob, ms, pet.asleep ? rgb(25, 60, 50) : SHADOW);

  int s = petSize(look.stage);
  int hy = headY(look.stage);

  if (anim == ANIM_MEAL || anim == ANIM_TREAT) {
    int bite = t / 450;  // 0..3
    if (bite < 3) {
      drawIcon(c, anim == ANIM_MEAL ? ICON_RICEBALL : ICON_CUPCAKE, x + s + 14, GROUND_Y - s,
               22 - bite * 5);
    }
  }
  if (anim == ANIM_CLEAN && broomX < SCREEN_W + 10) {
    c.drawWideLine(broomX + 14, GROUND_Y - 34, broomX + 2, GROUND_Y - 6, 2, rgb(150, 100, 60));
    c.fillTriangle(broomX + 3, GROUND_Y - 8, broomX - 10, GROUND_Y + 8, broomX + 14, GROUND_Y + 8,
                   rgb(240, 200, 80));
  }
  if (anim == ANIM_CLEAN && t > 1200) {
    for (int i = 0; i < 7; i++) star(c, 30 + i * 44, GROUND_Y - 8 - (i % 2) * 16, 4, WHITE);
  }
  if (anim == ANIM_CUDDLE) {
    for (int i = 0; i < 3; i++) {
      int rise = (int)((t + i * 300) % 900) / 12;
      heart(c, x - 26 + i * 26, hy + 8 - rise, 5, HIGHLIGHT);
    }
  }

  if (mood == MOOD_ASLEEP && anim == ANIM_NONE) drawZzz(c, x + s, hy + 6, ms);
  if (mood == MOOD_PAUSED) {
    c.fillRoundRect(CX - 80, TOP_H + 6, 160, 38, 8, WHITE);
    c.drawRoundRect(CX - 80, TOP_H + 6, 160, 38, 8, HIGHLIGHT);
    text(c, "School mode", CX, TOP_H + 17, INK);
    text(c, "see you later!", CX, TOP_H + 33, SOFT_INK);
  }

  if (ui.bubble[0] && (int32_t)(ui.bubbleUntil - ms) > 0) {
    bubbleText(c, ui.bubble, x, hy - 14);
  } else if (anim == ANIM_NONE) {
    PetNeed need = petNeed(pet);
    if (need != NEED_NONE && blinkOn(ms, 3000, 2000)) drawThought(c, need, x + s + 22, hy - 4);
  }

  drawTopBar(c, pet, ui, ms);
  if (ui.screen == SCR_FEED) {
    // Two big choices in the middle of the bar.
    drawBarBackground(c);
    for (int i = 0; i < FEED_COUNT; i++) {
      int bx = CX - 50 + i * 100;
      bool sel = i == ui.cursor;
      c.fillRoundRect(bx - 44, BAR_Y + 3, 88, SCREEN_H - BAR_Y - 6, 8, sel ? WHITE : CREAM);
      if (sel) c.drawRoundRect(bx - 44, BAR_Y + 3, 88, SCREEN_H - BAR_Y - 6, 8, HIGHLIGHT);
      drawIcon(c, FEED_ICONS[i], bx - 22, BAR_Y + 20, 18, sel ? WHITE : CREAM);
      text(c, FEED_LABELS[i], bx + 8, BAR_Y + 20, sel ? INK : SOFT_INK, &fonts::Font2,
           lgfx::middle_left);
    }
    arrow(c, 14, BAR_Y + 20, 5, 3, CREAM_DARK);
    arrow(c, SCREEN_W - 14, BAR_Y + 20, 5, 1, CREAM_DARK);
  } else {
    PetNeed need = petNeed(pet);
    int attention = need == NEED_FOOD ? ACT_FEED : need == NEED_FUN ? ACT_PLAY
                  : need == NEED_SLEEP ? ACT_LIGHT : need == NEED_CLEAN ? ACT_CLEAN : -1;
    drawIconBar(c, HOME_ICONS, HOME_LABELS, ACT_COUNT, ui.cursor, attention);
  }
}

static void screenEgg(LGFX_Sprite& c, const UiState& ui, uint32_t ms) {
  uint32_t t = ms - ui.animStart;
  if (ui.anim == ANIM_HATCH && t > 300) {  // white flash
    c.fillRect(0, 0, SCREEN_W, SCREEN_H, WHITE);
    star(c, CX, 78, 10 + t / 18, STAR_YELLOW);
    return;
  }
  drawScene(c, 0, BAR_Y, false, ms);
  int wob = 0;
  if (ui.anim == ANIM_HATCH || (ms % 2000) < 400) wob = (int)(sinf(ms / 50.0f) * 3);
  int ex = CX + wob, ey = 92;

  c.fillEllipse(CX, ey + 34, 26, 4, SHADOW);
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

  bubbleText(c, taps == 0 ? "An egg!" : (taps == 1 ? "It moved!" : "Almost..."), CX, 28);
  drawHint(c, "Press A to hatch!", nullptr);
}

static void screenPick(LGFX_Sprite& c, const UiState& ui, uint32_t ms, bool colors) {
  drawScene(c, TOP_H, BAR_Y, false, ms);
  drawTitle(c, colors ? "Pick a color" : "Pick a style", colors ? "1 of 3" : "2 of 3");
  PetLook p = {};
  p.color = ui.pickColor;
  p.ears = ui.pickEars;
  p.stage = STAGE_KID;
  p.face = FACE_HAPPY;
  drawPet(c, p, CX, GROUND_Y, sinf(ms / 260.0f) * 1.2f, ms);
  int pulse = blinkOn(ms, 800, 400) ? 2 : 0;
  arrow(c, CX - 70 - pulse, GROUND_Y - 28, 9, 3, WHITE);
  arrow(c, CX + 70 + pulse, GROUND_Y - 28, 9, 1, WHITE);
  bubbleText(c, colors ? PET_COLOR_NAMES[ui.pickColor] : EAR_NAMES[ui.pickEars], CX,
             headY(STAGE_KID) - 14);
  drawHint(c, "Left / Right: change", colors ? "A: choose" : "A: choose    B: go back");
}

static void screenName(LGFX_Sprite& c, const UiState& ui, uint32_t ms) {
  c.fillRect(0, 0, SCREEN_W, SCREEN_H, CREAM);
  drawTitle(c, "Name your pet", "3 of 3");

  PetLook p = {};
  p.color = ui.pickColor;
  p.ears = ui.pickEars;
  p.stage = STAGE_BABY;
  drawPet(c, p, 46, 112, 0, ms, CREAM_DARK);

  // Eight letter slots; the one being changed has arrows above and below.
  const int boxW = 24, gap = 3, left = 96, y = 78;
  for (int i = 0; i < PET_NAME_MAX; i++) {
    int x = left + i * (boxW + gap);
    bool cur = i == ui.nameCursor;
    c.fillRoundRect(x, y - 17, boxW, 34, 5, cur ? WHITE : rgb(250, 240, 225));
    if (cur) c.drawRoundRect(x, y - 17, boxW, 34, 5, HIGHLIGHT);
    char ch[2] = {ui.nameBuf[i], 0};
    if (ch[0] != ' ') text(c, ch, x + boxW / 2, y + 1, INK, &fonts::Font4);
    else c.drawFastHLine(x + 6, y + 10, boxW - 12, CREAM_DARK);
    if (cur) {
      int pulse = blinkOn(ms, 800, 400) ? 1 : 0;
      arrow(c, x + boxW / 2, y - 26 - pulse, 5, 0, HIGHLIGHT);
      arrow(c, x + boxW / 2, y + 26 + pulse, 5, 2, HIGHLIGHT);
    }
  }
  drawHint(c, "Up/Down: letter   Left/Right: move", "A: done   B: erase   SELECT: surprise");
}

static void screenClock(LGFX_Sprite& c, const UiState& ui, uint32_t ms) {
  c.fillRect(0, 0, SCREEN_W, SCREEN_H, CREAM);
  drawTitle(c, "What time is it?", "24-hour clock");
  drawIcon(c, ICON_MOON, 40, 78, 30);

  char hh[4], mm[4];
  snprintf(hh, sizeof(hh), "%02d", ui.clockHour);
  snprintf(mm, sizeof(mm), "%02d", ui.clockMinute);
  int y = 78;
  for (int f = 0; f < 2; f++) {
    int x = f == 0 ? CX - 36 : CX + 36;
    bool active = ui.clockField == f;
    c.fillRoundRect(x - 28, y - 22, 56, 44, 8, active ? WHITE : CREAM);
    if (active) {
      c.drawRoundRect(x - 28, y - 22, 56, 44, 8, HIGHLIGHT);
      arrow(c, x, y - 32, 6, 0, HIGHLIGHT);
      arrow(c, x, y + 32, 6, 2, HIGHLIGHT);
    }
    text(c, f == 0 ? hh : mm, x, y + 1, INK, &fonts::Font4);
  }
  text(c, ":", CX, y - 1, INK, &fonts::Font4);
  (void)ms;
  drawHint(c, "Up/Down: change   Left/Right: hours / minutes", "A: done");
}

static void screenGame(LGFX_Sprite& c, const PetState& pet, const UiState& ui, uint32_t ms) {
  drawScene(c, TOP_H, BAR_Y, false, ms);
  drawTopBar(c, pet, ui, ms);

  PetLook look = lookFor(pet);
  look.face = FACE_NORMAL;
  int x = CX;
  bool revealed = ui.gameGuess >= 0;
  bool won = revealed && ui.gameGuess == ui.gameLook;
  bool finished = revealed && ui.gameRound >= GAME_ROUNDS;
  if (revealed) {
    look.look = ui.gameLook ? 1 : -1;
    look.face = won ? FACE_HAPPY : FACE_OOPS;
    x += look.look * 14;
  } else {
    look.look = (ms / 300) % 3 - 1;  // looking around, thinking
  }
  drawPet(c, look, x, GROUND_Y, sinf(ms / 260.0f) * 1.2f, ms);

  int hy = headY(look.stage);
  if (finished && ms - ui.gameShownAt > 900) {
    char buf[24];
    snprintf(buf, sizeof(buf), "You got %d!", ui.gameScore);
    bubbleText(c, buf, CX, hy - 14);
  } else if (revealed) {
    bubbleText(c, won ? "Yay! Again!" : "Hehe, nope!", x, hy - 14);
  } else {
    bubbleText(c, "Which way?", CX, hy - 14);
    int pulse = blinkOn(ms, 600, 300) ? 3 : 0;
    arrow(c, 50 - pulse, GROUND_Y - 30, 14, 3, WHITE);
    arrow(c, SCREEN_W - 50 + pulse, GROUND_Y - 30, 14, 1, WHITE);
  }

  // score stars
  for (int i = 0; i < GAME_ROUNDS; i++) {
    int sx = 22 + i * 18;
    if (i < ui.gameScore) star(c, sx, TOP_H + 14, 7, STAR_YELLOW);
    else c.drawCircle(sx, TOP_H + 14, 4, INK);
  }
  char round[16];
  snprintf(round, sizeof(round), "Round %d of %d", ui.gameRound, GAME_ROUNDS);
  text(c, round, 22 + GAME_ROUNDS * 18, TOP_H + 14, INK, &fonts::Font2, lgfx::middle_left);

  drawHint(c, "Guess with Left or Right!", "B: stop playing");
}

static void statRow(LGFX_Sprite& c, Icon icon, const char* name, float value, int x, int y) {
  drawIcon(c, icon, x + 8, y, 14);
  text(c, name, x + 20, y - 1, INK, &fonts::Font2, lgfx::middle_left);
  int v = (int)(value + 0.5f);
  uint16_t col = v > 50 ? GOOD : (v > NEED_LOW ? MEH : BAD);
  c.drawRoundRect(x + 74, y - 7, 82, 14, 4, CREAM_DARK);
  if (v > 0) c.fillRoundRect(x + 75, y - 6, 80 * v / 100 + 1, 12, 3, col);
}

static void screenInfo(LGFX_Sprite& c, const PetState& pet, const UiState& ui, uint32_t ms) {
  c.fillRect(0, 0, SCREEN_W, SCREEN_H, CREAM);
  drawTitle(c, pet.name, "B: back");

  PetLook look = lookFor(pet);
  drawPet(c, look, 70, 140, sinf(ms / 260.0f) * 1.2f, ms, CREAM_DARK);

  const int x = 146;
  statRow(c, ICON_APPLE, "Food", pet.food, x, 40);
  statRow(c, ICON_STAR, "Fun", pet.fun, x, 62);
  statRow(c, ICON_BOLT, "Energy", pet.energy, x, 84);

  static const char* const STAGES[] = {"Egg", "Baby", "Kid", "Grown-up"};
  char buf[40];
  uint32_t days = pet.ageSeconds / 86400, hours = pet.ageSeconds / 3600 % 24;
  snprintf(buf, sizeof(buf), "%s, %lu days %lu hours", STAGES[petStage(pet)], (unsigned long)days,
           (unsigned long)hours);
  text(c, buf, x, 112, INK, &fonts::Font2, lgfx::middle_left);
  heart(c, x + 8, 136, 6, HIGHLIGHT);
  snprintf(buf, sizeof(buf), "%lu hearts", (unsigned long)pet.hearts);
  text(c, buf, x + 20, 136, INK, &fonts::Font2, lgfx::middle_left);
  (void)ui;
}

static void screenSettings(LGFX_Sprite& c, const PetState& pet, const UiState& ui) {
  c.fillRect(0, 0, SCREEN_W, SCREEN_H, CREAM);
  drawTitle(c, "Settings", "A: change   B: back");

  static const char* const NAMES[SET_COUNT] = {
    "School mode", "Sound", "Brightness", "Auto sleep", "Bedtime (8pm-7am)", "Set clock",
    "Update over WiFi", "New egg"};
  static const char* const BRIGHT[3] = {"Low", "Mid", "High"};
  const int rowH = 18, top = TOP_H + 4;
  for (int i = 0; i < SET_COUNT; i++) {
    int y = top + i * rowH + rowH / 2;
    if (i == ui.cursor) {
      c.fillRoundRect(4, y - rowH / 2 + 1, SCREEN_W - 8, rowH - 2, 6, WHITE);
      c.drawRoundRect(4, y - rowH / 2 + 1, SCREEN_W - 8, rowH - 2, 6, HIGHLIGHT);
    }
    text(c, NAMES[i], 12, y, INK, &fonts::Font2, lgfx::middle_left);
    char val[12] = "";
    switch (i) {
      case SET_SCHOOL: strcpy(val, pet.schoolMode ? "ON" : "off"); break;
      case SET_SOUND: strcpy(val, ui.soundOn ? "on" : "off"); break;
      case SET_BRIGHTNESS: strcpy(val, BRIGHT[ui.brightness % 3]); break;
      case SET_AUTO_OFF: snprintf(val, sizeof(val), "%d min", AUTO_OFF_MINUTES[ui.autoOff % 3]); break;
      case SET_BEDTIME: strcpy(val, pet.bedtimeOn ? "on" : "off"); break;
      case SET_CLOCK:
        if (ui.clock.valid) snprintf(val, sizeof(val), "%d:%02d", ui.clock.hour, ui.minute);
        else strcpy(val, "not set");
        break;
      default: break;
    }
    uint16_t col = (i == SET_SCHOOL && pet.schoolMode) ? HIGHLIGHT : SOFT_INK;
    text(c, val, SCREEN_W - 14, y, col, &fonts::Font2, lgfx::middle_right);
  }
}

static void screenNewEgg(LGFX_Sprite& c, const PetState& pet, const UiState& ui, uint32_t ms) {
  c.fillRect(0, 0, SCREEN_W, SCREEN_H, CREAM);
  drawTitle(c, "New egg?");
  PetLook look = lookFor(pet);
  look.face = FACE_OOPS;
  drawPet(c, look, 70, 124, 0, ms, CREAM_DARK);
  text(c, "This replaces", 210, 52, INK);
  char buf[32];
  snprintf(buf, sizeof(buf), "%s forever.", pet.name);
  text(c, buf, 210, 72, INK);

  c.drawRoundRect(140, 94, 140, 16, 5, INK);
  int w = 136 * ui.holdMs / 3000;
  if (w > 0) c.fillRoundRect(142, 96, w, 12, 4, HIGHLIGHT);
  drawHint(c, "Hold A for 3 seconds", "B: no, keep it!");
}

static void wifiIcon(LGFX_Sprite& c, int x, int y, uint16_t color, int bars) {
  c.fillCircle(x, y, 4, color);
  for (int i = 1; i <= 3; i++) {
    if (i <= bars) c.fillArc(x, y, 6 + i * 9, 1 + i * 9, 225, 315, color);
  }
}

static void screenWifi(LGFX_Sprite& c, const UiState& ui, uint32_t ms) {
  c.fillRect(0, 0, SCREEN_W, SCREEN_H, CREAM);
  drawTitle(c, "Update over WiFi", ui.wifiUpdating ? nullptr : "B: stop");
  const WifiUpdateStatus& w = ui.wifi;
  const int tx = 104;
  char line[64];

  if (ui.wifiUpdating) {
    wifiIcon(c, 52, 100, HIGHLIGHT, 3);
    text(c, "Getting the new version...", tx, 52, INK, &fonts::Font2, lgfx::middle_left);
    text(c, "Don't switch off!", tx, 72, SOFT_INK, &fonts::Font2, lgfx::middle_left);
    c.drawRoundRect(tx, 92, 200, 18, 6, INK);
    int fill = 196 * ui.wifiProgress / 100;
    if (fill > 0) c.fillRoundRect(tx + 2, 94, fill, 14, 5, HIGHLIGHT);
    snprintf(line, sizeof(line), "%d%%", ui.wifiProgress);
    text(c, line, tx + 100, 124, INK);
    return;
  }

  switch (w.state) {
    case WIFI_CONNECTING:
      wifiIcon(c, 52, 100, SOFT_INK, 1 + (ms / 400) % 3);
      snprintf(line, sizeof(line), "Joining %s", w.network);
      text(c, line, tx, 70, INK, &fonts::Font2, lgfx::middle_left);
      text(c, (ms / 400) % 4 == 0 ? "" : (ms / 400) % 4 == 1 ? "." : (ms / 400) % 4 == 2 ? ".." : "...",
           tx, 90, INK, &fonts::Font2, lgfx::middle_left);
      break;
    case WIFI_READY: {
      wifiIcon(c, 52, 100, GOOD, 3);
      // "pocketpet-3a7f at 192.168.1.23": the name on one line, the address below.
      char name[48];
      snprintf(name, sizeof(name), "%s", w.address);
      char* at = strstr(name, " at ");
      if (at) *at = '\0';
      text(c, "Ready for a new version!", tx, 40, INK, &fonts::Font2, lgfx::middle_left);
      text(c, "In the Arduino IDE, pick this", tx, 62, SOFT_INK, &fonts::Font2, lgfx::middle_left);
      text(c, "under Tools > Port:", tx, 80, SOFT_INK, &fonts::Font2, lgfx::middle_left);
      c.fillRoundRect(tx - 6, 92, SCREEN_W - tx - 4, 40, 6, WHITE);
      text(c, name, tx, 103, INK, &fonts::Font2, lgfx::middle_left);
      if (at) text(c, at + 1, tx, 121, SOFT_INK, &fonts::Font2, lgfx::middle_left);
      text(c, "then click Upload.", tx, 148, SOFT_INK, &fonts::Font2, lgfx::middle_left);
      break;
    }
    case WIFI_FAILED:
      wifiIcon(c, 52, 100, BAD, 1);
      snprintf(line, sizeof(line), "Couldn't join %s.", w.network);
      text(c, line, tx, 60, INK, &fonts::Font2, lgfx::middle_left);
      text(c, "Check the WiFi name and", tx, 84, SOFT_INK, &fonts::Font2, lgfx::middle_left);
      text(c, "password in wifi_secrets.h.", tx, 102, SOFT_INK, &fonts::Font2, lgfx::middle_left);
      break;
    case WIFI_UNAVAILABLE:
      wifiIcon(c, 52, 100, CREAM_DARK, 3);
      text(c, "This is the browser version.", tx, 70, INK, &fonts::Font2, lgfx::middle_left);
      text(c, "WiFi updates are for the handheld.", tx, 92, SOFT_INK, &fonts::Font2, lgfx::middle_left);
      break;
    default:  // not set up
      wifiIcon(c, 52, 100, CREAM_DARK, 3);
      text(c, "WiFi isn't set up yet.", tx, 52, INK, &fonts::Font2, lgfx::middle_left);
      text(c, "Put your WiFi name and", tx, 78, SOFT_INK, &fonts::Font2, lgfx::middle_left);
      text(c, "password in wifi_secrets.h,", tx, 96, SOFT_INK, &fonts::Font2, lgfx::middle_left);
      text(c, "then upload once over USB.", tx, 114, SOFT_INK, &fonts::Font2, lgfx::middle_left);
      break;
  }
}

static void screenBye(LGFX_Sprite& c, const PetState& pet, uint32_t ms) {
  drawScene(c, 0, SCREEN_H, true, ms);
  if (pet.hatched) {
    PetLook look = lookFor(pet);
    look.face = FACE_HAPPY;
    drawPet(c, look, CX, GROUND_Y + 10, 0, ms, rgb(25, 60, 50));
  }
  bubbleText(c, "Bye bye!", CX, 34);
  text(c, "Press A to wake me up", CX, SCREEN_H - 10, WHITE);
}

void uiDraw(LGFX_Sprite& c, const PetState& pet, const UiState& ui, uint32_t ms) {
  switch (ui.screen) {
    case SCR_EGG: screenEgg(c, ui, ms); break;
    case SCR_PICK_COLOR: screenPick(c, ui, ms, true); break;
    case SCR_PICK_EARS: screenPick(c, ui, ms, false); break;
    case SCR_PICK_NAME: screenName(c, ui, ms); break;
    case SCR_SET_CLOCK: screenClock(c, ui, ms); break;
    case SCR_HOME:
    case SCR_FEED: screenHome(c, pet, ui, ms); break;
    case SCR_GAME: screenGame(c, pet, ui, ms); break;
    case SCR_INFO: screenInfo(c, pet, ui, ms); break;
    case SCR_SETTINGS: screenSettings(c, pet, ui); break;
    case SCR_NEW_EGG: screenNewEgg(c, pet, ui, ms); break;
    case SCR_WIFI: screenWifi(c, ui, ms); break;
    case SCR_BYE: screenBye(c, pet, ms); break;
  }
}
