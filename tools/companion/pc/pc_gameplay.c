/* Gameplay tweaks page, see pc_gameplay.h. */
#include "pc_gameplay.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ minimal JSON tree */
enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ };
struct jn { int type, first, next; double num; int b; char *s; char *key; };
struct jt { struct jn *n; int cnt, cap; char *own; };

static int jnew(struct jt *t, int type) {
  if (t->cnt == t->cap) {
    int c = t->cap ? t->cap * 2 : 128;
    struct jn *p = realloc(t->n, (size_t)c * sizeof *p);
    if (!p) return -1;
    t->n = p; t->cap = c;
  }
  memset(&t->n[t->cnt], 0, sizeof t->n[0]);
  t->n[t->cnt].type = type; t->n[t->cnt].first = -1; t->n[t->cnt].next = -1;
  return t->cnt++;
}
static const char *jws(const char *p) { while (*p && isspace((unsigned char)*p)) p++; return p; }
static char *jstr(struct jt *t, const char **pp) {                 /* *pp at the opening quote; the copy is owned by the tree */
  const char *p = *pp + 1;
  size_t n = 0;
  char *o = malloc(strlen(p) + 1);
  if (!o) return NULL;
  for (; *p && *p != '"'; p++) {
    if (*p == '\\' && p[1]) {
      p++;
      o[n++] = *p == 'n' ? '\n' : *p == 't' ? '\t' : *p == 'u' ? '?' : *p;
      if (*p == 'u') for (int i = 0; i < 4 && isxdigit((unsigned char)p[1]); i++) p++;
    } else o[n++] = *p;
  }
  if (*p != '"') { free(o); return NULL; }
  o[n] = 0;
  *pp = p + 1;
  (void)t;
  return o;
}
static int jparse(struct jt *t, const char **pp, int depth) {
  const char *p = jws(*pp);
  int id;
  if (depth > 16) return -1;
  if (*p == '{' || *p == '[') {
    int obj = *p == '{', last = -1;
    char close = obj ? '}' : ']';
    id = jnew(t, obj ? J_OBJ : J_ARR);
    if (id < 0) return -1;
    p = jws(p + 1);
    while (*p && *p != close) {
      char *key = NULL;
      int c;
      if (obj) {
        if (*p != '"') return -1;
        key = jstr(t, &p);
        if (!key) return -1;
        p = jws(p);
        if (*p != ':') { free(key); return -1; }
        p = jws(p + 1);
      }
      c = jparse(t, &p, depth + 1);
      if (c < 0) { free(key); return -1; }
      t->n[c].key = key;
      if (last < 0) t->n[id].first = c; else t->n[last].next = c;
      last = c;
      p = jws(p);
      if (*p == ',') p = jws(p + 1);
      else if (*p != close) return -1;
    }
    if (*p != close) return -1;
    *pp = p + 1;
    return id;
  }
  if (*p == '"') {
    char *s = jstr(t, &p);
    if (!s) return -1;
    id = jnew(t, J_STR);
    if (id < 0) { free(s); return -1; }
    t->n[id].s = s; *pp = p;
    return id;
  }
  if (!strncmp(p, "true", 4) || !strncmp(p, "false", 5)) {
    id = jnew(t, J_BOOL);
    if (id < 0) return -1;
    t->n[id].b = *p == 't'; *pp = p + (*p == 't' ? 4 : 5);
    return id;
  }
  if (!strncmp(p, "null", 4)) { id = jnew(t, J_NULL); *pp = p + 4; return id; }
  {
    char *e;
    double v = strtod(p, &e);
    if (e == p) return -1;
    id = jnew(t, J_NUM);
    if (id < 0) return -1;
    t->n[id].num = v; *pp = e;
    return id;
  }
}
static int jtree(struct jt *t, const char *text) {
  const char *p = text;
  memset(t, 0, sizeof *t);
  if (jparse(t, &p, 0) != 0) return -1;
  p = jws(p);
  return *p ? -1 : 0;
}
static void jfree(struct jt *t) {
  for (int i = 0; i < t->cnt; i++) { free(t->n[i].s); free(t->n[i].key); }
  free(t->n);
}
static int jget(const struct jt *t, int obj, const char *key) {
  if (obj < 0 || t->n[obj].type != J_OBJ) return -1;
  for (int c = t->n[obj].first; c >= 0; c = t->n[c].next) if (t->n[c].key && !strcmp(t->n[c].key, key)) return c;
  return -1;
}
static void jcopy(const struct jt *t, int obj, const char *key, char *dst, size_t n) {
  int c = jget(t, obj, key);
  dst[0] = 0;
  if (c >= 0 && t->n[c].type == J_STR) snprintf(dst, n, "%s", t->n[c].s);
}

