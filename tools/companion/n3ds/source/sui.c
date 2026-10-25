#include <string.h>
#include "sui.h"

#define C_BG 0x101828u
#define C_GRID 0x1A2438u
#define C_PANEL 0x1C2A44u
#define C_SEL 0x2E4A7Au
#define C_TEXT 0xE8EEF8u
#define C_DIM 0x8AA0C0u
#define C_HEAD 0xF0C860u
#define C_OFF 0x5A6A82u
#define C_WARN 0x8A2A2Au

/* classic 5x7 column font, ASCII 32..126, bit 0 = top row, bit 7 = descender row */
static const uint8_t FONT[95][5] = {
  {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},
  {0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x56,0x20,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},
  {0x2A,0x1C,0x7F,0x1C,0x2A},{0x08,0x08,0x3E,0x08,0x08},{0x00,0x80,0x70,0x30,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x00,0x60,0x60,0x00},
  {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x72,0x49,0x49,0x49,0x46},{0x21,0x41,0x49,0x4D,0x33},
  {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x31},{0x41,0x21,0x11,0x09,0x07},{0x36,0x49,0x49,0x49,0x36},
  {0x46,0x49,0x49,0x29,0x1E},{0x00,0x00,0x14,0x00,0x00},{0x00,0x40,0x34,0x00,0x00},{0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},
  {0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x59,0x09,0x06},{0x3E,0x41,0x5D,0x59,0x4E},{0x7C,0x12,0x11,0x12,0x7C},{0x7F,0x49,0x49,0x49,0x36},
  {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x41,0x3E},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x41,0x51,0x73},
  {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
  {0x7F,0x02,0x1C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},
  {0x7F,0x09,0x19,0x29,0x46},{0x26,0x49,0x49,0x49,0x32},{0x03,0x01,0x7F,0x01,0x03},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
  {0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},{0x03,0x04,0x78,0x04,0x03},{0x61,0x59,0x49,0x4D,0x43},{0x00,0x7F,0x41,0x41,0x41},
  {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x41,0x7F},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},{0x00,0x03,0x07,0x08,0x00},
  {0x20,0x54,0x54,0x78,0x40},{0x7F,0x28,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x28},{0x38,0x44,0x44,0x28,0x7F},{0x38,0x54,0x54,0x54,0x18},
  {0x00,0x08,0x7E,0x09,0x02},{0x18,0xA4,0xA4,0x9C,0x78},{0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x40,0x3D,0x00},
  {0x7F,0x10,0x28,0x44,0x00},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x78,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
  {0xFC,0x18,0x24,0x24,0x18},{0x18,0x24,0x24,0x18,0xFC},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x24},{0x04,0x04,0x3F,0x44,0x24},
  {0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x4C,0x90,0x90,0x90,0x7C},
  {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x77,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},{0x02,0x01,0x02,0x04,0x02},
};

void sui_rect(uint32_t *px, int stride, int w, int h, int x, int y, int rw, int rh, uint32_t c) {
  if (x < 0) { rw += x; x = 0; }
  if (y < 0) { rh += y; y = 0; }
  if (x + rw > w) rw = w - x;
  if (y + rh > h) rh = h - y;
  for (int j = 0; j < rh; j++) for (int i = 0; i < rw; i++) px[(y + j) * stride + x + i] = c;
}
int sui_text_width(const char *s, int scale) { return (int)strlen(s) * 6 * scale; }
void sui_text_draw(uint32_t *px, int stride, int w, int h, int x, int y, int scale, uint32_t color, const char *s) {
  for (; *s; s++, x += 6 * scale) {
    int ch = (unsigned char)*s;
    if (ch < 32 || ch > 126) ch = '?';
    const uint8_t *g = FONT[ch - 32];
    for (int i = 0; i < 5; i++)
      for (int j = 0; j < 8; j++)
        if (g[i] & (1 << j)) sui_rect(px, stride, w, h, x + i * scale, y + j * scale, scale, scale, color);
  }
}
static void fit(char *dst, size_t n, const char *s, int maxchars) {   /* ".." when the text is longer than the column */
  size_t l = strlen(s);
  if ((int)l <= maxchars || maxchars < 3) { strncpy(dst, s, n - 1); dst[n - 1] = 0; if ((int)l > maxchars && maxchars >= 0 && (size_t)maxchars < n) dst[maxchars] = 0; return; }
  memcpy(dst, s, (size_t)maxchars - 2);
  dst[maxchars - 2] = '.'; dst[maxchars - 1] = '.'; dst[maxchars] = 0;
}

