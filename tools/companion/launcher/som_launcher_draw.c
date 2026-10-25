/* som_launcher_draw: renders the start menu in the Secret of Mana menu look (same palette, window style and game font as
 * tools/companion/ux/som_ui_draw.c; the primitives there are file-private, so the few needed here are repeated).  The graphics pack
 * is optional: without it a built-in 5x7 face is used.  Geometry comes from sl_layout(), the same function the touch handler uses. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "som_launcher.h"
#include "som_launcher_draw.h"

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

struct cv { uint32_t *px; int stride, W, H, s; };

/* Graphics pack of tools/companion/ux (format "SUXP" version 1, see build_assets.py); read here so the launcher does not depend on the UX library. */
struct pk_rect { uint16_t id, x, y, w, h; int16_t ox, oy; uint16_t adv; };
struct pack { struct pk_rect *r; int n, aw, ah; const uint8_t *rgba; };
static struct pack *g_pack;
static int g_audit; static char g_note[96];
static int pk_cmp(const void *a, const void *b) { return (int)((const struct pk_rect *)a)->id - (int)((const struct pk_rect *)b)->id; }
static int le16(const uint8_t *p) { return p[0] | (p[1] << 8); }
int sl_draw_load_pack(const uint8_t *b, size_t len) {
  sl_draw_free_pack();
  if (len < 12 || memcmp(b, "SUXP", 4) || le16(b + 4) != 1) return -1;
  int aw = le16(b + 6), ah = le16(b + 8), n = le16(b + 10);
  if ((size_t)12 + 16u * (unsigned)n + (size_t)aw * ah * 4 > len) return -1;
  struct pack *p = (struct pack *)calloc(1, sizeof *p);
  if (!p) return -1;
  p->r = (struct pk_rect *)malloc(sizeof(struct pk_rect) * (size_t)(n ? n : 1));
  if (!p->r) { free(p); return -1; }
  for (int i = 0; i < n; i++) {
    const uint8_t *q = b + 12 + 16 * i;
    p->r[i].id = (uint16_t)le16(q); p->r[i].x = (uint16_t)le16(q + 2); p->r[i].y = (uint16_t)le16(q + 4);
    p->r[i].w = (uint16_t)le16(q + 6); p->r[i].h = (uint16_t)le16(q + 8);
    p->r[i].ox = (int16_t)le16(q + 10); p->r[i].oy = (int16_t)le16(q + 12); p->r[i].adv = (uint16_t)le16(q + 14);
  }
  qsort(p->r, (size_t)n, sizeof(struct pk_rect), pk_cmp);
  p->aw = aw; p->ah = ah; p->n = n; p->rgba = b + 12 + 16 * n;
  g_pack = p;
  return 0;
}
void sl_draw_free_pack(void) { if (g_pack) { free(g_pack->r); free(g_pack); g_pack = NULL; } }
static const struct pk_rect *pk_find(const struct pack *p, int id) {
  if (!p) return NULL;
  struct pk_rect key;
  key.id = (uint16_t)id;
  return (const struct pk_rect *)bsearch(&key, p->r, (size_t)p->n, sizeof(struct pk_rect), pk_cmp);
}
static uint32_t pk_pixel(const struct pack *p, int x, int y) {
  const uint8_t *q = p->rgba + 4 * ((size_t)y * (size_t)p->aw + (size_t)x);
  return ((uint32_t)q[3] << 24) | ((uint32_t)q[0] << 16) | ((uint32_t)q[1] << 8) | q[2];
}
int sl_draw_audit(int reset) { int n = g_audit; if (reset) g_audit = 0; return n; }
const char *sl_draw_audit_note(void) { return g_note; }
static void audit(const char *what, const char *s) {
  if (!g_audit) snprintf(g_note, sizeof g_note, "%s: %.60s", what, s ? s : "");
  g_audit++;
}