/* ------------------------------------------------------------------ model */
static void load_presets(struct pg *g, const struct jt *t) {
  int arr = jget(t, 0, "presets");
  if (arr < 0 || t->n[arr].type != J_ARR) return;
  for (int c = t->n[arr].first; c >= 0 && g->npre < PG_MAX_PRE; c = t->n[c].next) {
    struct pg_preset *p = &g->pre[g->npre];
    char text[1024];
    size_t len;
    int set = jget(t, c, "set");
    if (t->n[c].type != J_OBJ || set < 0 || t->n[set].type != J_OBJ) continue;
    jcopy(t, c, "id", p->id, sizeof p->id); jcopy(t, c, "name", p->name, sizeof p->name); jcopy(t, c, "description", p->desc, sizeof p->desc);
    if (!p->name[0]) continue;
    len = (size_t)snprintf(text, sizeof text, "{\"options\": {");
    for (int k = t->n[set].first, first = 1; k >= 0 && len < sizeof text - 80; k = t->n[k].next) {
      int type = t->n[k].type;
      if (!t->n[k].key) continue;
      len += (size_t)snprintf(text + len, sizeof text - len, "%s\"%s\": ", first ? "" : ", ", t->n[k].key);
      if (type == J_BOOL) len += (size_t)snprintf(text + len, sizeof text - len, "%s", t->n[k].b ? "true" : "false");
      else if (type == J_NUM) len += (size_t)snprintf(text + len, sizeof text - len, "%d", (int)t->n[k].num);
      else len += (size_t)snprintf(text + len, sizeof text - len, "\"%s\"", type == J_STR ? t->n[k].s : "");
      first = 0;
    }
    snprintf(text + len, sizeof text - len, "}}");
    pg_reset(g);
    pg_load_values(g, text);
    for (int i = 0; i < g->n; i++) p->val[i] = (short)g->o[i].cur;
    pg_reset(g);
    g->npre++;
  }
}

int pg_load_menu(struct pg *g, const char *text) {
  struct jt t;
  int opts, ints, c;
  memset(g, 0, sizeof *g);
  if (jtree(&t, text) != 0) { jfree(&t); return -1; }
  opts = jget(&t, 0, "options");
  if (opts < 0 || t.n[opts].type != J_ARR) { jfree(&t); return -1; }
  ints = jget(&t, jget(&t, 0, "apply"), "integer_enums");
  for (c = t.n[opts].first; c >= 0 && g->n < PG_MAX_OPT; c = t.n[c].next) {
    struct pg_opt *o = &g->o[g->n];
    char type[16], def[40];
    int v, d;
    if (t.n[c].type != J_OBJ) continue;
    jcopy(&t, c, "key", o->key, sizeof o->key); jcopy(&t, c, "label", o->label, sizeof o->label);
    jcopy(&t, c, "group", o->group, sizeof o->group); jcopy(&t, c, "help", o->help, sizeof o->help);
    jcopy(&t, c, "type", type, sizeof type); jcopy(&t, c, "default", def, sizeof def);
    if (!o->key[0]) continue;
    if ((d = jget(&t, c, "advanced")) >= 0) o->advanced = t.n[d].type == J_BOOL && t.n[d].b;
    if ((d = jget(&t, c, "optional")) >= 0) o->optional = t.n[d].type == J_BOOL && t.n[d].b;
    if (!strcmp(type, "enum")) {
      o->type = PG_ENUM;
      d = jget(&t, c, "values");
      if (d < 0 || t.n[d].type != J_ARR) continue;
      for (v = t.n[d].first; v >= 0 && o->nvals < PG_MAX_VAL; v = t.n[v].next) {
        jcopy(&t, v, "v", o->val[o->nvals].v, sizeof o->val[0].v); jcopy(&t, v, "l", o->val[o->nvals].l, sizeof o->val[0].l);
        if (!strcmp(o->val[o->nvals].v, def)) o->def = o->nvals;
        o->nvals++;
      }
      if (o->nvals < 2) continue;
      if (ints >= 0) for (v = t.n[ints].first; v >= 0; v = t.n[v].next) if (t.n[v].type == J_STR && !strcmp(t.n[v].s, o->key)) o->integer = 1;
    } else if (!strcmp(type, "bool")) {
      o->type = PG_BOOL;
      o->def = !strcmp(def, "yes") || !strcmp(def, "true");
    } else if (!strcmp(type, "num")) {
      o->type = PG_NUM;
      o->min = (d = jget(&t, c, "min")) >= 0 ? (int)t.n[d].num : 0;
      o->max = (d = jget(&t, c, "max")) >= 0 ? (int)t.n[d].num : 255;
      o->def = def[0] ? atoi(def) : -1;
    } else continue;
    o->cur = o->def;
    g->n++;
  }
  if (g->n > 0) load_presets(g, &t);
  jfree(&t);
  return g->n > 0 ? 0 : -1;
}

