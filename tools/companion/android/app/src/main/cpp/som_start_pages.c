#include "som_start_pages.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc/pc_gameplay.h"
#include "som_gameplay_apply.h"

#define C_BG 0xFF101830u
#define C_PANEL 0xFF1C2A5Au
#define C_SEL 0xFF35508Cu
#define C_TXT 0xFFE8ECF8u
#define C_DIM 0xFF8C98C0u
#define C_HI 0xFFF0C040u
#define C_CHG 0xFF7CE08Cu
#define C_BTN 0xFF2C44A0u
#define C_EDGE 0xFFE8ECFFu

static struct pg gpg;
static int gp_ok, gp_dirty, page, cr_scroll, cr_lines, cr_vis;
static uint32_t rev_counter = 1;
static char credits[8192];

struct hit { int x, y, w, h, kind, a; };
static struct hit hits[PG_MAX_OPT * 2 + 16];
static int nhits;

static void bump(void) { rev_counter++; }

/* ------------------------------------------------------------------ model */
int ssp_gameplay_load(const char *menu_json, const char *values_json) {
  int k = 0;
  memset(&gpg, 0, sizeof gpg);
  gp_ok = menu_json && pg_load_menu(&gpg, menu_json) == 0;
  if (!gp_ok) return -1;
  for (int i = 0; i < gpg.n; i++)                                     /* drop what the C applier cannot do (a new option of the Python tool shows up in the parity test first) */
    if (sga_supports(gpg.o[i].key)) gpg.o[k++] = gpg.o[i];
  gpg.n = k;
  if (values_json) pg_load_values(&gpg, values_json);
  gp_dirty = 0;
  bump();
  return k;
}
int ssp_gameplay_json(char *out, size_t n) {
  int r;
  if (!gp_ok) { snprintf(out, n, "{\"options\":{}}"); return (int)strlen(out); }
  r = pg_to_json(&gpg, out, n);
  return r;
}
int ssp_gameplay_default(void) { return !gp_ok || pg_is_default(&gpg); }
void ssp_set_credits(const char *text) { snprintf(credits, sizeof credits, "%s", text ? text : ""); cr_scroll = 0; bump(); }