static uint32_t mix(uint32_t a, uint32_t b, int t) {
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
static void vgrad(struct cv *c, int x, int y, int w, int h, uint32_t top, uint32_t bot) {
  for (int j = 0; j < h; j++) frect(c, x, y + j, w, 1, mix(top, bot, h > 1 ? j * 256 / (h - 1) : 0));
}
static void background(struct cv *c) {
  frect(c, 0, 0, c->W / c->s + 1, c->H / c->s + 1, C_BG);
  for (int y = 0; y * 8 < c->H / c->s + 8; y++)
    for (int x = 0; x * 8 < c->W / c->s + 8; x++)
      if (((x + y) & 1) == 0) frect(c, x * 8, y * 8, 8, 8, C_BG2);
}
/* style 0 normal, 1 selected, 2 dim */
static void win(struct cv *c, int x, int y, int w, int h, int style) {
  uint32_t lt = style == 1 ? C_YEL : (style == 2 ? C_GREY : C_LIGHT), mid = style == 1 ? C_ORG : (style == 2 ? 0x2A3050u : C_MID);
  uint32_t top = style == 1 ? C_WIN_SEL_T : (style == 2 ? 0x1A2250u : C_WIN_T), bot = style == 2 ? 0x0A0F30u : C_WIN_B;
  if (w < 6 || h < 6) { frect(c, x, y, w, h, top); return; }
  frect(c, x + 1, y, w - 2, 1, C_DARK); frect(c, x + 1, y + h - 1, w - 2, 1, C_DARK);
  frect(c, x, y + 1, 1, h - 2, C_DARK); frect(c, x + w - 1, y + 1, 1, h - 2, C_DARK);
  frect(c, x + 2, y + 1, w - 4, 1, lt); frect(c, x + 2, y + h - 2, w - 4, 1, lt);
  frect(c, x + 1, y + 2, 1, h - 4, lt); frect(c, x + w - 2, y + 2, 1, h - 4, lt);
  frect(c, x + 2, y + 2, w - 4, 1, mid); frect(c, x + 2, y + h - 3, w - 4, 1, mid);
  frect(c, x + 2, y + 3, 1, h - 6, mid); frect(c, x + w - 3, y + 3, 1, h - 6, mid);
  vgrad(c, x + 3, y + 3, w - 6, h - 6, top, bot);
}

/* ---- text: pack glyphs (id 0x0100 + ch, 8x11 game face) at an integer scale; hi-res faces when the pack has the scale */
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

static const uint8_t HR_SCALE[5] = {2, 3, 4, 6, 8};
static const struct pk_rect *hr_glyph(const struct cv *c, int ch, int mul) {
  if (!g_pack || c->s < 2 || ch < 32 || ch > 126) return NULL;
  int k = c->s * mul;
  for (int i = 0; i < 5; i++) if (HR_SCALE[i] == k) return pk_find(g_pack, 0x4000 + i * 0x200 + ch);
  return NULL;
}
static int hr_on(const struct cv *c) {
  if (!g_pack || c->s < 2) return 0;
  for (int i = 0; i < 5; i++) if (HR_SCALE[i] == c->s && pk_find(g_pack, 0x4000 + i * 0x200 + 'H')) return 1;
  return 0;
}
static int adv_dev(const struct cv *c, int ch, int mul) {          /* advance in device pixels */
  const struct pk_rect *r = hr_glyph(c, ch, mul);
  if (r) return r->adv < 1 ? 1 : r->adv;
  const struct pk_rect *g = g_pack ? pk_find(g_pack, 0x0100 + ch) : NULL;
  int a = g ? (g->adv ? g->adv : g->w + 1) : 6;
  return a * c->s * mul;
}
static int text_w(const struct cv *c, const char *s, int mul) {     /* vp, rounded up */
  int w = 0;
  for (; *s; s++) w += adv_dev(c, (unsigned char)*s, mul);
  return (w + c->s - 1) / c->s;
}
static void builtin_glyph(struct cv *c, int x, int y, int ch, uint32_t col, int mul) {
  const uint8_t *cols = NULL;
  if (ch >= 'a' && ch <= 'z') ch -= 32;
  for (unsigned f = 0; f < sizeof font5x7 / sizeof font5x7[0]; f++) if (font5x7[f].c == ch) { cols = font5x7[f].col; break; }
  if (!cols) return;
  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 7; j++)
      if (cols[i] & (1 << j)) {
        int px = x + (i + 1) * mul, py = y + (j + 1) * mul;
        frect(c, px - mul, py, mul, mul, C_DARK); frect(c, px + mul, py, mul, mul, C_DARK);
        frect(c, px, py - mul, mul, mul, C_DARK); frect(c, px, py + mul, mul, mul, C_DARK);
      }
  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 7; j++)
      if (cols[i] & (1 << j)) frect(c, x + (i + 1) * mul, y + (j + 1) * mul, mul, mul, col);
}
static void game_glyph(struct cv *c, int x, int y, int ch, uint32_t col, int mul) {
  const struct pk_rect *r = g_pack ? pk_find(g_pack, 0x0100 + ch) : NULL;
  if (!r) { builtin_glyph(c, x, y, ch, col, mul); return; }
  const struct pk_rect *ref = pk_find(g_pack, 0x0100 + 'H');
  int top = ref ? ref->oy : 0, s = c->s * mul, ox = x * c->s, oy = y * c->s + (r->oy - top) * s;
  for (int j = 0; j < r->h; j++)
    for (int i = 0; i < r->w; i++) {
      uint32_t p = pk_pixel(g_pack, r->x + i, r->y + j);
      if ((p >> 24) < 128) continue;
      int lum = (int)(((p >> 16) & 255) + ((p >> 8) & 255) + (p & 255)) / 3;
      uint32_t cc = lum < 100 ? C_DARK : col;
      for (int b = 0; b < s; b++) for (int a = 0; a < s; a++) dpx(c, ox + (r->ox + i) * s + a, oy + j * s + b, cc);
    }
}
static void hr_draw(struct cv *c, int px_x, int y, const struct pk_rect *r, uint32_t col) {
  int gx = px_x + r->ox, gy = y * c->s + r->oy;
  for (int j = 0; j < r->h; j++)
    for (int i = 0; i < r->w; i++) {
      uint32_t p = pk_pixel(g_pack, r->x + i, r->y + j);
      int a = (int)(p >> 24), X = gx + i, Y = gy + j;
      if (!a || (unsigned)X >= (unsigned)c->W || (unsigned)Y >= (unsigned)c->H) continue;
      uint32_t cc = mix(C_DARK, col, (int)((p >> 8) & 255) * 256 / 255);
      uint32_t *d = &c->px[Y * c->stride + X];
      *d = a >= 255 ? cc : mix(*d, cc, a * 256 / 255);
    }
}
static int text(struct cv *c, int x, int y, const char *s, uint32_t col, int mul) {
  int pen = x * c->s, hr = hr_on(c);
  for (; *s; s++) {
    /* the hi-res face at every multiplier the pack was rasterised for, the pixel face otherwise */
    const struct pk_rect *r = hr ? hr_glyph(c, (unsigned char)*s, mul) : NULL;
    if (r) { if (r->w > 1 || r->h > 1) hr_draw(c, pen, y, r, col); }
    else game_glyph(c, pen / c->s, y, (unsigned char)*s, col, mul);
    pen += adv_dev(c, (unsigned char)*s, mul);
  }
  return (pen - x * c->s + c->s - 1) / c->s;
}
/* text cut to maxw vp; counts a cut in the audit */
static void text_fit(struct cv *c, int x, int y, int maxw, const char *s, uint32_t col, int mul) {
  char b[SL_TEXT + 4];
  int n = 0, w = 0;
  for (; s[n] && n < SL_TEXT; n++) {
    int a = adv_dev(c, (unsigned char)s[n], mul);
    if (w + a > maxw * c->s) break;
    w += a; b[n] = s[n];
  }
  b[n] = 0;
  if (s[n]) audit("text cut", s);
  text(c, x, y, b, col, mul);
}
static int g_ellipsis;
int sl_draw_ellipsized(int reset) { int n = g_ellipsis; if (reset) g_ellipsis = 0; return n; }
/* text that must fit: cut with ".." and counted (a long label next to a long value); the detail panel shows the full pair */
static void text_ellipsis(struct cv *c, int x, int y, int maxw, const char *s, uint32_t col) {
  char b[SL_TEXT + 4];
  snprintf(b, sizeof b, "%s", s);
  if (text_w(c, b, 1) > maxw) {
    size_t n = strlen(b);
    while (n > 2 && text_w(c, b, 1) + text_w(c, "..", 1) > maxw) b[--n] = 0;
    strcat(b, "..");
    g_ellipsis++;
  }
  text(c, x, y, b, col, 1);
}
static void text_center(struct cv *c, int x, int w, int y, const char *s, uint32_t col, int mul) {
  int tw = text_w(c, s, mul);
  if (tw > w) { audit("text wider than its box", s); tw = w; }
  text(c, x + (w - tw) / 2, y, s, col, mul);
}
static int wrap(struct cv *c, int x, int y, int w, const char *s, uint32_t col, int maxlines) {
  int lines = 0;
  char b[SL_TEXT + 4];
  while (*s && lines < maxlines) {
    int n = 0, lw = 0, brk = -1;
    while (s[n] && n < SL_TEXT) {
      int a = adv_dev(c, (unsigned char)s[n], 1);
      if (lw + a > w * c->s) break;
      lw += a; if (s[n] == ' ') brk = n; n++;
    }
    int take = n;
    if (s[n] && brk > 0) take = brk;
    memcpy(b, s, (size_t)take); b[take] = 0;
    s += take; while (*s == ' ') s++;
    if (*s && lines == maxlines - 1) {                          /* more text than room: end the last line with ".." */
      while (take > 2 && text_w(c, b, 1) + text_w(c, "..", 1) > w) b[--take] = 0;
      strcat(b, "..");
    }
    text(c, x, y + lines * 11, b, col, 1);
    lines++;
  }
  return lines;
}

static void tri(struct cv *c, int cx, int cy, int dir, uint32_t col) {   /* filled triangle 4 vp wide, 7 tall, tip towards dir */
  for (int i = 0; i < 4; i++) {
    int hgt = 7 - 2 * i, x = dir > 0 ? cx - 2 + i : cx + 1 - i;
    frect(c, x, cy - hgt / 2, 1, hgt, col);
  }
}
static void arrow(struct cv *c, int cx, int cy, int dir, uint32_t col) {
  tri(c, cx + 1, cy + 1, dir, C_DARK);
  tri(c, cx, cy, dir, col);
}

static void button(struct cv *c, const struct sl_rect *r, const char *label, int style, uint32_t col) {
  win(c, r->x, r->y, r->w, r->h, style);
  int mul = 1;
  text_center(c, r->x + 3, r->w - 6, r->y + (r->h - 11) / 2, label, col, mul);
}

/* ---- screens */
static void header(struct cv *c, const struct sl_metrics *m, const char *title, const char *sub) {
  win(c, m->col_x, 1, m->col_w, m->head_h - 3, 0);
  text_center(c, m->col_x + 4, m->col_w - 8, sub ? 3 : (m->head_h - 3 - 11) / 2 + 1, title, C_TXT, 1);
  if (sub) text_center(c, m->col_x + 4, m->col_w - 8, 16, sub, C_DIM, 1);
}

