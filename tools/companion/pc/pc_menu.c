/* The in-game menu of the SoM PC frontend, see pc_menu.h. */
#include "pc_menu.h"

#include <stdio.h>
#include <string.h>

#include "pc_gameplay.h"
#include "pc_shader.h"

#define C_PANEL 0xF0101830u
#define C_CARD 0xFF1C2A5Au
#define C_SEL 0xFF35508Cu
#define C_RIM 0xFFF0C040u
#define C_TXT 0xFFE8ECF8u
#define C_DIM 0xFF8C98C0u
#define C_WARN 0xFFFF9060u
#define C_OK 0xFF7CE08Cu

enum { R_RESUME = 0, R_SAVE, R_LOAD, R_SLOTS, R_VIDEO, R_CREDITS, R_LAUNCHER, R_QUIT };
static const char *const R_NAME[] = {"RESUME", "SAVE STATE", "LOAD STATE", "QUICK SLOTS L1 R1 L2 R2", "VIDEO", "CREDITS", "START MENU", "QUIT"};

static int root_ids(const struct pcm *m, int *ids) {
  int n = 0;
  if (m->in_game) { ids[n++] = R_RESUME; ids[n++] = R_SAVE; ids[n++] = R_LOAD; ids[n++] = R_SLOTS; }
  ids[n++] = R_VIDEO;
  ids[n++] = R_CREDITS;
  if (m->can_launcher) ids[n++] = R_LAUNCHER;
  ids[n++] = R_QUIT;
  return n;
}
int pcm_root_count(const struct pcm *m) { int ids[8]; return root_ids(m, ids); }
const char *pcm_root_label(const struct pcm *m, int i) { int ids[8], n = root_ids(m, ids); return i >= 0 && i < n ? R_NAME[ids[i]] : ""; }

static void touch(struct pcm *m) { m->rev++; }
static void post(struct pcm *m, int act, int arg) { m->act = act; m->act_arg = arg; m->act_pending = 1; }

void pcm_init(struct pcm *m, struct pc_cfg *cfg) {
  memset(m, 0, sizeof *m);
  m->cfg = cfg;
  m->credits = "";
}
void pcm_note(struct pcm *m, const char *t) { snprintf(m->note, sizeof m->note, "%s", t ? t : ""); touch(m); }
void pcm_open(struct pcm *m, int page) {
  m->open = 1; m->page = page >= 0 && page < PCM_PAGES ? page : PCM_ROOT;
  m->confirm = 0; m->note[0] = 0; m->scroll = 0;
  if (m->page == PCM_SAVE || m->page == PCM_LOAD) m->cursor[m->page] = m->cfg ? m->cfg->state_slot : 0;
  if (m->page == PCM_ROOT) m->cursor[PCM_ROOT] = 0;
  touch(m);
}
void pcm_close(struct pcm *m) { m->open = 0; m->confirm = 0; m->note[0] = 0; touch(m); }
int pcm_poll(struct pcm *m, int *act, int *arg) {
  if (!m->act_pending) return 0;
  *act = m->act; if (arg) *arg = m->act_arg;
  m->act_pending = 0;
  return 1;
}

/* ------------------------------------------------------------------ video page */
static const char *const SCREEN_NAME[PC_SCREEN_COUNT] = {"WINDOWED", "FULLSCREEN", "EXCLUSIVE"};
void pcm_video_value(const struct pcm *m, int row, char *out, size_t n) {
  const struct pc_cfg *c = m->cfg;
  out[0] = 0;
  if (!c) return;
  switch (row) {
  case PCM_V_FILTER: snprintf(out, n, "%s", pc_shader_name(c->shader)); break;
  case PCM_V_SIZE: pc_win_name(c->win_w, c->win_h, out, n); break;
  case PCM_V_SCREEN: snprintf(out, n, "%s", c->screen >= 0 && c->screen < PC_SCREEN_COUNT ? SCREEN_NAME[c->screen] : "?"); break;
  case PCM_V_ASPECT: snprintf(out, n, "%s", pc_aspect_name(c->wide)); break;
  case PCM_V_SCALING: snprintf(out, n, "%s", c->fit == PC_FIT_INTEGER ? "-" : pc_fit_name(c->fit)); break;
  case PCM_V_INTEGER: snprintf(out, n, "%s", c->fit == PC_FIT_INTEGER ? "ON" : "OFF"); break;
  case PCM_V_VSYNC: snprintf(out, n, "%s", c->vsync ? "ON" : "OFF"); break;
  default: break;
  }
}
static const char *const V_LABEL[PCM_V_COUNT] = {"FILTER", "WINDOW SIZE", "SCREEN", "PICTURE", "SCALING", "INTEGER SCALE", "VSYNC", "BACK"};