void pg_reset(struct pg *g) { for (int i = 0; i < g->n; i++) g->o[i].cur = g->o[i].def; }
int pg_is_default(const struct pg *g) {
  for (int i = 0; i < g->n; i++) if (g->o[i].cur != g->o[i].def) return 0;
  return 1;
}

int pg_preset_match(const struct pg *g) {
  for (int p = 0; p < g->npre; p++) {
    int same = 1;
    for (int i = 0; i < g->n && same; i++) same = g->o[i].cur == g->pre[p].val[i];
    if (same) return p;
  }
  return -1;
}

int pg_to_json(const struct pg *g, char *out, size_t n) {
  size_t len = 0;
  int first = 1;
  int w = snprintf(out, n, "{\n \"options\": {");
  if (w < 0 || (size_t)w >= n) return -1;
  len = (size_t)w;
  for (int i = 0; i < g->n; i++) {
    const struct pg_opt *o = &g->o[i];
    char val[64];
    if (o->cur == o->def) continue;
    if (o->type == PG_BOOL) snprintf(val, sizeof val, "%s", o->cur ? "true" : "false");
    else if (o->type == PG_NUM) snprintf(val, sizeof val, "%d", o->cur);
    else if (o->integer) snprintf(val, sizeof val, "%s", o->val[o->cur].v);
    else snprintf(val, sizeof val, "\"%s\"", o->val[o->cur].v);
    w = snprintf(out + len, n - len, "%s\n  \"%s\": %s", first ? "" : ",", o->key, val);
    if (w < 0 || len + (size_t)w >= n) return -1;
    len += (size_t)w; first = 0;
  }
  w = snprintf(out + len, n - len, "%s}\n}\n", first ? "" : "\n ");
  if (w < 0 || len + (size_t)w >= n) return -1;
  return (int)(len + (size_t)w);
}

int pg_load_values(struct pg *g, const char *text) {
  struct jt t;
  int opts;
  if (jtree(&t, text) != 0) { jfree(&t); return -1; }
  opts = jget(&t, 0, "options");
  if (opts < 0 && t.cnt > 0 && t.n[0].type == J_OBJ) opts = 0;     /* a flat object is the same file for som_gameplay.py */
  if (opts < 0 || t.n[opts].type != J_OBJ) { jfree(&t); return -1; }
  for (int i = 0; i < g->n; i++) {
    struct pg_opt *o = &g->o[i];
    int c = jget(&t, opts, o->key);
    if (c < 0) continue;
    if (o->type == PG_BOOL && t.n[c].type == J_BOOL) o->cur = t.n[c].b;
    else if (o->type == PG_NUM && t.n[c].type == J_NUM) { int v = (int)t.n[c].num; if (v >= o->min && v <= o->max) o->cur = v; }
    else if (o->type == PG_ENUM) {
      char buf[40] = "";
      if (t.n[c].type == J_STR) snprintf(buf, sizeof buf, "%s", t.n[c].s);
      else if (t.n[c].type == J_NUM) snprintf(buf, sizeof buf, "%d", (int)t.n[c].num);
      for (int v = 0; v < o->nvals; v++) if (buf[0] && !strcmp(buf, o->val[v].v)) o->cur = v;
    }
  }
  jfree(&t);
  return 0;
}