static void footer(struct cv *c, const struct sl_launcher *L, const struct sl_metrics *m, const struct sl_hit *hits, int n) {
  int fy = m->H - m->foot_h + 2;
  for (int i = 0; i < n; i++) {
    const struct sl_hit *h = &hits[i];
    if (h->kind == SH_BACK) button(c, &h->r, "BACK", L->focus == SF_BACK, C_TXT);
    else if (h->kind == SH_GO) button(c, &h->r, "GO", L->focus == SF_GO, L->gen_ok == 0 ? C_GREY : C_GRN);
  }
  /* scroll arrows are drawn as plain triangles: up and down */
  for (int i = 0; i < n; i++) {
    const struct sl_hit *h = &hits[i];
    if (h->kind == SH_SCROLL_UP || h->kind == SH_SCROLL_DOWN) {
      int cx = h->r.x + h->r.w / 2, cy = h->r.y + h->r.h / 2, up = h->kind == SH_SCROLL_UP;
      win(c, h->r.x, h->r.y, h->r.w, h->r.h, L->focus == (up ? SF_UP : SF_DOWN));
      for (int k = 0; k < 4; k++) frect(c, cx - k, cy + (up ? k - 2 : 2 - k), 2 * k + 1, 1, C_TXT);
    }
  }
  int x0 = m->col_x + 48, x1 = m->col_x + m->col_w - (L->screen == SL_TWEAK ? 102 : 56);
  if (L->screen != SL_PRESETS && L->screen != SL_TWEAK && L->screen != SL_MULTI) x1 = m->col_x + m->col_w;
  if (L->notice[0]) text_fit(c, x0, fy + 3, x1 - x0, L->notice, C_YEL, 1);
  else if (L->screen == SL_PRESETS || L->screen == SL_TWEAK || L->screen == SL_MULTI) {
    char b[24];
    snprintf(b, sizeof b, "%d/%d", L->cursor + 1, L->nrows);
    text_fit(c, x0, fy + 3, x1 - x0, b, C_DIM, 1);
  }
}

static void draw_main(struct cv *c, const struct sl_launcher *L, const struct sl_metrics *m, const struct sl_hit *hits, int n) {
  header(c, m, "SECRET OF MANA", NULL);
  for (int i = 0; i < n; i++) {
    const struct sl_hit *h = &hits[i];
    int sel = 0;
    const char *label = "", *sub = "";
    char nb[32];
    for (int k = 0; k < sl_main_count(L); k++) if (sl_main_kind(L, k) == h->kind && L->cursor == k) sel = 1;
    switch (h->kind) {
      case SH_MAIN_ORIGINAL: label = "SoM Original"; sub = L->status_original; break;
      case SH_MAIN_RANDOM: label = "Create Randomized"; sub = L->status_generator; break;
      case SH_MAIN_SAVED:
        label = "Saved Randomized";
        { int k = sl_saved_count(L); if (k) snprintf(nb, sizeof nb, "%d of %d saved", k, SL_SAVED_MAX); else snprintf(nb, sizeof nb, "No saved games yet"); }
        sub = nb;
        break;
      case SH_MAIN_TWEAKS: label = "Gameplay Tweaks"; sub = L->status_tweaks; break;
      case SH_MAIN_DISPLAY: label = "Display"; break;
      case SH_MAIN_CREDITS: label = "Credits"; break;
      default: continue;
    }
    win(c, h->r.x, h->r.y, h->r.w, h->r.h, sel ? 1 : 0);
    int mul = text_w(c, label, 2) <= h->r.w - 12 && h->r.h >= (sub[0] ? 40 : 24) ? 2 : 1;
    int th = 11 * mul, ty = h->r.y + (sub[0] ? (h->r.h - th - 13) / 2 : (h->r.h - th) / 2);
    text_center(c, h->r.x + 6, h->r.w - 12, ty, label, C_TXT, mul);
    if (sub[0]) {
      char b[SL_TEXT];
      snprintf(b, sizeof b, "%s", sub);
      int mw = h->r.w - 12;
      while (text_w(c, b, 1) > mw && strlen(b) > 1) b[strlen(b) - 1] = 0;
      if (strcmp(b, sub)) audit("status cut", sub);
      text_center(c, h->r.x + 6, h->r.w - 12, ty + th + 3, b, C_DIM, 1);
    }
  }
  int fy = m->H - m->foot_h + 4;
  text_center(c, m->col_x, m->col_w, fy, "Randomizer by Moppleton, LGPL-2.1", C_DIM, 1);
}

static void draw_credits(struct cv *c, const struct sl_launcher *L, const struct sl_metrics *m, const struct sl_hit *hits, int n) {
  int total = sl_credits_count(L), vis = L->cred_vis > 0 ? L->cred_vis : 8, first = L->cred_scroll, y0 = m->head_h + 3, bh = vis * 11 + 4;
  char line[SL_TEXT];
  if (first > total - vis) first = total - vis;
  if (first < 0) first = 0;
  header(c, m, "CREDITS", NULL);
  win(c, m->col_x, y0, m->col_w, bh, 2);
  for (int i = 0; i < vis && first + i < total; i++) {
    if (!sl_credits_line(L, first + i, line, sizeof line) || !line[0]) continue;
    text_fit(c, m->col_x + 6, y0 + 3 + i * 11, m->col_w - 16, line, i + first < 1 ? C_YEL : C_TXT, 1);
  }
  if (total > vis) {                                          /* scroll bar */
    int th = bh * vis / total, ty = y0 + (bh - th) * first / (total - vis);
    frect(c, m->col_x + m->col_w - 8, y0 + 3, 3, bh - 6, C_MID);
    frect(c, m->col_x + m->col_w - 8, ty + 3, 3, th > 4 ? th - 6 : 2, C_YEL);
  }
  for (int i = 0; i < n; i++) {
    const struct sl_hit *h = &hits[i];
    if (h->kind == SH_BACK) button(c, &h->r, "BACK", L->focus == SF_BACK, C_TXT);
    else if (h->kind == SH_SCROLL_UP || h->kind == SH_SCROLL_DOWN) {
      int cx = h->r.x + h->r.w / 2, cy = h->r.y + h->r.h / 2, up = h->kind == SH_SCROLL_UP;
      win(c, h->r.x, h->r.y, h->r.w, h->r.h, L->focus == (up ? SF_UP : SF_DOWN));
      for (int k = 0; k < 4; k++) frect(c, cx - k, cy + (up ? k - 2 : 2 - k), 2 * k + 1, 1, C_TXT);
    }
  }
}