static void video_change(struct pcm *m, int row, int dir) {
  struct pc_cfg *c = m->cfg;
  int changed = 1;
  if (!c) return;
  switch (row) {
  case PCM_V_FILTER: c->shader = pc_shader_next(c->shader, dir); break;
  case PCM_V_SIZE: { int w, h; pc_win_step(c->win_w, c->win_h, dir, &w, &h); c->win_w = w; c->win_h = h; break; }
  case PCM_V_SCREEN: c->screen = (c->screen + (dir < 0 ? PC_SCREEN_COUNT - 1 : 1)) % PC_SCREEN_COUNT; break;
  case PCM_V_ASPECT: c->wide = pc_aspect_step(c->wide, dir); break;
  case PCM_V_SCALING: if (c->fit == PC_FIT_INTEGER) changed = 0; else c->fit = pc_fit_step(c->fit, dir); break;
  case PCM_V_INTEGER: c->fit = pc_fit_toggle_integer(c->fit); break;
  case PCM_V_VSYNC: c->vsync = !c->vsync; break;
  default: changed = 0; break;
  }
  if (changed) post(m, PCM_A_VIDEO, row);
  touch(m);
}

/* ------------------------------------------------------------------ input */
static void go_page(struct pcm *m, int page);
static void back_from(struct pcm *m) { if (m->single_page) post(m, PCM_A_CLOSE, 0); else go_page(m, PCM_ROOT); }
static void go_page(struct pcm *m, int page) {
  int keep = m->open;
  pcm_open(m, page);
  m->open = keep;
}

void pcm_key(struct pcm *m, int key) {
  int ids[8], n;
  if (!m->open) return;
  m->note[0] = 0;
  if (key != PCM_ACCEPT) m->confirm = 0;
  switch (m->page) {
  case PCM_ROOT:
    n = root_ids(m, ids);
    if (key == PCM_UP) m->cursor[PCM_ROOT] = (m->cursor[PCM_ROOT] + n - 1) % n;
    else if (key == PCM_DOWN) m->cursor[PCM_ROOT] = (m->cursor[PCM_ROOT] + 1) % n;
    else if (key == PCM_BACK) { if (m->in_game) post(m, PCM_A_CLOSE, 0); }
    else if (key == PCM_ACCEPT) {
      switch (ids[m->cursor[PCM_ROOT]]) {
      case R_RESUME: post(m, PCM_A_CLOSE, 0); break;
      case R_SAVE: go_page(m, PCM_SAVE); break;
      case R_LOAD: go_page(m, PCM_LOAD); break;
      case R_SLOTS: post(m, PCM_A_SLOTS, 0); break;
      case R_VIDEO: go_page(m, PCM_VIDEO); break;
      case R_CREDITS: go_page(m, PCM_CREDITS); break;
      case R_LAUNCHER: post(m, PCM_A_LAUNCHER, 0); break;
      case R_QUIT: post(m, PCM_A_QUIT, 0); break;
      default: break;
      }
    }
    break;
  case PCM_SAVE: case PCM_LOAD: {
    int *c = &m->cursor[m->page];
    if (key == PCM_LEFT) { if (*c % 2) (*c)--; }
    else if (key == PCM_RIGHT) { if (*c % 2 == 0) (*c)++; }
    else if (key == PCM_UP) *c = (*c + PCS_SLOTS - 2) % PCS_SLOTS;
    else if (key == PCM_DOWN) *c = (*c + 2) % PCS_SLOTS;
    else if (key == PCM_TAB_PREV || key == PCM_TAB_NEXT) { int cur = *c; go_page(m, m->page == PCM_SAVE ? PCM_LOAD : PCM_SAVE); m->cursor[m->page] = cur; }
    else if (key == PCM_BACK) back_from(m);
    else if (key == PCM_ACCEPT) {
      int s = *c;
      if (m->page == PCM_SAVE) {
        if (m->slot[s].status == PCS_EMPTY || m->confirm == s + 1) { m->confirm = 0; post(m, PCM_A_SAVE, s); }
        else m->confirm = s + 1;                              /* a used slot asks once more before it is replaced */
      } else if (m->slot[s].status == PCS_OK) post(m, PCM_A_LOAD, s);
      else if (m->slot[s].status == PCS_BAD) pcm_note(m, "THIS SLOT IS DAMAGED");
      else if (m->slot[s].status == PCS_INCOMPAT) pcm_note(m, "THIS SLOT BELONGS TO ANOTHER VERSION");
    }
    break;
  }
  case PCM_VIDEO: {
    int *c = &m->cursor[PCM_VIDEO];
    if (key == PCM_UP) *c = (*c + PCM_V_COUNT - 1) % PCM_V_COUNT;
    else if (key == PCM_DOWN) *c = (*c + 1) % PCM_V_COUNT;
    else if (key == PCM_LEFT) video_change(m, *c, -1);
    else if (key == PCM_RIGHT) video_change(m, *c, 1);
    else if (key == PCM_ACCEPT) { if (*c == PCM_V_BACK) back_from(m); else video_change(m, *c, 1); }
    else if (key == PCM_BACK) back_from(m);
    break;
  }
  case PCM_CREDITS:
    if (key == PCM_UP) pcm_scroll(m, -1);
    else if (key == PCM_DOWN) pcm_scroll(m, 1);
    else if (key == PCM_TAB_PREV || key == PCM_LEFT) pcm_scroll(m, -(m->credits_vis > 2 ? m->credits_vis - 1 : 1));
    else if (key == PCM_TAB_NEXT || key == PCM_RIGHT) pcm_scroll(m, m->credits_vis > 2 ? m->credits_vis - 1 : 1);
    else if (key == PCM_BACK || key == PCM_ACCEPT) back_from(m);
    break;
  default: break;
  }
  touch(m);
}