void sui_draw_grid(uint32_t *px, int stride, int w, int h) {
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) px[y * stride + x] = (x % 16 == 0 || y % 16 == 0) ? C_GRID : C_BG;
}

/* ---- list */
#define LIST_X 12
#define LIST_Y 28
#define ROW_H 18
#define LIST_ROWS 9

void sui_list_clear(struct sui_list *l, const char *title) {
  memset(l, 0, sizeof *l);
  strncpy(l->title, title, sizeof l->title - 1);
}
struct sui_row *sui_list_add(struct sui_list *l, int kind, const char *label, const char *value, const char *help) {
  if (l->n >= SUI_MAX_ROWS) return NULL;
  struct sui_row *r = &l->rows[l->n++];
  memset(r, 0, sizeof *r);
  r->kind = (uint8_t)kind;
  if (label) strncpy(r->label, label, sizeof r->label - 1);
  if (value) strncpy(r->value, value, sizeof r->value - 1);
  if (help) strncpy(r->help, help, sizeof r->help - 1);
  return r;
}
int sui_list_visible_rows(void) { return LIST_ROWS; }
static void keep_visible(struct sui_list *l) {
  if (l->sel < l->scroll) l->scroll = l->sel;
  if (l->sel >= l->scroll + LIST_ROWS) l->scroll = l->sel - LIST_ROWS + 1;
  if (l->sel > 0 && l->rows[l->sel - 1].kind == SUI_ROW_HEADER && l->scroll > l->sel - 1 && l->sel - 1 < l->scroll) l->scroll = l->sel - 1;
  if (l->scroll < 0) l->scroll = 0;
}
void sui_list_move(struct sui_list *l, int d) {
  if (l->n == 0) return;
  for (int tries = 0; tries < l->n; tries++) {
    l->sel = (l->sel + d + l->n) % l->n;
    if (l->rows[l->sel].kind == SUI_ROW_ITEM) break;
  }
  keep_visible(l);
}
void sui_list_page(struct sui_list *l, int dir) {
  int target = l->sel + dir * LIST_ROWS;
  if (target < 0) target = 0;
  if (target >= l->n) target = l->n - 1;
  l->sel = target;
  while (l->sel > 0 && l->rows[l->sel].kind != SUI_ROW_ITEM) l->sel += dir > 0 ? -1 : 1;
  if (l->sel < 0 || l->sel >= l->n || l->rows[l->sel].kind != SUI_ROW_ITEM) { l->sel = 0; sui_list_move(l, 1); }
  keep_visible(l);
}

void sui_draw_list(uint32_t *px, int stride, const struct sui_list *l) {
  const int W = 400, H = 240;
  sui_draw_grid(px, stride, W, H);
  sui_rect(px, stride, W, H, 0, 0, W, 22, C_PANEL);
  sui_text_draw(px, stride, W, H, LIST_X, 5, 2, C_HEAD, l->title);
  int listw = l->thumb ? 236 : W - 2 * LIST_X - 16;   /* room for the scroll marks */
  int chars = listw / 12;
  for (int i = 0; i < LIST_ROWS && l->scroll + i < l->n; i++) {
    const struct sui_row *r = &l->rows[l->scroll + i];
    int y = LIST_Y + i * ROW_H, sel = l->scroll + i == l->sel;
    if (sel) sui_rect(px, stride, W, H, LIST_X - 4, y - 2, listw + 8, ROW_H, C_SEL);
    uint32_t col = r->kind == SUI_ROW_HEADER ? C_HEAD : r->kind == SUI_ROW_DISABLED ? C_OFF : C_TEXT;
    char lab[64], val[64];
    int vlen = (int)strlen(r->value), llen = (int)strlen(r->label);
    int labmax = llen, valmax = vlen;
    if (vlen && llen + vlen + 1 > chars) {   /* neither side may squeeze the other below 40 percent of the row */
      if (vlen <= chars * 6 / 10) labmax = chars - vlen - 1;
      else { labmax = chars * 4 / 10; valmax = chars - labmax - 1; }
    } else if (!vlen && llen > chars) labmax = chars;
    fit(lab, sizeof lab, r->label, labmax);
    sui_text_draw(px, stride, W, H, LIST_X, y, 2, col, lab);
    if (vlen) {
      fit(val, sizeof val, r->value, valmax);
      sui_text_draw(px, stride, W, H, LIST_X + listw - sui_text_width(val, 2), y, 2, sel ? C_HEAD : C_DIM, val);
    }
  }
  if (l->scroll > 0) sui_text_draw(px, stride, W, H, W - 16, 24, 2, C_DIM, "^");
  if (l->scroll + LIST_ROWS < l->n) sui_text_draw(px, stride, W, H, W - 16, LIST_Y + LIST_ROWS * ROW_H - 14, 2, C_DIM, "v");
  if (l->thumb) {
    int tx = 260, ty = 30;
    sui_rect(px, stride, W, H, tx - 2, ty - 2, SUI_THUMB_W + 4, SUI_THUMB_H + 4, C_PANEL);
    for (int y = 0; y < SUI_THUMB_H; y++) for (int x = 0; x < SUI_THUMB_W; x++) px[(ty + y) * stride + tx + x] = l->thumb[y * SUI_THUMB_W + x];
    if (l->thumb_caption[0]) sui_text_draw(px, stride, W, H, tx, ty + SUI_THUMB_H + 6, 1, C_DIM, l->thumb_caption);
  }
  if (l->sel >= 0 && l->sel < l->n && l->rows[l->sel].help[0]) {   /* help: two lines of 66 characters at scale 1 */
    const char *h = l->rows[l->sel].help;
    char line[70];
    int y = 200;
    sui_rect(px, stride, W, H, 0, 196, W, 28, C_PANEL);
    for (int k = 0; k < 2 && *h; k++) {
      int take = 64;
      if ((int)strlen(h) > take) { int c = take; while (c > 20 && h[c] != ' ') c--; take = c > 20 ? c : 64; }
      memcpy(line, h, (size_t)take); line[take] = 0;
      sui_text_draw(px, stride, W, H, LIST_X, y + k * 11, 1, C_DIM, line);
      h += take; while (*h == ' ') h++;
    }
  }
  if (l->notice[0]) {
    sui_rect(px, stride, W, H, 0, 222, W, 18, C_WARN);
    char nb[40];
    fit(nb, sizeof nb, l->notice, 32);
    sui_text_draw(px, stride, W, H, LIST_X, 223, 2, C_TEXT, nb);
  } else if (l->hint[0]) sui_text_draw(px, stride, W, H, LIST_X, 228, 1, C_OFF, l->hint);
}