static void draw_list(struct cv *c, const struct sl_launcher *L, const struct sl_metrics *m, const struct sl_hit *hits, int n) {
  const char *title = L->screen == SL_PRESETS ? "CREATE RANDOMIZED" : (L->screen == SL_MULTI ? sl_opt(L, L->multi_opt)->label :
                      (L->cfg.preset >= 0 ? sl_presets[L->cfg.preset].name : "Custom"));
  const char *sub = L->screen == SL_PRESETS ? "Choose a preset" : (L->screen == SL_TWEAK ? sl_modes[L->cfg.mode].name : "Choose at least one");
  header(c, m, title, sub);
  int rows = L->vis_rows > 0 ? L->vis_rows : m->rows;
  /* headers have no hit: draw them from the row list */
  for (int i = 0; i < rows && L->scroll + i < L->nrows; i++) {
    const struct sl_row *r = &L->rows[L->scroll + i];
    int y = m->head_h + i * m->row_h;
    if (r->kind == SR_HEADER) {
      char lab[SL_TEXT], val[SL_TEXT];
      sl_row_texts(L, r, lab, sizeof lab, val, sizeof val);
      frect(c, m->col_x + 4, y + m->row_h - 4, m->col_w - 8, 1, C_GREY);
      text_fit(c, m->col_x + 6, y + 4, m->col_w - 12, lab, C_CYAN, 1);
    }
  }
  for (int i = 0; i < n; i++) {
    const struct sl_hit *h = &hits[i];
    if (h->kind != SH_ROW) continue;
    const struct sl_row *r = &L->rows[h->a];
    char lab[SL_TEXT], val[SL_TEXT];
    sl_row_texts(L, r, lab, sizeof lab, val, sizeof val);
    int sel = h->a == L->cursor && L->focus == SF_LIST, y = h->r.y + (h->r.h - 11) / 2;
    win(c, h->r.x, h->r.y, h->r.w, h->r.h, sel ? 1 : 0);
    uint32_t col = (r->kind == SR_ACTION && r->a == SA_GENERATE) ? C_GRN : C_TXT;
    int has_val = val[0] != 0;
    int arrows = L->screen == SL_TWEAK && (r->kind == SR_OPT || r->kind == SR_DIFF) && !(r->kind == SR_OPT && sl_opt(L, r->a)->type == SL_O_MULTI);
    int xr = h->r.x + h->r.w - (arrows ? 56 : 6), vw = has_val ? text_w(c, val, 1) : 0;
    if (vw > 150) vw = 150;
    if (has_val) text_ellipsis(c, xr - vw, y, vw, val, (r->kind == SR_MULTI_ITEM && !strcmp(val, "No")) || r->kind == SR_PRESET ? C_GREY : C_YEL);
    text_ellipsis(c, h->r.x + 6, y, xr - vw - (has_val ? 8 : 0) - (h->r.x + 6), lab, col);
  }
  for (int i = 0; i < n; i++) {
    const struct sl_hit *h = &hits[i];
    if (h->kind == SH_ROW_DEC) arrow(c, h->r.x + 11, h->r.y + h->r.h / 2, -1, C_LIGHT);
    else if (h->kind == SH_ROW_INC) arrow(c, h->r.x + 11, h->r.y + h->r.h / 2, 1, C_LIGHT);
  }
  /* detail panel */
  int dy = m->head_h + rows * m->row_h + 1, dh = m->H - m->foot_h - dy - 1;
  if (dh >= 14 && L->screen != SL_MULTI) {
    win(c, m->col_x, dy, m->col_w, dh, 2);
    const char *d = "";
    char tmp[SL_TEXT * 2];
    if (L->cursor < L->nrows) {
      const struct sl_row *r = &L->rows[L->cursor];
      if (L->screen == SL_PRESETS && r->kind == SR_PRESET)
        d = r->a < 0 ? "A random preset with random settings and a random seed." : sl_presets[r->a].desc;
      else if (r->kind == SR_OPT) {
        const struct sl_option *o = sl_opt(L, r->a);
        d = o->help[0] ? o->help : o->group;
        if (o->type == SL_O_ENUM && o->values[L->cfg.val[r->a]].full[0]) { snprintf(tmp, sizeof tmp, "%s: %s", o->label, o->values[L->cfg.val[r->a]].full); d = tmp; }
      }
      else if (r->kind == SR_ACTION && r->a == SA_GENERATE) { snprintf(tmp, sizeof tmp, "Make the ROM from your own game file and play it."); d = tmp; }
      else if (r->kind == SR_ACTION && r->a == SA_SEED) d = "The same seed and settings always make the same game.";
      else if (r->kind == SR_ACTION && r->a == SA_RANDOM_ALL) d = "Pick every setting at random. The seed stays.";
      else if (r->kind == SR_ACTION && r->a == SA_RESET) d = "Back to the values of the chosen preset.";
      else if (r->kind == SR_DIFF) d = "Difficulty preset of the generator.";
      else if (r->kind == SR_TOGGLE_ADV) d = "Show or hide the less common options.";
    }
    wrap(c, m->col_x + 6, dy + 4, m->col_w - 12, d, C_DIM, dh >= 26 ? 2 : 1);
  }
}

static void draw_saved(struct cv *c, const struct sl_launcher *L, const struct sl_metrics *m, const struct sl_hit *hits, int n) {
  header(c, m, "SAVED RANDOMIZED", "Up to four games, newest first");
  for (int i = 0; i < n; i++) {
    const struct sl_hit *h = &hits[i];
    if (h->kind == SH_SAVED_SLOT) {
      const struct sl_saved *v = &L->saved[h->a];
      int sel = L->cursor == h->a && L->focus != SF_BACK && L->focus != SF_DEL;
      win(c, h->r.x, h->r.y, h->r.w, h->r.h, v->used ? (sel ? 1 : 0) : 2);
      char b[SL_TEXT * 2];
      if (!v->used) { snprintf(b, sizeof b, "Slot %d  -  empty", h->a + 1); text_fit(c, h->r.x + 6, h->r.y + (h->r.h - 11) / 2, h->r.w - 12, b, C_GREY, 1); continue; }
      const char *pn = v->preset[0] ? v->preset : v->mode;
      for (int k = 0; k < sl_npresets; k++) if (!strcmp(sl_presets[k].id, v->preset)) pn = sl_presets[k].name;
      snprintf(b, sizeof b, "%d  %s", h->a + 1, pn);
      text_fit(c, h->r.x + 6, h->r.y + 4, h->r.w - 12, b, C_TXT, 1);
      snprintf(b, sizeof b, "Seed %s   %s", v->seed, v->date);
      text_fit(c, h->r.x + 6, h->r.y + 4 + 13, h->r.w - 12, b, C_YEL, 1);
    } else if (h->kind == SH_SAVED_DEL) {
      int sure = L->del_confirm == h->a + 1;
      button(c, &h->r, sure ? "SURE?" : "DELETE", L->focus == SF_DEL && L->cursor == h->a, sure ? C_RED : C_DIM);
    } else if (h->kind == SH_BACK) button(c, &h->r, "BACK", L->focus == SF_BACK, C_TXT);
  }
  int fy = m->H - m->foot_h + 2, x0 = m->col_x + 48;
  if (L->notice[0]) text_fit(c, x0, fy + 3, m->col_x + m->col_w - x0, L->notice, C_YEL, 1);
  else if (L->saved_hidden > 0) { char b[48]; snprintf(b, sizeof b, "%d older not shown", L->saved_hidden); text_fit(c, x0, fy + 3, m->col_x + m->col_w - x0, b, C_DIM, 1); }
}

static void draw_seed(struct cv *c, const struct sl_launcher *L, const struct sl_metrics *m, const struct sl_hit *hits, int n) {
  header(c, m, "SEED", "Same seed and settings, same game");
  char digits[SL_SEED_DIGITS + 1];
  snprintf(digits, sizeof digits, "%0*ld", SL_SEED_DIGITS, atol(L->cfg.seed));
  int lead = 0;
  while (lead < SL_SEED_DIGITS - 1 && digits[lead] == '0') lead++;
  for (int i = 0; i < n; i++) {
    const struct sl_hit *h = &hits[i];
    char b[2] = {0, 0};
    if (h->kind == SH_SEED_DIGIT) {
      b[0] = digits[h->a];
      win(c, h->r.x, h->r.y, h->r.w, h->r.h, h->a == L->seed_pos ? 1 : 0);
      text_center(c, h->r.x, h->r.w, h->r.y + (h->r.h - 22) / 2, b, h->a < lead ? C_GREY : C_TXT, 2);
    } else if (h->kind == SH_KEY) {
      const char *lab = b;
      uint32_t col = C_TXT;
      if (h->a <= 9) b[0] = (char)('0' + h->a);
      else if (h->a == 10) lab = "DEL";
      else if (h->a == 11) { lab = "RANDOM SEED"; col = C_YEL; }
      else { lab = "OK"; col = C_GRN; }
      button(c, &h->r, lab, 0, col);
    } else if (h->kind == SH_BACK) button(c, &h->r, "BACK", 0, C_TXT);
  }
}

