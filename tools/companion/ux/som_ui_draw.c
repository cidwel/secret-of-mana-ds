/* som_ui_draw: layout, drawing and touch handling of the context-aware companion screen.  See som_ui_draw.h.
 *
 * One layout builder (build_layout) produces every rectangle and every tap target of a mode; the drawing code and the
 * touch code both consume it, so what is drawn is exactly what is tappable.  All geometry is in virtual pixels (vp). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "som_ui_priv.h"

/* ------------------------------------------------------------------ palette (Secret of Mana menu look) */
#define C_BG 0x0B1030u
#define C_BG2 0x0F1740u
#define C_WIN_T 0x2C44A0u
#define C_WIN_B 0x0E1A5Cu
#define C_WIN_SEL_T 0x4A5CC8u
#define C_LIGHT 0xE8ECFFu
#define C_MID 0x8F9AD8u
#define C_DARK 0x070A22u
#define C_TXT 0xF8F8F8u
#define C_DIM 0xB0B8E8u
#define C_YEL 0xF8E060u
#define C_RED 0xF06060u
#define C_GRN 0x68E078u
#define C_CYAN 0x60D0F8u
#define C_ORG 0xF8A040u
#define C_GREY 0x4A5078u
#define C_PURPLE 0xC060E0u

struct R { int x, y, w, h; };
static int inr(const struct R *r, int x, int y) { return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h; }

/* ------------------------------------------------------------------ canvas */
struct cv { uint32_t *px; int stride, W, H, s; const struct som_ux_pack *pk; int sq; int hr; };   /* sq: 0 game face, 1 tight (1 vp less per glyph), 2 squeezed (3/4 width) */

/* Layout audit (tests): every cut text, text outside its box or overlapping text rows is counted here while drawing. */
static int g_audit; static char g_audit_note[96];
static void audit(const char *what, const char *s) {
  if (!g_audit) snprintf(g_audit_note, sizeof g_audit_note, "%s: %.60s", what, s ? s : "");
  g_audit++;
}
/* Text log of the hero cards (tests): every text box registered by card() since the last som_ux_audit(1). */
static struct { char s[24]; int x, y, w, h; } g_tlog[96]; static int g_tn;
static int g_mp_bars_reset(void);
int som_ux_audit(int reset) { int n = g_audit; if (reset) { g_audit = 0; g_tn = 0; g_mp_bars_reset(); } return n; }
int som_ux_text_log(int i, char *s, size_t sn, int *x, int *y, int *w, int *h) {
  if (i < 0 || i >= g_tn) return 0;
  snprintf(s, sn, "%s", g_tlog[i].s); *x = g_tlog[i].x; *y = g_tlog[i].y; *w = g_tlog[i].w; *h = g_tlog[i].h;
  return 1;
}
const char *som_ux_audit_note(void) { return g_audit_note; }

static uint32_t mix(uint32_t a, uint32_t b, int t) {   /* t = weight of b, 0..256 */
  uint32_t r = 0;
  for (int sh = 0; sh <= 16; sh += 8) {
    int ca = (a >> sh) & 255, cb = (b >> sh) & 255;
    r |= (uint32_t)((ca * (256 - t) + cb * t) >> 8) << sh;
  }
  return 0xFF000000u | r;
}
static void dpx(struct cv *c, int x, int y, uint32_t col) {
  if ((unsigned)x < (unsigned)c->W && (unsigned)y < (unsigned)c->H) c->px[y * c->stride + x] = 0xFF000000u | col;
}
static void frect(struct cv *c, int x, int y, int w, int h, uint32_t col) {
  int s = c->s, x0 = x * s, y0 = y * s, x1 = (x + w) * s, y1 = (y + h) * s;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > c->W) x1 = c->W;
  if (y1 > c->H) y1 = c->H;
  for (int j = y0; j < y1; j++)
    for (int i = x0; i < x1; i++) c->px[j * c->stride + i] = 0xFF000000u | col;
}
static void brect(struct cv *c, int x, int y, int w, int h, uint32_t col, int a) {   /* blended, a = 0..256 */
  int s = c->s, x0 = x * s, y0 = y * s, x1 = (x + w) * s, y1 = (y + h) * s;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > c->W) x1 = c->W;
  if (y1 > c->H) y1 = c->H;
  for (int j = y0; j < y1; j++)
    for (int i = x0; i < x1; i++) c->px[j * c->stride + i] = mix(c->px[j * c->stride + i], col, a);
}
static void vgrad(struct cv *c, int x, int y, int w, int h, uint32_t top, uint32_t bot) {
  for (int j = 0; j < h; j++) frect(c, x, y + j, w, 1, mix(top, bot, h > 1 ? j * 256 / (h - 1) : 0));
}

static void background(struct cv *c) {
  frect(c, 0, 0, c->W / c->s + 1, c->H / c->s + 1, C_BG);
  for (int y = 0; y * 8 < c->H / c->s + 8; y++)
    for (int x = 0; x * 8 < c->W / c->s + 8; x++)
      if (((x + y) & 1) == 0) frect(c, x * 8, y * 8, 8, 8, C_BG2);
}

/* window: dark outline, light border, mid inner line, blue gradient body.  style 0 normal, 1 selected, 2 dim.
 * While g_press is set (the button under the finger) the window is drawn depressed: dimmer border, darker body and an inner shadow
 * on the top and left edges; the caller also moves the contents 1 vp down and right by shrinking the rectangle. */
static int g_press;
static void win(struct cv *c, int x, int y, int w, int h, int style) {
  uint32_t lt = style == 1 ? C_YEL : (style == 2 ? C_GREY : C_LIGHT), mid = style == 1 ? C_ORG : (style == 2 ? 0x2A3050u : C_MID);
  uint32_t top = style == 1 ? C_WIN_SEL_T : (style == 2 ? 0x1A2250u : C_WIN_T), bot = style == 2 ? 0x0A0F30u : C_WIN_B;
  if (g_press) { lt = mix(lt, C_DARK, 80); mid = mix(mid, C_DARK, 110); top = mix(top, C_DARK, 130); bot = mix(bot, C_DARK, 90); }
  if (w < 6 || h < 6) { frect(c, x, y, w, h, top); return; }
  frect(c, x + 1, y, w - 2, 1, C_DARK); frect(c, x + 1, y + h - 1, w - 2, 1, C_DARK);
  frect(c, x, y + 1, 1, h - 2, C_DARK); frect(c, x + w - 1, y + 1, 1, h - 2, C_DARK);
  frect(c, x + 2, y + 1, w - 4, 1, lt); frect(c, x + 2, y + h - 2, w - 4, 1, lt);
  frect(c, x + 1, y + 2, 1, h - 4, lt); frect(c, x + w - 2, y + 2, 1, h - 4, lt);
  frect(c, x + 2, y + 2, w - 4, 1, mid); frect(c, x + 2, y + h - 3, w - 4, 1, mid);
  frect(c, x + 2, y + 3, 1, h - 6, mid); frect(c, x + w - 3, y + 3, 1, h - 6, mid);
  vgrad(c, x + 3, y + 3, w - 6, h - 6, top, bot);
  if (g_press) { brect(c, x + 3, y + 3, w - 6, 2, C_DARK, 170); brect(c, x + 3, y + 5, 2, h - 8, C_DARK, 150); }
}

/* ------------------------------------------------------------------ text */
static const struct { char c; uint8_t col[5]; } font5x7[] = {
  {' ', {0, 0, 0, 0, 0}}, {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}}, {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
  {'2', {0x42, 0x61, 0x51, 0x49, 0x46}}, {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}}, {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
  {'5', {0x27, 0x45, 0x45, 0x45, 0x39}}, {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}}, {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
  {'8', {0x36, 0x49, 0x49, 0x49, 0x36}}, {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
  {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}}, {'B', {0x7F, 0x49, 0x49, 0x49, 0x36}}, {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
  {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}}, {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}}, {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
  {'G', {0x3E, 0x41, 0x49, 0x49, 0x7A}}, {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}}, {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
  {'J', {0x20, 0x40, 0x41, 0x3F, 0x01}}, {'K', {0x7F, 0x08, 0x14, 0x22, 0x41}}, {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
  {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}}, {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}}, {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
  {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}}, {'Q', {0x3E, 0x41, 0x51, 0x21, 0x5E}}, {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
  {'S', {0x46, 0x49, 0x49, 0x49, 0x31}}, {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}}, {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},
  {'V', {0x1F, 0x20, 0x40, 0x20, 0x1F}}, {'W', {0x3F, 0x40, 0x38, 0x40, 0x3F}}, {'X', {0x63, 0x14, 0x08, 0x14, 0x63}},
  {'Y', {0x07, 0x08, 0x70, 0x08, 0x07}}, {'Z', {0x61, 0x51, 0x49, 0x45, 0x43}},
  {'/', {0x20, 0x10, 0x08, 0x04, 0x02}}, {'%', {0x23, 0x13, 0x08, 0x64, 0x62}}, {':', {0x00, 0x36, 0x36, 0x00, 0x00}},
  {'.', {0x00, 0x60, 0x60, 0x00, 0x00}}, {'-', {0x08, 0x08, 0x08, 0x08, 0x08}}, {'+', {0x08, 0x08, 0x3E, 0x08, 0x08}},
  {'?', {0x02, 0x01, 0x51, 0x09, 0x06}}, {'#', {0x14, 0x7F, 0x14, 0x7F, 0x14}}, {'>', {0x41, 0x22, 0x14, 0x08, 0x00}},
  {'<', {0x08, 0x14, 0x22, 0x41, 0x00}}, {'!', {0x00, 0x00, 0x5F, 0x00, 0x00}}, {',', {0x00, 0x80, 0x60, 0x00, 0x00}},
  {'\'', {0x00, 0x05, 0x03, 0x00, 0x00}}, {'~', {0x04, 0x02, 0x04, 0x08, 0x04}}, {'*', {0x14, 0x08, 0x3E, 0x08, 0x14}},
  {'[', {0x00, 0x7F, 0x41, 0x41, 0x00}}, {']', {0x00, 0x41, 0x41, 0x7F, 0x00}},
};

/* sizes: 0 small (adv 6), 1 normal (adv 8).  Pack fonts are used when present, else the 5x7 table. */
/* One face only: the game's own outlined 8x11 font (pack id 0x0100+ch), drawn at an integer scale.  The `size` argument of the
 * text functions is kept for call compatibility and ignored; large text is `mul` = 2 on the Thor (see lgm()). */
static int font_ref_top(struct cv *c, int size) {
  (void)size;
  const struct pk_rect *r = pk_find(c->pk, 0x0100 + 'H');
  return r ? r->oy : 0;
}
static int glyph_adv(struct cv *c, int ch, int size) {
  (void)size;
  const struct pk_rect *r = c->pk ? pk_find(c->pk, 0x0100 + ch) : NULL;
  int a = r ? (r->adv ? r->adv : r->w + 1) : 7;
  return c->sq == 2 ? (a * 3 + 2) / 4 : (c->sq == 1 ? a - 1 : a);
}
/* Hi-res UI face (pack ids 0x4000 + scale index * 0x200 + face * 0x100 + ch): anti-aliased glyphs rasterised per device scale, used when the
 * canvas scale is 2 or more (the Thor).  At scale 1 (3DS) the game's own pixel font is used.  Widths are tracked in device pixels. */
static const uint8_t HR_SCALE[5] = {2, 3, 4, 6, 8};
static const struct pk_rect *hr_glyph(const struct cv *c, int ch, int mul) {
  if (!c->hr || ch < 32 || ch > 126) return NULL;
  int k = c->s * mul;
  for (int i = 0; i < 5; i++)
    if (HR_SCALE[i] == k) return pk_find(c->pk, 0x4000 + i * 0x200 + (c->sq == 2 ? 0x100 : 0) + ch);
  return NULL;
}
static int hr_enabled(const struct cv *c) {                      /* the pack holds the UI font for this scale */
  if (c->s < 2 || !c->pk) return 0;
  for (int i = 0; i < 5; i++) if (HR_SCALE[i] == c->s && pk_find(c->pk, 0x4000 + i * 0x200 + 'H')) return 1;
  return 0;
}
static int adv_dev(const struct cv *c, int ch, int mul) {       /* advance of one glyph in device pixels */
  const struct pk_rect *r = hr_glyph(c, ch, mul);
  if (r) { int a = r->adv - (c->sq == 1 ? (c->s * mul * 6 + 5) / 10 : 0); return a < 1 ? 1 : a; }
  return glyph_adv((struct cv *)c, ch, 0) * c->s * mul;
}
static int text_wm(struct cv *c, const char *s, int mul) {       /* width in vp (rounded up) of a string drawn at `mul` */
  int w = 0;
  for (; *s; s++) w += adv_dev(c, (unsigned char)*s, mul);
  return (w + c->s - 1) / c->s;
}
static int text_w(struct cv *c, const char *s, int size) { (void)size; return text_wm(c, s, 1); }
static int text_h(int size) { (void)size; return 11; }

static void draw_builtin_glyph(struct cv *c, int x, int y, int ch, uint32_t col, int mul) {
  const uint8_t *cols = NULL;
  if (ch >= 'a' && ch <= 'z') ch -= 32;
  for (unsigned f = 0; f < sizeof font5x7 / sizeof font5x7[0]; f++)
    if (font5x7[f].c == ch) { cols = font5x7[f].col; break; }
  if (!cols) return;
  int m = mul < 1 ? 1 : mul;
  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 7; j++)
      if (cols[i] & (1 << j)) {   /* dark outline on four sides, then the fill (5x7 face scaled by 1x1 pixels per vp) */
        int px = x + (i + 1) * m, py = y + (j + 1) * m;
        frect(c, px - m, py, m, m, C_DARK); frect(c, px + m, py, m, m, C_DARK);
        frect(c, px, py - m, m, m, C_DARK); frect(c, px, py + m, m, m, C_DARK);
      }
  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 7; j++)
      if (cols[i] & (1 << j)) frect(c, x + (i + 1) * m, y + (j + 1) * m, m, m, col);
}

/* one glyph; mul scales the whole glyph in vp (title text) */
static void draw_glyph(struct cv *c, int x, int y, int ch, uint32_t col, int size, int mul) {
  const struct pk_rect *r = c->pk ? pk_find(c->pk, 0x0100 + ch) : NULL;
  if (!r) { draw_builtin_glyph(c, x, y, ch, col, mul); return; }
  int top = font_ref_top(c, size), s = c->s * mul;
  int ox = x * c->s, oy = (y * c->s) + (r->oy - top) * s;
  int gox = c->sq == 2 ? r->ox * 3 / 4 : r->ox;
  for (int j = 0; j < r->h; j++)
    for (int i = 0; i < r->w; i++) {
      if (c->sq == 2 && i % 4 == 2) continue;                 /* squeezed face: every fourth source column is dropped */
      uint32_t p = pk_pixel(c->pk, r->x + i, r->y + j);
      if ((p >> 24) < 128) continue;
      int lum = (int)(((p >> 16) & 255) + ((p >> 8) & 255) + (p & 255)) / 3;
      uint32_t cc = lum < 100 ? C_DARK : col;
      int di = c->sq == 2 ? i - (i + 1) / 4 : i;
      int gx = ox + (gox + di) * s, gy = oy + j * s;
      for (int b = 0; b < s; b++)
        for (int a = 0; a < s; a++) dpx(c, gx + a, gy + b, cc);
    }
}
static void draw_hr(struct cv *c, int px_x, int y, const struct pk_rect *r, uint32_t col) {
  int gx = px_x + r->ox, gy = y * c->s + r->oy;
  for (int j = 0; j < r->h; j++)
    for (int i = 0; i < r->w; i++) {
      uint32_t p = pk_pixel(c->pk, r->x + i, r->y + j);
      int a = (int)(p >> 24);
      int X = gx + i, Y = gy + j;
      if (!a || (unsigned)X >= (unsigned)c->W || (unsigned)Y >= (unsigned)c->H) continue;
      uint32_t cc = mix(C_DARK, col, (int)((p >> 8) & 255) * 256 / 255);
      uint32_t *d = &c->px[Y * c->stride + X];
      *d = a >= 255 ? cc : mix(*d, cc, a * 256 / 255);
    }
}
/* Every string drawn since the start of the last som_ux_draw (tests: what does the screen say). */
static char g_slog[256][40]; static int g_sn;
int som_ux_screen_has_text(const char *s) { for (int i = 0; i < g_sn; i++) if (!strcmp(g_slog[i], s)) return 1; return 0; }
static int text_m(struct cv *c, int x, int y, const char *s, uint32_t col, int size, int mul) {
  if (g_sn < 256) snprintf(g_slog[g_sn++], sizeof g_slog[0], "%s", s);
  if (c->hr) {
    int pen = x * c->s;
    for (; *s; s++) {
      const struct pk_rect *r = hr_glyph(c, (unsigned char)*s, mul);
      if (r) { if (r->w > 1 || r->h > 1) draw_hr(c, pen, y, r, col); }
      else draw_glyph(c, pen / c->s, y, (unsigned char)*s, col, size, mul);
      pen += adv_dev(c, (unsigned char)*s, mul);
    }
    return (pen - x * c->s + c->s - 1) / c->s;
  }
  int x0 = x;
  for (; *s; s++) {
    draw_glyph(c, x, y, (unsigned char)*s, col, size, mul);
    x += glyph_adv(c, (unsigned char)*s, size) * mul;
  }
  return x - x0;
}
static int text(struct cv *c, int x, int y, const char *s, uint32_t col, int size) { return text_m(c, x, y, s, col, size, 1); }
static void text_c(struct cv *c, struct R r, int y, const char *s, uint32_t col, int size) {
  text(c, r.x + (r.w - text_w(c, s, size)) / 2, y, s, col, size);
}
static void text_fit(struct cv *c, int x, int y, int maxw, const char *s, uint32_t col, int size) {
  if (g_sn < 256) snprintf(g_slog[g_sn++], sizeof g_slog[0], "%s", s);
  char b[48];
  int n = 0, w = 0;
  for (; s[n] && n < 46; n++) {
    int a = adv_dev(c, (unsigned char)s[n], 1);
    if (w + a > maxw * c->s) break;
    w += a; b[n] = s[n];
  }
  b[n] = 0;
  if (s[n]) audit("text cut", s);
  text(c, x, y, b, col, size);
}
static void text_fit_m(struct cv *c, int x, int y, int maxw, const char *s, uint32_t col, int mul) {
  if (g_sn < 256) snprintf(g_slog[g_sn++], sizeof g_slog[0], "%s", s);
  char b[48];
  int n = 0, w = 0;
  for (; s[n] && n < 46; n++) {
    int a = adv_dev(c, (unsigned char)s[n], mul);
    if (w + a > maxw * c->s) break;
    w += a; b[n] = s[n];
  }
  b[n] = 0;
  if (s[n]) audit("text cut", s);
  text_m(c, x, y, b, col, 0, mul);
}
/* wrapped text: returns the number of lines drawn */
static int text_wrap(struct cv *c, int x, int y, int w, const char *s, uint32_t col, int size, int maxlines) {
  int lines = 0;
  char b[64];
  while (*s && lines < maxlines) {
    int n = 0, lw = 0, brk = -1;
    while (s[n] && n < 60) {
      int a = adv_dev(c, (unsigned char)s[n], 1);
      if (lw + a > w * c->s) break;
      lw += a; if (s[n] == ' ') brk = n; n++;
    }
    int take = n;
    if (s[n] && brk > 0) take = brk;
    memcpy(b, s, (size_t)take); b[take] = 0;
    text(c, x, y + lines * (text_h(size) + 1), b, col, size);
    s += take; while (*s == ' ') s++;
    lines++;
  }
  return lines;
}

/* ------------------------------------------------------------------ art: icons, portraits, gauges */
static const uint8_t ICON_ITEM[11] = {1, 7, 6, 5, 2, 4, 9, 8, 11, 10, 3};
static const uint8_t ICON_WEAPON[8] = {26, 19, 20, 21, 22, 23, 24, 25};   /* glove, sword, axe, spear, whip, bow, boomerang, javelin: verified against the game ring */
static const uint8_t ICON_SPIRIT[8] = {34, 27, 28, 29, 30, 31, 33, 32};   /* raw spirit bytes: gnome undine salamando sylphid luna dryad shade lumina (sprite owns 6, girl owns 7); verified against the game ring headers */
static const uint8_t ICON_SUBMENU[7] = {17, 18, 12, 13, 14, 15, 16};   /* equip armor, targeting, status, levels, action grid, controller, window: game-art cells of spr_iconsGeneral (the sheet carries the game's own EQUIP / STAT. / LEVEL / ACT. / EDIT labels) */
static const uint32_t SPIRIT_COL[8] = {0xC08040u, 0x4090F0u, 0xF05030u, 0x60D060u, 0xB080F0u, 0x40B060u, 0x8060A0u, 0xF8E060u};
#define ITEM_FALLBACK UX_ITEM_NAME
#define WEAPON_FALLBACK UX_WEAPON_NAME
#define SPIRIT_NAME UX_SPIRIT_NAME
static const char *const HERO_DEFAULT[3] = {"RANDI", "PURIM", "POPOI"};   /* the game's own default names; never BOY / GIRL / SPRITE */
static const struct som_ux *g_hn_ux;                                   /* names come from the object being drawn */
static const char *hname(int i) {
  if (i < 0 || i > 2) i = 0;
  return g_hn_ux && g_hn_ux->hero_name[i][0] ? g_hn_ux->hero_name[i] : HERO_DEFAULT[i];
}
const char *som_ux_hero_name(const struct som_ux *ux, int slot) {
  if (slot < 0 || slot > 2) slot = 0;
  return ux && ux->hero_name[slot][0] ? ux->hero_name[slot] : HERO_DEFAULT[slot];
}
static const uint32_t HERO_COL[3] = {0xF08040u, 0xF0D060u, 0xE06080u};

static void icon(struct cv *c, int x, int y, int n, int kind, uint32_t tint, int grey) {
  const struct pk_rect *r = c->pk ? pk_find(c->pk, 0x1000 + n) : NULL;
  if (r) {
    for (int j = 0; j < 16; j++)
      for (int i = 0; i < 16; i++) {
        uint32_t p = pk_pixel(c->pk, r->x + i, r->y + j);
        if ((p >> 24) < 128) continue;
        uint32_t col = p & 0xFFFFFFu;
        if (grey) { int l = (int)(((col >> 16) & 255) + ((col >> 8) & 255) + (col & 255)) / 3; col = mix((uint32_t)(l * 0x010101), C_BG, 150); }
        for (int b = 0; b < c->s; b++)
          for (int a = 0; a < c->s; a++) dpx(c, (x + i) * c->s + a, (y + j) * c->s + b, col);
      }
    return;
  }
  /* placeholder: rounded square in the category colour with an initial */
  uint32_t base = grey ? 0x40466Cu : tint;
  frect(c, x + 1, y + 1, 14, 14, C_DARK);
  frect(c, x + 2, y + 2, 12, 12, base);
  frect(c, x + 2, y + 2, 12, 2, mix(base, 0xFFFFFFu, 90));
  char b[2] = {(char)(kind == 0 ? 'W' : kind == 1 ? 'I' : 'S'), 0};
  text(c, x + 5, y + 4, b, grey ? C_DIM : C_TXT, 0);
}

static void portrait_t(struct cv *c, int hero, int x, int y, int h, int dead, int tint) {
  int w = h * 98 / 108;
  const struct pk_rect *big = c->pk ? pk_find(c->pk, 0x2000 + hero) : NULL;
  const struct pk_rect *sm = c->pk ? pk_find(c->pk, 0x2010 + hero) : NULL;
  if (big && sm) {
    int dw = w * c->s, dh = h * c->s, ox = x * c->s, oy = y * c->s;
    int use_small = (dh <= 40);
    const struct pk_rect *r = use_small ? sm : big;
    for (int j = 0; j < dh; j++)
      for (int i = 0; i < dw; i++) {          /* bilinear: the portraits are painted art, not pixel art */
        float fx = (i + 0.5f) * r->w / dw - 0.5f, fy = (j + 0.5f) * r->h / dh - 0.5f;
        int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
        float tx = fx - x0, ty = fy - y0, ar = 0, ag = 0, ab = 0, aa = 0;
        for (int q = 0; q < 4; q++) {
          int xx = x0 + (q & 1), yy = y0 + (q >> 1);
          xx = xx < 0 ? 0 : (xx >= r->w ? r->w - 1 : xx); yy = yy < 0 ? 0 : (yy >= r->h ? r->h - 1 : yy);
          float wgt = ((q & 1) ? tx : 1 - tx) * ((q >> 1) ? ty : 1 - ty);
          uint32_t p = pk_pixel(c->pk, r->x + xx, r->y + yy);
          float al = (p >> 24) / 255.0f * wgt;
          ar += ((p >> 16) & 255) * al; ag += ((p >> 8) & 255) * al; ab += (p & 255) * al; aa += al;
        }
        if (aa < 0.5f) continue;
        uint32_t col = ((uint32_t)(ar / aa) << 16) | ((uint32_t)(ag / aa) << 8) | (uint32_t)(ab / aa);
        if (dead) { int l = (int)(((col >> 16) & 255) + ((col >> 8) & 255) + (col & 255)) / 3; col = mix((uint32_t)(l * 0x010101), C_BG, 140); }
        if (tint) col = mix(col, C_RED, tint);
        dpx(c, ox + i, oy + j, col);
      }
    return;
  }
  /* placeholder: ring + coloured head with the initial */
  int cx = x + w / 2, cy = y + h / 2, rad = (w < h ? w : h) / 2;
  for (int j = -rad; j <= rad; j++)
    for (int i = -rad; i <= rad; i++) {
      int d2 = i * i + j * j;
      if (d2 > rad * rad) continue;
      uint32_t col = d2 > (rad - 2) * (rad - 2) ? 0x8A8A50u : (dead ? 0x303450u : mix(HERO_COL[hero], C_DARK, 80));
      frect(c, cx + i, cy + j, 1, 1, col);
    }
  char b[2] = {hname(hero)[0], 0};
  text(c, cx - 2, cy - 4, b, dead ? C_DIM : C_TXT, 0);
}

static void portrait(struct cv *c, int hero, int x, int y, int h, int dead) { portrait_t(c, hero, x, y, h, dead, 0); }

static void gauge(struct cv *c, int x, int y, int w, int h, unsigned num, unsigned den, uint32_t col) {
  frect(c, x, y, w, h, C_DARK);
  frect(c, x + 1, y + 1, w - 2, h - 2, 0x1A1F44u);
  if (!den) return;
  if (num > den) num = den;
  int fw = (int)((unsigned long)(w - 2) * num / den);
  if (num && fw < 1) fw = 1;
  frect(c, x + 1, y + 1, fw, h - 2, col);
  if (h >= 5) frect(c, x + 1, y + 1, fw, 1, mix(col, 0xFFFFFFu, 110));
}

/* ------------------------------------------------------------------ layout */
enum hit_kind {
  HK_NONE = 0, HK_PARTY, HK_TAB, HK_CELL, HK_T_ALLY, HK_T_ENEMY, HK_T_ALL_ALLIES, HK_T_ALL_ENEMIES, HK_CANCEL, HK_PAD,
  HK_LETTER, HK_NEXT, HK_KEYROW, HK_KEYS_SCROLL, HK_BIND_CANCEL, HK_BIND_CLEAR,
  HK_OPT, HK_OPTION, HK_OPT_NAV, HK_AVATAR, HK_QUICK, HK_BIND_ENTRY, HK_BIND_MENU, HK_QE_TAB, HK_QE_CELL, HK_QE_PAGE,
  HK_QE_CLEAR, HK_QE_KEY, HK_MAPVIEW, HK_POLICY, HK_POL_CANCEL, HK_QE_BACK, HK_REVIVE, HK_REV_CANCEL,
  HK_ST_SLOT, HK_ST_SAVE, HK_ST_LOAD, HK_CR_SCROLL, HK_NAME_LAST
};
typedef char hit_name_last_matches_public[(int)HK_NAME_LAST == (int)SOM_UX_HIT_NAME_LAST ? 1 : -1];
typedef char hit_enum_matches_public[((int)HK_REV_CANCEL == (int)SOM_UX_HIT_REV_CANCEL && (int)HK_REVIVE == (int)SOM_UX_HIT_REVIVE && (int)HK_QE_BACK == (int)SOM_UX_HIT_QE_BACK && (int)HK_POLICY == (int)SOM_UX_HIT_POLICY && (int)HK_MAPVIEW == (int)SOM_UX_HIT_MAPVIEW && (int)HK_QE_KEY == (int)SOM_UX_HIT_QE_KEY && (int)HK_BIND_CLEAR == (int)SOM_UX_HIT_BIND_CLEAR && (int)HK_OPT == (int)SOM_UX_HIT_OPT && (int)HK_CR_SCROLL == (int)SOM_UX_HIT_CR_SCROLL) ? 1 : -1];
struct hit { struct R r; uint8_t kind, a, b, enabled; };
static int nav_selectable(const struct hit *t) { return t->enabled && t->kind != HK_NONE; }
#define MAXHIT 96
struct LAY {
  int form, vw, vh;       /* form: 0 compact (3DS), 1 roomy landscape, 2 roomy portrait */
  struct R header, boss, radar, elist, info, dlg, mapr, side, kbdbox;
  struct hit h[MAXHIT];
  int n, cols, rows;
};
static struct hit *add_hit(struct LAY *l, int kind, int a, int b, int x, int y, int w, int h, int en) {
  if (l->n >= MAXHIT) return &l->h[MAXHIT - 1];
  struct hit *t = &l->h[l->n++];
  t->kind = (uint8_t)kind; t->a = (uint8_t)a; t->b = (uint8_t)b; t->enabled = (uint8_t)en;
  t->r.x = x; t->r.y = y; t->r.w = w; t->r.h = h;
  return t;
}

static int present_slots(const struct som_ux *ux, int *slot) {
  int n = 0;
  for (int i = 0; i < SOM_PARTY; i++) if (ux->st.hero[i].present) slot[n++] = i;
  return n;
}

static int pending(const struct som_ux *ux) { return ux->pend_kind != 0 && ux->pend_class != SOM_UX_TC_NONE; }

/* which actors a target class accepts */
static int target_ok_hero(const struct som_ux *ux, int slot) {
  const struct som_hero *h = &ux->st.hero[slot];
  if (!h->present) return 0;
  switch (ux->pend_class) {
  case SOM_UX_TC_ALLY_ONE: case SOM_UX_TC_ALLY_ONE_OR_ALL: return h->alive;
  case SOM_UX_TC_ALLY_DEAD_ONE: return !h->alive;
  case SOM_UX_TC_ALLY_STATUS_ONE: return h->alive && (h->status & 0x7FFF) != 0;
  case SOM_UX_TC_BOY_ONLY: return slot == 0 && h->alive;
  default: return 0;
  }
}
static int target_ok_enemy(const struct som_ux *ux, int slot) {
  const struct som_ux_actor *a = &ux->actor[slot];
  if (!a->present || !a->alive) return 0;
  return ux->pend_class == SOM_UX_TC_ENEMY_ONE || ux->pend_class == SOM_UX_TC_ENEMY_ONE_OR_ALL;
}