static int visible(const struct pg *g, int i) { return !g->o[i].advanced || g->show_adv; }
int pg_entries(const struct pg *g) {
  int n = 0;
  for (int i = 0; i < g->n; i++) if (visible(g, i)) n++;
  return n + 3;
}
static int entry_opt(const struct pg *g, int e) {                    /* option index of entry e, or -1 */
  for (int i = 0; i < g->n; i++) if (visible(g, i) && e-- == 0) return i;
  return -1;
}
void pg_value_text(const struct pg_opt *o, int cur, char *out, size_t n) {
  if (o->type == PG_BOOL) snprintf(out, n, "%s", cur ? "Yes" : "No");
  else if (o->type == PG_NUM) { if (cur < 0) snprintf(out, n, "Default"); else snprintf(out, n, "%d", cur); }
  else snprintf(out, n, "%s", o->val[cur >= 0 && cur < o->nvals ? cur : 0].l);
}
static void step(struct pg_opt *o, int d) {
  if (o->type == PG_BOOL) o->cur = !o->cur;
  else if (o->type == PG_ENUM) o->cur = (o->cur + d + o->nvals) % o->nvals;
  else {
    int v = o->cur;
    if (v < 0) v = d > 0 ? o->min : (o->optional ? -1 : o->min);
    else { v += d; if (v < o->min) v = o->optional ? -1 : o->min; if (v > o->max) v = o->max; }
    o->cur = v;
  }
}
void pg_key(struct pg *g, int key) {
  int ne = pg_entries(g), nopt = ne - 3, oi;
  if (g->cursor >= ne) g->cursor = ne - 1;
  oi = g->cursor < nopt ? entry_opt(g, g->cursor) : -1;
  switch (key) {
  case PG_UP: g->cursor = (g->cursor + ne - 1) % ne; break;
  case PG_DOWN: g->cursor = (g->cursor + 1) % ne; break;
  case PG_LEFT: case PG_RIGHT: case PG_FAST_LEFT: case PG_FAST_RIGHT: {
    int d = (key == PG_LEFT || key == PG_FAST_LEFT) ? -1 : 1;
    if (oi >= 0) step(&g->o[oi], (key == PG_FAST_LEFT || key == PG_FAST_RIGHT) && g->o[oi].type == PG_NUM ? d * 10 : d);
    else if (g->cursor == nopt) g->show_adv = !g->show_adv;
    break;
  }
  case PG_ACCEPT:
    if (oi >= 0) { if (g->o[oi].type == PG_NUM && g->o[oi].cur >= 0) g->o[oi].cur = -1; else step(&g->o[oi], 1); }
    else if (g->cursor == nopt) g->show_adv = !g->show_adv;
    else if (g->cursor == nopt + 1) pg_reset(g);
    else g->done = 1;
    break;
  case PG_RESET: if (oi >= 0) g->o[oi].cur = g->o[oi].def; else pg_reset(g); break;
  case PG_BACK: g->done = 1; break;
  case PG_PRESET:
    if (g->npre > 0) {
      int next = (pg_preset_match(g) + 1) % g->npre;
      for (int i = 0; i < g->n; i++) g->o[i].cur = g->pre[next].val[i];
    }
    break;
  default: break;
  }
  ne = pg_entries(g);
  if (g->cursor >= ne) g->cursor = ne - 1;
}

/* ------------------------------------------------------------------ drawing (5x7 face, scaled) */
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
  {'[', {0x00, 0x7F, 0x41, 0x41, 0x00}}, {']', {0x00, 0x41, 0x41, 0x7F, 0x00}}, {'(', {0x00, 0x1C, 0x22, 0x41, 0x00}},
  {')', {0x00, 0x41, 0x22, 0x1C, 0x00}}, {'=', {0x14, 0x14, 0x14, 0x14, 0x14}}, {'_', {0x40, 0x40, 0x40, 0x40, 0x40}},
};
static void frect(uint32_t *px, int stride, int w, int h, int x, int y, int rw, int rh, uint32_t col) {
  if (x < 0) { rw += x; x = 0; }
  if (y < 0) { rh += y; y = 0; }
  if (x + rw > w) rw = w - x;
  if (y + rh > h) rh = h - y;
  for (int j = 0; j < rh; j++) for (int i = 0; i < rw; i++) px[(y + j) * stride + x + i] = col;
}
int pg_text_width(const char *s, int mul) { return (int)strlen(s) * 6 * mul; }
void pg_text(uint32_t *px, int stride, int w, int h, int x, int y, const char *s, uint32_t col, int mul) {
  for (; *s; s++, x += 6 * mul) {
    int ch = toupper((unsigned char)*s);
    const uint8_t *cols = NULL;
    for (unsigned f = 0; f < sizeof font5x7 / sizeof font5x7[0]; f++) if (font5x7[f].c == ch) { cols = font5x7[f].col; break; }
    if (!cols) continue;
    for (int i = 0; i < 5; i++) for (int j = 0; j < 7; j++) if (cols[i] & (1 << j)) frect(px, stride, w, h, x + i * mul, y + j * mul, mul, mul, col);
  }
}
/* word wrap into lines of at most `cols` characters; returns the number of lines drawn */
static int para(uint32_t *px, int stride, int w, int h, int x, int y, int cols, int lh, const char *s, uint32_t col, int mul, int max_lines) {
  int lines = 0;
  while (*s && lines < max_lines) {
    int n = (int)strlen(s), cut;
    char line[200];
    if (n > cols) { cut = cols; while (cut > 0 && s[cut] != ' ') cut--; if (cut == 0) cut = cols; } else cut = n;
    if (cut >= (int)sizeof line) cut = (int)sizeof line - 1;
    memcpy(line, s, (size_t)cut); line[cut] = 0;
    pg_text(px, stride, w, h, x, y + lines * lh, line, col, mul);
    s += cut; while (*s == ' ') s++;
    lines++;
  }
  return lines;
}