static void draw_generate(struct cv *c, const struct sl_launcher *L, const struct sl_metrics *m, const struct sl_hit *hits, int n) {
  header(c, m, L->gen == SL_GEN_RUNNING ? "GENERATING" : (L->gen == SL_GEN_ASK ? "PREPARING" :
         (L->gen == SL_GEN_UNAVAILABLE ? "NOT AVAILABLE" : (L->gen == SL_GEN_ERROR ? "FAILED" : "NEW GAME"))), NULL);
  char key[17], b[SL_TEXT * 2];
  sl_config_key(L, key);
  int y = m->head_h + 6;
  win(c, m->col_x, y, m->col_w, 46, 0);
  snprintf(b, sizeof b, "%s", L->cfg.preset >= 0 ? sl_presets[L->cfg.preset].name : "Custom");
  text_fit(c, m->col_x + 8, y + 5, m->col_w - 16, b, C_TXT, 1);
  snprintf(b, sizeof b, "Seed %s", L->cfg.seed);
  text_fit(c, m->col_x + 8, y + 17, m->col_w - 16, b, C_YEL, 1);
  snprintf(b, sizeof b, "Key %.8s", key);
  text_fit(c, m->col_x + 8, y + 29, m->col_w - 16, b, C_DIM, 1);
  y += 54;
  if (L->gen == SL_GEN_RUNNING || L->gen == SL_GEN_ASK) {
    int bw = m->col_w - 16, bx = m->col_x + 8, pct = L->gen_total > 0 ? L->gen_step * 100 / L->gen_total : 0;
    win(c, bx, y, bw, 14, 2);
    if (pct > 0) frect(c, bx + 3, y + 3, (bw - 6) * pct / 100, 8, C_GRN);
    snprintf(b, sizeof b, "%d%%", pct);
    text_center(c, bx, bw, y + 14 + 4, b, C_TXT, 1);
    text_fit(c, bx, y + 14 + 17, bw, L->gen_name[0] ? L->gen_name : (L->gen == SL_GEN_ASK ? "Looking for a finished ROM" : "Starting"), C_DIM, 1);
  } else {
    wrap(c, m->col_x + 8, y, m->col_w - 16, L->gen_msg, L->gen == SL_GEN_ERROR ? C_RED : C_ORG, 6);
  }
  for (int i = 0; i < n; i++) {
    const struct sl_hit *h = &hits[i];
    if (h->kind == SH_CANCEL) button(c, &h->r, "CANCEL", 0, C_TXT);
    else if (h->kind == SH_RETRY) button(c, &h->r, "RETRY", 0, C_YEL);
    else if (h->kind == SH_BACK) button(c, &h->r, "BACK", 0, C_TXT);
  }
}

static void draw_play(struct cv *c, const struct sl_launcher *L, const struct sl_metrics *m) {
  header(c, m, "STARTING", NULL);
  win(c, m->col_x, m->head_h + 20, m->col_w, 50, 1);
  text_center(c, m->col_x + 6, m->col_w - 12, m->head_h + 28, "Starting the game", C_TXT, 1);
  const char *p = L->rom_path, *base = strrchr(p, '/');
  text_center(c, m->col_x + 6, m->col_w - 12, m->head_h + 46, base ? base + 1 : p, C_DIM, 1);
}

void sl_draw_label(uint32_t *px, int stride, int w, int h, int rx, int ry, int rw, int rh, const char *label, uint32_t col) {
  struct sl_metrics m;
  sl_metrics_for(w, h, &m);
  struct cv c = {px, stride, w, h, m.s};
  text_center(&c, rx / m.s, rw / m.s, (ry + (rh - 11 * m.s) / 2) / m.s, label, col, 1);
}

void sl_draw(const struct sl_launcher *L, uint32_t *px, int stride, int w, int h) {
  struct cv c = {px, stride, w, h, 1};
  struct sl_metrics m;
  struct sl_hit hits[SL_MAX_HITS];
  sl_metrics_for(w, h, &m);
  c.s = m.s;
  int n = sl_layout(L, &m, hits, SL_MAX_HITS);
  background(&c);
  switch (L->screen) {
    case SL_MAIN: draw_main(&c, L, &m, hits, n); break;
    case SL_PRESETS: case SL_TWEAK: case SL_MULTI: draw_list(&c, L, &m, hits, n); footer(&c, L, &m, hits, n); break;
    case SL_SEED: draw_seed(&c, L, &m, hits, n); break;
    case SL_GENERATE: draw_generate(&c, L, &m, hits, n); break;
    case SL_PLAY: draw_play(&c, L, &m); break;
    case SL_SAVED: draw_saved(&c, L, &m, hits, n); break;
    case SL_CREDITS: draw_credits(&c, L, &m, hits, n); break;
    default: break;
  }
}

/* ================================================================== top screen
 * The second screen of the Thor.  The start menu is driven on the bottom screen; the top one shows a procedural Secret of Mana style wallpaper
 * on the main menu (drawn here, no game asset involved) and, while a game is being set up, a summary of what is going to be created:
 * preset, seed, party, goal and every option in effect.  Everything shown is read from the configuration model; what the configuration does not
 * say (HP, MP, equipment of the characters) is stated as such, never guessed. */
static uint32_t lcg(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s >> 8; }

static void blend_px(struct cv *c, int x, int y, uint32_t col, int a) {
  if ((unsigned)x >= (unsigned)c->W || (unsigned)y >= (unsigned)c->H || a <= 0) return;
  uint32_t *d = &c->px[y * c->stride + x];
  *d = mix(*d, col, a > 255 ? 255 : a);
}
static void disc(struct cv *c, int cx, int cy, int r, uint32_t col) {          /* vp coordinates */
  for (int j = -r; j <= r; j++) {
    int hw = 0;
    while ((hw + 1) * (hw + 1) + j * j <= r * r) hw++;
    if (j * j <= r * r) frect(c, cx - hw, cy + j, 2 * hw + 1, 1, col);
  }
}
static void glow(struct cv *c, int cx, int cy, int r, uint32_t col, int peak) {   /* soft radial tint over what is already there, device pixels */
  int s = c->s;
  for (int y = (cy - r) * s; y < (cy + r) * s; y++)
    for (int x = (cx - r) * s; x < (cx + r) * s; x++) {
      long dx = x - cx * s, dy = y - cy * s, d2 = dx * dx + dy * dy, r2 = (long)r * s * r * s;
      if (d2 < r2) { int f = (int)(256 - d2 * 256 / r2); blend_px(c, x, y, col, f * f / 256 * peak / 256); }
    }
}
static int hill_y(int x, int base, int amp, int f1, int f2, int ph) {
  return base + (int)(amp * sin((x + ph) * f1 / 1000.0) + amp * 0.5 * sin((x + ph * 3) * f2 / 1000.0));
}