static int class_is_enemy(int cls) { return cls == SOM_UX_TC_ENEMY_ONE || cls == SOM_UX_TC_ENEMY_ONE_OR_ALL; }
static int class_is_ally(int cls) {
  return cls == SOM_UX_TC_ALLY_ONE || cls == SOM_UX_TC_ALLY_ONE_OR_ALL || cls == SOM_UX_TC_ALLY_DEAD_ONE || cls == SOM_UX_TC_ALLY_STATUS_ONE ||
         cls == SOM_UX_TC_BOY_ONLY;
}
/* The target selector: a full-width modal.  Allies are large portrait cards, enemies a grid of large cards with the enemy picture, and the group
 * choice (ALL ALLIES / ALL ENEMIES) is a wide prominent button next to CANCEL.  Every card is at least 58 vp (14 mm on the Thor) on both sides. */
#define TARGET_MIN 58
/* Where the ring starts: the selected tab, else the top-left target. */
static const struct hit *nav_initial(const struct LAY *l, const struct som_ux *ux) {
  const struct hit *best = NULL;
  for (int i = 0; i < l->n; i++) if (nav_selectable(&l->h[i]) && l->h[i].kind == HK_TAB && l->h[i].a == ux->tab) return &l->h[i];
  for (int i = 0; i < l->n; i++) {
    if (!nav_selectable(&l->h[i])) continue;
    if (!best || l->h[i].r.y < best->r.y || (l->h[i].r.y == best->r.y && l->h[i].r.x < best->r.x)) best = &l->h[i];
  }
  return best;
}

static void layout_target(struct LAY *l, const struct som_ux *ux, int ax, int ay, int aw, int ah) {
  int cls = ux->pend_class;
  int bhh = l->form == 0 ? 34 : 59;                       /* bottom buttons: 58 vp tall on the roomy forms */
  int y = ay + 13, by = ay + ah - bhh;
  int availh = by - 3 - y;
  int all_ok = cls == SOM_UX_TC_ALLY_ONE_OR_ALL || (cls == SOM_UX_TC_ENEMY_ONE_OR_ALL && ux->n_enemies > 0);
  if (class_is_ally(cls)) {
    int slot[SOM_PARTY], np = present_slots(ux, slot);
    int ch = availh > 120 ? 120 : availh;
    for (int i = 0; i < np; i++)
      add_hit(l, HK_T_ALLY, slot[i], 0, ax + i * (aw / np), y, aw / np - 2, ch, target_ok_hero(ux, slot[i]));
  } else {
    int n = ux->n_enemies; if (n > 3) n = 3;     /* the game never shows more than three */
    int cols = n <= 3 ? (n ? n : 1) : 3;
    int rows = (n + cols - 1) / cols, maxrows = availh / TARGET_MIN; if (maxrows < 1) maxrows = 1;
    if (rows > maxrows) rows = maxrows;
    int ch = rows ? availh / rows : availh;
    for (int i = 0; i < n && i < cols * rows; i++) {
      int sl = ux->enemy_idx[i];
      add_hit(l, HK_T_ENEMY, sl, 0, ax + (i % cols) * (aw / cols), y + (i / cols) * ch, aw / cols - 2, ch - 2, target_ok_enemy(ux, sl));
    }
  }
  if (all_ok) {
    int aw2 = aw * 64 / 100;
    add_hit(l, class_is_ally(cls) ? HK_T_ALL_ALLIES : HK_T_ALL_ENEMIES, 0, 0, ax, by, aw2 - 2, bhh - 1, 1);
    add_hit(l, HK_CANCEL, 0, 0, ax + aw2, by, aw - aw2 - 2, bhh - 1, 1);
  } else add_hit(l, HK_CANCEL, 0, 0, ax, by, aw - 2, bhh - 1, 1);
}

static void layout_pad(struct LAY *l, const int *btn, int n, int cols, int ax, int ay, int aw, int ah) {
  int rows = (n + cols - 1) / cols;
  int cw = aw / cols, rh = ah / rows;
  for (int i = 0; i < n; i++)
    add_hit(l, HK_PAD, btn[i], 0, ax + (i % cols) * cw, ay + (i / cols) * rh, cw - 2, rh - 2, 1);
}

/* Quick slots.  Slots 0..3 are the shoulder slots (L1 R1 L2 R2), 4..7 the free touch-only ones.  The shoulder slots sit at the two ends of the
 * area, two on each side, mirroring the console: an 8 x 1 row reads L2 L1 [4 5 6 7] R1 R2, a 4 x 2 grid L1 [4 5] R1 over L2 [6 7] R2. */
static int quick_slot_at(int cols, int rows, int v) {
  static const uint8_t row8[8] = {2, 0, 4, 5, 6, 7, 1, 3}, grid[8] = {0, 4, 5, 1, 2, 6, 7, 3};
  if (cols == 8 && rows == 1) return row8[v];
  if (cols == 4 && rows == 2) return grid[v];
  return v;
}
static void layout_quick(struct LAY *l, int x, int y, int w, int cols, int rows, int sh) {
  int cw = w / cols;
  for (int v = 0; v < SOM_UX_QUICK_N && v < cols * rows; v++)
    add_hit(l, HK_QUICK, quick_slot_at(cols, rows, v), 0, x + (v % cols) * cw, y + (v / cols) * sh, cw - 1, sh - 1, 1);
}
static int slot_h(const struct LAY *l) { return l->form == 0 && l->vw >= 316 ? 29 : 34; }   /* 3DS 1x: 29 vp; Thor: room for the icon above the permanent button label */

/* the SPELL tab has two levels: qe_spirit 0 lists the spirits that have spells, n lists the six spells of spirit n - 1 */
/* Round 3: entries the player never owned are not listed.  raw index -> shown or not. */
static int qe_shown(const struct som_ux *ux, int cat, int raw) {
  switch (cat % 5) {
  case 1: return (ux->seen_w >> raw) & 1;
  case 2: return raw < 11 && ((ux->seen_i >> raw) & 1);
  case 3: return (ux->seen_s >> raw) & 1;
  case 4: return ux->qe_spirit != 0 || ((ux->seen_s >> raw) & 1);
  default: return 1;
  }
}
static int qe_raw_count(const struct som_ux *ux, int cat) {
  static const uint8_t n[5] = {8, 8, 11, 8, 6};   /* MENU tab: the spirit ring and the seven config screens (round 3) */
  if (cat % 5 == 4 && ux->qe_spirit == 0) return 7;
  return n[cat % 5];
}
static int qe_count(const struct som_ux *ux, int cat) {
  int n = 0;
  for (int i = 0; i < qe_raw_count(ux, cat); i++) n += qe_shown(ux, cat, i);
  return n;
}
static int qe_raw(const struct som_ux *ux, int cat, int vidx) {
  for (int i = 0, k = 0; i < qe_raw_count(ux, cat); i++) if (qe_shown(ux, cat, i)) { if (k++ == vidx) return i; }
  return 0;
}
static void qe_entry(const struct som_ux *ux, int cat, int idx, int *kind, int *id) {
  idx = qe_raw(ux, cat, idx);
  static const uint8_t cats[4] = {SOM_CAT_WEAPON, SOM_CAT_ITEM, SOM_CAT_MAGIC, SOM_CAT_CONFIG};
  switch (cat) {
  case 0: if (idx < 1) { *kind = SOM_UX_BK_MENU; *id = cats[2]; } else { *kind = SOM_UX_BK_SUBMENU; *id = idx - 1; } break;
  case 1: *kind = SOM_UX_BK_WEAPON; *id = idx; break;
  case 2: *kind = SOM_UX_BK_ITEM; *id = idx; break;
  case 3: *kind = SOM_UX_BK_SPIRIT; *id = idx; break;
  default:
    if (ux->qe_spirit == 0) { *kind = SOM_UX_BK_SPIRIT; *id = idx; }
    else { *kind = SOM_UX_BK_SPELL; *id = (ux->qe_spirit - 1) * 6 + idx; }
    break;
  }
}
#define QE_PAGE 8

/* The options overlay: main page and the quick-slot editor (the old BINDINGS page is gone: opening it shows the main page). */
static void layout_options(struct LAY *l, const struct som_ux *ux, int y0) {
  int vw = l->vw, vh = l->vh, bh = l->form == 0 ? 28 : (l->form == 2 && vh < 285 ? 31 : 34);
  if (ux->opt_ext && (ux->opt_page == SOM_UX_OPT_MAIN || ux->opt_page == SOM_UX_OPT_BINDINGS)) {
    /* Thor: five rows of two options, then one row of five page buttons.  LANGUAGE (a single value) makes room for SHADER, FAST FWD and ENEMY HP. */
    static const int8_t cells[15] = {SOM_UX_OPT_WIDE, SOM_UX_OPT_FALLBACK, SOM_UX_OPT_HUD_HIDE, SOM_UX_OPT_CORE, SOM_UX_OPT_UI_SOUNDS, SOM_UX_OPT_HAPTICS,
                                     SOM_UX_OPT_FIT, SOM_UX_OPT_SHADER, SOM_UX_OPT_FFWD, SOM_UX_OPT_ENEMY_HP, -2, -4, -5, -6, -1};
    int rows = 6, rh = (vh - y0 - 2) / rows, cw = (vw - 4) / 2, qw = (vw - 4) / 5;
    if (rh > 60) rh = 60;
    for (int i = 0; i < 15; i++) {
      int x = i < 10 ? 2 + (i % 2) * cw : 2 + (i - 10) * qw, y = y0 + (i < 10 ? i / 2 : 5) * rh, w = i < 10 ? cw : qw;
      if (cells[i] >= 0) add_hit(l, HK_OPTION, cells[i], 0, x, y, w - 1, rh - 1, 1);
      else add_hit(l, HK_OPT_NAV, cells[i] == -1 ? 0 : -cells[i], 0, x, y, w - 1, rh - 1, 1);   /* 4 SAVE STATES, 2 QUICK SLOTS, 5 CREDITS, 6 MAIN MENU, 0 BACK */
    }
  } else if (ux->opt_page == SOM_UX_OPT_STATES) {
    int by = vh - bh - 2, hint = vh >= 200 ? 13 : 0, gy = y0 + 2, gh = by - hint - 3 - gy, rows = 4, cw = (vw - 6) / 2, ch = gh / rows;
    for (int i = 0; i < SOM_UX_STATE_SLOTS; i++) add_hit(l, HK_ST_SLOT, i, 0, 2 + (i % 2) * (cw + 2), gy + (i / 2) * ch, cw, ch - 2, 1);
    l->info.x = 2; l->info.y = by - hint - 1; l->info.w = vw - 4; l->info.h = hint;
    int w3 = (vw - 4 - 4) / 3;
    add_hit(l, HK_ST_SAVE, 0, 0, 2, by, w3, bh, 1);
    add_hit(l, HK_ST_LOAD, 0, 0, 2 + w3 + 2, by, w3, bh, ux->st_sel < SOM_UX_STATE_SLOTS && ux->st_slot[ux->st_sel].used);
    add_hit(l, HK_OPT_NAV, 3, 0, 2 + 2 * (w3 + 2), by, vw - 4 - 2 * (w3 + 2), bh, 1);
  } else if (ux->opt_page == SOM_UX_OPT_CREDITS) {
    int by = vh - bh - 2, w3 = (vw - 4 - 4) / 3;
    l->info.x = 2; l->info.y = y0 + 2; l->info.w = vw - 4; l->info.h = by - y0 - 5;
    add_hit(l, HK_CR_SCROLL, 0, 0, 2, by, w3, bh, ux->cr_scroll > 0);
    add_hit(l, HK_CR_SCROLL, 1, 0, 2 + w3 + 2, by, w3, bh, ux->cr_scroll + ux->cr_vis < ux->cr_lines);
    add_hit(l, HK_OPT_NAV, 3, 0, 2 + 2 * (w3 + 2), by, vw - 4 - 2 * (w3 + 2), bh, 1);
  } else if (ux->opt_page == SOM_UX_OPT_MAIN || ux->opt_page == SOM_UX_OPT_BINDINGS) {
    int rows = 5, rh = (vh - y0 - 2) / rows, cw = (vw - 4) / 2;
    if (rh > 60) rh = 60;
    for (int i = 0; i < 10; i++) {
      int x = 2 + (i % 2) * cw, y = y0 + (i / 2) * rh;
      if (i < SOM_UX_OPT_SHADER) add_hit(l, HK_OPTION, i, 0, x, y, cw - 1, rh - 1, 1);   /* the classic page has no SHADER */
      else if (i == 8) add_hit(l, HK_OPT_NAV, 2, 0, x, y, cw - 1, rh - 1, 1);            /* QUICK SLOTS */
      else if (i == 9) add_hit(l, HK_OPT_NAV, 0, 0, x, y, cw - 1, rh - 1, 1);            /* BACK */
    }
  } else {                                           /* quick-slot editor */
    int y = y0, sh = slot_h(l);
    layout_quick(l, 2, y, vw - 4, ux->qe_four ? SOM_UX_SHOULDERS : SOM_UX_QUICK_N, 1, sh);   /* PC: the four shoulder slots only */
    y += sh + 2;
    int th = l->form == 0 ? 22 : 30, tw = (vw - 4) / 5;
    for (int i = 0; i < 5; i++) add_hit(l, HK_QE_TAB, i, 0, 2 + i * tw, y, tw - 1, th, 1);
    y += th + 2;
    int by = vh - bh - 2, py = by - bh - 2;
    int gh = py - y - 2, rows = 2, cols = 4;
    if (l->form == 2) { rows = 4; cols = 2; }          /* portrait: wide cells so spell names fit */
    int cw = (vw - 4) / cols, chh = gh / rows;
    if (chh > 56) chh = 56;
    for (int i = 0; i < QE_PAGE; i++) {
      int kind, id, idx = ux->qe_page * QE_PAGE + i;
      if (idx >= qe_count(ux, ux->qe_cat)) break;
      qe_entry(ux, ux->qe_cat, idx, &kind, &id);
      add_hit(l, HK_QE_CELL, i, 0, 2 + (i % cols) * cw, y + (i / cols) * chh, cw, chh, 1);
    }
    if (ux->qe_cat == 4 && ux->qe_spirit) add_hit(l, HK_QE_BACK, 0, 0, 2, py, vw - 4, bh, 1);
    else if (qe_count(ux, ux->qe_cat) > QE_PAGE) {
      add_hit(l, HK_QE_PAGE, 0, 0, 2, py, (vw - 4) / 2 - 1, bh, ux->qe_page > 0);
      add_hit(l, HK_QE_PAGE, 1, 0, 2 + (vw - 4) / 2, py, (vw - 4) / 2 - 1, bh, (ux->qe_page + 1) * QE_PAGE < qe_count(ux, ux->qe_cat));
    }
    int w2 = (vw - 4) / 2;
    add_hit(l, HK_QE_CLEAR, 0, 0, 2, by, w2 - 1, bh, ux->qe_slot < SOM_UX_QUICK_N && ux->quick[ux->qe_slot].kind);
    add_hit(l, HK_OPT_NAV, 3, 0, 2 + w2, by, vw - 4 - w2, bh, 1);
  }
}

static int entry_class(const struct som_ux *ux, int kind, int id);
static int spell_caster(int id);
int ux_spell_cost(const struct som_ux *ux, int id);
static const uint8_t POL_ORDER[4] = {SOM_UX_POL_SELECT, SOM_UX_POL_NEAR, SOM_UX_POL_LESS_HP, SOM_UX_POL_ALL};
int som_ux_entry_policies(const struct som_ux *ux, int kind, int id);

/* Target-policy chooser: an info card (icon, name, who it targets), one big button per allowed policy, CANCEL. */
static void layout_policy(struct LAY *l, const struct som_ux *ux, int y) {
  int vw = l->vw, vh = l->vh, port = l->form == 2;
  int cb = l->form == 0 ? 30 : (port ? 50 : 58), ih = l->form == 0 ? 36 : (port ? 44 : 56);
  int mask = som_ux_entry_policies(ux, ux->pol_kind, ux->pol_id), n = 0, pl[4];
  for (int i = 0; i < 4; i++) if (mask & (1 << POL_ORDER[i])) pl[n++] = POL_ORDER[i];
  l->info.x = 2; l->info.y = y; l->info.w = vw - 4; l->info.h = ih;
  int by = y + ih + 3, bh = vh - cb - 5 - by;
  if (n < 1) n = 1;
  if (port) {
    int h1 = (bh - (n - 1) * 3) / n;
    for (int i = 0; i < n && i < 4; i++) add_hit(l, HK_POLICY, pl[i], 0, 2, by + i * (h1 + 3), vw - 4, h1, 1);
  } else {
    int w1 = (vw - 4 - (n - 1) * 3) / n;
    if (bh > 90) bh = 90;
    for (int i = 0; i < n && i < 4; i++) add_hit(l, HK_POLICY, pl[i], 0, 2 + i * (w1 + 3), by, w1, bh, 1);
  }
  add_hit(l, HK_POL_CANCEL, 0, 0, 2, vh - cb - 2, vw - 4, cb, 1);
}

/* ---- REVIVE chooser ---- */
#define REV_MAX 6
static int rev_user(const struct som_ux *ux) {          /* hero that uses an item: the viewed one when standing, else the first standing hero */
  if (ux->hero < SOM_PARTY && ux->st.hero[ux->hero].present && ux->st.hero[ux->hero].alive && ux->hero != ux->rev_hero) return ux->hero;
  for (int i = 0; i < SOM_PARTY; i++) if (i != ux->rev_hero && ux->st.hero[i].present && ux->st.hero[i].alive) return i;
  return -1;
}
static int rev_options(const struct som_ux *ux, struct som_ux_revive_opt *o) {
  int n = 0, user = rev_user(ux);
  for (int pass = 0; pass < 11 && n < REV_MAX; pass++) {
    int id = pass == 0 ? 5 : pass - 1;                      /* Cup of Wishes first, then the other items of the table that revive */
    if (pass > 0 && (id == 5 || som_ux_item_class(id) != SOM_UX_TC_ALLY_DEAD_ONE)) continue;
    int qty = 0;
    for (int i = 0; i < SOM_ITEM_RECORDS; i++) if (ux->st.item[i].qty && ux->st.item[i].id == id) qty += ux->st.item[i].qty;
    struct som_ux_revive_opt *q = &o[n++];
    memset(q, 0, sizeof *q);
    q->kind = 1; q->id = (uint8_t)id; q->qty = (uint8_t)(qty > 255 ? 255 : qty); q->caster = (uint8_t)(user < 0 ? 0xFF : user);
    q->reason = qty == 0 ? 1 : (user < 0 ? 4 : 0);
    q->enabled = q->reason == 0;
  }
  for (int id = 0; id < 42 && n < REV_MAX; id++) {
    if (id != 33 && ux->spell_class[id] != SOM_UX_TC_ALLY_DEAD_ONE) continue;
    int caster = spell_caster(id), g = id / 6, spirit = (g == 6 && caster == 1) ? 7 : g, cost = ux_spell_cost(ux, id);
    const struct som_hero *h = &ux->st.hero[caster];
    if (!h->present) continue;
    struct som_ux_revive_opt *q = &o[n++];
    memset(q, 0, sizeof *q);
    q->kind = 2; q->id = (uint8_t)id; q->caster = (uint8_t)caster; q->mp_cost = (uint8_t)(cost < 0 ? 0 : cost);
    int st = som_ux_spell_state(ux, id);
    if (caster == ux->rev_hero || !h->alive) q->reason = 4;
    else if (som_spirit_ring_index(&ux->st, caster, spirit) < 0) q->reason = 3;
    else if (cost >= 0 && h->mp < cost) q->reason = 2;
    else if (st == SOM_UX_AV_DISABLED) q->reason = 2;
    q->queued = st == SOM_UX_AV_QUEUED && q->reason == 0;
    q->enabled = q->reason == 0 && st == SOM_UX_AV_ENABLED;
  }
  return n;
}
static void layout_revive(struct LAY *l, const struct som_ux *ux, int y) {
  int vw = l->vw, vh = l->vh, cb = l->form == 0 ? 30 : (l->form == 2 ? 50 : 58), ih = l->form == 0 ? 36 : (l->form == 2 ? 50 : 48);
  struct som_ux_revive_opt o[REV_MAX];
  int n = rev_options(ux, o);
  l->info.x = 2; l->info.y = y; l->info.w = vw - 4; l->info.h = ih;
  int by = y + ih + 3, avail = vh - cb - 5 - by, rh = n ? (avail - (n - 1) * 3) / n : avail;
  if (rh > 70) rh = 70;
  for (int i = 0; i < n; i++) add_hit(l, HK_REVIVE, i, 0, 2, by + i * (rh + 3), vw - 4, rh, o[i].enabled || o[i].queued);
  add_hit(l, HK_REV_CANCEL, 0, 0, 2, vh - cb - 2, vw - 4, cb, 1);
}

/* Round 3, the quiet bottom screen: 0 normal, 1 nothing at all (black), 2 a faint "LOOK UP" (a dialogue is on the top screen).  Nothing is shown at the title, in
 * the game select, in cutscenes and in the whole intro until the story flag $CF00 says the boy took the first sword (ux->armed); only the name entry (touch
 * input needed) and the overlays opened from a visible screen are drawn. */
static int quiet_level(const struct som_ux *ux) {
  if (ux->opt_page || ux->rev_wait || ux->pol_wait || ux->fe_name_entry) return 0;
  if (ux->mode == SOM_UX_BOOT) return 1;
  if (ux->mode == SOM_UX_FRONTEND) return ux->fe_name_entry ? 0 : 1;
  if (!ux->armed) return 1;
  if (ux->mode == SOM_UX_EVENT) return ux->had_dialogue ? 2 : 1;
  return 0;
}

int som_ux_quiet(const struct som_ux *ux) { return ux ? quiet_level(ux) : 0; }

/* Field grid visibility (round 3): weapon types and spirits appear once owned at least once in this playthrough (ux->seen_*), items while a record holds them. */
static int cell_visible(const struct som_ux *ux, int i) {
  if (ux->tab == SOM_UX_TAB_WEAPON) return ((ux->seen_w >> i) & 1) || ux->st.weapon_slot[i] != 0xFF;
  if (ux->tab == SOM_UX_TAB_ITEM) return ux->st.item[i].qty != 0;
  if (ux->tab == SOM_UX_TAB_MAGIC && ux->spirit == 0xFF) return ((ux->seen_s >> i) & 1) || som_spirit_ring_index(&ux->st, ux->hero, i) >= 0;
  return 1;
}

static void build_layout(struct LAY *l, const struct som_ux *ux, int vw, int vh) {
  memset(l, 0, sizeof *l);
  l->vw = vw; l->vh = vh;
  l->form = ((vw <= 330 && vh <= 250) || (vw >= vh && vw <= 330 && vh <= 256)) ? 0 : (vw >= vh ? 1 : 2);
  l->header.x = 0; l->header.y = 0; l->header.w = vw; l->header.h = l->form == 0 ? 20 : 29;
  int hdr = l->header.h, y0 = hdr + 1;
  { int gw = l->form == 0 ? 48 : 60; add_hit(l, HK_OPT, 0, 0, vw - gw - 1, 0, gw, hdr, 1); }
  int slot[SOM_PARTY], np = present_slots(ux, slot);
  int y = y0;
  if (quiet_level(ux)) { l->n = 0; return; }
  if (ux->rev_wait) { layout_revive(l, ux, y); return; }
  if (ux->pol_wait) { layout_policy(l, ux, y); return; }
  if (ux->opt_page) { layout_options(l, ux, y0); return; }
  switch (ux->fe_name_entry ? SOM_UX_FRONTEND : ux->mode) {   /* an in-game name entry gets the new game's keyboard */
  case SOM_UX_BOOT: {
    static const int b[3] = {SOM_PAD_B, SOM_PAD_START, SOM_PAD_A};
    l->info.x = 6; l->info.y = y0 + 8; l->info.w = vw - 12; l->info.h = vh * 40 / 100;
    layout_pad(l, b, 3, 3, 4, vh - (l->form == 0 ? 50 : 64), vw - 8, l->form == 0 ? 46 : 60);
    break;
  }
  case SOM_UX_FRONTEND: {
    if (ux->fe_name_entry) {
      l->info.x = 6; l->info.y = y0; l->info.w = vw - 12; l->info.h = 24;
      int ky = y0 + 28, kh = vh - ky - (l->form == 0 ? 52 : 64);
      int cols = 9, rows = 3, cw = (vw - 4) / cols, rh = kh / rows;
      for (int i = 0; i < 27; i++)
        add_hit(l, HK_LETTER, i, 0, 2 + (i % cols) * cw, ky + (i / cols) * rh, cw - 2, rh - 2, 1);
      static const int b[1] = {SOM_PAD_START};     /* letters are typed directly; START confirms */
      int py = vh - (l->form == 0 ? 48 : 60), ph = l->form == 0 ? 44 : 56, hw = (vw - 8) / 2, hn = som_ux_name_hero(ux);
      add_hit(l, HK_NAME_LAST, 0, 0, 4, py, hw - 2, ph - 2, hn >= 0 && ux->name_last[hn][0]);   /* the last name this hero was given (host-stored) */
      layout_pad(l, b, 1, 1, 4 + hw, py, vw - 8 - hw, ph);
    } else {
      l->info.x = 6; l->info.y = y0 + 4; l->info.w = vw - 12; l->info.h = vh / 4;
      static const int b[6] = {SOM_PAD_UP, SOM_PAD_DOWN, SOM_PAD_B, SOM_PAD_A, SOM_PAD_START, SOM_PAD_SELECT};
      layout_pad(l, b, 6, 3, 4, vh * 40 / 100, vw - 8, vh * 55 / 100);
    }
    break;
  }
  case SOM_UX_EVENT: case SOM_UX_TRANSITION: case SOM_UX_SHOP: {
    int ev = ux->mode == SOM_UX_EVENT;
    struct R d;
    if (l->form == 1) {                 /* landscape: heroes stacked on the left, dialogue or shop info on the right */
      int ph = (vh - y - 2) / 3 - 1; if (ph > 80) ph = 80;
      for (int i = 0; i < np; i++) add_hit(l, HK_PARTY, slot[i], 0, 2, y + i * (ph + 1), 140, ph, 0);
      int rx = 145, rw = vw - rx - 2, bh = 46;
      d.x = rx; d.y = y; d.w = rw; d.h = vh - y - bh - 4;
      if (ev) add_hit(l, HK_NEXT, 0, 0, rx, vh - bh - 2, rw, bh, 1);
      else d.h = vh - y - 2;
    } else {
      int ph = l->form == 0 ? 48 : 52;
      int cw = (vw - 6) / 3;
      if (l->form == 2) {
        for (int i = 0; i < np; i++) add_hit(l, HK_PARTY, slot[i], 0, 2, y + i * (ph + 1), vw - 4, ph, 0);
        y += np * (ph + 1) + 2;
      } else {
        for (int i = 0; i < np; i++) add_hit(l, HK_PARTY, slot[i], 0, 2 + i * (cw + 1), y, cw, ph, 0);
        y += ph + 3;
      }
      if (ev) {
        int bh = l->form == 0 ? 30 : 46;
        d.x = 2; d.y = y; d.w = vw - 4; d.h = vh - y - bh - 4;
        add_hit(l, HK_NEXT, 0, 0, 2, vh - bh - 2, vw - 4, bh, 1);
      } else { d.x = 2; d.y = y; d.w = vw - 4; d.h = vh - y - 2; }
    }
    if (ux->mode == SOM_UX_SHOP) l->info = d; else l->dlg = d;
    break;
  }
  case SOM_UX_RING: {
    /* no pad buttons: the game ring is driven from the game.  Info on top, the other heroes, the quick slots. */
    int hav = l->form == 0 ? 30 : 34, hb = l->form == 0 ? 24 : 30, sh = slot_h(l);
    int ybot = vh - 2 - sh;                                        /* quick slot row */
    int ybind = ybot - 2 - hb, yav = ybind - 2 - hav;
    l->info.x = 2; l->info.y = y; l->info.w = vw - 4; l->info.h = yav - 2 - y;
    int others[2], no = 0;
    int ow = ux->st.ring.owner < SOM_PARTY ? ux->st.ring.owner : 0;
    for (int i = 0; i < np; i++) if (slot[i] != ow) others[no++] = slot[i];
    for (int i = 0; i < no; i++) {
      int aw = (vw - 4) / no;
      add_hit(l, HK_AVATAR, others[i], 0, 2 + i * aw, yav, aw - 1, hav, 1);
    }
    /* round 3: binding a whole ring only makes sense for the spirits (the weapon and item rings are the WEAPON and ITEM tabs); the button exists only there */
    int magic = ux->st.ring.category == SOM_CAT_MAGIC, hw = magic ? (vw - 4) / 2 : vw - 4;
    add_hit(l, HK_BIND_ENTRY, 0, 0, 2, ybind, hw - 1, hb, ux->sel.valid && ux->sel.kind);
    if (magic) add_hit(l, HK_BIND_MENU, 0, 0, 2 + hw, ybind, vw - 4 - hw, hb, 1);
    layout_quick(l, 2, ybot, vw - 4, SOM_UX_QUICK_N, 1, sh);
    break;
  }
  case SOM_UX_FLAMMIE: {
    int bh = l->form == 0 ? 26 : 40;
    if (vw >= vh) {
      int side = vw - vh + bh;                                  /* right column width */
      if (side > vw * 36 / 100) side = vw * 36 / 100;
      int m = vh - hdr - 2; if (m > vw - side - 4) m = vw - side - 4;
      l->mapr.x = 2; l->mapr.y = y0; l->mapr.w = m; l->mapr.h = m;
      l->side.x = m + 6; l->side.y = y0; l->side.w = vw - m - 8; l->side.h = vh - y0 - bh - 6;
      static const int b[1] = {SOM_PAD_START};
      layout_pad(l, b, 1, 1, l->side.x, vh - bh - 2, l->side.w, bh);
    } else {
      int m = vw - 4;
      l->mapr.x = 2; l->mapr.y = y0; l->mapr.w = m; l->mapr.h = m;
      l->side.x = 2; l->side.y = y0 + m + 3; l->side.w = vw - 4; l->side.h = vh - l->side.y - bh - 4;
      static const int b[1] = {SOM_PAD_START};
      layout_pad(l, b, 1, 1, 2, vh - bh - 2, vw - 4, bh);
    }
    break;
  }
  case SOM_UX_FIELD: default: {
    int has_boss = ux->boss >= 0;
    if (has_boss) { l->boss.x = 2; l->boss.y = y; l->boss.w = vw - 4; l->boss.h = 13; y += 14; }
    if (pending(ux)) { layout_target(l, ux, 2, y, vw - 4, vh - y - 2); break; }
    int ntab = 4, sh = slot_h(l);
    int ncell = (ux->tab == SOM_UX_TAB_ITEM) ? SOM_ITEM_RECORDS : (ux->tab == SOM_UX_TAB_WEAPON ? SOM_WEAPON_TYPES : 0);
    if (ux->tab == SOM_UX_TAB_MAGIC) ncell = ux->spirit == 0xFF ? SOM_SPIRITS : 4;
    int gx, gw, gy;                                 /* the content column: x, width, top */
    if (l->form == 1) {                             /* landscape: heroes + quick slots on the left, tabs and content on the right */
      int qh = 2 * sh + 1;
      int ph = (vh - y - qh - 4) / 3 - 1; if (ph > 84) ph = 84;
      int quick_y = y + 3 * (ph + 1) + 1;
      for (int i = 0; i < np; i++) add_hit(l, HK_PARTY, slot[i], 0, 2, y + i * (ph + 1), 140, ph, 1);
      gx = 145; gw = vw - gx - 2; gy = y;
      layout_quick(l, 2, quick_y, 140, 4, 2, sh);
      int th = 30;                              /* 7.2 mm on the Thor */
      for (int i = 0; i < ntab; i++) {          /* two rows: WEAPON ITEM / MAGIC MAP */
        int tw2 = gw / 2;
        add_hit(l, HK_TAB, i, 0, gx + (i % 2) * tw2, gy + (i / 2) * (th + 1), tw2 - 1, th, 1);
      }
      gy += 2 * (th + 1) + 1;
    } else {
      int ph;
      if (l->form == 2) {
        ph = 31;                                  /* slim cards: two text rows */
        for (int i = 0; i < np; i++) add_hit(l, HK_PARTY, slot[i], 0, 2, y + i * (ph + 1), vw - 4, ph, 1);
        y += np * (ph + 1) + 1;
      } else {
        int pw = (vw - 6) / 3; ph = 48;
        for (int i = 0; i < np; i++) add_hit(l, HK_PARTY, slot[i], 0, 2 + i * (pw + 1), y, pw, ph, 1);
        y += ph + 2;
      }
      gx = 2; gw = vw - 4; gy = y;
      int th = l->form == 0 ? 22 : 30, tw = gw / ntab;
      /* the ITEM grid has three rows and each cell needs 38 vp for icon + two label lines; on a short panel the quick strip yields */
      layout_quick(l, 2, gy, vw - 4, SOM_UX_QUICK_N, 1, sh); gy += sh + 1;   /* the shoulder slots are always on screen */
      for (int i = 0; i < ntab; i++) add_hit(l, HK_TAB, i, 0, gx + i * tw, gy, tw - 1, th, 1);
      gy += th + 1;
    }
    int gh = vh - gy - 2;
    if (ux->tab == SOM_UX_TAB_MAP) {
      if (l->form == 1) {
        l->radar.x = gx; l->radar.y = gy; l->radar.w = gw; l->radar.h = gw * 224 / 256 + 2; if (l->radar.h > gh * 56 / 100) l->radar.h = gh * 56 / 100;   /* the rest is the three enemy cards */
        l->elist.x = gx; l->elist.y = gy + l->radar.h + 3; l->elist.w = gw; l->elist.h = vh - l->elist.y - 2;
      } else {
        l->radar.x = gx; l->radar.y = gy; l->radar.h = gh; l->radar.w = gh * 256 / 224; if (l->radar.w > gw * 60 / 100) l->radar.w = gw * 60 / 100;
        l->elist.x = l->radar.x + l->radar.w + 3; l->elist.y = gy; l->elist.w = gx + gw - l->elist.x; l->elist.h = gh;
      }
      add_hit(l, HK_MAPVIEW, 0, 0, l->radar.x, l->radar.y, l->radar.w, l->radar.h, 1);
      break;
    }
    int cols, rows;
    if (l->form == 1) { cols = 2; rows = (ncell + 1) / 2; if (rows < 1) rows = 1; if (ux->tab == SOM_UX_TAB_ITEM) rows = 6; else if (rows < 4) rows = 4; }
    else {
      cols = 4; rows = (ncell + 3) / 4; if (rows < 1) rows = 1; if (rows > 3) rows = 3;
      if (l->form == 2 && ux->tab == SOM_UX_TAB_MAGIC && ux->spirit != 0xFF) { cols = 2; rows = 2; }   /* spell names need the width */
      if (ux->tab == SOM_UX_TAB_ITEM && gh < 3 * 38) { cols = 6; rows = 2; }   /* short panel: two rows of six keep the slot row visible */
    }
    int cw = gw / cols, chh = gh / rows; if (chh > 56) chh = 56;
    l->cols = cols; l->rows = rows;
    int vis[SOM_ITEM_RECORDS > 12 ? SOM_ITEM_RECORDS : 12], nv = 0;           /* round 3: only what the player has owned at least once, packed */
    for (int i = 0; i < ncell && nv < 12; i++) if (cell_visible(ux, i)) vis[nv++] = i;
    for (int j = 0; j < nv && j < cols * rows; j++) {
      int i = vis[j], en = 0;
      const struct som_state *st = &ux->st;
      if (ux->tab == SOM_UX_TAB_WEAPON) en = st->weapon_slot[i] != 0xFF;
      else if (ux->tab == SOM_UX_TAB_ITEM) en = st->item[i].qty != 0;
      else if (ux->spirit == 0xFF) en = som_spirit_ring_index(st, ux->hero, i) >= 0;
      else en = 1;
      add_hit(l, HK_CELL, i, 0, gx + (j % cols) * cw, gy + (j / cols) * chh, cw, chh, en);   /* drawn 1 vp smaller: the gap is tappable */
    }
    break;
  }
  }
}