void pcm_scroll(struct pcm *m, int rows) {
  int max = m->credits_lines - m->credits_vis;
  if (m->page != PCM_CREDITS) return;
  m->scroll += rows;
  if (m->scroll > max) m->scroll = max;
  if (m->scroll < 0) m->scroll = 0;
  touch(m);
}

/* ------------------------------------------------------------------ drawing */
struct cv { uint32_t *px; int stride, w, h, u; };

static void fill(struct cv *c, int x, int y, int w, int h, uint32_t col) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > c->w) w = c->w - x;
  if (y + h > c->h) h = c->h - y;
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) c->px[(size_t)(y + j) * c->stride + x + i] = col;
}
static void frame(struct cv *c, int x, int y, int w, int h, int t, uint32_t col) {
  fill(c, x, y, w, t, col); fill(c, x, y + h - t, w, t, col); fill(c, x, y, t, h, col); fill(c, x + w - t, y, t, h, col);
}
static void text(struct cv *c, int x, int y, const char *s, uint32_t col, int mul) { pg_text(c->px, c->stride, c->w, c->h, x, y, s, col, mul); }
static void text_fit(struct cv *c, int x, int y, int maxw, const char *s, uint32_t col, int mul) {
  char b[160];
  size_t n = strlen(s);
  snprintf(b, sizeof b, "%s", s);
  n = strlen(b);
  while (n > 1 && pg_text_width(b, mul) > maxw) b[--n] = 0;
  text(c, x, y, b, col, mul);
}

static void thumb_draw(struct cv *c, int x, int y, int w, int h, const struct pcm_slot *s) {
  int tw = s->tw, th = s->th, dw, dh, ox, oy;
  fill(c, x, y, w, h, 0xFF000000u);
  if (!s->thumb || tw < 1 || th < 1) return;
  dh = h; dw = (int)((int64_t)tw * dh / th);
  if (dw > w) { dw = w; dh = (int)((int64_t)th * dw / tw); }
  ox = x + (w - dw) / 2; oy = y + (h - dh) / 2;
  for (int j = 0; j < dh; j++) {
    const uint32_t *src = s->thumb + (size_t)(j * th / dh) * tw;
    for (int i = 0; i < dw; i++) {
      int px = ox + i, py = oy + j;
      if (px >= 0 && py >= 0 && px < c->w && py < c->h) c->px[(size_t)py * c->stride + px] = src[i * tw / dw];
    }
  }
}