static void wallpaper(struct cv *c, int dark) {
  int W = c->W / c->s + 1, H = c->H / c->s + 1;
  /* twilight sky in 3 vp bands */
  static const uint32_t stops[4] = {0x090C34u, 0x1E2A74u, 0x6A4A9Au, 0xF0A47Cu};
  for (int y = 0; y < H; y += 3) {
    int t = y * 1000 / (H * 78 / 100 > 0 ? H * 78 / 100 : 1); if (t > 999) t = 999;
    int seg = t / 334, k = (t % 334) * 256 / 334;
    frect(c, 0, y, W, 3, mix(stops[seg], stops[seg + 1], k));
  }
  uint32_t r = 12345;
  for (int i = 0; i < 140; i++) {                                              /* stars */
    int x = (int)(lcg(&r) % (unsigned)W), y = (int)(lcg(&r) % (unsigned)(H * 55 / 100)), b = (int)(lcg(&r) % 3);
    frect(c, x, y, 1, 1, b == 0 ? 0xFFFFFFu : (b == 1 ? 0xB8C8FFu : 0x8090D8u));
    if (b == 0 && (i & 3) == 0) { frect(c, x - 1, y, 3, 1, 0xB8C8FFu); frect(c, x, y - 1, 1, 3, 0xB8C8FFu); }
  }
  int cx = W / 2, ground = H * 80 / 100, top = H * 24 / 100, cr = H * 24 / 100;
  glow(c, cx, top + cr, H * 58 / 100, 0x80F0C0u, 150);                         /* mana glow behind the tree */
  /* far hills, then the tree, then the near hills */
  for (int x = 0; x < W; x++) { int y = hill_y(x, ground - H / 12, H / 30, 21, 47, 130); frect(c, x, y, 1, H - y, 0x2A2E6Au); }
  for (int x = 0; x < W; x++) { int y = hill_y(x, ground - H / 40, H / 36, 13, 37, 40); frect(c, x, y, 1, H - y, 0x1B3F58u); }
  /* trunk: tapered, widening into roots */
  for (int y = top + cr; y <= ground + 2; y++) {
    int t = y - (top + cr), len = ground - (top + cr), hw = H / 40 + t * (H / 30) / (len > 0 ? len : 1) * 2 / 3;
    if (y > ground - H / 14) hw += (y - (ground - H / 14)) * 2 / 3;
    frect(c, cx - hw, y, 2 * hw, 1, 0x4A2E26u);
    frect(c, cx - hw, y, hw / 2 + 1, 1, 0x6A4636u);
    frect(c, cx + hw - hw / 3, y, hw / 3, 1, 0x34201Cu);
  }
  /* canopy: three tones, the light one offset up and left */
  static const struct { int dx, dy, r; } lobes[] = {{-9, 3, 7}, {-5, -3, 8}, {0, -6, 9}, {5, -3, 8}, {9, 3, 7}, {-4, 4, 8}, {4, 4, 8}, {0, 1, 9}, {-12, 9, 5}, {12, 9, 5}, {0, 8, 7}};
  for (int pass = 0; pass < 3; pass++) {
    uint32_t col = pass == 0 ? 0x155A4Au : (pass == 1 ? 0x2A9A6Cu : 0x86E0A8u);
    int off = pass == 0 ? 0 : (pass == 1 ? -cr / 14 : -cr / 6), shrink = pass == 0 ? 0 : (pass == 1 ? cr / 18 : cr / 6);
    for (unsigned i = 0; i < sizeof lobes / sizeof lobes[0]; i++) {
      int rr = lobes[i].r * cr / 22 - shrink;
      if (rr > 1) disc(c, cx + lobes[i].dx * cr / 22 + off, top + cr + lobes[i].dy * cr / 22 + off, rr, col);
    }
  }
  for (int i = 0; i < 46; i++) {                                               /* mana motes among the leaves and drifting up */
    int a = (int)(lcg(&r) % 360), d = (int)(lcg(&r) % (unsigned)(cr * 3 / 2));
    int x = cx + (int)(cos(a * 0.01745) * d), y = top + cr + (int)(sin(a * 0.01745) * d * 0.8) - cr / 4;
    if (y > ground - 4) continue;
    frect(c, x, y, 1, 1, 0xFFFFE0u);
    if (i % 3 == 0) { frect(c, x - 1, y, 3, 1, 0xC8FFE8u); frect(c, x, y - 1, 1, 3, 0xC8FFE8u); }
  }
  /* the Mana Sword in the grass to the left of the tree */
  int sx = cx - H * 30 / 100, sy = ground - H / 30;
  frect(c, sx, sy - H * 14 / 100, 3, H * 14 / 100, 0xE8F0FFu); frect(c, sx + 1, sy - H * 14 / 100, 1, H * 14 / 100, 0xB8C8F0u);
  frect(c, sx - 4, sy - H * 14 / 100 - 2, 11, 2, 0xE8C050u); frect(c, sx, sy - H * 14 / 100 - 7, 3, 5, 0xB89030u);
  glow(c, sx + 1, sy - H * 8 / 100, H / 10, 0xB8E8FFu, 110);
  /* near hills */
  for (int x = 0; x < W; x++) { int y = hill_y(x, ground + H / 14, H / 40, 17, 53, 300); frect(c, x, y, 1, H - y, 0x0E2A34u); }
  for (int i = 0; i < 70; i++) {                                               /* grass tufts and fireflies */
    int x = (int)(lcg(&r) % (unsigned)W), y = ground + H / 16 + (int)(lcg(&r) % (unsigned)(H / 8));
    if (y < H) frect(c, x, y, 1, 2, 0x2A6A58u);
    if (i % 6 == 0) { frect(c, x, y - H / 9, 2, 2, 0xE8F8A0u); blend_px(c, x * c->s, (y - H / 9) * c->s, 0xE8F8A0u, 90); }
  }
  if (dark) { for (int y = 0; y < c->H; y++) for (int x = 0; x < c->W; x++) blend_px(c, x, y, 0x050818u, dark); }
}

static void title_big(struct cv *c, int W, int y) {
  const char *t = "SECRET OF MANA";
  int mul = 4;
  while (mul > 1 && text_w(c, t, mul) > W - 40) mul--;
  int tw = text_w(c, t, mul), x = (W - tw) / 2;
  for (int k = 2; k >= 0; k--) { text(c, x + k, y + k, t, k == 0 ? C_YEL : C_DARK, mul); }
  const char *sub = "DUAL SCREEN COMPANION";
  text_center(c, 0, W, y + 11 * mul + 6, sub, C_LIGHT, 1);
}

/* ---- the summary */
static int opt_text(const struct sl_launcher *L, const char *key, char *b, size_t n, int full) {
  int i = sl_find_opt(L, key);
  if (i < 0 || !sl_opt_visible(L, i)) return 0;
  const struct sl_option *o = sl_opt(L, i);
  if (full && o->type == SL_O_ENUM && o->values[L->cfg.val[i]].full[0]) snprintf(b, n, "%s", o->values[L->cfg.val[i]].full);
  else sl_value_text(L, i, b, n);
  return 1;
}
static int opt_yes(const struct sl_launcher *L, const char *key) {          /* 1 yes, 0 no, -1 not in this mode */
  int i = sl_find_opt(L, key);
  if (i < 0 || !sl_opt_visible(L, i)) return -1;
  return L->cfg.val[i] != 0;
}
static int is_default(const struct sl_launcher *L, int i) { return L->cfg.val[i] == sl_opt(L, i)->def; }

struct member { const char *name; char status[SL_TEXT]; char l1[SL_TEXT * 2], l2[SL_TEXT * 2], l3[SL_TEXT * 2]; uint32_t col; };