/* ------------------------------------------------------------------ pieces */
static uint32_t hp_col(unsigned hp, unsigned mx) { return hp * 4 < mx ? C_RED : (hp * 2 < mx ? C_YEL : C_GRN); }

/* ---- small shapes: status icons and the danger marker (shape-coded, never colour only) ---- */
static void blit(struct cv *c, int x, int y, const char *const *rows, int h, uint32_t fill, uint32_t line, uint32_t hi) {
  for (int j = 0; j < h; j++)
    for (int i = 0; rows[j][i]; i++) {
      char k = rows[j][i];
      if (k == '#') frect(c, x + i, y + j, 1, 1, fill);
      else if (k == 'o') frect(c, x + i, y + j, 1, 1, line);
      else if (k == '+') frect(c, x + i, y + j, 1, 1, hi);
    }
}
static const char *const SH_PSN[11] = {"....o....", "...o#o...", "..o###o..", "..o###o..", ".o#####o.", ".o##+##o.", ".o#+###o.", ".o#####o.", "..o###o..", "...ooo...", "........."};
static const char *const SH_AIL[11] = {"....o....", "...o#o...", "..o###o..", ".o##o##o.", "o###o###o", "o###o###o", ".o#####o.", "..o#o#o..", "...o#o...", "....o....", "........."};
static const char *const SH_KO[11] = {"ooo...ooo", "o#oo.oo#o", ".o##o##o.", "..o###o..", "...o#o...", "..o###o..", ".o##o##o.", "o#oo.oo#o", "ooo...ooo", ".........", "........."};
static const char *const SH_WARN[11] = {"....o....", "...o#o...", "..o#o#o..", "..o#o#o..", ".o##o##o.", ".o##o##o.", "o###+###o", "o###o###o", "ooooooooo", ".........", "........."};

/* HP danger: 0 fine, 1 below 25 percent, 2 below 10 percent, 3 down (KO) */
static int danger(const struct som_hero *h) {
  if (!h->alive) return 3;
  if (!h->hp_max) return 0;
  if (h->hp * 10u < h->hp_max) return 2;
  if (h->hp * 4u < h->hp_max) return 1;
  return 0;
}
static int lgm(const struct cv *c) { return c->s >= 3 ? 2 : 1; }   /* large text multiplier: 2 on the Thor, 1 on the 3DS */

/* gauge with an optional diagonal hatch on the fill (pattern cue for colour-blind players) */
static void bar(struct cv *c, int x, int y, int w, int h, unsigned num, unsigned den, uint32_t col, int hatch) {
  gauge(c, x, y, w, h, num, den, col);
  if (!hatch || !den || h < 4) return;
  int fw = (int)((unsigned long)(w - 2) * (num > den ? den : num) / den);
  for (int j = 1; j < h - 1; j++)
    for (int i = 0; i < fw; i++)
      if (((i + j) % (hatch == 2 ? 3 : 5)) == 0) frect(c, x + 1 + i, y + j, 1, 1, C_DARK);
}

static void card_frame(struct cv *c, struct R r, int dg, int phase) {
  uint32_t col = phase ? C_RED : 0x9A2A2Au;
  int th = dg == 2 ? 2 : 1;
  if (dg == 2) {                                         /* critical: solid frame */
    frect(c, r.x, r.y, r.w, th, col); frect(c, r.x, r.y + r.h - th, r.w, th, col);
    frect(c, r.x, r.y, th, r.h, col); frect(c, r.x + r.w - th, r.y, th, r.h, col);
  } else {                                               /* low: dashed frame */
    for (int i = 0; i < r.w; i += 8) { int n = r.w - i < 4 ? r.w - i : 4; frect(c, r.x + i, r.y, n, th, col); frect(c, r.x + i, r.y + r.h - th, n, th, col); }
    for (int j = 0; j < r.h; j += 8) { int n = r.h - j < 4 ? r.h - j : 4; frect(c, r.x, r.y + j, th, n, col); frect(c, r.x + r.w - th, r.y + j, th, n, col); }
  }
}

/* vertical text rows of one card: each new band must start below the previous one and end inside the card */
struct bands { int end, limit; };
static void band(struct bands *b, int y, int h, const char *what) {
  if (y < b->end || y + h > b->limit) audit(what, "rows overlap or leave the card");
  if (y + h > b->end) b->end = y + h;
}

static int rects_meet(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
  return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}
/* Text boxes of the card being drawn: every one must stay inside the card, clear of the portrait and of every earlier box. */
static struct { int n; struct { int x, y, w, h; } b[12]; struct R card, face; } cb;
static void tbox(int x, int y, int w, int h, const char *what) {
  if (x < cb.card.x || y < cb.card.y || x + w > cb.card.x + cb.card.w || y + h > cb.card.y + cb.card.h) audit("card text outside", what);
  if (rects_meet(x, y, w, h, cb.face.x, cb.face.y, cb.face.w, cb.face.h)) audit("card text over portrait", what);
  for (int i = 0; i < cb.n; i++)
    if (rects_meet(x, y, w, h, cb.b[i].x, cb.b[i].y, cb.b[i].w, cb.b[i].h)) audit("card text overlap", what);
  if (g_tn < 96) { snprintf(g_tlog[g_tn].s, sizeof g_tlog[g_tn].s, "%s", what); g_tlog[g_tn].x = x; g_tlog[g_tn].y = y; g_tlog[g_tn].w = w; g_tlog[g_tn].h = h; g_tn++; }
  if (cb.n < 12) { cb.b[cb.n].x = x; cb.b[cb.n].y = y; cb.b[cb.n].w = w; cb.b[cb.n].h = h; cb.n++; }
}
/* One "HP 523/524" row of a narrow card: label, a real gap, then the numbers.  Faces are tried from the normal one down to the squeezed
 * one; the last resort keeps the label and shrinks the gap to 2 vp.  Never glued ("H523/524" was the old failure), never cut. */
static void value_row(struct cv *c, int x, int y, int maxw, const char *label, unsigned cur, unsigned mx, uint32_t col) {
  char num[24];
  snprintf(num, sizeof num, "%u/%u", cur, mx);
  int saved = c->sq;
  for (int step = 0; step < 4; step++) {
    int sq = step < 3 ? step : 2, gap = step < 3 ? 0 : 2;
    c->sq = sq;
    int lw = text_w(c, label, 0), sp = gap ? gap : text_w(c, " ", 0), nw = text_w(c, num, 0);
    if (lw + sp + nw > maxw && step < 3) continue;
    if (lw + sp + nw > maxw) audit("card value row", label);
    tbox(x, y, lw, 11, label); tbox(x + lw + sp, y, nw, 11, num);
    if (sp < 2) audit("card label glued", label);
    text(c, x, y, label, col, 0);
    text(c, x + lw + sp, y, num, col, 0);
    c->sq = saved;
    return;
  }
  c->sq = saved;
}
/* Big-number row of a tall/mid card: label, gap, current value at L times size, "/max" small.  When the 3-digit/3-digit pair does not
 * fit, the value face tightens first, then drops to normal size; the label is always "HP"/"MP" and separated by 3 vp. */
static void big_row(struct cv *c, int x, int y, int maxw, const char *label, unsigned cur, unsigned mx, uint32_t col, int L) {
  char n1[16], n2[16];
  snprintf(n1, sizeof n1, "%u", cur); snprintf(n2, sizeof n2, "/%u", mx);
  static const int face[4] = {0, 1, 1, 2}, mul[4] = {0, 0, 1, 1};
  int saved = c->sq;
  for (int v = 0; v < 4; v++) {
    int m = mul[v] ? 1 : L;
    c->sq = face[v];
    int sq_face = c->sq; c->sq = saved;
    int lw = text_w(c, label, 0) + 3;                    /* the label keeps the caller's face, so the gap is measured with it */
    c->sq = sq_face;
    int nw = text_wm(c, n1, m), sw = text_w(c, n2, 0);
    if (lw + nw + sw > maxw && v < 3) continue;
    if (lw + nw + sw > maxw) audit("card big row", n2);
    c->sq = saved;
    tbox(x, y + 11 * (L - 1) / 2, text_w(c, label, 0), 11, label);
    text(c, x, y + 11 * (L - 1) / 2, label, col, 0);
    c->sq = face[v];
    tbox(x + lw, y + 11 * (L - m) / 2, nw, 11 * m, n1); tbox(x + lw + nw, y + 11 * L - 11, sw, 11, n2);
    text_m(c, x + lw, y + 11 * (L - m) / 2, n1, col, 1, m);
    text(c, x + lw + nw, y + 11 * L - 11, n2, col, 0);
    c->sq = saved;
    return;
  }
  c->sq = saved;
}

/* MP bars of the cards drawn since the last som_ux_audit(1): the draw audit requires one for every standing hero with magic. */
static int g_card_slot, g_mp_bars[3];
static int g_mp_bars_reset(void) { g_mp_bars[0] = g_mp_bars[1] = g_mp_bars[2] = 0; return 0; }
int som_ux_mp_bars(int slot) { return slot >= 0 && slot < 3 ? g_mp_bars[slot] : 0; }
static void mpbar(struct cv *c, int x, int y, int w, int h, unsigned v, unsigned mx, uint32_t col) {
  bar(c, x, y, w, h, v, mx, col, 0);
  if (g_card_slot >= 0 && g_card_slot < 3) g_mp_bars[g_card_slot]++;
}
/* the boy has no magic: a dim "NO MP" marker where the MP row would be, never an empty gap */
static void nomp(struct cv *c, int x, int y, int w, int unused) { (void)c; (void)x; (void)y; (void)w; (void)unused; }   /* round 3: a hero without MP shows nothing in the MP row */

/* Status background of a hero card (round 3).  Incapacitating states get a strong, distinct tint; the rest a faint one.  Shape icons stay as the non-colour cue.
 * masks: docs/rom-combat.md 12.1 (0010 sleep, 0020 frozen, 0040 stone, 0100 balloon, 0200 moogle, 0400 barrel, 1000 pygmy, 4000 burning). */
static int status_bg(unsigned st, uint32_t *col, int *a) {
  if (st & 0x8000) return 0;
  static const struct { unsigned m; uint32_t col; } inc[] = {{0x0040, 0x909090u}, {0x0020, 0x40D0F0u}, {0x0010, 0x6040E0u}, {0x0100, 0xF070B0u}, {0x4000, 0xF06020u}, {0x0200, 0xB08040u}, {0x1000, 0xB08040u}, {0x0400, 0x806040u}};
  for (unsigned i = 0; i < sizeof inc / sizeof inc[0]; i++) if (st & inc[i].m) { *col = inc[i].col; *a = 150; return 2; }
  if (st & 0x2000) { *col = 0x40C040u; *a = 70; return 1; }
  if (st & 0x0080) { *col = 0xA060C0u; *a = 70; return 1; }
  if (st & 0x000C) { *col = 0xC0A040u; *a = 60; return 1; }
  return 0;
}

#define BARH_T 8   /* HP / MP bar thickness of the tall and mid cards (round 3: was 6) */
static void draw_clock(struct cv *c, int cx, int cy, int r);
static void card_inner(struct cv *c, const struct som_ux *ux, int slot, struct R r, int sel, int dim) {
  const struct som_hero *h = &ux->st.hero[slot];
  char b[40];
  int dg = danger(h), calm = dim;                       /* calm: cutscenes get a static marker, no pulse */
  int phase = dg == 2 ? ((ux->frame_no >> 3) & 1) : ((ux->frame_no >> 4) & 1);
  int tall = r.h >= 64, wide = r.w >= 160, L = lgm(c);
  int mid = !tall && !wide && r.h >= 54 && c->s >= 3;   /* Thor landscape column with the quick slots below */
  int slim = wide && r.h < 46;                          /* Thor portrait: two rows, name and level in a left column */
  win(c, r.x, r.y, r.w, r.h, (dim || dg == 3) ? 2 : (sel ? 1 : 0));
  { uint32_t sc; int sa; if (!dim && status_bg(h->status, &sc, &sa)) brect(c, r.x + 2, r.y + 2, r.w - 4, r.h - 4, sc, sa); }
  int narrow = ((tall || mid) && r.w < 160) || r.w < 120;                /* level goes onto the portrait */
  int ph = tall ? (narrow ? 38 : 44) : (mid ? 36 : (slim ? r.h - 10 : (wide ? r.h - 12 : (narrow ? r.h - 24 : r.h - 22)))); if (ph > r.h - 8) ph = r.h - 8; if (ph > 48) ph = 48;
  int pw = ph * 98 / 108, px = r.x + 4, py = r.y + (slim ? 3 : 4);
  cb.n = 0; cb.card = r; cb.face.x = px; cb.face.y = py; cb.face.w = pw; cb.face.h = ph;
  portrait_t(c, slot, px, py, ph, dg == 3 || dim, ((dg == 1 || dg == 2) && !calm) ? (phase ? 110 : 40) : 0);
  int cy = py + ph + 2;
  if (cy + 3 <= r.y + r.h - 2) gauge(c, px, cy, pw, tall ? 5 : 3, h->recharge_pct, 100, (dim || dg == 3) ? C_GREY : (h->recharge_pct >= 100 ? C_ORG : 0xC07030u));
  int tx = px + pw + 4, tw = r.x + r.w - 4 - tx, ty = r.y + (slim ? 3 : 2);
  uint32_t tc = (dim || dg == 3) ? C_DIM : C_TXT;
  struct bands bd = {ty, r.y + r.h - 2};
  /* statuses: right aligned, shape coded; the name always stays whole, so when the row is short the least important icon goes
   * first (warning triangle, then ailment): the bar hatch and the card frame still say "danger" */
  int ix = r.x + r.w - 4;
  int gh = tall ? 5 : 3, lvy = cy + gh + 1;                /* narrow cards: level under the portrait gauge, never over the face */
  int lvl_below = 0;                                    /* round 3: the portrait is the name; the first row carries only the level */
  (void)lvy;
  char nmb[24];
  snprintf(nmb, sizeof nmb, "L%d", h->level);
  int nmw = text_w(c, nmb, 0);
  int room = r.x + r.w - 4 - tx - nmw - 2;
  int show_ko = dg == 3, show_psn = !show_ko && (h->status & 0x2000), show_ail = !show_ko && (h->status & 0x5FFF & ~0x2000u);
  int show_warn = (dg == 1 || dg == 2) && (!calm || dg == 2);
  { int need = (show_ko ? 9 : 0) + (show_psn ? 10 : 0) + (show_ail ? 10 : 0) + (show_warn ? 11 : 0);
    if (need > room && show_warn) { show_warn = 0; need -= 11; }
    if (need > room && show_ail) { show_ail = 0; need -= 10; }
    if (need > room && show_psn && !show_ko) { show_psn = 0; } }
  if (show_ko) { ix -= 9; blit(c, ix, ty + 1, SH_KO, 9, C_LIGHT, C_DARK, C_LIGHT); }
  if (show_psn) { ix -= 10; blit(c, ix, ty + 1, SH_PSN, 10, C_GRN, C_DARK, C_LIGHT); }
  if (show_ail) { ix -= 10; blit(c, ix, ty + 1, SH_AIL, 10, C_PURPLE, C_DARK, C_LIGHT); }
  if (show_warn) { ix -= 11; blit(c, ix, ty + 1, SH_WARN, 9, (dg == 2 && phase && !calm) ? C_RED : C_YEL, C_DARK, C_DARK); }
  { int qd = 0;                                          /* a cast of this caster is waiting for its moment: a clock on its card */
    if (slot == 1 || slot == 2) for (int i = 0; i < 42 && !qd; i++) if (((i % 6) >= 3 ? 1 : 2) == slot && ux->spell_avail[i] == SOM_UX_AV_QUEUED) qd = 1;
    if (qd && !show_ko && room - (show_psn ? 10 : 0) - (show_ail ? 10 : 0) - (show_warn ? 11 : 0) >= 12) { ix -= 12; draw_clock(c, ix + 5, ty + 6, 5); } }
  int has_mp = h->mp_max > 0 || slot > 0;               /* the girl and the sprite always show their MP; the boy has none */
  uint32_t hc = dim ? C_GREY : hp_col(h->hp, h->hp_max), mc = dim ? C_GREY : (h->mp_max && h->mp * 4 < h->mp_max ? C_ORG : 0x58A0F0u);
  g_card_slot = slot;
  int hatch = (dg == 1 || dg == 2) ? (dg == 2 ? 2 : 1) : 0;
  const char *hl = "HP", *ml = "MP";

  if (slim) {
    /* [name / level] [HP text / MP text] [HP bar / MP bar]; rows 13 vp apart, glyph rows 11 tall */
    int y1 = ty, y2 = ty + 13, ca = 52;
    band(&bd, y1, 11, "card row 1"); band(&bd, y2, 11, "card row 2");
    snprintf(b, sizeof b, "L%d", h->level); tbox(tx, y1, text_w(c, b, 0) < ca ? text_w(c, b, 0) : ca, 11, b); text_fit(c, tx, y1, ca, b, tc, 0);
    if (dg == 3) {                                       /* KO: boxed text in the second row */
      int kw = text_w(c, "KO", 0) + 8;
      frect(c, tx, y2 - 1, kw, 13, C_LIGHT); text(c, tx + 4, y2, "KO", C_DARK, 0);
      return;
    }
    char t1[24], t2[24];
    snprintf(t1, sizeof t1, "%s %u/%u", hl, h->hp, h->hp_max);
    snprintf(t2, sizeof t2, "%s %u/%u", ml, h->mp, h->mp_max);
    int cb_x = tx + ca + 4, w1 = text_w(c, t1, 0), w2 = has_mp ? text_w(c, t2, 0) : 0, wm = w1 > w2 ? w1 : w2;
    int bx = cb_x + wm + 4;
    value_row(c, cb_x, y1, wm + 1, hl, h->hp, h->hp_max, tc);
    if (has_mp) value_row(c, cb_x, y2, wm + 1, ml, h->mp, h->mp_max, tc); else nomp(c, cb_x, y2, r.x + r.w - 4 - cb_x, 0);
    int bw1 = ix - 3 - bx, bw2 = r.x + r.w - 4 - bx;
    if (bw1 < 20) audit("card bar too short", t1);
    bar(c, bx, y1 + 1, bw1, 9, h->hp, h->hp_max, hc, hatch);
    if (has_mp) mpbar(c, bx, y2 + 1, bw2, 9, h->mp, h->mp_max, mc);
    if ((dg == 1 || dg == 2) && !calm) card_frame(c, r, dg, phase);
    else if ((dg == 1 || dg == 2) && calm) card_frame(c, r, 1, 0);
    return;
  }
  (void)lvl_below;
  snprintf(b, sizeof b, "L%d", h->level);
  band(&bd, ty, 11, "card name");
  tbox(tx, ty, text_w(c, b, 0) < ix - tx - 2 ? text_w(c, b, 0) : ix - tx - 2, 11, b);
  text_fit(c, tx, ty, ix - tx - 2, b, tc, 0);
  if (dg == 3) {                                         /* KO: boxed text, not colour */
    int kw = text_w(c, "KO", 0) + 8, ky = ty + 14;
    band(&bd, ky, 13, "card KO");
    frect(c, tx, ky, kw, 13, C_LIGHT); text(c, tx + 4, ky + 1, "KO", C_DARK, 0);
    return;
  }
  int Lc = L;                                           /* big rows drop to normal size when an HP + MP card is too short for them; the */
  if (tall && 35 + 22 * Lc > r.h) Lc = 1;               /* boy's card follows the same rule so all three cards use one HP size */
  if (mid && 35 + 11 * Lc > r.h) Lc = 1;
  if (tall) {                                            /* big numbers: current value at 2x, maximum small */
    int y1 = ty + 13;
    band(&bd, y1, 11 * Lc, "card HP");
    big_row(c, tx, y1, r.x + r.w - 3 - tx, hl, h->hp, h->hp_max, tc, Lc);
    band(&bd, y1 + 11 * Lc + 1, BARH_T, "card HP bar");
    bar(c, tx, y1 + 11 * Lc + 1, tw, BARH_T, h->hp, h->hp_max, hc, hatch);
    if (has_mp) {
      int y2 = y1 + 11 * Lc + 3 + BARH_T;
      band(&bd, y2, 11 * Lc, "card MP");
      big_row(c, tx, y2, r.x + r.w - 3 - tx, ml, h->mp, h->mp_max, tc, Lc);
      band(&bd, y2 + 11 * Lc + 1, BARH_T, "card MP bar");
      mpbar(c, tx, y2 + 11 * Lc + 1, tw, BARH_T, h->mp, h->mp_max, mc);
    } else nomp(c, tx, y1 + 11 * Lc + 9 + (11 * Lc - 11) / 2, tw, 6);
  } else if (mid) {                                      /* big HP number, bar, then MP text and bar on one row */
    int y1 = ty + 11;
    band(&bd, y1, 11 * Lc, "card HP");
    big_row(c, tx, y1, r.x + r.w - 3 - tx, hl, h->hp, h->hp_max, tc, Lc);
    band(&bd, y1 + 11 * Lc, BARH_T, "card HP bar");
    bar(c, tx, y1 + 11 * Lc, tw, BARH_T, h->hp, h->hp_max, hc, hatch);
    if (has_mp) {
      int y2 = y1 + 11 * Lc + 1 + BARH_T;
      snprintf(b, sizeof b, "%s %u/%u", ml, h->mp, h->mp_max);
      int mw = text_w(c, b, 0);
      band(&bd, y2, 11, "card MP");
      value_row(c, tx, y2, mw, ml, h->mp, h->mp_max, tc);
      if (tw - mw - 3 > 12) mpbar(c, tx + mw + 3, y2 + 2, tw - mw - 3, BARH_T, h->mp, h->mp_max, mc);
      else audit("card MP bar too short", b);
    } else nomp(c, tx, y1 + 11 * Lc + 7, tw, 6);
  } else if (wide) {                                     /* stacked rows: label and numbers left, long bar right */
    int bx = tx + 84, bw = r.x + r.w - 4 - bx;
    band(&bd, ty + 13, 11, "card HP");
    value_row(c, tx, ty + 13, 82, hl, h->hp, h->hp_max, tc);
    bar(c, bx, ty + 14, bw, 9, h->hp, h->hp_max, hc, hatch);
    if (has_mp) {
      band(&bd, ty + 25, 11, "card MP");
      value_row(c, tx, ty + 25, 82, ml, h->mp, h->mp_max, tc);
      mpbar(c, bx, ty + 26, bw, 9, h->mp, h->mp_max, mc);
    } else nomp(c, tx, ty + 25, bw + 84, 7);
  } else {                                               /* compact (3DS): value, bar below */
    band(&bd, ty + 11, 11, "card HP");
    value_row(c, tx, ty + 11, tw, hl, h->hp, h->hp_max, tc);
    bar(c, tx, ty + 22, tw, 6, h->hp, h->hp_max, hc, hatch);
    if (has_mp) {
      band(&bd, ty + 27, 11, "card MP");
      value_row(c, tx, ty + 27, tw, ml, h->mp, h->mp_max, tc);
      mpbar(c, tx, ty + 37, tw, 6, h->mp, h->mp_max, mc);
    } else nomp(c, tx, ty + 27, tw, 4);
  }
  if ((dg == 1 || dg == 2) && !calm) card_frame(c, r, dg, phase);
  else if ((dg == 1 || dg == 2) && calm) card_frame(c, r, 1, 0);
}

static void card(struct cv *c, const struct som_ux *ux, int slot, struct R r, int sel, int dim) {
  int before = g_mp_bars[slot];
  card_inner(c, ux, slot, r, sel, dim);
  const struct som_hero *h = &ux->st.hero[slot];
  if (slot > 0 && h->present && danger(h) != 3 && g_mp_bars[slot] == before) audit("hero with magic shows no MP bar", hname(slot));
}

#define weapon_attack ux_weapon_attack
static const char *weapon_label(const struct som_ux *ux, int t, char *buf) {
  int tier = ux->st.weapon_slot[t] & 15;
  const char *n = som_ux_name(ux, NAME_WEAPON + t * 9 + (tier < 9 ? tier : 0));
  snprintf(buf, 24, "%s", n ? n : WEAPON_FALLBACK[t]);
  return buf;
}
static const char *item_label(const struct som_ux *ux, int id, char *buf) {
  const char *n = som_ux_name(ux, NAME_ITEM + id);
  snprintf(buf, 24, "%s", n ? n : (id < 11 ? ITEM_FALLBACK[id] : "?"));
  return buf;
}
/* The girl's Lumina is raw spirit byte 7 but spell position 6 (ids 36..41 hold the sprite's Shade and the girl's Lumina). */
static int spell_id(const struct som_ux *ux, int hero, int spirit, int slot) { (void)ux; return 6 * (hero == 1 && spirit == 7 ? 6 : spirit) + (hero == 1 ? 3 : 0) + slot; }
static int spell_cost(const struct som_ux *ux, int id) {
  size_t len; const uint8_t *r = ux_rom(ux, &len);
  if (!r || id < 0 || id > 41 || len < 0x102AD0u + 64u * 42u) return -1;
  return r[0x102AD0u + 64u * (unsigned)id + 15];
}

/* Hotkey badge: a light pill with the bound physical button (ZL, R, X ...), drawn at (x, y) on a button. */
static int draw_pill(struct cv *c, int x, int y, const char *label) {
  int w = text_w(c, label, 0) + 5;
  frect(c, x, y, w, 12, 0xDCE0F8u);
  frect(c, x + 1, y + 1, w - 2, 10, 0x161C48u);
  text(c, x + 3, y + 1, label, C_TXT, 0);
  return w;
}

/* ---- label fitting: the whole name, never cut.  Tries the normal face on one line, then two lines (break at a space or after a
 * hyphen), then the squeezed face the same way.  A name that fits none of them is drawn cut and counted by the audit. */