/* ------------------------------------------------------------------ credits text from data/credits.json */
static const char *jws(const char *p) { while (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r') p++; return p; }
/* reads a JSON string at p (the opening quote) into out as printable ASCII; returns the position after the closing quote or NULL */
static const char *jstr(const char *p, char *out, size_t n) {
  size_t k = 0;
  for (p++; *p && *p != '"'; p++) {
    unsigned c = (unsigned char)*p;
    if (c == '\\' && p[1]) {
      p++;
      if (*p == 'n') c = ' '; else if (*p == 'u') { c = '?'; for (int i = 0; i < 4 && p[1]; i++) p++; } else c = (unsigned char)*p;
    } else if (c >= 0x80) {                              /* UTF-8: Latin-1 letters lose their accent, everything else becomes ? */
      static const char base[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
      unsigned cp = 0;
      if (c >= 0xC0 && c < 0xE0 && ((unsigned char)p[1] & 0xC0) == 0x80) { cp = (c & 0x1Fu) << 6 | ((unsigned char)p[1] & 0x3Fu); p++; }
      else while (((unsigned char)p[1] & 0xC0) == 0x80) p++;
      c = cp >= 0xC0 && cp <= 0xFF ? (unsigned char)base[cp - 0xC0] : '?';
    }
    if (c < 32 || c > 126) c = '?';
    if (k + 1 < n) out[k++] = (char)c;
  }
  out[k] = 0;
  return *p == '"' ? p + 1 : NULL;
}
static const char *jskip(const char *p) {   /* skips one value */
  p = jws(p);
  if (*p == '"') { char t[2]; return jstr(p, t, sizeof t); }
  if (*p == '{' || *p == '[') {
    char close = *p == '{' ? '}' : ']';
    p = jws(p + 1);
    while (*p && *p != close) {
      if (close == '}') { char t[2]; p = jstr(jws(p), t, sizeof t); if (!p) return NULL; p = jws(p); if (*p == ':') p++; }
      p = jskip(p);
      if (!p) return NULL;
      p = jws(p);
      if (*p == ',') p = jws(p + 1);
    }
    return *p ? p + 1 : NULL;
  }
  while (*p && *p != ',' && *p != '}' && *p != ']') p++;
  return p;
}

int ssp_credits_text(const char *json, char *out, size_t n) {
  size_t k = 0;
  const char *p = jws(json ? json : ""), *entries = NULL;
  char title[96] = "SPECIAL THANKS", note[400] = "";
  if (*p != '{') { if (n) out[0] = 0; return -1; }
  p = jws(p + 1);
  while (*p && *p != '}') {
    char key[48];
    const char *v;
    p = jstr(p, key, sizeof key);
    if (!p) return -1;
    p = jws(p); if (*p == ':') p++;
    v = jws(p);
    if (!strcmp(key, "title") && *v == '"') jstr(v, title, sizeof title);
    else if (!strcmp(key, "note") && *v == '"') note[0] = 0;   /* the note is for the people editing the file, not for the player */
    else if (!strcmp(key, "entries") && *v == '[') entries = v;
    p = jskip(v);
    if (!p) return -1;
    p = jws(p); if (*p == ',') p = jws(p + 1);
  }
  (void)note;
#define PUT(...) do { if (k < n) k += (size_t)snprintf(out + k, n - k, __VA_ARGS__); } while (0)
  PUT("# %s\n\n", title);
  if (entries) {
    p = jws(entries + 1);
    while (*p && *p != ']') {
      char who[260] = "", lines[8][260];
      char keys[8][48];
      int nl = 0;
      if (*p != '{') { p = jskip(p); if (!p) break; p = jws(p); if (*p == ',') p = jws(p + 1); continue; }
      p = jws(p + 1);
      while (*p && *p != '}') {
        char key[48], val[260];
        p = jstr(p, key, sizeof key);
        if (!p) break;
        p = jws(p); if (*p == ':') p++;
        p = jws(p);
        if (*p == '"') {
          p = jstr(p, val, sizeof val);
          if (!p) break;
          if (!who[0] && (!strcmp(key, "who") || !strcmp(key, "author") || !strcmp(key, "name"))) snprintf(who, sizeof who, "%s", val);
          else if (nl < 8 && val[0] && strcmp(val, "n/a") && strcmp(val, "see survey")) { snprintf(lines[nl], sizeof lines[nl], "%s", val); snprintf(keys[nl++], sizeof keys[0], "%s", key); }
        } else { p = jskip(p); if (!p) break; }
        p = jws(p); if (*p == ',') p = jws(p + 1);
      }
      if (!p) break;
      if (*p == '}') p = jws(p + 1);
      if (who[0]) PUT("# %s\n", who);
      for (int i = 0; i < nl; i++) {
        if (!strcmp(keys[i], "what") || !strcmp(keys[i], "patch") || !strcmp(keys[i], "description")) PUT("%s\n", lines[i]);
        else if (!strcmp(keys[i], "url")) PUT("%s\n", lines[i]);
        else { char up[24]; size_t j = 0; for (; keys[i][j] && j < sizeof up - 1; j++) up[j] = keys[i][j] == '_' ? ' ' : keys[i][j]; up[j] = 0; PUT("%s: %s\n", up, lines[i]); }
      }
      PUT("\n");
      p = jws(p); if (*p == ',') p = jws(p + 1);
    }
  }
  if (k >= n && n) k = n - 1;
  if (n) out[k] = 0;
  return (int)k;
}
void ssp_set_credits_json(const char *json) { char t[8192]; if (ssp_credits_text(json, t, sizeof t) >= 0) ssp_set_credits(t); }

void ssp_open(int p) { page = p; cr_scroll = 0; if (p == SSP_GAMEPLAY) { gpg.done = 0; } bump(); }
void ssp_close(void) { page = SSP_NONE; bump(); }
int ssp_page(void) { return page; }
uint32_t ssp_rev(void) { return rev_counter; }
int ssp_dirty(int clear) { int d = gp_dirty; if (clear) gp_dirty = 0; return d; }

/* ------------------------------------------------------------------ drawing */
static void frect(uint32_t *px, int stride, int w, int h, int x, int y, int rw, int rh, uint32_t col) {
  if (x < 0) { rw += x; x = 0; }
  if (y < 0) { rh += y; y = 0; }
  if (x + rw > w) rw = w - x;
  if (y + rh > h) rh = h - y;
  for (int j = 0; j < rh; j++) for (int i = 0; i < rw; i++) px[(size_t)(y + j) * stride + x + i] = col;
}
static void hit_add(int x, int y, int w, int h, int kind, int a) {
  if (nhits < (int)(sizeof hits / sizeof hits[0])) { hits[nhits].x = x; hits[nhits].y = y; hits[nhits].w = w; hits[nhits].h = h; hits[nhits].kind = kind; hits[nhits++].a = a; }
}
static int scale_of(int h) { int m = h / 300; return m < 1 ? 1 : m; }

static void draw_back(uint32_t *px, int stride, int w, int h, int m, int x0, int y) {
  int bw = 34 * m, bh = 14 * m, bx = w - x0 - bw;
  frect(px, stride, w, h, bx, y, bw, bh, C_EDGE);
  frect(px, stride, w, h, bx + m, y + m, bw - 2 * m, bh - 2 * m, C_BTN);
  pg_text(px, stride, w, h, bx + (bw - pg_text_width("BACK", m)) / 2, y + (bh - 7 * m) / 2, "BACK", C_TXT, m);
  hit_add(bx, y, bw, bh, 3, 0);
}

static void draw_gameplay(uint32_t *px, int stride, int w, int h) {
  const struct pg *g = &gpg;
  int m = scale_of(h), rh = 11 * m, x0 = w / 12, lw = w - 2 * x0, vx = x0 + lw * 5 / 9, ne = pg_entries(g), nopt = ne - 3, y0, rows, first = 0;
  char buf[160];
  frect(px, stride, w, h, 0, 0, w, h, C_BG);
  pg_text(px, stride, w, h, x0, h / 20, "GAMEPLAY TWEAKS", C_HI, m * 2);
  pg_text(px, stride, w, h, x0, h / 20 + 16 * m, "APPLIED WHEN THE GAME STARTS. YOUR ROM STAYS UNTOUCHED.", C_DIM, m);
  draw_back(px, stride, w, h, m, x0, h / 20);
  y0 = h / 20 + 32 * m;
  rows = (h - y0 - 62 * m) / rh;
  if (rows < 4) rows = 4;
  {
    int row_e[PG_MAX_OPT * 2 + 4], row_o[PG_MAX_OPT * 2 + 4], nr = 0, crow = 0, y = y0;
    const char *lastg = "";
    for (int e = 0; e < ne; e++) {
      int oi = -1;
      if (e < nopt) { int c = e; for (int i = 0; i < g->n; i++) if (!g->o[i].advanced || g->show_adv) { if (c-- == 0) { oi = i; break; } } }
      if (oi >= 0 && strcmp(lastg, g->o[oi].group) != 0) { row_e[nr] = -1; row_o[nr++] = oi; lastg = g->o[oi].group; }
      if (e == g->cursor) crow = nr;
      row_e[nr] = e; row_o[nr++] = oi;
    }
    if (crow > 0 && row_e[crow - 1] < 0) crow--;
    while (first < crow && crow - first >= rows) first++;
    for (int r = first; r < nr && r - first < rows; r++, y += rh) {
      int e = row_e[r], oi = row_o[r], sel = e == g->cursor;
      const struct pg_opt *o = oi >= 0 ? &g->o[oi] : NULL;
      if (e < 0) { pg_text(px, stride, w, h, x0, y + m, g->o[oi].group, C_HI, m); continue; }
      frect(px, stride, w, h, x0 - 4 * m, y - m, lw + 8 * m, rh, sel ? C_SEL : C_PANEL);
      hit_add(x0 - 4 * m, y - m, lw + 8 * m, rh, 1, e);
      if (o) {
        pg_text(px, stride, w, h, x0 + 8 * m, y + m, o->label, sel ? C_TXT : C_DIM, m);
        pg_value_text(o, o->cur, buf, sizeof buf);
        if (sel) { char b2[180]; snprintf(b2, sizeof b2, "< %s >", buf); pg_text(px, stride, w, h, vx, y + m, b2, o->cur != o->def ? C_CHG : C_TXT, m); }
        else pg_text(px, stride, w, h, vx + 12 * m, y + m, buf, o->cur != o->def ? C_CHG : C_TXT, m);
      } else if (e == nopt) {
        snprintf(buf, sizeof buf, "SHOW ADVANCED OPTIONS: %s", g->show_adv ? "YES" : "NO");
        pg_text(px, stride, w, h, x0 + 8 * m, y + m, buf, sel ? C_TXT : C_DIM, m);
      } else pg_text(px, stride, w, h, x0 + 8 * m, y + m, e == nopt + 1 ? "RESET ALL TO VANILLA" : "DONE", sel ? C_TXT : C_DIM, m);
    }
  }
  {
    int oi = -1, hy = h - 54 * m, cols = lw / (6 * m);
    if (g->cursor < nopt) { int c = g->cursor; for (int i = 0; i < g->n; i++) if (!g->o[i].advanced || g->show_adv) { if (c-- == 0) { oi = i; break; } } }
    frect(px, stride, w, h, x0 - 4 * m, hy - 3 * m, lw + 8 * m, 36 * m, C_PANEL);
    if (oi >= 0) {   /* help text, wrapped to three lines */
      const char *s = g->o[oi].help;
      for (int ln = 0; ln < 3 && *s; ln++) {
        int n = (int)strlen(s), cut = n > cols ? cols : n;
        char line[200];
        if (n > cols) { while (cut > 0 && s[cut] != ' ') cut--; if (cut == 0) cut = cols; }
        if (cut >= (int)sizeof line) cut = (int)sizeof line - 1;
        memcpy(line, s, (size_t)cut); line[cut] = 0;
        pg_text(px, stride, w, h, x0, hy + ln * 9 * m, line, C_TXT, m);
        s += cut; while (*s == ' ') s++;
      }
    }
    pg_text(px, stride, w, h, x0, h - 14 * m, "UP/DOWN SELECT   LEFT/RIGHT CHANGE   Y DEFAULT   B BACK", C_DIM, m);
  }
}

struct cr_line { int off, len, head; };
static int cr_wrap(int cols, struct cr_line *out, int max) {
  int n = 0, i = 0;
  const char *t = credits;
  while (t[i] && n < max) {
    int e = i;
    while (t[e] && t[e] != '\n') e++;
    int head = t[i] == '#' && t[i + 1] == ' ', a = i + (head ? 2 : 0);
    if (a >= e) { out[n].off = a; out[n].len = 0; out[n++].head = 0; }
    while (a < e && n < max) {
      int take = e - a;
      if (take > cols) { take = cols; while (take > 0 && t[a + take] != ' ') take--; if (take == 0) take = cols; }
      out[n].off = a; out[n].len = take; out[n++].head = head;
      a += take;
      while (a < e && t[a] == ' ') a++;
    }
    i = t[e] ? e + 1 : e;
  }
  return n;
}

static void draw_credits(uint32_t *px, int stride, int w, int h) {
  static struct cr_line ln[600];
  int m = scale_of(h), x0 = w / 12, lw = w - 2 * x0, y0 = h / 20 + 24 * m, lh = 10 * m, cols = lw / (6 * m), n, vis;
  frect(px, stride, w, h, 0, 0, w, h, C_BG);
  pg_text(px, stride, w, h, x0, h / 20, "CREDITS", C_HI, m * 2);
  draw_back(px, stride, w, h, m, x0, h / 20);
  n = cr_wrap(cols, ln, 600);
  vis = (h - y0 - 22 * m) / lh;
  if (vis < 3) vis = 3;
  cr_lines = n; cr_vis = vis;
  if (cr_scroll > n - vis) cr_scroll = n - vis > 0 ? n - vis : 0;
  if (cr_scroll < 0) cr_scroll = 0;
  frect(px, stride, w, h, x0 - 4 * m, y0 - 3 * m, lw + 8 * m, vis * lh + 6 * m, C_PANEL);
  for (int k = 0; k < vis && cr_scroll + k < n; k++) {
    const struct cr_line *q = &ln[cr_scroll + k];
    char b[200];
    int len = q->len < (int)sizeof b - 1 ? q->len : (int)sizeof b - 1;
    memcpy(b, credits + q->off, (size_t)len); b[len] = 0;
    if (len) pg_text(px, stride, w, h, x0, y0 + k * lh, b, q->head ? C_HI : C_TXT, m);
  }
  if (n > vis) {
    int bh = vis * lh, th = bh * vis / n > 6 * m ? bh * vis / n : 6 * m, ty = y0 + (bh - th) * cr_scroll / (n - vis);
    frect(px, stride, w, h, x0 + lw + 2 * m, y0, 2 * m, bh, C_BG);
    frect(px, stride, w, h, x0 + lw + 2 * m, ty, 2 * m, th, C_SEL);
  }
  hit_add(x0, y0, lw, vis * lh / 2, 4, -1);                    /* upper half of the text: page up, lower half: page down */
  hit_add(x0, y0 + vis * lh / 2, lw, vis * lh - vis * lh / 2, 4, 1);
  pg_text(px, stride, w, h, x0, h - 14 * m, "UP/DOWN SCROLL   L/R PAGE   B BACK", C_DIM, m);
}

void ssp_draw(uint32_t *px, int stride, int w, int h) {
  nhits = 0;
  if (page == SSP_GAMEPLAY) draw_gameplay(px, stride, w, h);
  else if (page == SSP_CREDITS) draw_credits(px, stride, w, h);
}

/* ------------------------------------------------------------------ input */
static void gp_key(int key) {
  int before = pg_is_default(&gpg), cur0 = gpg.cursor;
  char a[2048], b[2048];
  pg_to_json(&gpg, a, sizeof a);
  pg_key(&gpg, key);
  pg_to_json(&gpg, b, sizeof b);
  if (strcmp(a, b)) gp_dirty = 1;
  (void)before; (void)cur0;
  bump();
}

int ssp_key(int button) {
  if (page == SSP_NONE) return 0;
  if (page == SSP_GAMEPLAY) {
    switch (button) {
    case SSP_UP: gp_key(PG_UP); break;
    case SSP_DOWN: gp_key(PG_DOWN); break;
    case SSP_LEFT: gp_key(PG_LEFT); break;
    case SSP_RIGHT: gp_key(PG_RIGHT); break;
    case SSP_L: gp_key(PG_FAST_LEFT); break;
    case SSP_R: gp_key(PG_FAST_RIGHT); break;
    case SSP_A: gp_key(PG_ACCEPT); break;
    case SSP_Y: gp_key(PG_RESET); break;
    case SSP_B: case SSP_START: ssp_close(); break;
    default: break;
    }
    if (gpg.done) { gpg.done = 0; ssp_close(); }
    return 1;
  }
  switch (button) {
  case SSP_UP: ssp_scroll(-1); break;
  case SSP_DOWN: ssp_scroll(1); break;
  case SSP_L: ssp_scroll(-(cr_vis > 2 ? cr_vis - 1 : 1)); break;
  case SSP_R: ssp_scroll(cr_vis > 2 ? cr_vis - 1 : 1); break;
  case SSP_B: case SSP_A: case SSP_START: ssp_close(); break;
  default: break;
  }
  return 1;
}

void ssp_scroll(int rows) {
  if (page == SSP_GAMEPLAY) { for (int i = 0; i < (rows < 0 ? -rows : rows); i++) gp_key(rows < 0 ? PG_UP : PG_DOWN); return; }
  if (page != SSP_CREDITS) return;
  cr_scroll += rows;
  if (cr_scroll < 0) cr_scroll = 0;
  if (cr_scroll > cr_lines - cr_vis) cr_scroll = cr_lines - cr_vis > 0 ? cr_lines - cr_vis : 0;
  bump();
}

int ssp_tap(int x, int y, int w, int h) {
  (void)w; (void)h;
  if (page == SSP_NONE) return 0;
  for (int i = nhits - 1; i >= 0; i--) {
    const struct hit *t = &hits[i];
    if (x < t->x || y < t->y || x >= t->x + t->w || y >= t->y + t->h) continue;
    if (t->kind == 3) { ssp_close(); return 1; }
    if (t->kind == 4) { ssp_scroll(t->a * (cr_vis > 2 ? cr_vis - 1 : 1)); return 1; }
    if (t->kind == 1) {   /* a row: the first tap takes the focus and steps the value on the value side; the label side only focuses */
      int vx = w / 12 + (w - 2 * (w / 12)) * 5 / 9, vend = w - w / 12;
      while (gpg.cursor < t->a) gp_key(PG_DOWN);
      while (gpg.cursor > t->a) gp_key(PG_UP);
      if (x >= vx) gp_key(x < vx + (vend - vx) / 3 ? PG_LEFT : PG_RIGHT);
      else if (gpg.cursor >= pg_entries(&gpg) - 3) gp_key(PG_ACCEPT);   /* the toggle, RESET and DONE rows act on a tap */
      return 1;
    }
  }
  return 1;
}

/* ------------------------------------------------------------------ main-screen footer */
static void footer_rects(int w, int h, int r[2][4]) {
  int m = scale_of(h), bh = 22 * m, y = h - bh - 6 * m, bw = (w - 8 * m * 3) / 2;
  r[0][0] = 8 * m; r[0][1] = y; r[0][2] = bw; r[0][3] = bh;
  r[1][0] = 16 * m + bw; r[1][1] = y; r[1][2] = bw; r[1][3] = bh;
}

void ssp_draw_footer(uint32_t *px, int stride, int w, int h, int gameplay_active) {
  int r[2][4], m = scale_of(h);
  static const char *const lab[2] = {"X  GAMEPLAY", "Y  CREDITS"};
  footer_rects(w, h, r);
  frect(px, stride, w, h, 0, r[0][1] - 4 * m, w, h - (r[0][1] - 4 * m), C_BG);   /* covers the old one-line credit text; the credits page carries it */
  for (int i = 0; i < 2; i++) {
    uint32_t edge = i == 0 && gameplay_active ? C_CHG : C_EDGE;
    frect(px, stride, w, h, r[i][0], r[i][1], r[i][2], r[i][3], edge);
    frect(px, stride, w, h, r[i][0] + m, r[i][1] + m, r[i][2] - 2 * m, r[i][3] - 2 * m, C_BTN);
    pg_text(px, stride, w, h, r[i][0] + (r[i][2] - pg_text_width(lab[i], m * 2)) / 2, r[i][1] + (r[i][3] - 14 * m) / 2, lab[i], C_TXT, m * 2);
  }
}

int ssp_footer_hit(int x, int y, int w, int h) {
  int r[2][4];
  footer_rects(w, h, r);
  for (int i = 0; i < 2; i++)
    if (x >= r[i][0] && y >= r[i][1] && x < r[i][0] + r[i][2] && y < r[i][1] + r[i][3]) return i == 0 ? SSP_H_GAMEPLAY : SSP_H_CREDITS;
  return SSP_H_NONE;
}