static void draw_states(struct cv *c, struct pcm *m, int x0, int y0, int pw, int ph) {
  int u = c->u, cols = 2, rows = PCS_SLOTS / 2;
  int top = y0 + 20 * u, bot = y0 + ph - 32 * u, gap = 3 * u;
  int cw = (pw - 12 * u - gap) / cols, chh = (bot - top - (rows - 1) * gap) / rows;
  int save = m->page == PCM_SAVE;
  text(c, x0 + 6 * u, y0 + 5 * u, save ? "SAVE STATE" : "LOAD STATE", C_TXT, 2 * u);
  for (int i = 0; i < PCS_SLOTS; i++) {
    int cx = x0 + 6 * u + (i % cols) * (cw + gap), cy = top + (i / cols) * (chh + gap);
    int sel = m->cursor[m->page] == i, tw_ = chh * 3 / 2 < cw / 2 ? chh * 3 / 2 : cw / 2;
    char t[48];
    fill(c, cx, cy, cw, chh, sel ? C_SEL : C_CARD);
    if (sel) frame(c, cx, cy, cw, chh, u > 1 ? u : 1, C_RIM);
    thumb_draw(c, cx + 2 * u, cy + 2 * u, tw_ - 2 * u, chh - 4 * u, &m->slot[i]);
    snprintf(t, sizeof t, "SLOT %d%s", i + 1, m->cfg && m->cfg->state_slot == i ? " *" : "");
    text_fit(c, cx + tw_ + 2 * u, cy + 3 * u, cw - tw_ - 4 * u, t, C_TXT, u);
    if (save && m->confirm == i + 1) text_fit(c, cx + tw_ + 2 * u, cy + 13 * u, cw - tw_ - 4 * u, "A: OVERWRITE", C_WARN, u);
    else if (m->slot[i].status == PCS_OK) {                     /* date and time on two lines: the card is narrow */
      char dt[32], *sp;
      snprintf(dt, sizeof dt, "%s", m->slot[i].when);
      sp = strchr(dt, ' ');
      if (sp) { *sp = 0; text_fit(c, cx + tw_ + 2 * u, cy + 23 * u, cw - tw_ - 4 * u, sp + 1, C_DIM, u); }
      text_fit(c, cx + tw_ + 2 * u, cy + 13 * u, cw - tw_ - 4 * u, dt, C_DIM, u);
    }
    else if (m->slot[i].status == PCS_EMPTY) text(c, cx + tw_ + 2 * u, cy + 13 * u, "EMPTY", C_DIM, u);
    else text(c, cx + tw_ + 2 * u, cy + 13 * u, m->slot[i].status == PCS_BAD ? "DAMAGED" : "OTHER VERSION", C_WARN, u);
  }
  text_fit(c, x0 + 6 * u, y0 + ph - 12 * u, pw - 12 * u, save ? "A: SAVE   L R: LOAD PAGE   B: BACK" : "A: LOAD   L R: SAVE PAGE   B: BACK", C_DIM, u);
}

static void draw_list(struct cv *c, struct pcm *m, int x0, int y0, int pw, int ph, const char *title, int n, const char *(*label)(struct pcm *, int, char *, size_t), char *valbuf, int value_col) {
  int u = c->u, lh = 15 * u, y = y0 + 24 * u;
  (void)ph;
  text(c, x0 + 6 * u, y0 + 5 * u, title, C_TXT, 2 * u);
  for (int i = 0; i < n; i++) {
    int sel = m->cursor[m->page] == i;
    const char *l = label(m, i, valbuf, 64);
    fill(c, x0 + 6 * u, y - 2 * u, pw - 12 * u, lh - u, sel ? C_SEL : C_CARD);
    if (sel) frame(c, x0 + 6 * u, y - 2 * u, pw - 12 * u, lh - u, u > 1 ? u : 1, C_RIM);
    text_fit(c, x0 + 10 * u, y + u, pw / 2, l, C_TXT, u);
    if (value_col && valbuf[0]) {
      char b2[96];
      snprintf(b2, sizeof b2, sel ? "< %s >" : "%s", valbuf);
      text_fit(c, x0 + pw / 2 + 4 * u, y + u, pw / 2 - 16 * u, b2, sel ? C_RIM : C_OK, u);
    }
    y += lh;
  }
}
static const char *root_row(struct pcm *m, int i, char *v, size_t n) { (void)n; v[0] = 0; return pcm_root_label(m, i); }
static const char *video_row(struct pcm *m, int i, char *v, size_t n) { pcm_video_value(m, i, v, n); return V_LABEL[i]; }