struct lbl { int sq, n, w[2]; char l[2][28]; };
static int split_ok(struct cv *c, const char *s, int at, int sq, int maxw, struct lbl *p) {
  char a[28], b[28];
  int n = (int)strlen(s);
  if (n > 27) return 0;
  memcpy(a, s, (size_t)at); a[at] = 0;
  const char *rest = s + at; while (*rest == ' ') rest++;
  snprintf(b, sizeof b, "%s", rest);
  int sv = c->sq; c->sq = sq;
  int wa = text_w(c, a, 0), wb = text_w(c, b, 0);
  c->sq = sv;
  if (wa > maxw || wb > maxw || !a[0] || !b[0]) return 0;
  p->sq = sq; p->n = 2; p->w[0] = wa; p->w[1] = wb;
  snprintf(p->l[0], sizeof p->l[0], "%s", a); snprintf(p->l[1], sizeof p->l[1], "%s", b);
  return 1;
}
static int plan_label_sq(struct cv *c, const char *s, int maxw, int maxl, int maxsq, struct lbl *p) {   /* maxsq: tightest face allowed */
  memset(p, 0, sizeof *p);
  for (int sq = 0; sq <= maxsq; sq++) {
    int sv = c->sq; c->sq = sq;
    int w = text_w(c, s, 0);
    c->sq = sv;
    if (w <= maxw && strlen(s) < sizeof p->l[0]) { p->sq = sq; p->n = 1; p->w[0] = w; snprintf(p->l[0], sizeof p->l[0], "%s", s); return 1; }
    if (maxl >= 2) {
      int best = -1, bw = 1 << 20;
      for (int i = 1; s[i]; i++) {
        if (s[i] == ' ' || s[i - 1] == '-') {
          int at = s[i] == ' ' ? i : i;
          struct lbl t;
          if (split_ok(c, s, at, sq, maxw, &t)) { int m = t.w[0] > t.w[1] ? t.w[0] : t.w[1]; if (m < bw) { bw = m; best = at; } }
        }
      }
      if (best > 0) return split_ok(c, s, best, sq, maxw, p);
    }
  }
  if (maxl >= 2 && !strchr(s, ' ') && strlen(s) >= 7 && strlen(s) < 26) {       /* last resort for narrow cells: break one long word with a hyphen */
    char h[28]; int n = (int)strlen(s), k = (n + 1) / 2;
    memcpy(h, s, (size_t)k); h[k] = '-'; memcpy(h + k + 1, s + k, (size_t)(n - k) + 1);
    for (int sq = 0; sq <= maxsq; sq++) if (split_ok(c, h, k + 1, sq, maxw, p)) return 1;
  }
  return 0;
}
static int plan_label(struct cv *c, const char *s, int maxw, int maxl, struct lbl *p) { return plan_label_sq(c, s, maxw, maxl, 2, p); }
static void put_label(struct cv *c, int x, int y, int w, const struct lbl *p, uint32_t col, int center) {
  int sv = c->sq; c->sq = p->sq;
  for (int i = 0; i < p->n; i++) text(c, center ? x + (w - p->w[i]) / 2 : x, y + i * 11, p->l[i], col, 0);
  c->sq = sv;
}

/* queued badge: a small clock (ring, hour and minute hands) centred at (cx, cy) vp */
static void draw_clock(struct cv *c, int cx, int cy, int r) {
  for (int j = -r; j <= r; j++)
    for (int i = -r; i <= r; i++) {
      int q = i * i + j * j;
      if (q <= r * r) frect(c, cx + i, cy + j, 1, 1, q >= (r - 1) * (r - 1) ? C_YEL : C_DARK);
    }
  frect(c, cx, cy - r + 2, 1, r - 1, C_YEL);
  frect(c, cx, cy, r - 2, 1, C_YEL);
}

static void draw_cell(struct cv *c, struct R r, int ic, int kind, uint32_t tint, const char *label, const char *badge, int state, const char *bind,
                      const char *note) {
  /* state: 0 disabled, 1 normal, 2 selected, 3 queued (normal with a clock badge on the icon).  note: optional top-left text (weapon attack).
   * Three shapes, chosen so the whole name fits and nothing overlaps (the frame is 3 vp thick, text keeps 1 vp off it):
   *   vertical: icon on top, up to two label lines below;
   *   horizontal: icon in a left column, up to two label lines on the right, the badge right-aligned;
   *   header: badge and note on a top row, then the horizontal shape below it (cells too short for icon + label + top row). */
  int queued = state == 3;
  if (queued) state = 1;
  win(c, r.x, r.y, r.w, r.h, state == 2 || queued ? 1 : (state == 0 ? 2 : 0));
  uint32_t lc = state == 0 ? C_GREY : C_TXT;
  int bw = badge && badge[0] ? text_w(c, badge, 0) : 0, nw = note && note[0] ? text_w(c, note, 0) : 0;
  const char *lab = label ? label : "";
  int in = r.w < 40 ? 3 : 4, lw0 = r.w - 2 * in;
  struct lbl p;
  /* the icon may sit on the top row only when the badge and the note leave the middle 18 vp free */
  int top = (bw && bw + in > r.w / 2 - 9) || (nw && nw + in > r.w / 2 - 9) ? 14 : 0;
  int iy = top ? top : (r.h >= 46 ? 4 : 3);
  int maxl = (r.h - 3 - (iy + 17)) / 11; if (maxl > 2) maxl = 2;
  int hdr = (bw || nw) && r.h >= 14 + 24 ? 14 : 0;          /* header shape: badge and note move up, the label gets the full width */
  int y0 = r.y + hdr, h0 = r.h - hdr;
  int lx = r.x + in + 18, lw = r.x + r.w - in - lx - (!hdr && bw ? bw + 2 : 0);
  int ol = r.w < 56 ? 1 : 2, oy = r.h >= 46 ? 4 : 1, om = (r.h - 2 - (oy + 12)) / 11; if (om > 2) om = 2;
  if (!lab[0] || (maxl >= 1 && plan_label_sq(c, lab, lw0, maxl, 1, &p))) {
    if (ic >= 0) icon(c, r.x + (r.w - 16) / 2, r.y + iy, ic, kind, tint, state == 0);
    if (queued) draw_clock(c, r.x + (r.w - 16) / 2 + 2, r.y + iy + 11, 5);
    if (lab[0]) put_label(c, r.x + in, r.y + r.h - 3 - p.n * 11, lw0, &p, lc, 1);
  } else if (lw > 0 && plan_label_sq(c, lab, lw, 2, 1, &p) && h0 >= p.n * 11 + 4) {
    if (ic >= 0) icon(c, r.x + in - 1, y0 + (h0 - 16) / 2, ic, kind, tint, state == 0);
    if (queued) draw_clock(c, r.x + in + 13, y0 + (h0 - 16) / 2 + 12, 5);
    put_label(c, lx, y0 + (h0 - p.n * 11) / 2, lw, &p, lc, 0);
    if (!hdr) {
      if (bw) text(c, r.x + r.w - in - bw, r.y + (r.h - 11) / 2, badge, state == 0 ? C_GREY : C_YEL, 0);
      bw = 0; nw = 0;
    }
  } else if (om >= 1 && plan_label(c, lab, r.w - 2 * ol, om, &p)) {   /* small cells (3DS): the icon's transparent bottom rows may touch the label */
    if (ic >= 0) icon(c, r.x + (r.w - 16) / 2, r.y + oy, ic, kind, tint, state == 0);
    if (queued) draw_clock(c, r.x + (r.w - 16) / 2 + 2, r.y + oy + 11, 5);
    put_label(c, r.x + ol, r.y + r.h - 2 - p.n * 11, r.w - 2 * ol, &p, lc, 1);
    if (bw) text(c, r.x + r.w - bw - 3, r.y + 2, badge, state == 0 ? C_GREY : C_YEL, 0);
    bw = 0; nw = 0;
  } else {
    audit("label does not fit", lab);
    struct lbl q; memset(&q, 0, sizeof q); q.sq = 2; q.n = 1; snprintf(q.l[0], sizeof q.l[0], "%.12s", lab);
    if (ic >= 0) icon(c, r.x + in - 1, y0 + (h0 - 16) / 2, ic, kind, tint, state == 0);
    put_label(c, lx, y0 + (h0 - 11) / 2, lw, &q, lc, 0);
  }
  if (bw) text(c, r.x + r.w - in - bw, r.y + 3, badge, state == 0 ? C_GREY : C_YEL, 0);
  if (nw && nw + bw + 6 + 2 * in <= r.w) text(c, r.x + in, r.y + 3, note, state == 0 ? C_GREY : C_ORG, 0);
  if (bind && bind[0]) draw_pill(c, r.x + 2, r.y + 2, bind);
}

static void draw_btn(struct cv *c, struct R r, const char *label, int style) {
  win(c, r.x, r.y, r.w, r.h, style);
  int lw = text_w(c, label, 0), x = r.x + (r.w - (lw < r.w - 4 ? lw : r.w - 4)) / 2;
  text_fit(c, x, r.y + (r.h - 11) / 2, r.w - 4, label, style == 2 ? C_GREY : C_TXT, 0);
}

static void tri(struct cv *c, int cx, int cy, int rad, int dx, int dy, uint32_t col) {   /* arrow triangle in vp */
  for (int j = -rad; j <= rad; j++)
    for (int i = -rad; i <= rad; i++) {
      int along = i * dx + j * dy, across = i * dy - j * dx;
      if (across < 0) across = -across;
      if (along >= -rad && along <= rad && across * 2 <= (rad - along)) frect(c, cx + i, cy + j, 1, 1, col);
    }
}

static void pad_button(struct cv *c, struct R r, int pad, int en) {
  win(c, r.x, r.y, r.w, r.h, en ? 0 : 2);
  int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
  uint32_t col = en ? C_TXT : C_GREY;
  int rad = r.h / 3; if (rad > 9) rad = 9;
  switch (pad) {
  case SOM_PAD_LEFT: tri(c, cx, cy, rad, -1, 0, col); break;
  case SOM_PAD_RIGHT: tri(c, cx, cy, rad, 1, 0, col); break;
  case SOM_PAD_UP: tri(c, cx, cy, rad, 0, -1, col); break;
  case SOM_PAD_DOWN: tri(c, cx, cy, rad, 0, 1, col); break;
  case SOM_PAD_B: text_c(c, r, cy - 5, "OK (B)", col, 0); break;
  case SOM_PAD_A: text_c(c, r, cy - 5, "BACK (A)", col, 0); break;
  case SOM_PAD_X: text_c(c, r, cy - 5, "HERO (X)", col, 0); break;
  case SOM_PAD_START: text_c(c, r, cy - 5, "START", col, 0); break;
  case SOM_PAD_SELECT: text_c(c, r, cy - 5, "SELECT", col, 0); break;
  default: break;
  }
}

/* ------------------------------------------------------------------ header */
static void draw_gear(struct cv *c, int cx, int cy, int R, uint32_t fill, uint32_t line) {
  for (int pass = 0; pass < 2; pass++)
    for (int j = -R - 1; j <= R + 1; j++)
      for (int i = -R - 1; i <= R + 1; i++) {
        double a = atan2((double)j, (double)i) + 6.283185307179586 * 2;
        int tooth = fmod(a, 0.7853981633974483) < 0.39269908169872414;     /* eight teeth, half duty */
        int ro = tooth ? R : R - 2, ri = R / 3, grow = pass == 0 ? 1 : 0, d2 = i * i + j * j;
        if (d2 <= (ro + grow) * (ro + grow) && d2 >= (ri - grow) * (ri - grow) && !(pass == 1 && d2 < ri * ri))
          frect(c, cx + i, cy + j, 1, 1, pass == 0 ? line : fill);
      }
}

static void draw_notice(struct cv *c, const struct som_ux *ux, int hh, int x0, int x1);
static void draw_header(struct cv *c, const struct som_ux *ux, const struct som_macro *m, struct R hr) {
  char b[40];
  int gw = hr.h >= 24 ? 60 : 48;
  vgrad(c, 0, 0, hr.w, hr.h, 0x1A2460u, 0x0A1040u);
  frect(c, 0, hr.h - 1, hr.w, 1, C_MID);
  static const char *const lab[SOM_UX_MODE_COUNT] = {"TITLE", "MENU", "FIELD", "EVENT", "WAIT", "RING", "SHOP", "FLAMMIE"};
  const char *l = lab[ux->mode];
  uint32_t col = C_GRN;
  if (ux->fe_name_entry) l = "NAME";
  if (ux->fe_name_entry) col = C_CYAN;
  else if (ux->mode == SOM_UX_FIELD) { if (!ux->st.safe_to_inject) { l = "LOCKED"; col = C_RED; } else l = "READY"; }
  else if (ux->mode == SOM_UX_EVENT || ux->mode == SOM_UX_TRANSITION) col = C_RED;
  else if (ux->mode == SOM_UX_RING || ux->mode == SOM_UX_SHOP) col = C_YEL;
  else col = C_CYAN;
  int editing = 0;                                                  /* the header names what is being edited */
  if (ux->opt_page) { l = ux->opt_page == SOM_UX_OPT_QUICK ? "SLOT" : ux->opt_page == SOM_UX_OPT_STATES ? "SAVE STATES" : ux->opt_page == SOM_UX_OPT_CREDITS ? "CREDITS" : "OPTIONS"; col = C_ORG; editing = 1; }
  if (ux->rev_wait) { l = "REVIVE"; col = C_ORG; editing = 1; }
  else if (ux->pol_wait) { l = "TARGET"; col = C_ORG; editing = 1; }
  else if (ux->mode == SOM_UX_FIELD && ux->pend_kind && ux->pend_class != SOM_UX_TC_NONE) { l = "TARGET"; col = C_ORG; editing = 1; }
  if (ux->opt_page == SOM_UX_OPT_QUICK && !ux->rev_wait && !ux->pol_wait && ux->qe_slot < SOM_UX_SHOULDERS) { snprintf(b, sizeof b, "SLOT %s", som_ux_shoulder_label(ux, ux->qe_slot)); l = b; }
  else if (ux->opt_page == SOM_UX_OPT_QUICK && !ux->rev_wait && !ux->pol_wait) { snprintf(b, sizeof b, "SLOT %d", ux->qe_slot - SOM_UX_FIRST_FREE_SLOT + 1); l = b; }
  /* one size for every header text: the status chip is a 15 vp pill with the same 11 vp text as everything else */
  int chh = 15, cy = (hr.h - 1 - chh) / 2, ty = cy + (chh - 11) / 2;
  /* round 3: no READY / LOCKED / EVENT / WAIT / TITLE / MENU / NAME chip; the chip stays only for what the player is editing and for ring, shop and flight */
  int chip = editing || ux->mode == SOM_UX_RING || ux->mode == SOM_UX_SHOP || ux->mode == SOM_UX_FLAMMIE;
  int w = chip ? text_w(c, l, 0) + 10 : 0;
  if (chip) { frect(c, 2, cy, w, chh, col); text(c, 7, ty, l, C_TXT, 0); }
  int x = chip ? 2 + w + 6 : 6, right = hr.w - gw - 6;
  if (m && m->result != SOM_RUNNING && m->frames) { text(c, x, ty, som_result_name(m->result), m->result == SOM_DONE ? C_GRN : C_RED, 0); x += text_w(c, som_result_name(m->result), 0) + 8; }
  else if (m && som_macro_busy(m)) { text(c, x, ty, "RUNNING", C_YEL, 0); x += text_w(c, "RUNNING", 0) + 8; }
  if (ux->notice_t && ux->notice_kind) { draw_notice(c, ux, hr.h, x, right); return; }
  if (ux->st.valid && ux->mode >= SOM_UX_FIELD && !editing && !ux->fe_name_entry) {
    snprintf(b, sizeof b, "%lu G", (unsigned long)ux->st.gold);
    int gwid = text_w(c, b, 0);
    text(c, right - gwid, ty, b, C_YEL, 0);
    const char *an = ux->mode == SOM_UX_FLAMMIE ? "WORLD MAP" : som_ux_area_name(ux->st.map_id);
    if (an && x + text_w(c, an, 0) + 8 < right - gwid) text(c, x, ty, an, C_TXT, 0);
  }
}

/* ------------------------------------------------------------------ radar and enemy list */
static void draw_radar(struct cv *c, const struct som_ux *ux, struct R r) {
  if (r.w <= 0) return;
  win(c, r.x, r.y, r.w, r.h, 0);
  int ix = r.x + 3, iy = r.y + 3, iw = r.w - 6, ih = r.h - 6;
  frect(c, ix, iy, iw, ih, 0x0A1230u);
  for (int i = 1; i < 4; i++) { brect(c, ix + iw * i / 4, iy, 1, ih, C_MID, 40); brect(c, ix, iy + ih * i / 4, iw, 1, C_MID, 40); }
  for (int i = 0; i < ux->n_enemies; i++) {
    const struct som_ux_actor *a = &ux->actor[ux->enemy_idx[i]];
    if (!a->on_screen) continue;
    int x = ix + a->sx * iw / 256, y = iy + a->sy * ih / 224;
    if (a->boss) { frect(c, x - 3, y - 3, 7, 7, C_DARK); frect(c, x - 2, y - 2, 5, 5, C_PURPLE); }
    else { frect(c, x - 2, y - 2, 5, 5, C_DARK); frect(c, x - 1, y - 1, 3, 3, C_RED); }
  }
  for (int s = 0; s < SOM_PARTY; s++) {
    const struct som_ux_actor *a = &ux->actor[s];
    if (!a->present || !a->on_screen) continue;
    int x = ix + a->sx * iw / 256, y = iy + a->sy * ih / 224;
    frect(c, x - 3, y - 3, 7, 7, s == ux->hero ? C_TXT : C_DARK);
    frect(c, x - 2, y - 2, 5, 5, a->alive ? HERO_COL[s] : C_GREY);
  }
}

/* The zone map: the area drawn from the game's own map data, centred on the active hero when zoomed in, with the party and the enemies on top. */
static void dev_disc(struct cv *c, int cx, int cy, int rad, uint32_t col, uint32_t edge) {
  for (int j = -rad - 1; j <= rad + 1; j++)
    for (int i = -rad - 1; i <= rad + 1; i++) {
      int d2 = i * i + j * j;
      if (d2 <= (rad + 1) * (rad + 1)) dpx(c, cx + i, cy + j, d2 <= rad * rad ? col : edge);
    }
}
static void draw_zone(struct cv *c, const struct som_ux *ux, struct R r) {
  if (r.w <= 0) return;
  win(c, r.x, r.y, r.w, r.h, 0);
  int ix = r.x + 3, iy = r.y + 3, iw = r.w - 6, ih = r.h - 6;
  if (!ux->zone_ok || !ux->zone || ux->zone_w < 1) { draw_radar(c, ux, r); return; }
  int s = c->s, dw = iw * s, dh = ih * s, zw = ux->zone_w, zh = ux->zone_h, ppt = ux->zone_ppt > 0 ? ux->zone_ppt : 8;
  float kfit = (float)dw / zw < (float)dh / zh ? (float)dw / zw : (float)dh / zh;
  float k = kfit, kclose = (float)dw / (14.0f * ppt);
  int large = !ux->map_small;
  if (!large && kclose > kfit) k = kclose;
  /* centre of the view: the active hero in image pixels */
  int hs = ux->st.pad_hero < SOM_PARTY && ux->actor[ux->st.pad_hero].present ? ux->st.pad_hero : (ux->hero < SOM_PARTY ? ux->hero : 0);   /* the hero the pad controls, not the card selected for the tabs */
  float cxp = zw / 2.0f, cyp = zh / 2.0f;
  if (ux->actor[hs].present) {
    cxp = ((float)ux->cam_x + (int16_t)ux->actor[hs].sx) * ppt / 16.0f; cyp = ((float)ux->cam_y + (int16_t)ux->actor[hs].sy) * ppt / 16.0f;
  }
  float vw = dw / k, vh = dh / k, ox = cxp - vw / 2, oy = cyp - vh / 2;
  if (vw >= zw) ox = (zw - vw) / 2; else { if (ox < 0) ox = 0; if (ox + vw > zw) ox = zw - vw; }
  if (vh >= zh) oy = (zh - vh) / 2; else { if (oy < 0) oy = 0; if (oy + vh > zh) oy = zh - vh; }
  frect(c, ix, iy, iw, ih, 0x050818u);
  for (int Y = 0; Y < dh; Y++) {
    int sy = (int)floorf(oy + (Y + 0.5f) / k);
    if (sy < 0 || sy >= zh) continue;
    for (int X = 0; X < dw; X++) {
      int sx = (int)floorf(ox + (X + 0.5f) / k);
      if (sx < 0 || sx >= zw) continue;
      dpx(c, ix * s + X, iy * s + Y, ux->zone[sy * zw + sx]);
    }
  }
  #define ZX(wx) (ix * s + (int)((((float)(wx)) * ppt / 16.0f - ox) * k))
  #define ZY(wy) (iy * s + (int)((((float)(wy)) * ppt / 16.0f - oy) * k))
  /* the game view */
  { int x0 = ZX(ux->cam_x), y0 = ZY(ux->cam_y), x1 = ZX(ux->cam_x + 256), y1 = ZY(ux->cam_y + 224);
    for (int X = x0; X <= x1; X++) { if (X >= ix * s && X < (ix + iw) * s) { if (y0 >= iy * s && y0 < (iy + ih) * s) dpx(c, X, y0, 0xD8E0FFu); if (y1 >= iy * s && y1 < (iy + ih) * s) dpx(c, X, y1, 0xD8E0FFu); } }
    for (int Y = y0; Y <= y1; Y++) { if (Y >= iy * s && Y < (iy + ih) * s) { if (x0 >= ix * s && x0 < (ix + iw) * s) dpx(c, x0, Y, 0xD8E0FFu); if (x1 >= ix * s && x1 < (ix + iw) * s) dpx(c, x1, Y, 0xD8E0FFu); } } }
  int rad = (large ? 4 : 2) * s;
  for (int i = 0; i < ux->n_enemies; i++) {
    const struct som_ux_actor *a = &ux->actor[ux->enemy_idx[i]];
    float wx = (float)ux->cam_x + (int16_t)a->sx, wy = (float)ux->cam_y + (int16_t)a->sy;
    if (wx < 0 || wy < 0 || wx * ppt / 16 >= zw || wy * ppt / 16 >= zh) continue;
    int X = ZX(wx), Y = ZY(wy);
    if (X < ix * s || X >= (ix + iw) * s || Y < iy * s || Y >= (iy + ih) * s) continue;
    dev_disc(c, X, Y, a->boss ? rad + s : rad, a->boss ? C_PURPLE : C_RED, C_DARK);
  }
  for (int pass = 0; pass < 2; pass++)
  for (int sl = 0; sl < SOM_PARTY; sl++) {
    const struct som_ux_actor *a = &ux->actor[sl];
    if (!a->present || (sl == hs) != pass) continue;                 /* the active hero is drawn last, on top */
    float wx = (float)ux->cam_x + (int16_t)a->sx, wy = (float)ux->cam_y + (int16_t)a->sy;
    int X = ZX(wx), Y = ZY(wy);
    if (X < ix * s || X >= (ix + iw) * s || Y < iy * s || Y >= (iy + ih) * s) continue;
    if (sl == hs && large) dev_disc(c, X, Y, rad + 3 * s, 0x7080C0u, C_TXT);      /* halo around the active hero */
    dev_disc(c, X, Y, sl == hs ? rad + s : rad + s / 2, a->alive ? HERO_COL[sl] : C_GREY, sl == hs ? C_TXT : C_DARK);
  }
  #undef ZX
  #undef ZY
  if (large) {                                                       /* area name over the map */
    const char *an = ux->mode == SOM_UX_FLAMMIE ? "WORLD MAP" : som_ux_area_name(ux->st.map_id);
    if (an) {
      int tw = text_w(c, an, 0) + 10;
      if (tw > r.w - 10) tw = r.w - 10;
      win(c, r.x + 5, r.y + 5, tw, 17, 0);
      text_fit(c, r.x + 10, r.y + 8, tw - 10, an, C_TXT, 0);
    }
  }
}

/* Enemy HP is shown per the ENEMY HP option: FAIR (default) only for enemy kinds a hero cast Analyzer on, ALWAYS, or HIDDEN. */
int som_ux_enemy_hp_shown(const struct som_ux *ux, int slot) {
  if (slot < 0 || slot >= 16) return 0;
  int o = ux->opt_val[SOM_UX_OPT_ENEMY_HP], id = ux->actor[slot].id;
  return o == 1 || (o == 0 && (ux->analyzed[id >> 3] >> (id & 7) & 1));
}

static void draw_boss_bar(struct cv *c, const struct som_ux *ux, struct R r) {
  if (ux->boss < 0) return;
  const struct som_ux_actor *a = &ux->actor[ux->boss];
  char b[40];
  const char *n = som_ux_name(ux, NAME_ENEMY + a->id);
  frect(c, r.x, r.y, r.w, r.h, C_DARK);
  snprintf(b, sizeof b, "%s", n ? n : "BOSS");
  text(c, r.x + 3, r.y + 1, b, C_PURPLE, 0);
  int nw = text_w(c, b, 0) + 8;
  if (!som_ux_enemy_hp_shown(ux, ux->boss)) { text(c, r.x + r.w - text_w(c, "HP ???", 0) - 3, r.y + 1, "HP ???", C_DIM, 0); return; }   /* until Analyzer */
  snprintf(b, sizeof b, "%u/%u", a->hp, a->hp_max);
  int bw = text_w(c, b, 0);
  gauge(c, r.x + nw, r.y + 3, r.w - nw - bw - 8, 7, a->hp, a->hp_max, hp_col(a->hp, a->hp_max));
  text(c, r.x + r.w - bw - 3, r.y + 1, b, C_TXT, 0);
}

static void draw_enemy_pic(struct cv *c, const struct som_ux *ux, int enemy_id, int x, int y, int side);
static void fit_center(struct cv *c, struct R r, int y, int maxw, const char *t, uint32_t col);
/* The enemy list of the MAP tab: at most three big cards (the game never shows more) that share the whole area.  Tall areas stack the cards and
 * put the sprite on the left, wide areas put them side by side with the sprite on top. */
static void draw_enemy_list(struct cv *c, const struct som_ux *ux, struct R r) {
  if (r.w <= 0 || r.h < 12) return;
  if (!ux->n_enemies) { win(c, r.x, r.y, r.w, r.h, 0); text_c(c, r, r.y + r.h / 2 - 5, "NO ENEMIES", C_DIM, 0); return; }
  int n = ux->n_enemies > 3 ? 3 : ux->n_enemies;
  int wide = r.w > r.h;
  for (int i = 0; i < n; i++) {
    const struct som_ux_actor *a = &ux->actor[ux->enemy_idx[i]];
    const char *nm = som_ux_name(ux, NAME_ENEMY + a->id);
    struct R q;
    if (wide) { q.w = r.w / 3; q.x = r.x + i * q.w; q.y = r.y; q.h = r.h; q.w -= 2; }
    else { q.h = r.h / 3; q.y = r.y + i * q.h; q.x = r.x; q.w = r.w; q.h -= 2; }
    win(c, q.x, q.y, q.w, q.h, 0);
    uint32_t ec = a->boss ? C_PURPLE : C_TXT;
    if (q.w > q.h * 13 / 10) {                       /* sprite left, name and gauge right */
      int side = q.h - 6; if (side > q.w / 2) side = q.w / 2;
      draw_enemy_pic(c, ux, a->id, q.x + 3, q.y + (q.h - side) / 2, side);
      int tx = q.x + side + 6, tw = q.w - side - 9;
      text_fit(c, tx, q.y + q.h / 2 - 12, tw, nm ? nm : "ENEMY", ec, 0);
      if (som_ux_enemy_hp_shown(ux, a->slot)) gauge(c, tx, q.y + q.h / 2 + 3, tw, 7, a->hp, a->hp_max, hp_col(a->hp, a->hp_max));
    } else {                                         /* sprite on top; a long name takes two lines */
      char l1[40] = "", l2[40] = "";
      const char *full = nm ? nm : "ENEMY";
      snprintf(l1, sizeof l1, "%s", full);
      if (text_w(c, l1, 0) > q.w - 6) {
        const char *sp = NULL;
        for (const char *t = full; *t; t++) if (*t == ' ') { char tmp[40]; snprintf(tmp, sizeof tmp, "%.*s", (int)(t - full), full); if (text_w(c, tmp, 0) <= q.w - 6) sp = t; }
        if (sp) { snprintf(l1, sizeof l1, "%.*s", (int)(sp - full), full); snprintf(l2, sizeof l2, "%s", sp + 1); }
      }
      int lines = l2[0] ? 2 : 1;
      int side = q.h - 3 - 1 - 12 * lines - 1 - 7 - 3; if (side > q.w - 8) side = q.w - 8;
      int content = (side >= 16 ? side + 1 : 0) + 12 * lines + 7;
      int y = q.y + (q.h - content) / 2;
      if (side >= 16) { draw_enemy_pic(c, ux, a->id, q.x + (q.w - side) / 2, y, side); y += side + 1; }
      fit_center(c, q, y, q.w - 6, l1, ec); y += 12;
      if (l2[0]) { fit_center(c, q, y, q.w - 6, l2, ec); y += 12; }
      if (som_ux_enemy_hp_shown(ux, a->slot)) gauge(c, q.x + 4, y + 1, q.w - 8, 7, a->hp, a->hp_max, hp_col(a->hp, a->hp_max));
    }
  }
}

/* ------------------------------------------------------------------ world map */
static void ensure_scaled(struct som_ux *ux, int dw, int dh) {
  if (!ux->world_ok) return;
  int key = dw * 4096 + dh;
  if (ux->world_scaled && ux->world_scaled_n == key) return;
  free(ux->world_scaled);
  ux->world_scaled = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)dw * (size_t)dh);
  ux->world_scaled_n = key;
  if (!ux->world_scaled) return;
  for (int y = 0; y < dh; y++) {
    int y0 = y * SOM_UX_WORLD / dh, y1 = (y + 1) * SOM_UX_WORLD / dh; if (y1 <= y0) y1 = y0 + 1;
    for (int x = 0; x < dw; x++) {
      int x0 = x * SOM_UX_WORLD / dw, x1 = (x + 1) * SOM_UX_WORLD / dw; if (x1 <= x0) x1 = x0 + 1;
      unsigned r = 0, g = 0, b = 0, n = 0;
      for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++) {
          uint32_t p = ux->world[j * SOM_UX_WORLD + i];
          r += (p >> 16) & 255; g += (p >> 8) & 255; b += p & 255; n++;
        }
      ux->world_scaled[y * dw + x] = 0xFF000000u | ((r / n) << 16) | ((g / n) << 8) | (b / n);
    }
  }
}

static void draw_world(struct cv *c, struct som_ux *ux, struct R r) {
  win(c, r.x - 1, r.y - 1, r.w + 2, r.h + 2, 0);
  int dw = r.w * c->s, dh = r.h * c->s;
  if (ux->world_ok) {
    ensure_scaled(ux, dw, dh);
    if (ux->world_scaled)
      for (int y = 0; y < dh; y++)
        for (int x = 0; x < dw; x++) dpx(c, r.x * c->s + x, r.y * c->s + y, ux->world_scaled[y * dw + x]);
  } else {
    frect(c, r.x, r.y, r.w, r.h, 0x10306Cu);
    text_c(c, r, r.y + r.h / 2 - 4, "NO ROM FOR MAP", C_DIM, 0);
  }
  if (!ux->fl_world_ok) {
    brect(c, r.x, r.y, r.w, r.h, C_DARK, 170);
    text_c(c, r, r.y + r.h / 2 - 8, "OTHER WORLD", C_YEL, 0);
    text_c(c, r, r.y + r.h / 2 + 3, "NO MAP DATA", C_DIM, 0);
    return;
  }
  /* trail, then arrow, in device pixels */
  double sx = (double)dw / 4096.0, sy = (double)dh / 4096.0;
  for (int i = 0; i < ux->trail_n; i++) {
    int x = (int)(ux->trail[i][0] * sx) + r.x * c->s, y = (int)(ux->trail[i][1] * sy) + r.y * c->s;
    for (int b = -c->s / 2; b <= c->s / 2; b++) for (int a = -c->s / 2; a <= c->s / 2; a++) dpx(c, x + a, y + b, 0xFFFFFFu);
  }
  double th = ux->fl_heading * 6.283185307179586 / 1024.0;
  double dx = -sin(th), dy = -cos(th);
  double px = ux->fl_x * sx + r.x * c->s, py = ux->fl_y * sy + r.y * c->s, L = 6.0 * c->s;
  double ax = px + dx * L, ay = py + dy * L, bx = px - dx * L * 0.7 - dy * L * 0.6, by = py - dy * L * 0.7 + dx * L * 0.6;
  double cx = px - dx * L * 0.7 + dy * L * 0.6, cy = py - dy * L * 0.7 - dx * L * 0.6;
  int x0 = (int)(px - L * 1.4), x1 = (int)(px + L * 1.4), y0 = (int)(py - L * 1.4), y1 = (int)(py + L * 1.4);
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      double d1 = (x - bx) * (ay - by) - (ax - bx) * (y - by), d2 = (x - cx) * (by - cy) - (bx - cx) * (y - cy),
             d3 = (x - ax) * (cy - ay) - (cx - ax) * (y - ay);
      int neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
      if (!(neg && pos)) dpx(c, x, y, 0xF8E060u);
    }
}