/* ---- text page */
void sui_text_clear(struct sui_text *t, const char *title) { memset(t, 0, sizeof *t); strncpy(t->title, title, sizeof t->title - 1); }
void sui_text_blank(struct sui_text *t) { if (t->n < SUI_WRAP_LINES) t->n++; }
void sui_text_add(struct sui_text *t, const char *s, int cols, int head, int dim) {
  if (cols > SUI_WRAP_COLS) cols = SUI_WRAP_COLS;
  while (*s && t->n < SUI_WRAP_LINES) {
    while (*s == ' ') s++;
    int len = (int)strlen(s), take = len;
    if (len > cols) {
      take = cols;
      int c = cols;
      while (c > cols / 2 && s[c] != ' ' && s[c - 1] != '/' && s[c - 1] != '-') c--;
      if (c > cols / 2) take = c;
    }
    memcpy(t->line[t->n], s, (size_t)take);
    t->line[t->n][take] = 0;
    t->head[t->n] = (uint8_t)head; t->dim[t->n] = (uint8_t)dim;
    t->n++;
    s += take;
  }
}
int sui_text_visible_lines(void) { return 11; }
void sui_text_scroll(struct sui_text *t, int d) {
  int max = t->n - sui_text_visible_lines();
  t->scroll += d;
  if (t->scroll > max) t->scroll = max;
  if (t->scroll < 0) t->scroll = 0;
}
void sui_draw_text(uint32_t *px, int stride, const struct sui_text *t) {
  const int W = 400, H = 240;
  sui_draw_grid(px, stride, W, H);
  sui_rect(px, stride, W, H, 0, 0, W, 22, C_PANEL);
  sui_text_draw(px, stride, W, H, LIST_X, 5, 2, C_HEAD, t->title);
  for (int i = 0; i < sui_text_visible_lines() && t->scroll + i < t->n; i++) {
    int k = t->scroll + i, y = 28 + i * 17;
    uint32_t col = t->head[k] ? C_HEAD : t->dim[k] ? C_DIM : C_TEXT;
    sui_text_draw(px, stride, W, H, LIST_X, y, 2, col, t->line[k]);
  }
  if (t->scroll > 0) sui_text_draw(px, stride, W, H, W - 16, 24, 2, C_DIM, "^");
  if (t->scroll + sui_text_visible_lines() < t->n) sui_text_draw(px, stride, W, H, W - 16, 212, 2, C_DIM, "v");
  sui_text_draw(px, stride, W, H, LIST_X, 228, 1, C_OFF, "D-PAD scroll   B back");
}