static void party_of(const struct sl_launcher *L, struct member m[3]) {
  static const char *const names[3] = {"BOY", "GIRL", "SPRITE"};
  const char *mk = sl_modes[L->cfg.mode].key;
  char b[SL_TEXT];
  for (int i = 0; i < 3; i++) { m[i].name = names[i]; m[i].status[0] = m[i].l1[0] = m[i].l2[0] = m[i].l3[0] = 0; m[i].col = C_TXT; }
  const char *pre = !strcmp(mk, "ancientcave") ? "ac" : (!strcmp(mk, "chaos") ? "ch" : (!strcmp(mk, "bossrush") ? "br" : NULL));
  if (pre) {
    for (int i = 0; i < 3; i++) {
      char key[24];
      snprintf(key, sizeof key, "%s%s", pre, i == 0 ? "Boy" : (i == 1 ? "Girl" : "Sprite"));
      int y = opt_yes(L, key);
      snprintf(m[i].status, sizeof m[i].status, "%s", y == 0 ? "NOT IN THE PARTY" : "IN THE PARTY");
      m[i].col = y == 0 ? C_GREY : C_GRN;
      if (y != 0) {
        int g = opt_yes(L, "starterGear");
        if (g >= 0) snprintf(m[i].l1, sizeof m[i].l1, "Starting gear: %s", g ? "yes" : "no");
      }
    }
  } else if (!strcmp(mk, "open")) {
    char chars[SL_TEXT] = "", start[SL_TEXT] = "", lv[SL_TEXT] = "", wp[SL_TEXT] = "", gold[SL_TEXT] = "", gear[SL_TEXT] = "";
    opt_text(L, "opCharacters", chars, sizeof chars, 1);
    opt_text(L, "opStartChar", start, sizeof start, 0);
    opt_text(L, "opStartLevel", lv, sizeof lv, 0);
    opt_text(L, "opStartWeapon", wp, sizeof wp, 0);
    opt_text(L, "opStartGold", gold, sizeof gold, 0);
    int g = opt_yes(L, "starterGear");
    snprintf(gear, sizeof gear, "%s", g > 0 ? "yes" : "no");
    int si = !strcmp(start, "Boy") ? 0 : (!strcmp(start, "Girl") ? 1 : (!strcmp(start, "Sprite") ? 2 : -1));
    static const char *const rk[3] = {"opBoyRole", "opGirlRole", "opSpriteRole"};
    for (int i = 0; i < 3; i++) {
      if (si == i) { snprintf(m[i].status, sizeof m[i].status, "STARTS WITH YOU"); m[i].col = C_GRN; }
      else if (si < 0) { snprintf(m[i].status, sizeof m[i].status, "%s", i == 0 ? "RANDOM START" : "SEE BELOW"); m[i].col = C_YEL; }
      else { snprintf(m[i].status, sizeof m[i].status, "%s", chars[0] ? chars : "?"); m[i].col = C_YEL; }
      char role[SL_TEXT];
      if (opt_text(L, rk[i], role, sizeof role, 0) && strcmp(role, names[i][0] == 'B' ? "Boy" : (names[i][0] == 'G' ? "Girl" : "Sprite")))
        snprintf(m[i].l1, sizeof m[i].l1, "Plays as: %.40s", role);
      if (si == i || si < 0) {
        if (lv[0]) snprintf(m[i].l2, sizeof m[i].l2, "Level %.8s, weapon %.30s", lv, wp[0] ? wp : "?");
        snprintf(m[i].l3, sizeof m[i].l3, "Gear: %.4s, gold %.12s", gear, gold[0] ? gold : "0");
      } else {
        int ci = sl_find_opt(L, "opCharacters");
        const char *cv = ci >= 0 ? sl_opt(L, ci)->values[L->cfg.val[ci]].v : "";
        if (strstr(cv, "L1")) snprintf(m[i].l2, sizeof m[i].l2, "Level 1 when found");
        else if (strstr(cv, "CL")) snprintf(m[i].l2, sizeof m[i].l2, "Party level when found");
        else if (!strcmp(cv, "startboth")) snprintf(m[i].l2, sizeof m[i].l2, "Joins at the start");
        else if (!strcmp(cv, "none") || !strcmp(cv, "start1only")) snprintf(m[i].l2, sizeof m[i].l2, "May not exist");
      }
    }
    (void)b;
  } else {
    for (int i = 0; i < 3; i++) { snprintf(m[i].status, sizeof m[i].status, "AS IN THE GAME"); m[i].col = C_DIM; }
    snprintf(m[0].l1, sizeof m[0].l1, "These settings do not change");
    snprintf(m[0].l2, sizeof m[0].l2, "the party or its levels");
    snprintf(m[1].l1, sizeof m[1].l1, "Joins when the story says");
    snprintf(m[2].l1, sizeof m[2].l1, "Joins when the story says");
  }
}

struct sline { char lab[SL_TEXT], val[SL_TEXT]; int head, opt, changed; };

static int option_lines(const struct sl_launcher *L, struct sline *ln, int max, int *hidden_adv) {
  int n = sl_nopts(L), k = 0;
  const char *seen[32];
  int ns = 0;
  *hidden_adv = 0;
  for (int i = 0; i < n; i++) {
    const struct sl_option *o = sl_opt(L, i);
    if (!sl_opt_visible(L, i)) continue;
    if (o->advanced && is_default(L, i)) { (*hidden_adv)++; continue; }
    int g = 0;
    for (; g < ns; g++) if (!strcmp(seen[g], o->group)) break;
    if (g < ns || ns >= 32) continue;
    seen[ns++] = o->group;
    if (k < max) { memset(&ln[k], 0, sizeof ln[k]); snprintf(ln[k].lab, sizeof ln[k].lab, "%s", o->group); ln[k].head = 1; ln[k].opt = -1; k++; }
    for (int j = i; j < n; j++) {
      const struct sl_option *p = sl_opt(L, j);
      if (strcmp(p->group, o->group) || !sl_opt_visible(L, j) || (p->advanced && is_default(L, j))) continue;
      if (k < max) {
        memset(&ln[k], 0, sizeof ln[k]);
        snprintf(ln[k].lab, sizeof ln[k].lab, "%s", p->label);
        sl_value_text(L, j, ln[k].val, sizeof ln[k].val);
        ln[k].opt = j; ln[k].changed = !is_default(L, j);
        k++;
      }
    }
  }
  return k;
}