/* Word-wraps `s` to cols characters; draws lines first..first+vis-1 when draw is set; returns the number of lines. */
static int wrap_lines(struct cv *c, const char *s, int cols, int first, int vis, int x, int y, int lh, int draw, uint32_t col, int mul) {
  int line = 0;
  while (*s) {
    const char *e = strchr(s, '\n');
    int len = e ? (int)(e - s) : (int)strlen(s), done = 0;
    if (len == 0) line++;                                     /* blank line */
    while (done < len) {
      int rest = len - done, cut = rest > cols ? cols : rest;
      char b[200];
      if (rest > cols) { int k = cut; while (k > 0 && s[done + k] != ' ') k--; if (k > 0) cut = k; }
      if (cut >= (int)sizeof b) cut = (int)sizeof b - 1;
      memcpy(b, s + done, (size_t)cut); b[cut] = 0;
      if (draw && line >= first && line < first + vis) text(c, x, y + (line - first) * lh, b, col, mul);
      line++;
      done += cut;
      while (done < len && s[done] == ' ') done++;
    }
    s += len;
    if (*s == '\n') s++;
  }
  return line;
}

static void draw_credits(struct cv *c, struct pcm *m, int x0, int y0, int pw, int ph) {
  int u = c->u, cols = (pw - 20 * u) / (6 * u), lh = 9 * u, top = y0 + 22 * u, vis = (ph - 22 * u - 18 * u) / lh;
  if (cols < 8) cols = 8;
  if (vis < 1) vis = 1;
  m->credits_vis = vis;
  m->credits_lines = wrap_lines(c, m->credits, cols, 0, 0, 0, 0, lh, 0, 0, 1);
  if (m->scroll > m->credits_lines - vis) m->scroll = m->credits_lines - vis;
  if (m->scroll < 0) m->scroll = 0;
  text(c, x0 + 6 * u, y0 + 5 * u, "CREDITS", C_TXT, 2 * u);
  wrap_lines(c, m->credits, cols, m->scroll, vis, x0 + 10 * u, top, lh, 1, C_TXT, u);
  if (m->credits_lines > vis) {                               /* scroll bar */
    int bh = ph - 40 * u, th = bh * vis / m->credits_lines, ty = top + (bh - th) * m->scroll / (m->credits_lines - vis);
    fill(c, x0 + pw - 6 * u, top, 2 * u, bh, C_CARD);
    fill(c, x0 + pw - 6 * u, ty, 2 * u, th < 2 * u ? 2 * u : th, C_RIM);
  }
  text_fit(c, x0 + 6 * u, y0 + ph - 12 * u, pw - 12 * u, "UP DOWN: SCROLL   L R: PAGE   B: BACK", C_DIM, u);
}

void pcm_draw(struct pcm *m, uint32_t *px, int stride, int w, int h) {
  struct cv c;
  int u, pw, ph, x0, y0;
  c.px = px; c.stride = stride; c.w = w; c.h = h;
  u = h / 270; if (u < 1) u = 1; if (u > 8) u = 8;
  c.u = u;
  for (int y = 0; y < h; y++) memset(px + (size_t)y * stride, 0, (size_t)w * 4);
  if (!m->open) return;
  pw = 330 * u; if (pw > w - 8) pw = w - 8;
  ph = 250 * u;
  if (m->page == PCM_ROOT) ph = (24 + 15 * pcm_root_count(m) + 26) * u;
  else if (m->page == PCM_VIDEO) ph = (24 + 15 * PCM_V_COUNT + 26) * u;
  if (ph > h - 8) ph = h - 8;
  x0 = (w - pw) / 2; y0 = (h - ph) / 2;
  fill(&c, x0, y0, pw, ph, C_PANEL);
  frame(&c, x0, y0, pw, ph, u, C_DIM);
  switch (m->page) {
  case PCM_ROOT: { char v[64]; draw_list(&c, m, x0, y0, pw, ph, "SECRET OF MANA", pcm_root_count(m), root_row, v, 0); break; }
  case PCM_SAVE: case PCM_LOAD: draw_states(&c, m, x0, y0, pw, ph); break;
  case PCM_VIDEO: { char v[64]; draw_list(&c, m, x0, y0, pw, ph, "VIDEO", PCM_V_COUNT, video_row, v, 1);
                    text_fit(&c, x0 + 6 * u, y0 + ph - 12 * u, pw - 12 * u, "LEFT RIGHT: CHANGE   B: BACK", C_DIM, u); break; }
  case PCM_CREDITS: draw_credits(&c, m, x0, y0, pw, ph); break;
  default: break;
  }
  if (m->page == PCM_ROOT) text_fit(&c, x0 + 6 * u, y0 + ph - 12 * u, pw - 12 * u, m->in_game ? "A: SELECT   B: RESUME" : "A: SELECT", C_DIM, u);
  if (m->note[0]) text_fit(&c, x0 + 6 * u, y0 + ph - 24 * u, pw - 12 * u, m->note, C_WARN, u);
}