#define C_BG 0xFF101830u
#define C_PANEL 0xFF1C2A5Au
#define C_SEL 0xFF35508Cu
#define C_TXT 0xFFE8ECF8u
#define C_DIM 0xFF8C98C0u
#define C_HI 0xFFF0C040u
#define C_CHG 0xFF7CE08Cu

void pg_draw(const struct pg *g, uint32_t *px, int stride, int w, int h, const char *note) {
  int m = h / 300, rh, x0, y0, lw, vx, rows, first = 0, ne = pg_entries(g), nopt = ne - 3;
  char buf[160];
  if (m < 1) m = 1;
  rh = 11 * m;
  frect(px, stride, w, h, 0, 0, w, h, C_BG);
  x0 = w / 12; lw = w - 2 * x0; vx = x0 + lw * 5 / 9;
  pg_text(px, stride, w, h, x0, h / 20, "GAMEPLAY TWEAKS", C_HI, m * 2);
  pg_text(px, stride, w, h, x0, h / 20 + 16 * m, "APPLIED TO THE ROM WHEN THE GAME STARTS. THE ORIGINAL ROM STAYS UNTOUCHED.", C_DIM, m);
  if (g->npre > 0) {
    int pm = pg_preset_match(g);
    snprintf(buf, sizeof buf, "PRESET: %s", pm >= 0 ? g->pre[pm].name : "CUSTOM");
    pg_text(px, stride, w, h, x0 + lw - pg_text_width(buf, m * 2) , h / 20, buf, pm >= 0 ? C_CHG : C_DIM, m * 2);
  }
  y0 = h / 20 + 32 * m;
  rows = (h - y0 - 62 * m) / rh;
  if (rows < 4) rows = 4;
  {
    int row_e[PG_MAX_OPT * 2 + 4], row_g[PG_MAX_OPT * 2 + 4], nr = 0, crow = 0, y = y0;
    const char *lastg = "";
    for (int e = 0; e < ne; e++) {
      int oi = e < nopt ? entry_opt(g, e) : -1;
      if (oi >= 0 && strcmp(lastg, g->o[oi].group) != 0) { row_e[nr] = -1; row_g[nr++] = oi; lastg = g->o[oi].group; }
      if (e == g->cursor) crow = nr;
      row_e[nr] = e; row_g[nr++] = oi;
    }
    if (crow > 0 && row_e[crow - 1] < 0) crow--;                    /* keep the heading above the cursor */
    first = 0;
    while (first < crow && g->cursor < ne) {
      int cur_row = crow;
      for (int r = first; r < nr; r++) if (row_e[r] == g->cursor) cur_row = r;
      if (cur_row - first < rows) break;
      first++;
    }
    for (int r = first; r < nr && r - first < rows; r++, y += rh) {
      int e = row_e[r], oi = row_g[r], sel = e == g->cursor;
      const struct pg_opt *o = oi >= 0 ? &g->o[oi] : NULL;
      if (e < 0) { pg_text(px, stride, w, h, x0, y + m, g->o[oi].group, C_HI, m); continue; }
      frect(px, stride, w, h, x0 - 4 * m, y - m, lw + 8 * m, rh, sel ? C_SEL : C_PANEL);
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
    int oi = g->cursor < nopt ? entry_opt(g, g->cursor) : -1;
    int cols = lw / (6 * m);
    int hy = h - 54 * m;
    frect(px, stride, w, h, x0 - 4 * m, hy - 3 * m, lw + 8 * m, 36 * m, C_PANEL);
    if (oi >= 0) para(px, stride, w, h, x0, hy, cols, 9 * m, g->o[oi].help, C_TXT, m, 3);
    if (note && *note) pg_text(px, stride, w, h, x0, hy + 29 * m, note, C_HI, m);
    pg_text(px, stride, w, h, x0, h - 14 * m, g->npre > 0 ? "UP/DOWN: SELECT   LEFT/RIGHT: CHANGE   PGUP/PGDN: +-10   DELETE: DEFAULT   P: PRESET   ESC: BACK" : "UP/DOWN: SELECT   LEFT/RIGHT: CHANGE   PGUP/PGDN: +-10   DELETE: DEFAULT   ESC: BACK", C_DIM, m);
  }
}