static void summary(struct cv *c, const struct sl_launcher *L, const char *tag, int selopt) {
  int W = c->W / c->s, H = c->H / c->s;
  char b[SL_TEXT * 2], key[17];
  sl_config_key(L, key);
  const struct sl_mode *mo = &sl_modes[L->cfg.mode];
  /* header */
  int hh = 44;
  win(c, 3, 2, W - 6, hh, 0);
  snprintf(b, sizeof b, "%s", L->cfg.preset >= 0 ? sl_presets[L->cfg.preset].name : "Custom");
  text_fit(c, 10, 6, W / 2, b, C_TXT, 2);
  const char *dn = NULL;
  snprintf(b, sizeof b, "%s%s%s%s%s", mo->name, sl_difficulty(L, &dn) ? "  -  " : "", dn ? sl_diff_label(dn) : "", tag[0] ? "  -  " : "", tag);
  text_fit(c, 10, 6 + 27, W / 2, b, C_DIM, 1);
  snprintf(b, sizeof b, "SEED %s", L->cfg.seed);
  int sw = text_w(c, b, 2);
  text(c, W - 10 - sw, 6, b, C_YEL, 2);
  snprintf(b, sizeof b, "KEY %.8s", key);
  text(c, W - 10 - text_w(c, b, 1), 6 + 27, b, C_DIM, 1);
  int y = 2 + hh + 3;
  /* party */
  struct member mem[3];
  party_of(L, mem);
  int cw = (W - 6 - 2 * 3) / 3, chh = 54;
  for (int i = 0; i < 3; i++) {
    int x = 3 + i * (cw + 3);
    win(c, x, y, cw, chh, 0);
    text(c, x + 6, y + 4, mem[i].name, C_CYAN, 1);
    int sx = x + 10 + text_w(c, mem[i].name, 1);
    text_fit(c, sx, y + 4, x + cw - sx - 4, mem[i].status, mem[i].col, 1);
    const char *ls[3] = {mem[i].l1, mem[i].l2, mem[i].l3};
    for (int k = 0, row = 0; k < 3; k++) if (ls[k][0]) text_fit(c, x + 6, y + 4 + 12 * (++row), cw - 12, ls[k], C_DIM, 1);
  }
  y += chh + 3;
  /* goal / mode line */
  int gh = 52;
  win(c, 3, y, W - 6, gh, 2);
  char goal[SL_TEXT * 2] = "", t1[SL_TEXT];
  if (opt_text(L, "opGoal", t1, sizeof t1, 1)) {
    snprintf(goal, sizeof goal, "Goal: %s", t1);
    if (opt_text(L, "opNumSeeds", t1, sizeof t1, 0)) { size_t l = strlen(goal); snprintf(goal + l, sizeof goal - l, "  (Mana seeds needed: %s)", t1); }
  } else snprintf(goal, sizeof goal, "Goal: %s", !strcmp(mo->key, "ancientcave") ? "climb the Ancient Cave to its end" : (!strcmp(mo->key, "bossrush") ? "beat the bosses back to back" : (!strcmp(mo->key, "chaos") ? "clear the randomized floors and bosses" : "finish the game as in the original")));
  text_fit(c, 9, y + 3, W - 18, goal, C_TXT, 1);
  int dl = wrap(c, 9, y + 3 + 11, W - 18, mo->desc, C_DIM, 2);
  text_fit(c, 9, y + 3 + 11 + dl * 11, W - 18, !strcmp(mo->key, "open") ? "HP and MP are not part of the settings: the game decides them from the level." : "Levels, HP, MP and equipment are not part of the settings: the game decides them.", C_GREY, 1);
  y += gh + 3;
  /* options in columns */
  static struct sline ln[SL_MAX_OPTS + 40];
  int hidden = 0, nl = option_lines(L, ln, (int)(sizeof ln / sizeof ln[0]), &hidden);
  int helph = 30, footh = 14, top = y, bot = H - helph - footh - 6, lh = 11;
  int ncol = W / 190; if (ncol < 1) ncol = 1; if (ncol > 4) ncol = 4;
  int need = (nl + ncol - 1) / ncol + 2;                                  /* balanced columns; roomy lines when there is space */
  lh = (bot - top - 8) / need; if (lh < 11) lh = 11; if (lh > 16) lh = 16;
  int rows = (bot - top - 8) / lh;
  if (need < rows) rows = need;
  int colw = (W - 6 - (ncol - 1) * 3) / ncol, col = 0, row = 0, shown = 0;
  win(c, 3, top, W - 6, bot - top, 2);
  int total_cap = rows * ncol;
  for (int i = 0; i < nl; i++) {
    if (ln[i].head && row >= rows - 1) { col++; row = 0; }                 /* a header never sits alone at the bottom */
    if (row >= rows) { col++; row = 0; }
    if (col >= ncol) break;
    int x = 3 + col * (colw + 3) + 4, yy = top + 4 + row * lh, w = colw - 8;
    if (ln[i].head) text_fit(c, x, yy, w, ln[i].lab, C_CYAN, 1);
    else {
      int vw = text_w(c, ln[i].val, 1);
      if (vw > w * 55 / 100) vw = w * 55 / 100;
      if (ln[i].opt == selopt) frect(c, x - 2, yy - 1 + (lh - 11) / 2, w + 4, 11, 0x34408Cu);
      text_ellipsis(c, x + w - vw, yy + (lh - 11) / 2, vw, ln[i].val, ln[i].changed ? C_YEL : C_DIM);
      text_ellipsis(c, x, yy + (lh - 11) / 2, w - vw - 4, ln[i].lab, ln[i].opt == selopt ? C_TXT : C_LIGHT);
    }
    row++; shown = i + 1;
  }
  (void)total_cap;
  if (shown < nl) { snprintf(b, sizeof b, "+%d more", nl - shown); text(c, W - 10 - text_w(c, b, 1), bot - 12, b, C_ORG, 1); }
  /* help line */
  const char *help = "";
  char tmp[SL_TEXT * 2];
  if (selopt >= 0) {
    const struct sl_option *o = sl_opt(L, selopt);
    char vt[SL_TEXT];
    sl_value_text(L, selopt, vt, sizeof vt);
    snprintf(tmp, sizeof tmp, "%s: %s", o->label, o->type == SL_O_ENUM && o->values[L->cfg.val[selopt]].full[0] ? o->values[L->cfg.val[selopt]].full : vt);
    if (o->help[0]) { size_t l = strlen(tmp); snprintf(tmp + l, sizeof tmp - l, ".  %s", o->help); }
    help = tmp;
  } else if (hidden > 0) { snprintf(tmp, sizeof tmp, "%d more options are at their default value. Yellow = changed from the mode default.", hidden); help = tmp; }
  else help = "Yellow = changed from the mode default.";
  win(c, 3, bot + 2, W - 6, helph, 0);
  wrap(c, 9, bot + 5, W - 18, help, C_DIM, 2);
  text_center(c, 0, W, H - footh + 2, sl_pad_hint(L), C_MID, 1);
}

void sl_draw_top(const struct sl_launcher *L, uint32_t *px, int stride, int w, int h) {
  struct cv c = {px, stride, w, h, 1};
  int mn = w < h ? w : h;
  c.s = (mn + 170) / 360; if (c.s < 1) c.s = 1;
  int W = w / c.s;
  static struct sl_launcher tmp;
  switch (L->screen) {
    case SL_MAIN: case SL_PLAY:
      wallpaper(&c, 0);
      title_big(&c, W, h / c.s * 5 / 100);
      return;
    case SL_SAVED: {
      const struct sl_saved *v = L->cursor < SL_SAVED_MAX ? &L->saved[L->cursor] : NULL;
      if (!v || !v->used) {
        wallpaper(&c, 90);
        title_big(&c, W, h / c.s * 5 / 100);
        text_center(&c, 0, W, h / c.s / 2, v ? "Empty slot" : "Choose a saved game", C_LIGHT, 2);
        text_center(&c, 0, W, h / c.s - 14, sl_pad_hint(L), C_MID, 1);
        return;
      }
      memcpy(&tmp, L, sizeof tmp);
      if (sl_load_config(&tmp, v->preset, v->mode, v->seed, v->diff, v->lines) != 0) { background(&c); return; }
      background(&c);
      char tag[48];
      snprintf(tag, sizeof tag, "saved %s", v->date);
      summary(&c, &tmp, tag, -1);
      return;
    }
    default: break;
  }
  background(&c);
  const struct sl_launcher *S = L;
  int selopt = -1;
  const char *tag = "";
  if (L->screen == SL_PRESETS && L->cursor < L->nrows && L->rows[L->cursor].kind == SR_PRESET && L->rows[L->cursor].a >= 0) {
    memcpy(&tmp, L, sizeof tmp);                                    /* preview of the highlighted preset: what choosing it would give */
    sl_apply_preset(&tmp, L->rows[L->cursor].a);
    S = &tmp; tag = "preview";
  } else if (L->screen == SL_PRESETS) {
    memcpy(&tmp, L, sizeof tmp);
    S = &tmp; tag = "random preset";
  }
  if (L->screen == SL_TWEAK && L->cursor < L->nrows && L->rows[L->cursor].kind == SR_OPT) selopt = L->rows[L->cursor].a;
  if (L->screen == SL_MULTI) selopt = L->multi_opt;
  if (L->screen == SL_GENERATE) tag = L->gen == SL_GEN_RUNNING ? "generating" : "";
  summary(&c, S, tag, selopt);
}