static const char *compass8(int bearing) {
  static const char *const n[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return n[((bearing + 22) / 45) & 7];
}

/* ------------------------------------------------------------------ shoulder slots */
static const char *const SH_DEFAULT[SOM_UX_SHOULDERS] = {"L1", "R1", "L2", "R2"};
const char *som_ux_shoulder_label(const struct som_ux *ux, int which) {
  if (which < 0 || which >= SOM_UX_SHOULDERS) return "";
  return ux->shoulder_label[which][0] ? ux->shoulder_label[which] : SH_DEFAULT[which];
}
void som_ux_set_shoulder_labels(struct som_ux *ux, const char *l1, const char *r1, const char *l2, const char *r2) {
  const char *v[SOM_UX_SHOULDERS] = {l1, r1, l2, r2};
  for (int i = 0; i < SOM_UX_SHOULDERS; i++) snprintf(ux->shoulder_label[i], sizeof ux->shoulder_label[i], "%s", v[i] && v[i][0] ? v[i] : SH_DEFAULT[i]);
}

/* ---- slot feedback animation ---- */
void som_ux_slot_flash(struct som_ux *ux, int slot, int kind) {
  if (!ux || slot < 0 || slot >= SOM_UX_QUICK_N || kind <= SOM_UX_FLASH_NONE || kind > SOM_UX_FLASH_REFUSED) return;
  ux->flash_t[slot] = SOM_UX_FLASH_FRAMES; ux->flash_kind[slot] = (uint8_t)kind;
}
void som_ux_flash_entry(struct som_ux *ux, int kind, int entry_kind, int entry_id) {
  if (!ux || kind <= SOM_UX_FLASH_NONE) return;
  for (int i = 0; i < SOM_UX_QUICK_N; i++)
    if (ux->quick[i].kind == entry_kind && ux->quick[i].id == entry_id) som_ux_slot_flash(ux, i, kind);
  if (entry_kind == SOM_UX_BK_SPELL && entry_id >= 0 && entry_id < 42) { ux->sflash_t = SOM_UX_FLASH_FRAMES; ux->sflash_id = (uint8_t)entry_id; }
}

const char *som_ux_bind_name(const struct som_ux *ux, int kind, int id, char *buf, size_t n) {
  const char *nm = NULL;
  if (kind == SOM_UX_BK_WEAPON && id >= 0 && id < 8) {
    int tier = ux->st.weapon_slot[id] & 15;
    nm = som_ux_name(ux, NAME_WEAPON + id * 9 + (tier < 9 ? tier : 0));
    if (!nm) nm = WEAPON_FALLBACK[id];
  } else if (kind == SOM_UX_BK_ITEM && id >= 0 && id < 11) {
    nm = som_ux_name(ux, NAME_ITEM + id);
    if (!nm) nm = ITEM_FALLBACK[id];
  } else if (kind == SOM_UX_BK_SPELL && id >= 0 && id < 42) {
    nm = som_ux_name(ux, NAME_SPELL + id);
    if (!nm) { snprintf(buf, n, "SPELL %d", id); return buf; }
  } else if (kind == SOM_UX_BK_SPIRIT && id >= 0 && id < 8) nm = UX_SPIRIT_NAME[id];
  else if (kind == SOM_UX_BK_MENU && id >= 0 && id < 4) nm = UX_MENU_NAME[id];
  else if (kind == SOM_UX_BK_SUBMENU && id >= 0 && id < 7) nm = UX_SUBMENU_NAME[id];
  if (!nm) return NULL;
  snprintf(buf, n, "%s", nm);
  return buf;
}

static int bind_icon(const struct cv *c, int kind, int id, int *kindcls, uint32_t *tint) {
  *tint = 0xA0A8D0u; *kindcls = 0;
  if (kind == SOM_UX_BK_WEAPON && id < 8) return ICON_WEAPON[id];
  if (kind == SOM_UX_BK_ITEM && id < 11) { *kindcls = 1; *tint = 0xF080C0u; return ICON_ITEM[id]; }
  if (kind == SOM_UX_BK_SPELL && id < 42) { *kindcls = 2; *tint = SPIRIT_COL[(id / 6) & 7]; return (c->pk && pk_find(c->pk, 0x1000 + 0x100 + id)) ? 0x100 + id : ICON_SPIRIT[(id / 6) & 7]; }
  if (kind == SOM_UX_BK_SPIRIT && id < 8) { *kindcls = 2; *tint = SPIRIT_COL[id]; return ICON_SPIRIT[id]; }
  if (kind == SOM_UX_BK_SUBMENU && id >= 0 && id < 7) return ICON_SUBMENU[id];
  if (kind == SOM_UX_BK_MENU) {          /* the ring's own category art: glove/sword, candy, gnome, and a drawn gear */
    *kindcls = 0;
    switch (id) { case SOM_CAT_WEAPON: return ICON_WEAPON[1]; case SOM_CAT_ITEM: *kindcls = 1; return ICON_ITEM[0];
                  case SOM_CAT_MAGIC: *kindcls = 2; *tint = SPIRIT_COL[0]; return ICON_SPIRIT[0]; default: return -2; }
  }
  return -1;
}

/* A quick slot or list icon, including the two drawn kinds (gear for the config ring, letters for submenus). */
static void entry_icon(struct cv *c, int x, int y, int kind, int id, int grey) {
  int k; uint32_t tn;
  int ic = bind_icon(c, kind, id, &k, &tn);
  uint32_t fg = grey ? C_GREY : C_TXT;
  if (ic >= 0) icon(c, x, y, ic, k, tn, grey);
  else if (ic == -2) draw_gear(c, x + 8, y + 8, 7, grey ? C_GREY : C_LIGHT, C_DARK);
  else if (kind == SOM_UX_BK_SUBMENU && id < 7) {
    frect(c, x, y + 1, 16, 14, C_DARK); frect(c, x + 1, y + 2, 14, 12, grey ? 0x2A3050u : 0x3A4C9Cu);
    text(c, x + 8 - text_w(c, UX_SUBMENU_SHORT[id], 0) / 2, y + 2, UX_SUBMENU_SHORT[id], fg, 0);
  }
  if (kind == SOM_UX_BK_MENU) {                     /* yellow frame marks a whole-menu hotlink */
    uint32_t fc = grey ? C_GREY : C_YEL;
    frect(c, x - 1, y - 1, 18, 1, fc); frect(c, x - 1, y + 16, 18, 1, fc); frect(c, x - 1, y, 1, 16, fc); frect(c, x + 16, y, 1, 16, fc);
  }
}

/* Opaque bounding box of a pack icon (icon art has uneven transparent margins, so slots centre the drawn part, not the 16x16 cell). */
static int icon_bbox(const struct cv *c, int kind, int id, int *x0, int *y0, int *x1, int *y1) {
  int k; uint32_t tn;
  int ic = bind_icon(c, kind, id, &k, &tn);
  const struct pk_rect *r = (ic >= 0 && c->pk) ? pk_find(c->pk, 0x1000 + ic) : NULL;
  *x0 = 0; *y0 = 0; *x1 = 15; *y1 = 15;
  if (!r) return 0;
  int ax = 16, ay = 16, bx = -1, by = -1;
  for (int j = 0; j < 16; j++)
    for (int i = 0; i < 16; i++)
      if ((pk_pixel(c->pk, r->x + i, r->y + j) >> 24) >= 128) {
        if (i < ax) ax = i;
        if (i > bx) bx = i;
        if (j < ay) ay = j;
        if (j > by) by = j;
      }
  if (bx < 0) return 0;
  *x0 = ax; *y0 = ay; *x1 = bx; *y1 = by;
  return 1;
}

/* An icon centred in the free part of a button: `top`..`bottom` is the vertical band left by the key badge. */
static void entry_icon_in(struct cv *c, struct R r, int top, int bottom, int kind, int id, int grey) {
  int x0, y0, x1, y1;
  icon_bbox(c, kind, id, &x0, &y0, &x1, &y1);
  int bw = x1 - x0 + 1, bh = y1 - y0 + 1;
  int ix = r.x + (r.w - bw) / 2 - x0, iy = top + (bottom - top - bh) / 2 - y0;
  entry_icon(c, ix, iy, kind, id, grey);
}

static int act_hero(const struct som_ux *ux) {
  return ux->mode == SOM_UX_RING && ux->st.ring.owner < SOM_PARTY ? ux->st.ring.owner : ux->hero;
}

/* spell button state: the host's per-spell array when it gave one, else derived from the party (caster alive, MP, spirit owned) */
static int spell_caster(int id) { return (id % 6) >= 3 ? 1 : 2; }
int som_ux_spell_state(const struct som_ux *ux, int id) {
  if (id < 0 || id > 41) return SOM_UX_AV_DISABLED;
  if (ux->spell_avail[id] >= SOM_UX_AV_ENABLED && ux->spell_avail[id] <= SOM_UX_AV_QUEUED) return ux->spell_avail[id];
  const struct som_state *st = &ux->st;
  int caster = spell_caster(id), g = id / 6, spirit = (g == 6 && caster == 1) ? 7 : g, cost = ux_spell_cost(ux, id);
  const struct som_hero *h = &st->hero[caster];
  if (!h->present || !h->alive) return SOM_UX_AV_DISABLED;
  if (cost >= 0 && h->mp < cost) return SOM_UX_AV_DISABLED;
  if (som_spirit_ring_index(st, caster, spirit) < 0) return SOM_UX_AV_DISABLED;
  return SOM_UX_AV_ENABLED;
}
void som_ux_set_spell_avail(struct som_ux *ux, const uint8_t *a) {
  if (!ux) return;
  if (a) memcpy(ux->spell_avail, a, sizeof ux->spell_avail); else memset(ux->spell_avail, 0, sizeof ux->spell_avail);
}

/* can the entry be used right now (greyed when not) */
static int entry_available(const struct som_ux *ux, int kind, int id) {
  const struct som_state *st = &ux->st;
  int hero = act_hero(ux);
  switch (kind) {
  case SOM_UX_BK_WEAPON: return id < 8 && st->weapon_slot[id] != 0xFF;
  case SOM_UX_BK_ITEM: for (int i = 0; i < SOM_ITEM_RECORDS; i++) if (st->item[i].qty && st->item[i].id == id) return 1; return 0;
  case SOM_UX_BK_SPELL: {
    return id < 42 && som_ux_spell_state(ux, id) != SOM_UX_AV_DISABLED;
  }
  case SOM_UX_BK_SPIRIT: return hero > 0 ? som_spirit_ring_index(st, hero, id) >= 0 : (st->hero[1].present || st->hero[2].present);
  default: return 1;
  }
}

/* ---- ring selection panel, avatars, quick slots, options ---- */
static void text_cm(struct cv *c, struct R r, int y, const char *s, uint32_t col, int mul) {
  int w = text_wm(c, s, mul);
  text_m(c, r.x + (r.w - w) / 2 > r.x + 1 ? r.x + (r.w - w) / 2 : r.x + 2, y, s, col, 0, mul);
}

static void draw_selinfo(struct cv *c, const struct som_ux *ux, struct R r) {
  const struct som_ux_selinfo *s = &ux->sel;
  win(c, r.x, r.y, r.w, r.h, 0);
  int ow = s->owner < SOM_PARTY ? s->owner : 0, L = lgm(c);
  int ph = r.h >= 104 ? 36 : 28, pw = ph * 98 / 108;
  portrait(c, ow, r.x + 5, r.y + 5, ph, !ux->st.hero[ow].alive);
  int tx = r.x + 5 + pw + 6, tw = r.x + r.w - 6 - tx;
  if (s->valid && s->kind) { win(c, r.x + r.w - 29, r.y + 4, 24, 24, 0); entry_icon(c, r.x + r.w - 25, r.y + 8, s->kind, s->id, 0); tw -= 28; }   /* the highlighted entry's own icon */
  text_fit(c, tx, r.y + 5 + (ph - 11) / 2, tw, s->head, C_CYAN, 0);      /* the portrait names the owner */
  int y = r.y + 5 + ph + 5;
  frect(c, r.x + 6, y - 3, r.w - 12, 1, C_MID);
  if (!s->valid) { text_m(c, r.x + 6, y + 4, "...", C_DIM, 0, L); return; }
  int m = L;
  while (m > 1 && text_wm(c, s->title, m) > r.w - 12) m--;
  if (text_wm(c, s->title, m) > r.w - 12) audit("ring title cut", s->title);
  text_m(c, r.x + 6, y, s->title, C_YEL, 0, m);
  y += 11 * m + 4;
  int avail = r.y + r.h - y - 3, mm = 1, cols = 2, found = 0;
  for (int tm = L; tm >= 1 && !found; tm--)
    for (int tc = 1; tc <= 2 && !found; tc++) {
      int rows0 = (s->n + tc - 1) / tc, colw0 = (r.w - 12) / tc, mx = 0;
      for (int i = 0; i < s->n; i++) { int w = text_wm(c, s->line[i], tm); if (w > mx) mx = w; }
      if (rows0 * (11 * tm + 3) <= avail && mx <= colw0 - 2) { found = 1; mm = tm; cols = tc; }
    }
  int rows = (s->n + cols - 1) / cols, colw = (r.w - 12) / cols;
  for (int i = 0; i < s->n; i++) {
    int cx = r.x + 6 + (i / rows) * colw, cy = y + (i % rows) * (11 * mm + 3);
    uint32_t col = strncmp(s->line[i], "NOT ENOUGH", 10) == 0 ? C_RED : C_TXT;
    text_fit_m(c, cx, cy, colw - 2, s->line[i], col, mm);
  }
}

static void draw_avatar(struct cv *c, const struct som_ux *ux, struct R r, int hero) {
  const struct som_hero *h = &ux->st.hero[hero];
  char b[24];
  int dg = danger(h);
  win(c, r.x, r.y, r.w, r.h, dg == 3 ? 2 : 0);
  int ph = r.h - 8, pw = ph * 98 / 108;
  portrait(c, hero, r.x + 4, r.y + 4, ph, dg == 3);
  int tx = r.x + 4 + pw + 5, tw = r.x + r.w - 4 - tx;
  int y1 = r.y + 3, y2 = r.y + 3 + 13;                  /* two rows, 13 vp apart: name, then "HP n" and the bar */
  if (y2 + 11 > r.y + r.h - 2) audit("avatar rows overlap", hname(hero));
  { char lv[8]; snprintf(lv, sizeof lv, "L%d", h->level); text_fit(c, tx, y1, tw, lv, dg == 3 ? C_DIM : C_TXT, 0); }
  if (dg == 3) {
    int kw = text_w(c, "KO", 0) + 8;
    frect(c, tx, y2 - 1, kw, 13, C_LIGHT); text(c, tx + 4, y2, "KO", C_DARK, 0);
    return;
  }
  snprintf(b, sizeof b, "HP %u", h->hp);
  int w1 = text_w(c, b, 0);
  text_fit(c, tx, y2, tw, b, C_TXT, 0);
  int bx = tx + w1 + 4, bw = r.x + r.w - 4 - bx;
  if (bw < 24) { audit("avatar bar too short", b); bw = 24; }
  bar(c, bx, y2 + 2, bw, 7, h->hp, h->hp_max, hp_col(h->hp, h->hp_max), dg == 1 ? 1 : (dg == 2 ? 2 : 0));
}

static void draw_plus(struct cv *c, int cx, int cy, uint32_t col) {
  frect(c, cx - 5, cy - 1, 11, 3, C_DARK); frect(c, cx - 1, cy - 5, 3, 11, C_DARK);
  frect(c, cx - 4, cy, 9, 1, col); frect(c, cx, cy - 4, 1, 9, col);
}

/* ~0.3 s feedback on a slot or the spell button of the spell list: a white flash that fades, a ring growing from the centre to the edges and a
 * glyph (check, cast star, star with a clock, cross).  t = frames left (SOM_UX_FLASH_FRAMES .. 1). */
static void draw_flash(struct cv *c, struct R r, int t, int kind) {
  int p = SOM_UX_FLASH_FRAMES - t, n = SOM_UX_FLASH_FRAMES;
  int a = 200 * t / n; if (a > 200) a = 200;
  uint32_t col = kind == SOM_UX_FLASH_REFUSED ? C_RED : (kind == SOM_UX_FLASH_OK ? C_GRN : (kind == SOM_UX_FLASH_QUEUED ? C_YEL : C_CYAN));
  brect(c, r.x + 1, r.y + 1, r.w - 2, r.h - 2, mix(col, C_LIGHT, 170), a);
  int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
  int rw = (r.w - 2) * (p + 2) / (n - 2) / 1, rh = (r.h - 2) * (p + 2) / (n - 2);
  if (rw > r.w - 2) rw = r.w - 2;
  if (rh > r.h - 2) rh = r.h - 2;
  if (p < n - 3) {
    int x0 = cx - rw / 2, y0 = cy - rh / 2;
    frect(c, x0, y0, rw, 1, col); frect(c, x0, y0 + rh - 1, rw, 1, col); frect(c, x0, y0, 1, rh, col); frect(c, x0 + rw - 1, y0, 1, rh, col);
  }
  if (p >= 2 && t > 3) {                                   /* glyph in the middle, about 9 vp */
    switch (kind) {
    case SOM_UX_FLASH_OK:
      for (int k = 0; k < 3; k++) { frect(c, cx - 5 + k, cy + k - 1, 2, 3, C_DARK); frect(c, cx - 5 + k, cy + k - 1, 2, 2, C_TXT); }
      for (int k = 0; k < 6; k++) { frect(c, cx - 2 + k, cy + 1 - k, 2, 3, C_DARK); frect(c, cx - 2 + k, cy + 1 - k, 2, 2, C_TXT); }
      break;
    case SOM_UX_FLASH_REFUSED:
      for (int k = -4; k <= 4; k++) { frect(c, cx + k - 1, cy + k - 1, 3, 3, C_DARK); frect(c, cx + k - 1, cy - k - 1, 3, 3, C_DARK); }
      for (int k = -4; k <= 4; k++) { frect(c, cx + k, cy + k, 1, 1, C_TXT); frect(c, cx + k, cy - k, 1, 1, C_TXT); }
      break;
    default:                                               /* cast star (a clock beside it when the cast was queued) */
      frect(c, cx - 5, cy - 1, 11, 3, C_DARK); frect(c, cx - 1, cy - 5, 3, 11, C_DARK);
      frect(c, cx - 4, cy, 9, 1, C_TXT); frect(c, cx, cy - 4, 1, 9, C_TXT);
      for (int k = 1; k <= 3; k++) { frect(c, cx + k, cy + k, 1, 1, C_YEL); frect(c, cx - k, cy - k, 1, 1, C_YEL); frect(c, cx + k, cy - k, 1, 1, C_YEL); frect(c, cx - k, cy + k, 1, 1, C_YEL); }
      if (kind == SOM_UX_FLASH_QUEUED) draw_clock(c, r.x + r.w - 7, r.y + 7, 5);
      break;
    }
  }
}

static void draw_slot(struct cv *c, const struct som_ux *ux, struct R r, int i, int hl) {
  const struct som_ux_quick *q = &ux->quick[i];
  int sh = i < SOM_UX_SHOULDERS;                                       /* shoulder slot: bound to a physical button by position */
  const char *lab = sh ? som_ux_shoulder_label(ux, i) : NULL;
  int av = q->kind ? entry_available(ux, q->kind, q->id) : 1;
  int qd = q->kind == SOM_UX_BK_SPELL && som_ux_spell_state(ux, q->id) == SOM_UX_AV_QUEUED;
  win(c, r.x, r.y, r.w, r.h, hl || qd ? 1 : (q->kind && !av ? 2 : 0));
  if (sh) {                                                            /* orange rim: a different kind of slot from the free ones */
    uint32_t rim = hl ? C_YEL : C_ORG;
    frect(c, r.x + 1, r.y + 1, r.w - 2, 1, rim); frect(c, r.x + 1, r.y + r.h - 2, r.w - 2, 1, rim);
    frect(c, r.x + 1, r.y + 1, 1, r.h - 2, rim); frect(c, r.x + r.w - 2, r.y + 1, 1, r.h - 2, rim);
  }
  int bottom = lab ? r.y + r.h - 14 : r.y + r.h - 2;
  if (!q->kind) draw_plus(c, r.x + r.w / 2, (r.y + 2 + bottom) / 2, hl ? C_YEL : C_DIM);
  else entry_icon_in(c, r, r.y + 2, bottom, q->kind, q->id, !av);
  if (lab) {                                                           /* the physical button, printed permanently */
    int pw = text_w(c, lab, 0) + 5, sv = c->sq;
    for (int k = 1; k <= 2 && pw > r.w - 2; k++) { c->sq = k; pw = text_w(c, lab, 0) + 5; }
    if (pw > r.w - 2) audit("slot badge wider than the slot", lab);
    draw_pill(c, r.x + (r.w - pw) / 2, r.y + r.h - 13, lab);
    c->sq = sv;
  }
  if (q->kind && qd) draw_clock(c, r.x + 8, r.y + 8, 5);
  if (q->kind && q->policy && som_ux_entry_policies(ux, q->kind, q->id)) {       /* target policy tag: N near, A all, L less HP */
    char tg[2] = {q->policy == SOM_UX_POL_NEAR ? 'N' : (q->policy == SOM_UX_POL_ALL ? 'A' : 'L'), 0};
    text(c, r.x + r.w - text_w(c, tg, 0) - 3, r.y + 3, tg, C_YEL, 0);
  }
  if (ux->flash_t[i]) draw_flash(c, r, ux->flash_t[i], ux->flash_kind[i]);
}

/* ---- PC overlay: the four shoulder slots alone, on a transparent canvas, at an explicit integer scale ---- */
#define SH_CELL_W 50
#define SH_CELL_H 34
#define SH_NAME_H 11
#define SH_GAP 2
#define SH_NOTICE_H 24   /* room above the slots for the queue / refusal chip */
void som_ux_shoulders_size(int scale, int names, int *w, int *h) {
  if (w) *w = (SOM_UX_SHOULDERS * SH_CELL_W + (SOM_UX_SHOULDERS - 1) * SH_GAP) * scale;
  if (h) *h = (SH_NOTICE_H + SH_CELL_H + (names ? SH_NAME_H + 1 : 0)) * scale;
}
void som_ux_draw_shoulders(struct som_ux *ux, uint32_t *px, int stride, int w, int h, int scale, int names) {
  struct cv c;
  if (!ux || !px || scale < 1) return;
  c.px = px; c.stride = stride; c.W = w; c.H = h; c.s = scale; c.pk = ux->pack; c.sq = 0; c.hr = 0; c.hr = hr_enabled(&c);
  g_sn = 0; g_hn_ux = ux;
  for (int y = 0; y < h; y++) memset(px + (size_t)y * stride, 0, (size_t)w * sizeof *px);   /* alpha 0: the host blends it over the game */
  for (int i = 0; i < SOM_UX_SHOULDERS; i++) {
    struct R r = {i * (SH_CELL_W + SH_GAP), SH_NOTICE_H, SH_CELL_W, SH_CELL_H};
    draw_slot(&c, ux, r, i, 0);
    if (names) {
      const struct som_ux_quick *q = &ux->quick[i];
      char nm[40];
      int ok = q->kind && som_ux_bind_name(ux, q->kind, q->id, nm, sizeof nm);
      int av = q->kind ? entry_available(ux, q->kind, q->id) : 1;
      frect(&c, r.x + 1, r.y + SH_CELL_H + 1, r.w - 2, SH_NAME_H, C_DARK);
      const char *label = ok ? nm : "EMPTY";
      for (int q2 = 0; q2 <= 2; q2++) { c.sq = q2; if (text_w(&c, label, 0) <= r.w - 4) break; }   /* normal, tight, then condensed face */
      int lw = text_w(&c, label, 0);
      text_fit(&c, r.x + 2 + (r.w - 4 - (lw < r.w - 4 ? lw : r.w - 4)) / 2, r.y + SH_CELL_H + 1, r.w - 4, label, ok ? (av ? C_TXT : C_GREY) : C_DIM, 0);
      c.sq = 0;
    }
  }
  draw_notice(&c, ux, SH_NOTICE_H, 0, (SOM_UX_SHOULDERS * SH_CELL_W + (SOM_UX_SHOULDERS - 1) * SH_GAP) - 8);
}

/* Every tap target of the current screen in device pixels, for gamepad navigation of the host. */
int som_ux_hit_list(struct som_ux *ux, int w, int h, int max, struct som_ux_hitrect *out) {
  struct LAY l;
  int s = som_ux_scale(w, h), n = 0;
  build_layout(&l, ux, w / s, h / s);
  for (int i = 0; i < l.n && n < max; i++) {
    out[n].x = l.h[i].r.x * s; out[n].y = l.h[i].r.y * s; out[n].w = l.h[i].r.w * s; out[n].h = l.h[i].r.h * s;
    out[n].kind = l.h[i].kind; out[n].a = l.h[i].a; out[n].enabled = l.h[i].enabled;
    n++;
  }
  return n;
}
void som_ux_set_four_slots(struct som_ux *ux, int on) { if (ux) ux->qe_four = on ? 1 : 0; }

static const char *const OPT_LABEL[SOM_UX_OPT_COUNT] = {"WIDE", "FALLBACK", "HUD HIDE", "CORE", "UI SOUNDS", "HAPTICS", "LANGUAGE", "TOP FIT", "SHADER", "FAST FWD", "ENEMY HP"};
static int opt_is_toggle(int id) { return id != SOM_UX_OPT_CORE && id != SOM_UX_OPT_LANGUAGE && id != SOM_UX_OPT_FIT && id != SOM_UX_OPT_WIDE && id != SOM_UX_OPT_SHADER && id != SOM_UX_OPT_FFWD && id != SOM_UX_OPT_ENEMY_HP;   /* WIDE: OFF / 17 / 27 / 46 */ }

static void draw_option(struct cv *c, const struct som_ux *ux, struct R r, int id) {
  win(c, r.x, r.y, r.w, r.h, 0);
  int M = lgm(c);
  while (M > 1 && 4 + 13 + 11 * M + 4 > r.h) M--;           /* short cards (navigation bar shown): the value drops to normal size */
  text_fit(c, r.x + 6, r.y + 4, r.w - 12, OPT_LABEL[id], C_DIM, 0);
  int vy = r.y + 4 + 13 + (r.h - 4 - 13 - 11 * M - 3 > 0 ? (r.h - 4 - 13 - 11 * M - 3) / 2 : 0);
  if (opt_is_toggle(id)) {
    int on = ux->opt_val[id] != 0;
    text_m(c, r.x + 6, vy, on ? "ON" : "OFF", on ? C_GRN : C_DIM, 0, M);
    int bx = r.x + r.w - 6 - 14, by = vy + 11 * M / 2 - 7;       /* checkbox: tick when on, empty when off (shape, not colour) */
    frect(c, bx, by, 14, 14, C_LIGHT); frect(c, bx + 1, by + 1, 12, 12, C_DARK);
    if (on) { for (int k = 0; k < 3; k++) frect(c, bx + 3 + k, by + 7 + k, 2, 2, C_GRN); for (int k = 0; k < 5; k++) frect(c, bx + 5 + k, by + 8 - k, 2, 2, C_GRN); }
  } else {
    const char *t = ux->opt_text[id][0] ? ux->opt_text[id] : "-";
    int Mv = M;
    while (Mv > 1 && text_wm(c, t, Mv) > r.w - 24) Mv--;
    text_fit_m(c, r.x + 6, vy + 11 * (M - Mv) / 2, r.w - 12 - 12, t, C_TXT, Mv);
    tri(c, r.x + r.w - 10, vy + 11 * M / 2, 5, 1, 0, C_YEL);
  }
}

/* ------------------------------------------------------------------ mode screens */
/* text centred in a button, on the first face that fits the width (normal, tight, condensed); counts as cut text when none fits */
static void fit_center(struct cv *c, struct R r, int y, int maxw, const char *t, uint32_t col) {
  int sv = c->sq;
  for (int q = 0; q <= 2; q++) { c->sq = q; if (text_w(c, t, 0) <= maxw) break; }
  int w = text_w(c, t, 0);
  if (w > maxw) audit("target card text", t);
  text_fit(c, r.x + (r.w - (w < maxw ? w : maxw)) / 2, y, maxw, t, col, 0);
  c->sq = sv;
}

/* alpha-blend one device pixel (straight alpha) */
static void dblend(struct cv *c, int x, int y, uint32_t argb) {
  if (x < 0 || y < 0 || x >= c->W || y >= c->H) return;
  unsigned a = argb >> 24;
  if (!a) return;
  uint32_t *d = &c->px[(size_t)y * c->stride + x];
  if (a >= 255) { *d = 0xFF000000u | (argb & 0xFFFFFFu); return; }
  unsigned r = (((argb >> 16) & 255) * a + ((*d >> 16) & 255) * (255 - a) + 127) / 255, g = (((argb >> 8) & 255) * a + ((*d >> 8) & 255) * (255 - a) + 127) / 255,
           b = ((argb & 255) * a + (*d & 255) * (255 - a) + 127) / 255;
  *d = 0xFF000000u | (r << 16) | (g << 8) | b;
}

/* generic enemy silhouette on the plain card: a dim blob with two eyes, used when the frontend has no sprite */
static void draw_silhouette(struct cv *c, int x, int y, int side) {
  int s = c->s, cx = (x * s) + side * s / 2, cy = y * s + side * s * 58 / 100, rx = side * s * 32 / 100, ry = side * s * 28 / 100;
  for (int j = -ry; j <= ry; j++)
    for (int i = -rx; i <= rx; i++)
      if ((long)i * i * ry * ry + (long)j * j * rx * rx <= (long)rx * rx * ry * ry) dblend(c, cx + i, cy + j, 0xFF3A4690u);
  int er = side * s / 14 + 1;
  for (int e = -1; e <= 1; e += 2)
    for (int j = -er; j <= er; j++) for (int i = -er; i <= er; i++) if (i * i + j * j <= er * er) dblend(c, cx + e * rx * 45 / 100 + i, cy - ry / 4 + j, 0xFFC8D0FFu);
}

/* The sprite of an enemy id: opaque part centred (on its bounding box) in the square (x, y, side) vp, integer scale, alpha blended, over a soft
 * card-coloured glow so thin or pale sprites (spider legs, eyes, slimes) stay visible, and a drop shadow under the feet.  Pixels that are only
 * partly opaque are strengthened and a sprite smaller than 45% of the box is enlarged to at least that (integer factor); one larger than the box
 * is reduced by area averaging, never by dropping rows.  The silhouette when the frontend has none. */
static uint8_t g_glow[640 * 640];
static void draw_enemy_pic(struct cv *c, const struct som_ux *ux, int enemy_id, int x, int y, int side) {
  int k = -1;
  for (int i = 0; i < SOM_UX_SPRITES; i++) if (ux->spr_ok[i] && ux->spr_id[i] == enemy_id) k = i;
  if (k < 0) { draw_silhouette(c, x, y, side); return; }
  const uint32_t *src = ux->sprite[k];
  int w = ux->spr_w[k], h = ux->spr_h[k], x0 = w, y0 = h, x1 = -1, y1 = -1;
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
      if ((src[j * SOM_UX_SPRITE_MAX + i] >> 24) >= 16) { if (i < x0) x0 = i; if (i > x1) x1 = i; if (j < y0) y0 = j; if (j > y1) y1 = j; }
  if (x1 < 0) { draw_silhouette(c, x, y, side); return; }
  int bw = x1 - x0 + 1, bh = y1 - y0 + 1, mx = bw > bh ? bw : bh, box = side * c->s, sh = box / 10, m = box - sh - 4 * c->s;
  int f = m / mx, dw, dh;
  if (f >= 1) { dw = bw * f; dh = bh * f; }
  else { dw = bw * m / mx; dh = bh * m / mx; f = 0; }
  if (dw < 1) dw = 1;
  if (dh < 1) dh = 1;
  int ox = x * c->s + (box - dw) / 2, oy = y * c->s + (box - sh - dh) / 2;
  int g = c->s * 2 + (dw > dh ? dw : dh) / 24;                                 /* glow radius in device pixels */
  /* glow: every opaque pixel paints a block of radius g (level 1) and g/2 (level 2) into a mask, blended once */
  int gw = dw + 2 * g, gh = dh + 2 * g;
  if (gw <= 640 && gh <= 640) {
    memset(g_glow, 0, (size_t)gw * gh);
    int cell = f ? f : 1;
    for (int j = 0; j < bh; j++)
      for (int i = 0; i < bw; i++) {
        if ((src[(y0 + j) * SOM_UX_SPRITE_MAX + x0 + i] >> 24) < 16) continue;
        int px0 = f ? i * f : i * dw / bw, py0 = f ? j * f : j * dh / bh, pw = f ? cell : (dw + bw - 1) / bw, ph = f ? cell : (dh + bh - 1) / bh;
        for (int lv = 1; lv <= 2; lv++) {
          int r = lv == 1 ? g : g / 2;
          for (int yy = py0 + g - r; yy < py0 + g + ph + r && yy < gh; yy++)
            for (int xx = px0 + g - r; xx < px0 + g + pw + r && xx < gw; xx++)
              if (yy >= 0 && xx >= 0 && g_glow[yy * gw + xx] < lv) g_glow[yy * gw + xx] = (uint8_t)lv;
        }
      }
    for (int j = 0; j < gh; j++)
      for (int i = 0; i < gw; i++) if (g_glow[j * gw + i]) dblend(c, ox - g + i, oy - g + j, (g_glow[j * gw + i] == 1 ? 0x30000000u : 0x28000000u) | 0xA8B8FFu);
  }
  int sy = oy + dh - dh / 12, srx = dw * 45 / 100 + 1, sry = c->s * 2 + dh / 14;   /* shadow ellipse under the feet */
  for (int j = -sry; j <= sry; j++)
    for (int i = -srx; i <= srx; i++)
      if ((long)i * i * sry * sry + (long)j * j * srx * srx <= (long)srx * srx * sry * sry) dblend(c, ox + dw / 2 + i, sy + j, 0x70000000u);
  for (int j = 0; j < dh; j++)
    for (int i = 0; i < dw; i++) {
      uint32_t p;
      if (f) p = src[(y0 + j / f) * SOM_UX_SPRITE_MAX + x0 + i / f];
      else {                                                                  /* area average, alpha weighted */
        int ax = i * bw / dw, bx = (i + 1) * bw / dw, ay = j * bh / dh, by = (j + 1) * bh / dh;
        if (bx <= ax) bx = ax + 1;
        if (by <= ay) by = ay + 1;
        unsigned long ar = 0, ag = 0, ab = 0, aa = 0, n = 0;
        for (int yy = ay; yy < by; yy++)
          for (int xx = ax; xx < bx; xx++) {
            uint32_t q = src[(y0 + yy) * SOM_UX_SPRITE_MAX + x0 + xx];
            unsigned qa = q >> 24;
            ar += ((q >> 16) & 255) * qa; ag += ((q >> 8) & 255) * qa; ab += (q & 255) * qa; aa += qa; n++;
          }
        p = aa ? (uint32_t)(aa / n) << 24 | (uint32_t)(ar / aa) << 16 | (uint32_t)(ag / aa) << 8 | (uint32_t)(ab / aa) : 0;
      }
      unsigned a = p >> 24;
      if (a && a < 255) { a = a >= 96 ? 255 : a * 2; p = (a << 24) | (p & 0xFFFFFFu); }   /* pale edge pixels become solid */
      dblend(c, ox + i, oy + j, p);
    }
}

/* ---- Thor round 4: save-state slots and credits ---- */
static void blit_thumb(struct cv *c, int x, int y, int w, int h, const uint32_t *src) {
  int s = c->s, dw = w * s, dh = h * s;
  for (int j = 0; j < dh; j++) {
    int sy = j * SOM_UX_THUMB_H / dh;
    for (int i = 0; i < dw; i++) dpx(c, x * s + i, y * s + j, src[sy * SOM_UX_THUMB_W + i * SOM_UX_THUMB_W / dw]);
  }
}

static void draw_state_slot(struct cv *c, const struct som_ux *ux, struct R r, int i) {
  char b[24];
  int sel = ux->st_sel == i, used = ux->st_slot[i].used, th = r.h - 8, tw = th * 16 / 9;
  win(c, r.x, r.y, r.w, r.h, sel ? 1 : 0);
  if (tw > r.w - 12 - 54) { tw = r.w - 12 - 54; th = tw * 9 / 16; }
  int tx = r.x + 6;
  if (th >= 14) {
    int ty = r.y + (r.h - th) / 2;
    frect(c, tx - 1, ty - 1, tw + 2, th + 2, C_DARK);
    if (used) blit_thumb(c, tx, ty, tw, th, ux->st_thumb[i]);
    else frect(c, tx, ty, tw, th, C_BG);
    tx += tw + 6;
  }
  snprintf(b, sizeof b, "SLOT %d", i + 1);
  int mw = r.x + r.w - tx - 4, three = r.h >= 36;
  text_fit(c, tx, r.y + (three ? 2 : 3), mw, b, sel ? C_YEL : C_TXT, 0);
  if (!used) text_fit(c, tx, r.y + (three ? 14 : 15), mw, "EMPTY", C_GREY, 0);
  else if (three) {                                      /* "10-20 18:30": date and time on two lines */
    char d[16], t[16];
    const char *sp = strchr(ux->st_slot[i].time, ' ');
    snprintf(d, sizeof d, "%.*s", sp ? (int)(sp - ux->st_slot[i].time) : (int)strlen(ux->st_slot[i].time), ux->st_slot[i].time);
    snprintf(t, sizeof t, "%s", sp ? sp + 1 : "");
    text_fit(c, tx, r.y + 14, mw, d, C_DIM, 0);
    text_fit(c, tx, r.y + 25, mw, t, C_DIM, 0);
  } else text_fit(c, tx, r.y + 15, mw, ux->st_slot[i].time, C_DIM, 0);
}

struct cr_line { int16_t off, len; uint8_t head; };
/* word wrap of the credits text to `w` vp; returns the number of lines (at most max) */
static int cr_wrap(struct cv *c, const char *t, int w, struct cr_line *out, int max) {
  int n = 0, i = 0;
  while (t[i] && n < max) {
    int e = i;
    while (t[e] && t[e] != '\n') e++;
    int head = t[i] == '#' && t[i + 1] == ' ';
    int a = i + (head ? 2 : 0);
    if (a >= e) { out[n].off = (int16_t)a; out[n].len = 0; out[n++].head = 0; }
    while (a < e && n < max) {
      int k = a, wd = 0, brk = -1;
      while (k < e) {
        char ch[2] = {t[k], 0};
        int aw = text_w(c, ch, 0);
        if (wd + aw > w) break;
        wd += aw; if (t[k] == ' ') brk = k; k++;
      }
      int take = k;
      if (k < e) { if (brk > a) take = brk; else if (k == a) take = a + 1; }
      out[n].off = (int16_t)a; out[n].len = (int16_t)(take - a); out[n++].head = (uint8_t)head;
      a = take;
      while (a < e && t[a] == ' ') a++;
    }
    i = t[e] ? e + 1 : e;
  }
  return n;
}

static void draw_credits(struct cv *c, struct som_ux *ux, struct R r) {
  static struct cr_line ln[400];
  win(c, r.x, r.y, r.w, r.h, 0);
  int n = cr_wrap(c, ux->cr_text, r.w - 20, ln, 400), vis = (r.h - 8) / 12;
  if (vis < 1) vis = 1;
  ux->cr_lines = (int16_t)n; ux->cr_vis = (int16_t)vis;
  if (ux->cr_scroll > n - vis) ux->cr_scroll = (int16_t)(n - vis > 0 ? n - vis : 0);
  if (ux->cr_scroll < 0) ux->cr_scroll = 0;
  for (int k = 0; k < vis && ux->cr_scroll + k < n; k++) {
    const struct cr_line *q = &ln[ux->cr_scroll + k];
    char b[96];
    int m = q->len < (int)sizeof b - 1 ? q->len : (int)sizeof b - 1;
    memcpy(b, ux->cr_text + q->off, (size_t)m); b[m] = 0;
    if (m) text(c, r.x + 8, r.y + 5 + k * 12, b, q->head ? C_YEL : C_TXT, 0);
  }
  if (n > vis) {   /* scroll bar */
    int bh = r.h - 12, th = bh * vis / n > 6 ? bh * vis / n : 6, ty = r.y + 6 + (bh - th) * ux->cr_scroll / (n - vis);
    frect(c, r.x + r.w - 8, r.y + 6, 3, bh, C_DARK);
    frect(c, r.x + r.w - 8, ty, 3, th, C_MID);
  }
}

static void draw_hit(struct cv *c, struct som_ux *ux, const struct LAY *l, const struct hit *h) {
  char b[48], b2[24];
  struct R r = h->r;
  if (h->kind == HK_CELL || h->kind == HK_QE_CELL) { r.w--; r.h--; }
  g_press = ux->press_on && ux->press_kind == h->kind && ux->press_a == h->a && h->enabled;
  if (g_press) { r.x++; r.y++; r.w--; r.h--; }
  int dim = ux->mode == SOM_UX_EVENT || ux->mode == SOM_UX_TRANSITION;
  switch (h->kind) {
  case HK_PARTY: card(c, ux, h->a, r, h->a == ux->hero && !dim && ux->mode == SOM_UX_FIELD, dim); break;
  case HK_TAB: {
    static const char *const n[4] = {"MAP", "WEAPON", "ITEM", "MAGIC"};
    int sel = ux->tab == h->a;
    win(c, r.x, r.y, r.w, r.h, sel ? 1 : 0);
    text_fit(c, r.x + (r.w - text_w(c, n[h->a], 0)) / 2 > r.x + 1 ? r.x + (r.w - text_w(c, n[h->a], 0)) / 2 : r.x + 2, r.y + (r.h - 11) / 2, r.w - 3, n[h->a], C_TXT, 0);
    break;
  }
  case HK_OPT: {
    win(c, r.x, r.y, r.w, r.h, ux->opt_page ? 1 : 0);
    int R = r.h >= 24 ? 9 : 6;
    draw_gear(c, r.x + 5 + R, r.y + r.h / 2, R, C_LIGHT, C_DARK);
    text_fit(c, r.x + 5 + 2 * R + 5, r.y + (r.h - 11) / 2, r.w - 2 * R - 12, "OPT", C_TXT, 0);
    break;
  }
  case HK_MAPVIEW: break;                           /* drawn by draw_zone */
  case HK_OPTION: draw_option(c, ux, r, h->a); break;
  case HK_OPT_NAV: {
    static const char *const n[7] = {"BACK", "", "QUICK SLOTS", "BACK", "SAVE STATES", "CREDITS", "MAIN MENU"},
                             *sn[7] = {"BACK", "", "SLOTS", "BACK", "STATES", "CREDITS", "MENU"};   /* narrow buttons */
    const char *t = h->a == 6 && ux->quit_armed ? "QUIT GAME?" : n[h->a];
    { int sv = c->sq; c->sq = 2; if (text_w(c, t, 0) > r.w - 8) t = h->a == 6 && ux->quit_armed ? "QUIT?" : sn[h->a]; c->sq = sv; }
    int arrow = (h->a == 2 || h->a == 4 || h->a == 5) && r.w >= 100;   /* the narrow buttons of the bottom row go without */
    win(c, r.x, r.y, r.w, r.h, h->a == 6 && ux->quit_armed ? 1 : 0);
    int M = (r.h >= 40 && lgm(c) == 2 && text_wm(c, t, 2) < r.w - 28) ? 2 : 1;
    struct R tr = r; if (arrow) tr.w -= 12;
    if (M == 2) text_cm(c, tr, r.y + (r.h - 22) / 2, t, C_TXT, 2);
    else fit_center(c, tr, r.y + (r.h - 11) / 2, tr.w - 8, t, h->a == 6 && ux->quit_armed ? C_YEL : C_TXT);
    if (arrow) tri(c, r.x + r.w - 10, r.y + r.h / 2, 5, 1, 0, C_YEL);
    break;
  }
  case HK_ST_SLOT: draw_state_slot(c, ux, r, h->a); break;
  case HK_ST_SAVE: draw_btn(c, r, "SAVE", h->enabled ? 0 : 2); break;
  case HK_ST_LOAD: draw_btn(c, r, "LOAD", h->enabled ? 0 : 2); break;
  case HK_CR_SCROLL:
    win(c, r.x, r.y, r.w, r.h, h->enabled ? 0 : 2);
    tri(c, r.x + r.w / 2, r.y + r.h / 2, r.h / 3 > 9 ? 9 : r.h / 3, 0, h->a ? 1 : -1, h->enabled ? C_TXT : C_GREY);
    break;
  case HK_AVATAR: draw_avatar(c, ux, r, h->a); break;
  case HK_QUICK: {
    int hl = ux->opt_page == SOM_UX_OPT_QUICK ? h->a == ux->qe_slot : ux->quick_arm != 0;
    draw_slot(c, ux, r, h->a, hl);
    break;
  }
  case HK_BIND_ENTRY: draw_btn(c, r, ux->quick_arm == 1 ? "PICK A SLOT" : "ENTRY > SLOT", ux->quick_arm == 1 ? 1 : (h->enabled ? 0 : 2)); break;
  case HK_BIND_MENU: draw_btn(c, r, ux->quick_arm == 2 ? "PICK A SLOT" : "SPIRITS > SLOT", ux->quick_arm == 2 ? 1 : (h->enabled ? 0 : 2)); break;
  case HK_QE_TAB: {
    static const char *const n[5] = {"MENU", "WEAPON", "ITEM", "SPIRIT", "SPELL"};
    win(c, r.x, r.y, r.w, r.h, ux->qe_cat == h->a ? 1 : 0);
    text_fit(c, r.x + (r.w - text_w(c, n[h->a], 0)) / 2 > r.x + 1 ? r.x + (r.w - text_w(c, n[h->a], 0)) / 2 : r.x + 2, r.y + (r.h - 11) / 2, r.w - 3, n[h->a], C_TXT, 0);
    break;
  }
  case HK_QE_CELL: {
    int kind, id;
    qe_entry(ux, ux->qe_cat, ux->qe_page * QE_PAGE + h->a, &kind, &id);
    char nm[32];
    const struct som_ux_quick *q = ux->qe_slot < SOM_UX_QUICK_N ? &ux->quick[ux->qe_slot] : NULL;
    int cur = q && q->kind == kind && q->id == id;
    int k; uint32_t tn;
    int ic = bind_icon(c, kind, id, &k, &tn);
    if (!som_ux_bind_name(ux, kind, id, nm, sizeof nm)) snprintf(nm, sizeof nm, "?");
    if (ic < 0) {                                   /* drawn kinds: gear or letter tile on a plain cell */
      draw_cell(c, r, -1, 0, 0, nm, "", cur ? 2 : 1, NULL, NULL);
      entry_icon(c, r.x + (r.w - 16) / 2, r.y + (r.h < 36 ? 2 : 4), kind, id, 0);
    } else {
      draw_cell(c, r, ic, k, tn, nm, "", cur ? 2 : 1, NULL, NULL);
    }
    break;
  }
  case HK_QE_PAGE: {
    win(c, r.x, r.y, r.w, r.h, h->enabled ? 0 : 2);
    tri(c, r.x + r.w / 2, r.y + r.h / 2, r.h / 3 > 9 ? 9 : r.h / 3, h->a ? 1 : -1, 0, h->enabled ? C_TXT : C_GREY);
    break;
  }
  case HK_QE_CLEAR: draw_btn(c, r, "CLEAR SLOT", h->enabled ? 0 : 2); break;
  case HK_QE_BACK: {
    win(c, r.x, r.y, r.w, r.h, 0);
    tri(c, r.x + 12, r.y + r.h / 2, r.h / 4 > 7 ? 7 : r.h / 4, -1, 0, C_YEL);
    text_fit(c, r.x + 26, r.y + (r.h - 11) / 2, r.w - 30, "SPIRITS", C_TXT, 0);
    break;
  }
  case HK_POLICY: {
    static const char *const nm[4] = {"SELECT", "NEAR", "ALL", "LESS HP"};
    int pol = h->a, en = class_is_enemy(entry_class(ux, ux->pol_kind, ux->pol_id));
    const char *sub = pol == SOM_UX_POL_SELECT ? "CHOOSE WHEN USED" : pol == SOM_UX_POL_NEAR ? "NEAREST ENEMY" :
                      pol == SOM_UX_POL_LESS_HP ? "WEAKEST ALLY" : (en ? "EVERY ENEMY" : "EVERY ALLY");
    win(c, r.x, r.y, r.w, r.h, pol == SOM_UX_POL_SELECT ? 1 : 0);
    int M = (r.h >= 40 && lgm(c) == 2 && text_wm(c, nm[pol], 2) < r.w - 12) ? 2 : 1;
    int th = 11 * M + 4 + 11, y = r.y + (r.h - th) / 2;
    text_cm(c, r, y, nm[pol], C_TXT, M);
    fit_center(c, r, y + 11 * M + 4, r.w - 8, sub, C_DIM);
    break;
  }
  case HK_POL_CANCEL: draw_btn(c, r, "CANCEL", 0); break;
  case HK_REV_CANCEL: draw_btn(c, r, "CANCEL", 0); break;
  case HK_REVIVE: {
    struct som_ux_revive_opt o[REV_MAX];
    int n = rev_options(ux, o);
    if (h->a >= n) break;
    const struct som_ux_revive_opt *q = &o[h->a];
    int kind = q->kind == 1 ? SOM_UX_BK_ITEM : SOM_UX_BK_SPELL, grey = !q->enabled && !q->queued;
    char nm[40], sub[32], rt[16];
    win(c, r.x, r.y, r.w, r.h, grey ? 2 : (q->queued ? 1 : 0));
    entry_icon(c, r.x + 8, r.y + (r.h - 16) / 2, kind, q->id, grey);
    if (q->queued) draw_clock(c, r.x + 21, r.y + (r.h - 16) / 2 + 14, 5);
    if (!som_ux_bind_name(ux, kind, q->id, nm, sizeof nm)) snprintf(nm, sizeof nm, "?");
    static const char *const why[5] = {"", "NONE LEFT", "NOT ENOUGH MP", "SPIRIT NOT OWNED", "CASTER DOWN"};
    if (q->queued) snprintf(sub, sizeof sub, "WAITING  TAP TO CANCEL");
    else if (q->reason) snprintf(sub, sizeof sub, "%s", why[q->reason]);
    else if (q->kind == 2) snprintf(sub, sizeof sub, "CAST BY %s", hname(q->caster < 3 ? q->caster : 0));
    else snprintf(sub, sizeof sub, "USE ON %s", hname(ux->rev_hero < 3 ? ux->rev_hero : 0));
    if (q->kind == 1) snprintf(rt, sizeof rt, "x%d", q->qty); else snprintf(rt, sizeof rt, "%d MP", q->mp_cost);
    int M = lgm(c), rw;
    while (M > 1 && text_wm(c, rt, M) > r.w / 4) M--;
    rw = text_wm(c, rt, M) + 10;
    int ty = r.y + (r.h - (11 + 2 + 11)) / 2;
    uint32_t col = grey ? C_GREY : C_TXT;
    text_fit(c, r.x + 32, ty, r.w - 32 - rw - 6, nm, col, 0);
    text_fit(c, r.x + 32, ty + 13, r.w - 32 - rw - 6, sub, grey ? C_GREY : (q->queued ? C_YEL : C_DIM), 0);
    if (q->kind == 1 || q->mp_cost) text_m(c, r.x + r.w - rw, r.y + (r.h - 11 * M) / 2, rt, grey ? C_GREY : C_YEL, 0, M);
    break;
  }
  case HK_CELL: {
    int i = h->a, st = h->enabled ? 1 : 0;
    const struct som_state *s = &ux->st;
    if (ux->tab == SOM_UX_TAB_WEAPON) {
      if (h->enabled && som_weapon_held_by(s, ux->hero, i)) st = 2;
      /* round 3: "level:progress" like 3:42 (docs/rom-combat.md 11.1: level 0..8, progress 0..99 to the next one) and the attack the swing deals */
      b[0] = 0; if (h->enabled) snprintf(b, sizeof b, "%d:%02d", s->hero[ux->hero].weapon_level[i], s->hero[ux->hero].weapon_prog[i] % 100);
      char d[24]; d[0] = 0;
      { int atk = weapon_attack(ux, ux->hero, i);
        if (h->enabled && atk >= 0) {
          snprintf(d, sizeof d, "ATK %d", atk);
          if (text_w(c, d, 0) + text_w(c, b, 0) + 14 > r.w) snprintf(d, sizeof d, "%d", atk);
        } }
      draw_cell(c, r, ICON_WEAPON[i], 0, 0xA0A8D0u, h->enabled ? weapon_label(ux, i, b2) : WEAPON_FALLBACK[i], b, st, NULL, r.h >= 40 ? d : NULL);
    } else if (ux->tab == SOM_UX_TAB_ITEM) {
      b[0] = 0;
      if (h->enabled) snprintf(b, sizeof b, "x%d", s->item[i].qty);
      int id = s->item[i].id;
      draw_cell(c, r, h->enabled && id < 11 ? ICON_ITEM[id] : -1, 1, 0xF080C0u, h->enabled ? item_label(ux, id, b2) : "", b, st, NULL, NULL);
    } else if (ux->spirit == 0xFF) {
      b[0] = 0; if (h->enabled) snprintf(b, sizeof b, "L%d", s->hero[ux->hero].spirit_level[i]);
      draw_cell(c, r, ICON_SPIRIT[i], 2, SPIRIT_COL[i], SPIRIT_NAME[i], b, st, NULL, NULL);
    } else if (i < 3) {
      int id = spell_id(ux, ux->hero, ux->spirit, i), cost = spell_cost(ux, id);
      const char *n = som_ux_name(ux, NAME_SPELL + id);
      { int ss = som_ux_spell_state(ux, id); if (ss == SOM_UX_AV_DISABLED) st = 0; else if (ss == SOM_UX_AV_QUEUED) st = 3; }
      b[0] = 0; if (cost >= 0) snprintf(b, sizeof b, "%dMP", cost);
      if (n) snprintf(b2, sizeof b2, "%s", n); else snprintf(b2, sizeof b2, "SPELL %d", i + 1);
      int k; uint32_t tn;
      int ic = bind_icon(c, SOM_UX_BK_SPELL, id, &k, &tn);
      draw_cell(c, r, ic, 2, SPIRIT_COL[ux->spirit & 7], b2, b, st, NULL, NULL);
      if (ux->sflash_t && ux->sflash_id == id) draw_flash(c, r, ux->sflash_t, SOM_UX_FLASH_CAST);
    } else draw_cell(c, r, -1, 1, 0, "BACK", "", 1, NULL, NULL);
    break;
  }
  case HK_T_ALLY: {
    const struct som_hero *hh = &ux->st.hero[h->a];
    win(c, r.x, r.y, r.w, r.h, h->enabled ? 0 : 2);
    uint32_t col = h->enabled ? C_TXT : C_GREY;
    int ph = r.h - 3 - 35 - 4, pw;
    if (ph > 72) ph = 72;
    if (ph * 98 / 108 > r.w - 12) ph = (r.w - 12) * 108 / 98;
    pw = ph * 98 / 108;
    int y = r.y + 4;
    if (ph >= 16) { portrait(c, h->a, r.x + (r.w - pw) / 2, y, ph, !h->enabled || hh->hp == 0); y += ph + 4; }
    else y = r.y + 4;
    snprintf(b, sizeof b, "HP %u/%u", hh->hp, hh->hp_max);
    fit_center(c, r, y, r.w - 8, b, col); y += 12;
    bar(c, r.x + 5, y, r.w - 10, 7, hh->hp, hh->hp_max, h->enabled ? hp_col(hh->hp, hh->hp_max) : C_GREY, danger(hh) == 1 ? 1 : (danger(hh) == 2 ? 2 : 0));
    break;
  }
  case HK_T_ENEMY: {
    const struct som_ux_actor *a = &ux->actor[h->a];
    const char *n = som_ux_name(ux, NAME_ENEMY + a->id);
    win(c, r.x, r.y, r.w, r.h, h->enabled ? 0 : 2);
    uint32_t ec = a->boss ? C_PURPLE : (h->enabled ? C_TXT : C_GREY);
    int side = r.h - 3 - 1 - 11 - 1 - 6 - 3; if (side > r.w - 10) side = r.w - 10;
    int y = r.y + 3;
    if (side >= 16) {
      int bx = r.x + (r.w - side) / 2;
      draw_enemy_pic(c, ux, a->id, bx, y, side);
      y += side + 1;
    }
    fit_center(c, r, y, r.w - 6, n ? n : "ENEMY", ec); y += 12;
    if (som_ux_enemy_hp_shown(ux, a->slot)) gauge(c, r.x + 4, y, r.w - 8, 6, a->hp, a->hp_max, hp_col(a->hp, a->hp_max));
    break;
  }
  case HK_T_ALL_ALLIES: case HK_T_ALL_ENEMIES: {
    win(c, r.x, r.y, r.w, r.h, 1);
    const char *t = h->kind == HK_T_ALL_ALLIES ? "ALL ALLIES" : "ALL ENEMIES";
    int M = (r.h >= 36 && lgm(c) == 2 && text_wm(c, t, 2) < r.w - 12) ? 2 : 1;
    text_cm(c, r, r.y + (r.h - 11 * M) / 2, t, C_TXT, M);
    break;
  }
  case HK_CANCEL: draw_btn(c, r, "CANCEL", 0); break;
  case HK_PAD: pad_button(c, r, h->a, h->enabled); break;
  case HK_LETTER: {
    int i = h->a;
    win(c, r.x, r.y, r.w, r.h, ux->name_cursor == i ? 1 : 0);
    if (i < 26) { char k[2] = {(char)('A' + i), 0}; text_c(c, r, r.y + (r.h - 11) / 2, k, C_TXT, 1); }
    else text_c(c, r, r.y + (r.h - 11) / 2, "DEL", C_ORG, 0);
    break;
  }
  case HK_NEXT: draw_btn(c, r, "NEXT (B)", 0); break;
  case HK_NAME_LAST: {
    int hn = som_ux_name_hero(ux);
    char t[24]; snprintf(t, sizeof t, "LAST: %s", hn >= 0 ? ux->name_last[hn] : "");
    draw_btn(c, r, h->enabled && text_w(c, t, 0) < r.w - 6 ? t : "LAST NAME", h->enabled ? 0 : 2);
    break;
  }
  default: break;
  }
  g_press = 0;
  (void)l;
}

/* Test sheet of the text faces (mocks and the font review): the alphabet, digits and punctuation in each face. */
void som_ux_text_sheet(struct som_ux *ux, uint32_t *px, int stride, int w, int h) {
  struct cv c;
  c.px = px; c.stride = stride; c.W = w; c.H = h; c.s = som_ux_scale(w, h); c.pk = ux->pack; c.sq = 0; c.hr = 0; c.hr = hr_enabled(&c);
  background(&c);
  static const char *const rows[] = {"ABCDEFGHIJKLM", "NOPQRSTUVWXYZ", "0123456789/%:.-+", "WEAPON ITEM MAGIC MAP", "MINNOW WHIM MAN NEW",
                                     "HP 523/524 MP 47/47 L50"};
  int y = 2, vw = w / c.s;
  for (int face = 0; face < 3; face++) {
    c.sq = face;
    char t[32]; snprintf(t, sizeof t, "FACE %d", face);
    text(&c, 2, y, t, C_YEL, 0); y += 12;
    for (unsigned i = 0; i < sizeof rows / sizeof rows[0]; i++) {
      if (text_w(&c, rows[i], 0) > vw - 4) continue;
      text(&c, 2, y, rows[i], C_TXT, 0); y += 12;
    }
    y += 3;
  }
  c.sq = 0;
}

static void draw_title_block(struct cv *c, struct R r, const char *t1, const char *t2) {
  win(c, r.x, r.y, r.w, r.h, 0);
  int mul = r.w >= 280 ? 2 : 1;
  int tw = text_wm(c, t1, mul);
  text_m(c, r.x + (r.w - tw) / 2, r.y + 8, t1, C_TXT, 1, mul);
  text_c(c, r, r.y + 8 + 12 * mul, t2, C_CYAN, 0);
}

/* ---- notices ---- */
void som_ux_notice(struct som_ux *ux, int kind, const char *t) {
  if (!ux || kind <= 0) return;
  char r[20];
  r[0] = 0;
  if (t) { size_t n = 0; for (const char *p = t; *p && n < 18; p++) r[n++] = (*p >= 'a' && *p <= 'z') ? (char)(*p - 32) : (*p == '_' || *p == '-' ? ' ' : *p); r[n] = 0; }
  switch (kind) {
  case SOM_UX_NOTICE_QUEUED: snprintf(ux->notice_text, sizeof ux->notice_text, r[0] ? "%s QUEUED" : "QUEUED", r); break;
  case SOM_UX_NOTICE_CANCELLED: snprintf(ux->notice_text, sizeof ux->notice_text, r[0] ? "%s CANCELLED" : "CANCELLED", r); break;
  case SOM_UX_NOTICE_DROPPED: snprintf(ux->notice_text, sizeof ux->notice_text, r[0] ? "DROPPED: %s" : "DROPPED", r); break;
  default: snprintf(ux->notice_text, sizeof ux->notice_text, "%s", r[0] ? r : "?"); break;
  }
  ux->notice_kind = (uint8_t)kind; ux->notice_t = SOM_UX_NOTICE_FRAMES;
}
void som_ux_notice_clear(struct som_ux *ux) { if (ux) ux->notice_t = 0; }

/* The chip lives in the header strip between the status pill and the gear (x0..x1), so it never covers the content below. */
static void draw_notice(struct cv *c, const struct som_ux *ux, int hh, int x0, int x1) {
  if (!ux->notice_t || !ux->notice_kind) return;
  int k = ux->notice_kind, sv = c->sq, chh = hh >= 24 ? 21 : 15, icw = 13, w, x, y = (hh - 1 - chh) / 2, vw = x1 - x0 + 8;
  uint32_t edge = k == SOM_UX_NOTICE_QUEUED ? C_YEL : (k == SOM_UX_NOTICE_CANCELLED ? C_MID : C_RED), tc = k == SOM_UX_NOTICE_CANCELLED ? C_DIM : C_TXT;
  for (int q = 0; q <= 2; q++) { c->sq = q; w = text_w(c, ux->notice_text, 0) + icw + 12; if (w <= vw - 8) break; }
  w = text_w(c, ux->notice_text, 0) + icw + 12;
  if (w > vw - 8) audit("notice wider than the screen", ux->notice_text);
  if (w > vw - 8) w = vw - 8;
  x = x0 + (x1 - x0 - w) / 2;
  win(c, x, y, w, chh, 0);
  frect(c, x + 1, y + chh - 3, w - 2, 1, edge);                      /* coloured underline: the kind is also told by the icon shape */
  int ix = x + 6, iy = y + chh / 2 - 1;
  if (k == SOM_UX_NOTICE_QUEUED) draw_clock(c, ix + 4, iy, 4);
  else if (k == SOM_UX_NOTICE_CANCELLED) { for (int i = -3; i <= 3; i++) { frect(c, ix + 4 + i, iy + i, 1, 1, C_DIM); frect(c, ix + 4 - i, iy + i, 1, 1, C_DIM); } }
  else { frect(c, ix + 3, iy - 4, 2, 6, C_RED); frect(c, ix + 3, iy + 3, 2, 2, C_RED); }   /* ! */
  text_fit(c, x + 6 + icw, y + (chh - 3 - 11) / 2 + 1, w - 12 - icw, ux->notice_text, tc, 0);
  c->sq = sv;
  if (ux->notice_t < 24) brect(c, x, y, w, chh, C_BG, 255 - ux->notice_t * 255 / 24 > 235 ? 235 : 255 - ux->notice_t * 255 / 24);   /* fade out */
}

static uint32_t redraw_hash(const struct som_ux *ux) {
  uint32_t h = 2166136261u;
  const uint8_t *p = (const uint8_t *)&ux->st;
  for (size_t i = 0; i < sizeof ux->st; i++) { h ^= p[i]; h *= 16777619u; }
  p = (const uint8_t *)ux->actor;
  for (size_t i = 0; i < sizeof ux->actor; i++) { h ^= p[i]; h *= 16777619u; }
  uint8_t u[] = {ux->nav_on, ux->nav_kind, ux->nav_a, ux->mode, ux->tab, ux->hero, ux->spirit, ux->opt_page, ux->sflash_t, ux->pol_wait, ux->rev_wait, ux->pend_kind, ux->map_small, ux->qe_slot, ux->qe_cat,
                 ux->qe_page, ux->qe_spirit, ux->quick_arm, ux->press_on, ux->press_kind, ux->fe_name_entry, (uint8_t)(ux->notice_t ? ux->notice_t : 0), ux->armed, ux->had_dialogue, ux->seen_w, ux->seen_s, (uint8_t)ux->seen_i, (uint8_t)(ux->seen_i >> 8), ux->opt_ext, ux->st_sel, (uint8_t)ux->cr_scroll, (uint8_t)(ux->cr_scroll >> 8), ux->quit_armed};
  for (size_t i = 0; i < sizeof ux->analyzed; i++) { h ^= ux->analyzed[i]; h *= 16777619u; }
  for (int i = 0; i < SOM_UX_OPT_COUNT; i++) { h ^= ux->opt_val[i] + 1u; h *= 16777619u; h ^= (uint8_t)ux->opt_text[i][0]; h *= 16777619u; h ^= (uint8_t)ux->opt_text[i][1]; h *= 16777619u; }
  { const uint8_t *q = (const uint8_t *)ux->st_slot; for (size_t i = 0; i < sizeof ux->st_slot; i++) { h ^= q[i]; h *= 16777619u; } h ^= ux->st_gen; h *= 16777619u; }
  for (size_t i = 0; i < sizeof u; i++) { h ^= u[i]; h *= 16777619u; }
  h ^= ux->name_hero; h *= 16777619u;
  for (int i = 0; i < 3; i++) for (int k = 0; k < SOM_UX_LAST_NAME; k++) { h ^= (uint8_t)ux->name_last[i][k]; h *= 16777619u; }
  for (int i = 0; i < SOM_UX_QUICK_N; i++) { h ^= ux->flash_t[i] * 31u + ux->flash_kind[i]; h *= 16777619u; h ^= ux->quick[i].kind; h *= 16777619u; h ^= ux->quick[i].id; h *= 16777619u; }
  h ^= ux->line[0] ^ (uint32_t)ux->log_n << 8 ^ (uint32_t)ux->zone_sum; h *= 16777619u;
  h ^= ux->fl_x ^ (uint32_t)ux->fl_y << 12 ^ (uint32_t)ux->fl_heading << 22 ^ (uint32_t)ux->trail_n << 27; h *= 16777619u;   /* Flammie map: arrow and trail */
  for (int i = 0; i < 42; i++) { h ^= ux->spell_avail[i]; h *= 16777619u; }
  h ^= (uint32_t)(ux->frame_no >> 3) * 977u * (ux->notice_t ? 1u : 0u);
  return h;
}
static void ux_mark_drawn(const struct som_ux *cux, int w, int h) {
  struct som_ux *ux = (struct som_ux *)cux;
  ux->drawn = 1; ux->drawn_w = (uint16_t)w; ux->drawn_h = (uint16_t)h; ux->drawn_hash = redraw_hash(ux);
}
int som_ux_needs_redraw(const struct som_ux *ux, int w, int h) {
  if (!ux || !ux->drawn || ux->drawn_w != w || ux->drawn_h != h) return 1;
  if (ux->notice_t || ux->press_on || ux->sflash_t) return 1;
  for (int i = 0; i < SOM_UX_QUICK_N; i++) if (ux->flash_t[i]) return 1;
  return redraw_hash(ux) != ux->drawn_hash;
}

void som_ux_draw(struct som_ux *ux, const struct som_macro *m, uint32_t *px, int stride, int w, int h) {
  struct cv c; struct LAY l;
  g_sn = 0; g_hn_ux = ux;
  c.px = px; c.stride = stride; c.W = w; c.H = h; c.s = som_ux_scale(w, h); c.pk = ux->pack; c.sq = 0; c.hr = 0; c.hr = hr_enabled(&c);
  int vw = w / c.s, vh = h / c.s;
  build_layout(&l, ux, vw, vh);
  { int q = quiet_level(ux);
    if (q) {
      for (int yy = 0; yy < h; yy++) for (int xx = 0; xx < w; xx++) px[yy * stride + xx] = 0xFF000000u;   /* the rows below the last full vp stay black */
      background(&c);                                        /* round 3d: the usual panel grid, nothing on it */
      if (q == 2) {
        struct R full = {0, 0, vw, vh};
        text_c(&c, full, vh / 2 - 5, "LOOK UP", 0x4A5078u, 0);
      }
      ux_mark_drawn(ux, w, h);
      return;
    }
  }
  background(&c);
  draw_header(&c, ux, m, l.header);
  char b[64];

  if (ux->rev_wait) {
    int hh = ux->rev_hero < 3 ? ux->rev_hero : 0, ph = l.info.h - 8;
    win(&c, l.info.x, l.info.y, l.info.w, l.info.h, 1);
    portrait(&c, hh, l.info.x + 6, l.info.y + 4, ph, 1);
    int tx = l.info.x + 6 + ph * 98 / 108 + 8;
    snprintf(b, sizeof b, "REVIVE");
    text_fit_m(&c, tx, l.info.y + (l.info.h - 11 * lgm(&c)) / 2, l.info.w - (tx - l.info.x) - 8, b, C_YEL, lgm(&c));
  } else if (ux->pol_wait) {
    char nm[40];
    int ix = l.info.x + 8, iy = l.info.y + (l.info.h - 16) / 2;
    win(&c, l.info.x, l.info.y, l.info.w, l.info.h, 1);
    win(&c, ix - 3, iy - 3, 22, 22, 0);
    entry_icon(&c, ix, iy, ux->pol_kind, ux->pol_id, 0);
    int en = class_is_enemy(entry_class(ux, ux->pol_kind, ux->pol_id));
    if (!som_ux_bind_name(ux, ux->pol_kind, ux->pol_id, nm, sizeof nm)) snprintf(nm, sizeof nm, "?");
    text_fit(&c, ix + 26, l.info.y + (l.info.h >= 40 ? 8 : 5), l.info.w - 40, nm, C_TXT, 0);
    static const char *const ctext[9] = {"", "ONE ALLY", "ONE OR ALL ALLIES", "ONE OR ALL ENEMIES", "ONE ENEMY", "ONE FALLEN ALLY", "ONE ALLY WITH AILMENT", "", ""};
    int cl = entry_class(ux, ux->pol_kind, ux->pol_id);
    char boyonly[24]; snprintf(boyonly, sizeof boyonly, "%s ONLY", hname(0));
    text_fit(&c, ix + 26, l.info.y + (l.info.h >= 40 ? 8 : 5) + 14, l.info.w - 40, cl == SOM_UX_TC_BOY_ONLY ? boyonly : (cl >= 0 && cl < 9 && ctext[cl][0] ? ctext[cl] : (en ? "ENEMY" : "ALLY")), C_YEL, 0);
  } else if (ux->opt_page == SOM_UX_OPT_CREDITS) {
    draw_credits(&c, ux, l.info);
  } else if (ux->opt_page == SOM_UX_OPT_STATES) {
    if (l.info.h >= 11) text_fit(&c, l.info.x + 2, l.info.y + 1, l.info.w - 4, "R3+R1 SAVE  R3+L1 LOAD  R3+R2 NEXT", C_DIM, 0);
  } else if (ux->opt_page) {
    /* the options pages draw only their hit targets */
    } else switch (ux->fe_name_entry ? SOM_UX_FRONTEND : ux->mode) {
  case SOM_UX_BOOT:
    draw_title_block(&c, l.info, "SECRET OF MANA", "COMPANION SCREEN");
    text_c(&c, l.info, l.info.y + l.info.h - 22, "PRESS START ON THE GAME OR BELOW", C_DIM, 0);
    break;
  case SOM_UX_FRONTEND:
    if (ux->fe_name_entry) {
      win(&c, l.info.x, l.info.y, l.info.w, l.info.h, 0);
      int hn = som_ux_name_hero(ux);
      if (hn > 0) { char t[40]; snprintf(t, sizeof t, "TAP LETTERS: NAME THE %s", hn == 1 ? "GIRL" : "SPRITE"); text_c(&c, l.info, l.info.y + (l.info.h - 11) / 2, t, C_TXT, 0); }
      else text_c(&c, l.info, l.info.y + (l.info.h - 11) / 2, "TAP LETTERS TO TYPE THE NAME", C_TXT, 0);
    } else {
      draw_title_block(&c, l.info, "GAME SELECT", "UP / DOWN THEN OK");
    }
    break;
  case SOM_UX_EVENT: case SOM_UX_TRANSITION: {
    if (ux->mode == SOM_UX_TRANSITION) break;                   /* round 3: only the party cards, no window and no WAIT chip */
    win(&c, l.dlg.x, l.dlg.y, l.dlg.w, l.dlg.h, 2);
    int ty = l.dlg.y + 5, tw = l.dlg.w - 10, lines;
    if (ux->line[0]) {
      lines = text_wrap(&c, l.dlg.x + 5, ty, tw, ux->line, C_TXT, 1, 3);
      ty += lines * 12 + 4;
    }
    frect(&c, l.dlg.x + 5, ty, tw, 1, C_MID);
    ty += 4;
    int maxl = (l.dlg.y + l.dlg.h - ty - 16) / 12;
    for (int i = 1; i < ux->log_n && i - 1 < maxl; i++) {
      uint32_t col = mix(C_DIM, C_BG, i * 30);
      text_fit(&c, l.dlg.x + 5, ty + (i - 1) * 12, tw, ux->log[i], col, 0);
    }
    break;
  }
  case SOM_UX_RING:
    draw_selinfo(&c, ux, l.info);
    break;
  case SOM_UX_SHOP: {
    static const char *const sm[4] = {"", "BUY", "SELL", "UPGRADE"};
    int L = lgm(&c);
    win(&c, l.info.x, l.info.y, l.info.w, l.info.h, 0);
    snprintf(b, sizeof b, "SHOP  %s", sm[ux->shop_mode & 3]);
    text_m(&c, l.info.x + 8, l.info.y + 8, b, C_YEL, 0, L);
    snprintf(b, sizeof b, "%lu G", (unsigned long)ux->st.gold);
    text(&c, l.info.x + 8, l.info.y + 12 + 11 * L, "GOLD", C_DIM, 0);
    text_m(&c, l.info.x + 8, l.info.y + 26 + 11 * L, b, C_TXT, 0, L);
    text_wrap(&c, l.info.x + 8, l.info.y + l.info.h - 26, l.info.w - 16, "PRICE AND NAME LIST: SEE THE TOP SCREEN", C_DIM, 0, 2);
    break;
  }
  case SOM_UX_FLAMMIE: {
    draw_world(&c, ux, l.mapr);
    win(&c, l.side.x, l.side.y, l.side.w, l.side.h, 0);
    int mx, my, bear; som_ux_flammie_map_pos(ux, &mx, &my, &bear);
    int y = l.side.y + 6;
    text(&c, l.side.x + 6, y, "FLAMMIE", C_YEL, 1); y += 15;
    snprintf(b, sizeof b, "HEADING %s", compass8(bear)); text(&c, l.side.x + 6, y, b, C_TXT, 0); y += 13;
    snprintf(b, sizeof b, "%d DEG", bear); text(&c, l.side.x + 6, y, b, C_DIM, 0); y += 14;
    const char *zone = som_ux_flammie_zone(ux);
    if (!ux->fl_world_ok) { text(&c, l.side.x + 6, y, "UNMAPPED WORLD", C_YEL, 0); y += 16; }
    else if (zone) y += 12 * text_wrap(&c, l.side.x + 6, y, l.side.w - 12, zone, C_TXT, 0, 2) + 4;
    else { snprintf(b, sizeof b, "X %d  Y %d", mx, my); text(&c, l.side.x + 6, y, b, C_TXT, 0); y += 16; }
    if (l.side.h > 90) {
      text_wrap(&c, l.side.x + 6, y, l.side.w - 12, "PLAY ON THE TOP SCREEN WITH THE PAD. START OPENS THE GLOBE.", C_GREY, 0, 5);
    }
    break;
  }
  case SOM_UX_FIELD: default: {
    draw_boss_bar(&c, ux, l.boss);
    if (ux->tab == SOM_UX_TAB_MAP && l.radar.w > 0) { draw_zone(&c, ux, l.radar); draw_enemy_list(&c, ux, l.elist); }
    if (pending(ux)) {
      snprintf(b, sizeof b, "%s", ux->pend_kind == 1 ? "USE ON WHOM?" : "CAST ON WHOM?");
      text_fit(&c, 4, l.header.h + 2 + (ux->boss >= 0 ? 14 : 0), l.vw - 8, b, C_YEL, 0);
    }
    break;
  }
  }
  for (int i = 0; i < l.n; i++) draw_hit(&c, ux, &l, &l.h[i]);
  if (ux->nav_on) {                                              /* pad panel mode: the focus ring */
    const struct hit *f = NULL;
    for (int i = l.n - 1; i >= 0 && !f; i--) if (l.h[i].kind == ux->nav_kind && l.h[i].a == ux->nav_a && l.h[i].enabled) f = &l.h[i];
    if (!f) {                                                    /* no focus yet, or it vanished with the screen */
      f = nav_initial(&l, ux);
      if (f) { ux->nav_kind = f->kind; ux->nav_a = f->a; }
    }
    if (f) {
      struct R r = f->r;
      static const uint32_t RING = 0x40F0FFu;
      brect(&c, r.x - 3, r.y - 3, r.w + 6, 3, RING, 255); brect(&c, r.x - 3, r.y + r.h, r.w + 6, 3, RING, 255);
      brect(&c, r.x - 3, r.y, 3, r.h, RING, 255); brect(&c, r.x + r.w, r.y, 3, r.h, RING, 255);
      brect(&c, r.x, r.y, r.w, 1, C_LIGHT, 255); brect(&c, r.x, r.y + r.h - 1, r.w, 1, C_LIGHT, 255);
      brect(&c, r.x, r.y, 1, r.h, C_LIGHT, 255); brect(&c, r.x + r.w - 1, r.y, 1, r.h, C_LIGHT, 255);
    }
  }
  /* cutscenes, dialogue and warps: no message, the whole bottom window is simply darkened */
  if (!ux->opt_page && ux->mode == SOM_UX_EVENT && !ux->fe_name_entry)
    brect(&c, 0, l.header.h, vw, vh - l.header.h, 0x03061Au, 120);
  ux_mark_drawn(ux, w, h);
}

/* ------------------------------------------------------------------ touch */
/* target class of a bindable entry (items and spells only; everything else needs no target) */
static int entry_class(const struct som_ux *ux, int kind, int id) {
  if (kind == SOM_UX_BK_ITEM && id >= 0 && id < 11) return som_ux_item_class(id);
  if (kind == SOM_UX_BK_SPELL && id >= 0 && id < 42) return ux->spell_class[id];
  return SOM_UX_TC_NONE;
}
/* policies that make sense for an entry: bit n = enum som_ux_policy n; 0 when the entry needs no target choice */
int som_ux_entry_policies(const struct som_ux *ux, int kind, int id) {
  switch (entry_class(ux, kind, id)) {
  case SOM_UX_TC_ENEMY_ONE: return 1 << SOM_UX_POL_SELECT | 1 << SOM_UX_POL_NEAR;
  case SOM_UX_TC_ENEMY_ONE_OR_ALL: return 1 << SOM_UX_POL_SELECT | 1 << SOM_UX_POL_NEAR | 1 << SOM_UX_POL_ALL;
  case SOM_UX_TC_ALLY_ONE: return 1 << SOM_UX_POL_SELECT | 1 << SOM_UX_POL_LESS_HP;
  case SOM_UX_TC_ALLY_ONE_OR_ALL: return 1 << SOM_UX_POL_SELECT | 1 << SOM_UX_POL_LESS_HP | 1 << SOM_UX_POL_ALL;
  default: return 0;
  }
}

/* nearest living enemy to the caster (screen distance, on-screen first); -1 when none */
static int nearest_enemy(const struct som_ux *ux, int caster) {
  int best = -1; long bd = 0;
  const struct som_ux_actor *c = &ux->actor[caster < 16 ? caster : 0];
  for (int i = 0; i < ux->n_enemies; i++) {
    int sl = ux->enemy_idx[i];
    const struct som_ux_actor *a = &ux->actor[sl];
    if (!a->present || !a->alive) continue;
    long dx = (int16_t)a->sx - (int16_t)c->sx, dy = (int16_t)a->sy - (int16_t)c->sy, d = dx * dx + dy * dy + (a->on_screen ? 0 : 1000000L);
    if (best < 0 || d < bd) { best = sl; bd = d; }
  }
  return best;
}
/* living ally with the lowest HP percentage (ties: lower slot); -1 when none */
static int weakest_ally(const struct som_ux *ux) {
  int best = -1; unsigned long br = 0;
  for (int i = 0; i < SOM_PARTY; i++) {
    const struct som_hero *h = &ux->st.hero[i];
    if (!h->present || !h->alive || !h->hp_max) continue;
    unsigned long r = (unsigned long)h->hp * 1000u / h->hp_max;
    if (best < 0 || r < br) { best = i; br = r; }
  }
  return best;
}

static void fill_use(struct som_ux_action *out, int kind, int arg, int spell, int hero, int tk, int ts) {
  out->kind = SOM_UX_ACT_CMD; out->cmd.hero = (uint8_t)hero;
  if (kind == 1) { out->cmd.kind = SOM_CMD_USE_ITEM; out->cmd.arg = (uint8_t)arg; }
  else { out->cmd.kind = SOM_CMD_CAST_MAGIC; out->cmd.arg = (uint8_t)arg; out->cmd.spell = (uint8_t)spell; }
  out->target_kind = (uint8_t)tk; out->target_slot = (uint8_t)ts;
}

/* Starts the use of an item (kind 1) or spell (kind 2) that needs a target.  Applies the policy: NEAR, LESS_HP and ALL return 1 with the
 * command in *out; SELECT opens the target selector (returns 0) unless the player is alone, when the only ally is the target. */
static int use_with_policy(struct som_ux *ux, int kind, int cls, int arg, int spell, int caster, int policy, struct som_ux_action *out) {
  int nh = 0, only = -1;
  for (int i = 0; i < SOM_PARTY; i++) if (ux->st.hero[i].present) { nh++; only = i; }
  if (policy == SOM_UX_POL_NEAR && class_is_enemy(cls)) {
    int e = nearest_enemy(ux, caster);
    if (e >= 0) { fill_use(out, kind, arg, spell, caster, 1, e); return 1; }
    return 0;
  }
  if (policy == SOM_UX_POL_LESS_HP && (cls == SOM_UX_TC_ALLY_ONE || cls == SOM_UX_TC_ALLY_ONE_OR_ALL)) {
    int a = weakest_ally(ux);
    if (a >= 0) { fill_use(out, kind, arg, spell, caster, 1, a); return 1; }
    return 0;
  }
  if (policy == SOM_UX_POL_ALL && cls == SOM_UX_TC_ALLY_ONE_OR_ALL) { fill_use(out, kind, arg, spell, caster, 2, 0); return 1; }
  if (policy == SOM_UX_POL_ALL && cls == SOM_UX_TC_ENEMY_ONE_OR_ALL && ux->n_enemies) { fill_use(out, kind, arg, spell, caster, 3, 0); return 1; }
  if ((cls == SOM_UX_TC_ALLY_ONE || cls == SOM_UX_TC_ALLY_ONE_OR_ALL) && nh == 1 && ux->st.hero[only].alive) {   /* alone: nobody to choose */
    fill_use(out, kind, arg, spell, caster, 1, only);
    return 1;
  }
  ux->pend_kind = (uint8_t)kind; ux->pend_class = (uint8_t)cls; ux->pend_arg = (uint8_t)arg;
  ux->pend_spell = (uint8_t)spell; ux->pend_hero = (uint8_t)caster;
  return 0;
}
static int start_pending(struct som_ux *ux, int kind, int cls, int arg, int spell, struct som_ux_action *out) {
  return use_with_policy(ux, kind, cls, arg, spell, ux->hero, SOM_UX_POL_SELECT, out);
}

static void qset(struct som_ux *ux, int slot, int kind, int id, struct som_ux_action *out) {
  ux->quick[slot].kind = (uint8_t)kind; ux->quick[slot].id = (uint8_t)id;
  out->kind = SOM_UX_ACT_QUICK_SET; out->slot = (uint8_t)slot; out->bind_kind = (uint8_t)kind; out->bind_id = (uint8_t)id;
  /* a finished bind always returns to the main panel: the editor, the options, the policy chooser and the slot arming all close */
  ux->opt_page = SOM_UX_OPT_CLOSED; ux->opt_from_main = 0; ux->pol_wait = 0; ux->quick_arm = 0; ux->qe_spirit = 0; ux->qe_page = 0;
}

/* Opens the target-policy chooser for an entry about to be stored (dest 0: quick slot, 1: physical hotkey).  Returns 0 when the entry needs no
 * choice (no target, or an ally-only choice while the player is alone). */
static int ask_policy(struct som_ux *ux, int kind, int id, int dest, int slot) {
  if (!som_ux_entry_policies(ux, kind, id)) return 0;
  int nh = 0, cls = entry_class(ux, kind, id);
  for (int i = 0; i < SOM_PARTY; i++) if (ux->st.hero[i].present) nh++;
  if (!class_is_enemy(cls) && nh <= 1) return 0;
  ux->pol_wait = 1; ux->pol_kind = (uint8_t)kind; ux->pol_id = (uint8_t)id; ux->pol_dest = (uint8_t)dest; ux->pol_slot = (uint8_t)slot;
  ux->quick_arm = 0;
  return 1;
}
/* store an entry in a quick slot, asking for its target policy first when it needs one */
static int set_slot(struct som_ux *ux, int slot, int kind, int id, struct som_ux_action *out) {
  if (ask_policy(ux, kind, id, 0, slot)) return 0;
  qset(ux, slot, kind, id, out);
  return 1;
}
/* Feedback for a slot that was used.  r = what quick_run returned: an executed action flashes every slot holding the entry (and the spell list
 * button), a refused one (unavailable entry, nobody to target) flashes a cross on the slot, a pending target selector flashes nothing yet. */
static void flash_result(struct som_ux *ux, int slot, int r, const struct som_ux_action *out) {
  const struct som_ux_quick *q = &ux->quick[slot];
  if (r && out->kind == SOM_UX_ACT_CMD) {
    int sp = q->kind == SOM_UX_BK_SPELL;
    som_ux_flash_entry(ux, sp ? SOM_UX_FLASH_CAST : SOM_UX_FLASH_OK, q->kind, q->id);
    som_ux_slot_flash(ux, slot, sp ? SOM_UX_FLASH_CAST : SOM_UX_FLASH_OK);
  } else if (r && out->kind == SOM_UX_ACT_OPEN_MENU) som_ux_slot_flash(ux, slot, SOM_UX_FLASH_OK);
  else if (r && out->kind == SOM_UX_ACT_CANCEL_QUEUED) som_ux_slot_flash(ux, slot, SOM_UX_FLASH_REFUSED);
  else if (!r && !ux->pend_kind) som_ux_slot_flash(ux, slot, SOM_UX_FLASH_REFUSED);
}

/* Runs a hotlink.  Returns 1 with *out filled when the host has something to do. */
static int quick_run(struct som_ux *ux, int kind, int id, int policy, struct som_ux_action *out) {
  const struct som_state *st = &ux->st;
  int hero = act_hero(ux), ring = ux->mode == SOM_UX_RING;
  if (kind == SOM_UX_BK_MENU || kind == SOM_UX_BK_SUBMENU) {
    out->kind = SOM_UX_ACT_OPEN_MENU; out->bind_kind = (uint8_t)kind; out->bind_id = (uint8_t)id; out->hero = (uint8_t)hero;
    return 1;
  }
  if (kind == SOM_UX_BK_SPIRIT) {                    /* UI only: show that spirit's spells for a caster */
    ux->tab = SOM_UX_TAB_MAGIC; ux->spirit = (uint8_t)id;
    if (ux->hero == 0 || !st->hero[ux->hero].present) ux->hero = st->hero[1].present ? 1 : 2;
    out->kind = SOM_UX_ACT_OPEN_MENU; out->bind_kind = SOM_UX_BK_SPIRIT; out->bind_id = (uint8_t)id; out->hero = (uint8_t)ux->hero;   /* the host also opens the game's own spell list (direct menu jump) */
    return 1;
  }
  if (kind == SOM_UX_BK_SPELL && id >= 0 && id < 42 && som_ux_spell_state(ux, id) == SOM_UX_AV_QUEUED) {
    out->kind = SOM_UX_ACT_CANCEL_QUEUED; out->bind_kind = SOM_UX_BK_SPELL; out->bind_id = (uint8_t)id; out->hero = (uint8_t)spell_caster(id);
    return 1;
  }
  if (!entry_available(ux, kind, id)) return 0;
  if (kind == SOM_UX_BK_WEAPON) {
    out->kind = SOM_UX_ACT_CMD; out->cmd.kind = SOM_CMD_EQUIP_WEAPON; out->cmd.hero = (uint8_t)hero; out->cmd.arg = (uint8_t)id;
    return 1;
  }
  if (kind == SOM_UX_BK_ITEM) {
    int rec = -1;
    for (int i = 0; i < SOM_ITEM_RECORDS; i++) if (st->item[i].qty && st->item[i].id == id) { rec = i; break; }
    if (rec < 0) return 0;
    int cls = som_ux_item_class(id);
    if (cls == SOM_UX_TC_UNSUPPORTED || cls == SOM_UX_TC_NONE) {
      out->kind = SOM_UX_ACT_CMD; out->cmd.kind = SOM_CMD_USE_ITEM; out->cmd.hero = (uint8_t)hero; out->cmd.arg = (uint8_t)rec;
      return 1;
    }
    if (use_with_policy(ux, 1, cls, rec, 0, hero, policy, out)) return 1;
    if (ring) out->kind = SOM_UX_ACT_CLOSE_RING;
    return ring;
  }
  if (kind == SOM_UX_BK_SPELL) {
    int caster = (id % 6) >= 3 ? 1 : 2, spirit = id / 6, slot = (id % 6) % 3;
    int cls = ux->spell_class[id];
    if (cls == SOM_UX_TC_UNSUPPORTED || cls == SOM_UX_TC_NONE) {
      out->kind = SOM_UX_ACT_CMD; out->cmd.kind = SOM_CMD_CAST_MAGIC; out->cmd.hero = (uint8_t)caster;
      out->cmd.arg = (uint8_t)spirit; out->cmd.spell = (uint8_t)slot;
      return 1;
    }
    if (use_with_policy(ux, 2, cls, spirit, slot, caster, policy, out)) return 1;
    if (ring) out->kind = SOM_UX_ACT_CLOSE_RING;
    return ring;
  }
  return 0;
}

/* a quick slot is used (tap or shoulder button) */
static int slot_run(struct som_ux *ux, int slot, struct som_ux_action *out) {
  const struct som_ux_quick *q = &ux->quick[slot];
  int r = quick_run(ux, q->kind, q->id, q->policy, out);
  flash_result(ux, slot, r, out);
  return r;
}
int som_ux_shoulder_press(struct som_ux *ux, int which, struct som_ux_action *out) {
  memset(out, 0, sizeof *out);
  if (!ux || which < 0 || which >= SOM_UX_SHOULDERS) return 0;
  if (!ux->quick[which].kind) { som_ux_slot_flash(ux, which, SOM_UX_FLASH_REFUSED); return 0; }
  return slot_run(ux, which, out);
}

int som_ux_touch(struct som_ux *ux, int x, int y, int w, int h, struct som_ux_action *out) {
  struct LAY l;
  int s = som_ux_scale(w, h), vx = x / s, vy = y / s;
  memset(out, 0, sizeof *out);
  build_layout(&l, ux, w / s, h / s);
  for (int i = l.n - 1; i >= 0; i--) {
    const struct hit *t = &l.h[i];
    if (!inr(&t->r, vx, vy)) continue;
    if (ux->quick_arm == 3 && t->kind != HK_QUICK) ux->quick_arm = 0;      /* a long-pressed entry waits for a slot tap only */
    if (!(t->kind == HK_OPT_NAV && t->a == 6) && ux->quit_armed) { ux->quit_armed = 0; som_ux_notice_clear(ux); }
    switch (t->kind) {
    case HK_PARTY:
      if (ux->mode == SOM_UX_FIELD && t->enabled) {
        if (ux->st.hero[t->a].present && !ux->st.hero[t->a].alive) { som_ux_revive_open(ux, t->a); return 0; }
        int again = ux->hero == t->a;
        ux->hero = t->a; ux->spirit = 0xFF;
        if (ux->st.hero[t->a].present && ux->st.hero[t->a].alive && !ux->nav_on && t->a != ux->st.pad_hero) {
          /* first tap selects (to see that hero's magic or items), a second tap on the selected card takes control */
          if (!again) { som_ux_notice(ux, SOM_UX_NOTICE_INFO, "TAP AGAIN TO LEAD"); return 0; }
          if (!strcmp(ux->notice_text, "TAP AGAIN TO LEAD")) som_ux_notice_clear(ux);
          out->kind = SOM_UX_ACT_SWITCH_HERO; out->hero = t->a;
          return 1;
        }
      }
      return 0;
    case HK_REVIVE: return som_ux_revive_choose(ux, t->a, out);
    case HK_REV_CANCEL: ux->rev_wait = 0; return 0;
    case HK_TAB:
      ux->tab = t->a; ux->spirit = 0xFF;
      if (t->a == SOM_UX_TAB_MAGIC && ux->hero == 0) ux->hero = ux->st.hero[1].present ? 1 : 2;
      return 0;
    case HK_OPT:
      ux->quick_arm = 0;
      ux->opt_page = ux->opt_page ? SOM_UX_OPT_CLOSED : SOM_UX_OPT_MAIN;
      return 0;
    case HK_OPTION: {
      int id = t->a;
      out->kind = SOM_UX_ACT_OPTION; out->opt_id = (uint8_t)id;
      if (opt_is_toggle(id)) { ux->opt_val[id] = ux->opt_val[id] ? 0 : 1; out->opt_val = ux->opt_val[id]; }
      else out->opt_val = (uint8_t)(ux->opt_val[id] + 1);
      return 1;
    }
    case HK_ST_SLOT: ux->st_sel = t->a; out->kind = SOM_UX_ACT_STATE; out->opt_id = 2; out->opt_val = t->a; return 1;
    case HK_ST_SAVE: out->kind = SOM_UX_ACT_STATE; out->opt_id = 0; out->opt_val = ux->st_sel; return 1;
    case HK_ST_LOAD:
      if (!t->enabled) return 0;
      out->kind = SOM_UX_ACT_STATE; out->opt_id = 1; out->opt_val = ux->st_sel;
      return 1;
    case HK_CR_SCROLL: {
      int step = ux->cr_vis > 2 ? ux->cr_vis - 1 : 1;
      if (!t->enabled) return 0;
      ux->cr_scroll = (int16_t)(t->a ? ux->cr_scroll + step : ux->cr_scroll - step);
      if (ux->cr_scroll < 0) ux->cr_scroll = 0;
      return 0;
    }
    case HK_OPT_NAV:
      if (t->a == 4) { ux->opt_page = SOM_UX_OPT_STATES; ux->opt_from_main = 1; ux->nav_kind = 0; }
      else if (t->a == 5) { ux->opt_page = SOM_UX_OPT_CREDITS; ux->opt_from_main = 1; ux->cr_scroll = 0; ux->nav_kind = 0; }
      else if (t->a == 6) {                         /* MAIN MENU: the first tap arms, the second quits (the game in progress is lost) */
        if (!ux->quit_armed) { ux->quit_armed = 1; som_ux_notice(ux, SOM_UX_NOTICE_INFO, "TAP AGAIN TO QUIT"); return 0; }
        ux->quit_armed = 0; som_ux_notice_clear(ux); out->kind = SOM_UX_ACT_MAIN_MENU; return 1;
      }
      else if (t->a == 0) ux->opt_page = SOM_UX_OPT_CLOSED;
      else if (t->a == 2) { ux->opt_page = SOM_UX_OPT_QUICK; ux->opt_from_main = 1; if (ux->qe_slot >= SOM_UX_QUICK_N) ux->qe_slot = 0; }
      else ux->opt_page = ux->opt_from_main ? SOM_UX_OPT_MAIN : SOM_UX_OPT_CLOSED;   /* BACK of the editor: previous screen only */
      return 0;
    case HK_AVATAR:
      if (ux->st.hero[t->a].present && !ux->st.hero[t->a].alive) { som_ux_revive_open(ux, t->a); return 0; }
      out->kind = SOM_UX_ACT_AVATAR; out->hero = t->a;
      return 1;
    case HK_BIND_ENTRY:
      if (!t->enabled) return 0;
      ux->quick_arm = ux->quick_arm == 1 ? 0 : 1;
      return 0;
    case HK_BIND_MENU:
      if (!t->enabled) return 0;
      ux->quick_arm = ux->quick_arm == 2 ? 0 : 2;
      return 0;
    case HK_QUICK: {
      int slot = t->a;
      if (ux->opt_page == SOM_UX_OPT_QUICK) { ux->qe_slot = (uint8_t)slot; return 0; }
      const struct som_ux_quick *q = &ux->quick[slot];
      int arm = ux->quick_arm;
      ux->quick_arm = 0;
      if (arm) {
        if (arm == 1 && ux->sel.valid && ux->sel.kind) return set_slot(ux, slot, ux->sel.kind, ux->sel.id, out);
        if (arm == 2 && ux->st.ring.category == SOM_CAT_MAGIC) { qset(ux, slot, SOM_UX_BK_MENU, ux->sel.menu, out); return 1; }
        if (arm == 3 && ux->arm_kind) return set_slot(ux, slot, ux->arm_kind, ux->arm_id, out);
        return 0;
      }
      if (!q->kind) {
        if (ux->mode == SOM_UX_RING && ux->sel.valid && ux->sel.kind) return set_slot(ux, slot, ux->sel.kind, ux->sel.id, out);
        ux->opt_page = SOM_UX_OPT_QUICK; ux->opt_from_main = 0; ux->qe_slot = (uint8_t)slot; ux->qe_cat = 0; ux->qe_page = 0; ux->qe_spirit = 0;
        return 0;
      }
      return slot_run(ux, slot, out);
    }
    case HK_MAPVIEW: ux->map_small = ux->map_small ? 0 : 1; return 0;
    case HK_QE_TAB: ux->qe_cat = t->a; ux->qe_page = 0; ux->qe_spirit = 0; return 0;
    case HK_QE_BACK: ux->qe_spirit = 0; ux->qe_page = 0; return 0;
    case HK_POL_CANCEL: ux->pol_wait = 0; return 0;
    case HK_POLICY: {
      int pol = t->a, kind = ux->pol_kind, id = ux->pol_id;
      ux->pol_wait = 0;
      int slot = ux->pol_slot < SOM_UX_QUICK_N ? ux->pol_slot : 0;
      qset(ux, slot, kind, id, out);
      ux->quick[slot].policy = (uint8_t)pol; out->policy = (uint8_t)pol;
      return 1;
    }
    case HK_QE_PAGE:
      if (!t->enabled) return 0;
      if (t->a) ux->qe_page++; else ux->qe_page--;
      return 0;
    case HK_QE_CELL: {
      int kind, id, slot = ux->qe_slot < SOM_UX_QUICK_N ? ux->qe_slot : 0;
      qe_entry(ux, ux->qe_cat, ux->qe_page * QE_PAGE + t->a, &kind, &id);
      ux->qe_slot = (uint8_t)slot;
      if (ux->qe_cat == 4 && ux->qe_spirit == 0) { ux->qe_spirit = (uint8_t)(id + 1); ux->qe_page = 0; return 0; }   /* first level: open the spirit */
      return set_slot(ux, slot, kind, id, out);
    }
    case HK_QE_CLEAR:
      if (!t->enabled) return 0;
      qset(ux, ux->qe_slot, 0, 0, out);
      return 1;
    case HK_CELL: {
      if (!t->enabled) return 0;
      int sel = t->a;
      if (ux->tab == SOM_UX_TAB_WEAPON) {
        out->kind = SOM_UX_ACT_CMD; out->cmd.kind = SOM_CMD_EQUIP_WEAPON; out->cmd.hero = ux->hero; out->cmd.arg = (uint8_t)sel;
        return 1;
      }
      if (ux->tab == SOM_UX_TAB_ITEM) {
        int cls = som_ux_item_class(ux->st.item[sel].id);
        if (cls == SOM_UX_TC_UNSUPPORTED) {
          out->kind = SOM_UX_ACT_CMD; out->cmd.kind = SOM_CMD_USE_ITEM; out->cmd.hero = ux->hero; out->cmd.arg = (uint8_t)sel;
          return 1;
        }
        return start_pending(ux, 1, cls, sel, 0, out);
      }
      if (ux->spirit == 0xFF) { ux->spirit = (uint8_t)sel; return 0; }
      if (sel == 3) { ux->spirit = 0xFF; return 0; }
      {
        int id = spell_id(ux, ux->hero, ux->spirit, sel);
        int cls = id >= 0 && id < 42 ? ux->spell_class[id] : SOM_UX_TC_UNSUPPORTED;
        if (id >= 0 && id < 42) {
          int ss = som_ux_spell_state(ux, id);
          if (ss == SOM_UX_AV_QUEUED) { out->kind = SOM_UX_ACT_CANCEL_QUEUED; out->bind_kind = SOM_UX_BK_SPELL; out->bind_id = (uint8_t)id; out->hero = (uint8_t)spell_caster(id); return 1; }
          if (ss == SOM_UX_AV_DISABLED) return 0;
        }
        if (cls == SOM_UX_TC_UNSUPPORTED || cls == SOM_UX_TC_NONE) {
          out->kind = SOM_UX_ACT_CMD; out->cmd.kind = SOM_CMD_CAST_MAGIC; out->cmd.hero = ux->hero; out->cmd.arg = ux->spirit;
          out->cmd.spell = (uint8_t)sel;
          som_ux_flash_entry(ux, SOM_UX_FLASH_CAST, SOM_UX_BK_SPELL, id);       /* stay in the spell list: the button flashes */
          return 1;
        }
        return start_pending(ux, 2, cls, ux->spirit, sel, out);
      }
    }
    case HK_T_ALLY: case HK_T_ENEMY: case HK_T_ALL_ALLIES: case HK_T_ALL_ENEMIES: {
      if (!t->enabled) return 0;
      out->kind = SOM_UX_ACT_CMD;
      out->cmd.hero = ux->pend_hero;
      if (ux->pend_kind == 1) { out->cmd.kind = SOM_CMD_USE_ITEM; out->cmd.arg = ux->pend_arg; }
      else { out->cmd.kind = SOM_CMD_CAST_MAGIC; out->cmd.arg = ux->pend_arg; out->cmd.spell = ux->pend_spell; }
      if (t->kind == HK_T_ALL_ALLIES) out->target_kind = 2;
      else if (t->kind == HK_T_ALL_ENEMIES) out->target_kind = 3;
      else { out->target_kind = 1; out->target_slot = t->a; }
      if (ux->pend_kind == 1) {
        if (ux->pend_arg < SOM_ITEM_RECORDS) som_ux_flash_entry(ux, SOM_UX_FLASH_OK, SOM_UX_BK_ITEM, ux->st.item[ux->pend_arg].id);
      } else som_ux_flash_entry(ux, SOM_UX_FLASH_CAST, SOM_UX_BK_SPELL, spell_id(ux, ux->pend_hero, ux->pend_arg, ux->pend_spell));
      ux->pend_kind = 0; ux->pend_class = 0;
      return 1;
    }
    case HK_CANCEL: ux->pend_kind = 0; ux->pend_class = 0; return 0;
    case HK_PAD: out->kind = SOM_UX_ACT_PAD; out->pad = (uint16_t)(1u << t->a); return 1;
    case HK_LETTER:
      if (t->a < 26) { out->kind = SOM_UX_ACT_NAME_LETTER; out->letter = t->a; }
      else { out->kind = SOM_UX_ACT_NAME_DELETE; }
      return 1;
    case HK_NEXT: out->kind = SOM_UX_ACT_PAD; out->pad = 1u << SOM_PAD_B; return 1;
    case HK_NAME_LAST:
      if (!t->enabled) return 0;
      out->kind = SOM_UX_ACT_NAME_LAST; out->hero = (uint8_t)som_ux_name_hero(ux);
      return 1;
    default: return 0;
    }
  }
  if (ux->quick_arm == 3) ux->quick_arm = 0;
  return 0;
}

/* ---- REVIVE chooser API ---- */
int som_ux_revive_open(struct som_ux *ux, int hero) {
  if (!ux || hero < 0 || hero >= SOM_PARTY || !ux->st.hero[hero].present || ux->st.hero[hero].alive) return 0;
  struct som_ux_revive_opt o[REV_MAX];
  ux->rev_wait = 1; ux->rev_hero = (uint8_t)hero; ux->quick_arm = 0; ux->pend_kind = 0; ux->pend_class = 0;
  return rev_options(ux, o);
}
int som_ux_revive_count(const struct som_ux *ux) { struct som_ux_revive_opt o[REV_MAX]; return ux && ux->rev_wait ? rev_options(ux, o) : 0; }
int som_ux_revive_option(const struct som_ux *ux, int idx, struct som_ux_revive_opt *out) {
  struct som_ux_revive_opt o[REV_MAX];
  if (!ux || !ux->rev_wait || idx < 0 || idx >= rev_options(ux, o)) return 0;
  *out = o[idx];
  return 1;
}
void som_ux_revive_close(struct som_ux *ux) { if (ux) ux->rev_wait = 0; }
int som_ux_revive_choose(struct som_ux *ux, int idx, struct som_ux_action *out) {
  struct som_ux_revive_opt o[REV_MAX];
  memset(out, 0, sizeof *out);
  if (!ux || !ux->rev_wait || idx < 0 || idx >= rev_options(ux, o)) return 0;
  const struct som_ux_revive_opt *q = &o[idx];
  int dead = ux->rev_hero;
  if (q->queued) {
    out->kind = SOM_UX_ACT_CANCEL_QUEUED; out->bind_kind = SOM_UX_BK_SPELL; out->bind_id = q->id; out->hero = q->caster;
    ux->rev_wait = 0;
    return 1;
  }
  if (!q->enabled) return 0;
  out->kind = SOM_UX_ACT_CMD; out->cmd.hero = q->caster; out->target_kind = 1; out->target_slot = (uint8_t)dead;
  if (q->kind == 1) {
    int rec = -1;
    for (int i = 0; i < SOM_ITEM_RECORDS && rec < 0; i++) if (ux->st.item[i].qty && ux->st.item[i].id == q->id) rec = i;
    if (rec < 0) { memset(out, 0, sizeof *out); return 0; }
    out->cmd.kind = SOM_CMD_USE_ITEM; out->cmd.arg = (uint8_t)rec;
  } else {
    int g = q->id / 6, spirit = (g == 6 && q->caster == 1) ? 7 : g;
    out->cmd.kind = SOM_CMD_CAST_MAGIC; out->cmd.arg = (uint8_t)spirit; out->cmd.spell = (uint8_t)(q->id % 3);
  }
  ux->rev_wait = 0;
  return 1;
}

/* ---- pressed state ---- */
static const struct hit *find_hit(const struct LAY *l, int kind, int a) {
  for (int i = l->n - 1; i >= 0; i--) if (l->h[i].kind == kind && l->h[i].a == a) return &l->h[i];
  return NULL;
}
/* ---- pad panel mode ---- */
int som_ux_nav_toggle(struct som_ux *ux) { ux->nav_on = !ux->nav_on; if (ux->nav_on) ux->nav_kind = 0; return ux->nav_on; }
int som_ux_nav_on(const struct som_ux *ux) { return ux->nav_on; }


int som_ux_nav_move(struct som_ux *ux, int dir, int w, int h) {
  struct LAY l;
  if (ux->opt_page == SOM_UX_OPT_CREDITS && dir < 2) {            /* the credits text scrolls with the pad (up / down); left / right still move between the buttons */
    int old = ux->cr_scroll;
    ux->cr_scroll = (int16_t)(dir == 0 ? old - 1 : old + 1);
    if (ux->cr_scroll < 0) ux->cr_scroll = 0;
    if (ux->cr_scroll > ux->cr_lines - ux->cr_vis) ux->cr_scroll = (int16_t)(ux->cr_lines - ux->cr_vis > 0 ? ux->cr_lines - ux->cr_vis : 0);
    return ux->cr_scroll != old;
  }
  int s = som_ux_scale(w, h);
  build_layout(&l, ux, w / s, h / s);
  const struct hit *cur = NULL;
  for (int i = l.n - 1; i >= 0 && !cur; i--) if (nav_selectable(&l.h[i]) && l.h[i].kind == ux->nav_kind && l.h[i].a == ux->nav_a) cur = &l.h[i];
  if (!cur) {                                                    /* no focus yet (or it vanished): the first selectable target, top left first */
    const struct hit *best = nav_initial(&l, ux);
    if (!best) return 0;
    ux->nav_kind = best->kind; ux->nav_a = best->a;
    return 1;
  }
  int cx = cur->r.x + cur->r.w / 2, cy = cur->r.y + cur->r.h / 2;
  const struct hit *best = NULL, *wrap = NULL;
  long bs = 0, ws = 0;
  for (int i = 0; i < l.n; i++) {
    const struct hit *t = &l.h[i];
    if (t == cur || !nav_selectable(t)) continue;
    int tx = t->r.x + t->r.w / 2, ty = t->r.y + t->r.h / 2, dx = tx - cx, dy = ty - cy;
    int along = dir == 0 ? -dy : dir == 1 ? dy : dir == 2 ? -dx : dx;      /* distance in the pressed direction */
    int across = (dir < 2) ? dx : dy;
    if (across < 0) across = -across;
    if (along > 0) {                                                       /* a target ahead; the one that is straighter and nearer wins */
      long sc = (long)along + 3L * across;
      if (!best || sc < bs) { best = t; bs = sc; }
    } else {                                                               /* wrap: the farthest one behind, nearest to the same line */
      long sc = -(long)along * 4 - across;
      if (!wrap || sc > ws) { wrap = t; ws = sc; }
    }
  }
  const struct hit *pick = best ? best : wrap;
  if (!pick) return 0;
  ux->nav_kind = pick->kind; ux->nav_a = pick->a;
  return 1;
}

int som_ux_nav_activate(struct som_ux *ux, int w, int h, struct som_ux_action *out) {
  struct LAY l;
  int s = som_ux_scale(w, h);
  memset(out, 0, sizeof *out);
  build_layout(&l, ux, w / s, h / s);
  for (int i = l.n - 1; i >= 0; i--) {
    const struct hit *t = &l.h[i];
    if (nav_selectable(t) && t->kind == ux->nav_kind && t->a == ux->nav_a)
      return som_ux_touch(ux, (t->r.x + t->r.w / 2) * s, (t->r.y + t->r.h / 2) * s, w, h, out);
  }
  if (som_ux_nav_move(ux, 1, w, h)) return 0;                  /* nothing focused: this press only puts the ring somewhere */
  return 0;
}

int som_ux_nav_back(struct som_ux *ux, int w, int h, struct som_ux_action *out) {
  struct LAY l;
  int s = som_ux_scale(w, h);
  memset(out, 0, sizeof *out);
  build_layout(&l, ux, w / s, h / s);
  static const uint8_t backs[] = {HK_BIND_CANCEL, HK_POL_CANCEL, HK_REV_CANCEL, HK_QE_BACK, HK_CANCEL};
  for (unsigned k = 0; k < sizeof backs; k++)
    for (int i = l.n - 1; i >= 0; i--)
      if (l.h[i].kind == backs[k] && l.h[i].enabled) return som_ux_touch(ux, (l.h[i].r.x + l.h[i].r.w / 2) * s, (l.h[i].r.y + l.h[i].r.h / 2) * s, w, h, out);
  if (ux->opt_page == SOM_UX_OPT_STATES || ux->opt_page == SOM_UX_OPT_CREDITS) { ux->opt_page = SOM_UX_OPT_MAIN; ux->nav_kind = 0; return 0; }
  if (ux->opt_page) { ux->opt_page = SOM_UX_OPT_CLOSED; ux->nav_kind = 0; }
  return 0;
}

int som_ux_touch_down(struct som_ux *ux, int x, int y, int w, int h) {
  struct LAY l;
  int s = som_ux_scale(w, h), vx = x / s, vy = y / s;
  build_layout(&l, ux, w / s, h / s);
  ux->press_on = 0;
  for (int i = l.n - 1; i >= 0; i--) {
    const struct hit *t = &l.h[i];
    if (!inr(&t->r, vx, vy)) continue;
    if (!t->enabled) return 0;
    ux->press_kind = t->kind; ux->press_a = t->a; ux->press_on = 1;
    return 1;
  }
  return 0;
}
void som_ux_touch_move(struct som_ux *ux, int x, int y, int w, int h) {
  struct LAY l;
  if (!ux->press_kind) return;
  int s = som_ux_scale(w, h);
  build_layout(&l, ux, w / s, h / s);
  const struct hit *t = find_hit(&l, ux->press_kind, ux->press_a);
  ux->press_on = t && inr(&t->r, x / s, y / s);
}
/* Android entry point (weak symbol in som_jni.c): finger down at (x, y) or release/cancel (down = 0). */
void som_ux_press(struct som_ux *ux, int x, int y, int w, int h, int down) {
  if (down) som_ux_touch_down(ux, x, y, w, h);
  else som_ux_touch_up(ux);
}
void som_ux_touch_up(struct som_ux *ux) { ux->press_on = 0; ux->press_kind = 0; ux->press_a = 0; }
void som_ux_press_hit(struct som_ux *ux, int hit, int a, int on) {
  if (!on) { som_ux_touch_up(ux); return; }
  ux->press_kind = (uint8_t)hit; ux->press_a = (uint8_t)a; ux->press_on = 1;
}
int som_ux_pressed(const struct som_ux *ux, int *hit, int *a) {
  if (hit) *hit = ux->press_on ? ux->press_kind : 0;
  if (a) *a = ux->press_on ? ux->press_a : 0;
  return ux->press_on;
}

int som_ux_longpress(struct som_ux *ux, int x, int y, int w, int h, struct som_ux_action *out) {
  struct LAY l;
  int s = som_ux_scale(w, h), vx = x / s, vy = y / s;
  memset(out, 0, sizeof *out);
  build_layout(&l, ux, w / s, h / s);
  for (int i = l.n - 1; i >= 0; i--) {
    const struct hit *t = &l.h[i];
    if (!inr(&t->r, vx, vy)) continue;
    int kind = 0, id = 0;
    if (t->kind == HK_AVATAR) {
      out->kind = SOM_UX_ACT_OPEN_MENU; out->bind_kind = SOM_UX_BK_SUBMENU; out->bind_id = SOM_UX_CFG_STATUS; out->hero = t->a;
      return 1;
    }
    if (t->kind == HK_QUICK && ux->opt_page != SOM_UX_OPT_QUICK) {
      ux->quick_arm = 0;
      ux->opt_page = SOM_UX_OPT_QUICK; ux->opt_from_main = 0; ux->qe_slot = t->a; ux->qe_page = 0;
      ux->qe_cat = 0; ux->qe_spirit = 0;
      out->kind = SOM_UX_ACT_QUICK_EDIT; out->slot = t->a;
      return 1;
    }
    if (t->kind == HK_CELL && t->enabled) {
      if (ux->tab == SOM_UX_TAB_WEAPON) { kind = SOM_UX_BK_WEAPON; id = t->a; }
      else if (ux->tab == SOM_UX_TAB_ITEM) { kind = SOM_UX_BK_ITEM; id = ux->st.item[t->a].id; if (id > 10) kind = 0; }
      else if (ux->tab == SOM_UX_TAB_MAGIC && ux->spirit != 0xFF && t->a < 3) { kind = SOM_UX_BK_SPELL; id = spell_id(ux, ux->hero, ux->spirit, t->a); }
      else if (ux->tab == SOM_UX_TAB_MAGIC && ux->spirit == 0xFF) { kind = SOM_UX_BK_SPIRIT; id = t->a; }
    }
    if (!kind) return 0;
    /* arm the entry: the next tap on a quick slot stores it there (the target policy is asked then) */
    ux->quick_arm = 3; ux->arm_kind = (uint8_t)kind; ux->arm_id = (uint8_t)id;
    som_ux_notice(ux, SOM_UX_NOTICE_INFO, "TAP A SLOT");
    return 0;
  }
  return 0;
}

/* The host's hotkey path: run a bound entry now.  Same behaviour as tapping a quick slot that holds it, with the policy the player chose. */
int som_ux_run_entry(struct som_ux *ux, int kind, int id, int policy, struct som_ux_action *out) {
  memset(out, 0, sizeof *out);
  if (policy < 0 || policy >= SOM_UX_POL_COUNT) policy = SOM_UX_POL_SELECT;
  return quick_run(ux, kind, id, policy, out);
}
int som_ux_quick_set_policy(struct som_ux *ux, int slot, int policy) {
  if (slot < 0 || slot >= SOM_UX_QUICK_N || policy < 0 || policy >= SOM_UX_POL_COUNT) return -1;
  ux->quick[slot].policy = (uint8_t)policy;
  return 0;
}
int som_ux_quick_policy(const struct som_ux *ux, int slot) { return slot >= 0 && slot < SOM_UX_QUICK_N ? ux->quick[slot].policy : 0; }
int som_ux_min_target_vp(struct som_ux *ux, int w, int h) {
  struct LAY l;
  int s = som_ux_scale(w, h), mn = 1 << 20;
  build_layout(&l, ux, w / s, h / s);
  for (int i = 0; i < l.n; i++) {
    int d = l.h[i].r.w < l.h[i].r.h ? l.h[i].r.w : l.h[i].r.h;
    if (l.h[i].enabled && d < mn) mn = d;
  }
  return mn == (1 << 20) ? 0 : mn;
}

int som_ux_target_center(struct som_ux *ux, int w, int h, int hit, int a, int *x, int *y) {
  struct LAY l;
  int s = som_ux_scale(w, h);
  build_layout(&l, ux, w / s, h / s);
  for (int i = 0; i < l.n; i++)
    if (l.h[i].kind == hit && l.h[i].a == a) {
      if (x) *x = (l.h[i].r.x + l.h[i].r.w / 2) * s + s / 2;
      if (y) *y = (l.h[i].r.y + l.h[i].r.h / 2) * s + s / 2;
      return 1;
    }
  return 0;
}

int som_ux_target_min_vp(struct som_ux *ux, int w, int h, int hit) {
  struct LAY l;
  int s = som_ux_scale(w, h), mn = 1 << 20;
  build_layout(&l, ux, w / s, h / s);
  for (int i = 0; i < l.n; i++)
    if (l.h[i].kind == hit && l.h[i].enabled) {
      int d = l.h[i].r.w < l.h[i].r.h ? l.h[i].r.w : l.h[i].r.h;
      if (d < mn) mn = d;
    }
  return mn == (1 << 20) ? 0 : mn;
}

void som_ux_set_view(struct som_ux *ux, int tab, int hero, int spirit) {
  ux->tab = (uint8_t)tab; ux->hero = (uint8_t)hero; ux->spirit = (uint8_t)spirit;
}
void som_ux_set_pending(struct som_ux *ux, int kind, int cls, int arg, int spell) {
  ux->pend_kind = (uint8_t)kind; ux->pend_class = (uint8_t)cls; ux->pend_arg = (uint8_t)arg;
  ux->pend_spell = (uint8_t)spell; ux->pend_hero = ux->hero;
}

/* ---- ring selection, quick slots, options ---- */
const struct som_ux_selinfo *som_ux_selection(const struct som_ux *ux) { return ux ? &ux->sel : NULL; }

int som_ux_quick_set(struct som_ux *ux, int slot, int kind, int id) {
  if (slot < 0 || slot >= SOM_UX_QUICK_N || kind < 0 || kind > SOM_UX_BK_LAST || id < 0 || id > 255) return -1;
  ux->quick[slot].kind = (uint8_t)kind; ux->quick[slot].id = (uint8_t)(kind ? id : 0); ux->quick[slot].policy = 0;
  return 0;
}
int som_ux_quick_get(const struct som_ux *ux, int slot, int *kind, int *id) {
  if (slot < 0 || slot >= SOM_UX_QUICK_N) return 0;
  if (kind) *kind = ux->quick[slot].kind;
  if (id) *id = ux->quick[slot].id;
  return ux->quick[slot].kind != 0;
}
int som_ux_quick_bind_selected(struct som_ux *ux, int slot) {
  if (!ux->sel.valid || !ux->sel.kind) return -1;
  return som_ux_quick_set(ux, slot, ux->sel.kind, ux->sel.id);
}
int som_ux_quick_bind_menu(struct som_ux *ux, int slot) {
  if (ux->st.ring.category >= 4) return -1;
  return som_ux_quick_set(ux, slot, SOM_UX_BK_MENU, ux->sel.menu);
}

void som_ux_set_option(struct som_ux *ux, int id, int value, const char *text) {
  if (id < 0 || id >= SOM_UX_OPT_COUNT) return;
  ux->opt_val[id] = (uint8_t)value;
  snprintf(ux->opt_text[id], sizeof ux->opt_text[id], "%s", text ? text : "");
}
int som_ux_get_option(const struct som_ux *ux, int id) { return id >= 0 && id < SOM_UX_OPT_COUNT ? ux->opt_val[id] : -1; }
void som_ux_set_options_ext(struct som_ux *ux, int on) { ux->opt_ext = on ? 1 : 0; }
void som_ux_set_state_slot(struct som_ux *ux, int slot, int used, const char *time, const uint32_t *thumb) {
  if (slot < 0 || slot >= SOM_UX_STATE_SLOTS) return;
  ux->st_slot[slot].used = used && thumb ? 1 : 0;
  snprintf(ux->st_slot[slot].time, sizeof ux->st_slot[slot].time, "%s", time ? time : "");
  if (ux->st_slot[slot].used) memcpy(ux->st_thumb[slot], thumb, sizeof ux->st_thumb[slot]);
  ux->st_gen++;
}
void som_ux_set_state_sel(struct som_ux *ux, int slot) { if (slot >= 0 && slot < SOM_UX_STATE_SLOTS) ux->st_sel = (uint8_t)slot; }
int som_ux_state_sel(const struct som_ux *ux) { return ux->st_sel; }
void som_ux_set_credits(struct som_ux *ux, const char *text) { snprintf(ux->cr_text, sizeof ux->cr_text, "%s", text ? text : ""); ux->cr_scroll = 0; }
void som_ux_open_options(struct som_ux *ux, int page) {
  ux->opt_page = (uint8_t)(page >= 0 && page <= SOM_UX_OPT_CREDITS ? page : 0);
  if (ux->opt_page >= SOM_UX_OPT_STATES) ux->opt_from_main = 1;
  if (ux->opt_page == SOM_UX_OPT_BINDINGS) ux->opt_page = SOM_UX_OPT_MAIN;   /* the page was removed in round 5 */
  if (ux->opt_page == SOM_UX_OPT_QUICK) ux->opt_from_main = 0;
  if (ux->opt_page == SOM_UX_OPT_QUICK && ux->qe_slot >= SOM_UX_QUICK_N) ux->qe_slot = 0;
}
int som_ux_options_page(const struct som_ux *ux) { return ux->opt_page; }

int som_ux_info(const struct som_ux *ux, int what) {
  switch (what) {
  case 0: return ux->n_enemies;
  case 1: return ux->boss;
  case 2: return ux->fl_x;
  case 3: return ux->fl_y;
  case 4: return ux->fl_heading;
  case 5: return ux->fl_world_ok;
  case 6: return ux->st.hero[0].present + ux->st.hero[1].present + ux->st.hero[2].present;
  case 7: return ux->log_n;
  default: return -1;
  }
}
const char *som_ux_line(const struct som_ux *ux) { return ux->line; }
