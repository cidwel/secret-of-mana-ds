/* Pure logic of the SoM PC frontend, see pc_logic.h. */
#include "pc_logic.h"
#include "pc_shader.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- tiny JSON helpers (flat objects, no escapes beyond \" and \\) ---- */
static const char *jfind(const char *t, const char *key) {
  char pat[64];
  snprintf(pat, sizeof pat, "\"%s\"", key);
  const char *p = strstr(t, pat);
  if (!p) return NULL;
  p += strlen(pat);
  while (*p && isspace((unsigned char)*p)) p++;
  if (*p != ':') return NULL;
  p++;
  while (*p && isspace((unsigned char)*p)) p++;
  return p;
}
static int jint(const char *t, const char *key, int *v) {
  const char *p = jfind(t, key);
  char *e;
  long x;
  if (!p) return 0;
  if (!strncmp(p, "true", 4)) { *v = 1; return 1; }
  if (!strncmp(p, "false", 5)) { *v = 0; return 1; }
  x = strtol(p, &e, 10);
  if (e == p) return 0;
  *v = (int)x;
  return 1;
}
static int jstr(const char *t, const char *key, char *out, size_t n) {
  const char *p = jfind(t, key);
  size_t i = 0;
  if (!p || *p != '"') return 0;
  p++;
  while (*p && *p != '"' && i + 1 < n) {
    if (*p == '\\' && p[1]) p++;
    out[i++] = *p++;
  }
  out[i] = 0;
  return 1;
}
static void jesc(char *out, size_t n, const char *s) {
  size_t i = 0;
  for (; *s && i + 2 < n; s++) {
    if (*s == '"' || *s == '\\') out[i++] = '\\';
    out[i++] = *s;
  }
  out[i] = 0;
}

/* ---- configuration ---- */
void pc_cfg_defaults(struct pc_cfg *c) {
  memset(c, 0, sizeof *c);
  c->wide = 46; c->fit = PC_FIT_FIT; c->hud_hide = 0; c->screen = PC_WINDOWED;
  c->win_w = 1920; c->win_h = 1080; c->vsync = 1; c->overlay = PC_OV_SMALL; c->corner = PC_TOP_RIGHT; c->names = 1;
  c->fire_on_press = 0; c->fallback_black = 0; c->volume = 100;
  c->shader = 1; c->state_slot = 0; c->excl_w = 1920; c->excl_h = 1080;
}
int pc_valid_wide(int n) { return n == 0 || n == 17 || n == 27 || n == 46; }
int pc_next_wide(int n) { return n == 0 ? 17 : n == 17 ? 27 : n == 27 ? 46 : 0; }
const char *pc_aspect_name(int wide) { return wide == 46 ? "WIDE 46" : wide == 27 ? "WIDE 27" : wide == 17 ? "WIDE 17" : "4:3"; }
int pc_aspect_step(int wide, int dir) {
  static const int ring[4] = {46, 27, 17, 0};
  int i = 0;
  for (int k = 0; k < 4; k++) if (ring[k] == wide) i = k;
  return ring[(i + (dir < 0 ? 3 : 1)) % 4];
}
int pc_fit_toggle_integer(int fit) { return fit == PC_FIT_INTEGER ? PC_FIT_FIT : PC_FIT_INTEGER; }
int pc_fit_step(int fit, int dir) {
  static const int ring[3] = {PC_FIT_FIT, PC_FIT_FILL, PC_FIT_STRETCH};
  int i = 0;
  for (int k = 0; k < 3; k++) if (ring[k] == fit) i = k;
  return ring[(i + (dir < 0 ? 2 : 1)) % 3];
}
static const struct { int w, h; const char *n; } WIN_PRESETS[] = {{1280, 720, "720P"}, {1600, 900, "900P"}, {1920, 1080, "1080P"}, {2560, 1440, "1440P"}, {3840, 2160, "2160P"}};
#define N_WIN_PRESETS 5
int pc_win_valid(int w, int h) { return w >= 320 && h >= 240 && w <= 16384 && h <= 16384; }
int pc_win_step(int w, int h, int dir, int *nw, int *nh) {
  int i = -1;
  for (int k = 0; k < N_WIN_PRESETS; k++) if (WIN_PRESETS[k].w == w && WIN_PRESETS[k].h == h) i = k;
  if (i < 0) {                                              /* custom: nearest larger preset for +1, nearest smaller for -1 */
    int up = N_WIN_PRESETS - 1, down = 0;
    for (int k = N_WIN_PRESETS - 1; k >= 0; k--) if (WIN_PRESETS[k].h >= h && WIN_PRESETS[k].w >= w) up = k;
    for (int k = 0; k < N_WIN_PRESETS; k++) if (WIN_PRESETS[k].h <= h && WIN_PRESETS[k].w <= w) down = k;
    i = dir < 0 ? down : up;
    *nw = WIN_PRESETS[i].w; *nh = WIN_PRESETS[i].h;
    return i;
  }
  i = (i + (dir < 0 ? N_WIN_PRESETS - 1 : 1)) % N_WIN_PRESETS;
  *nw = WIN_PRESETS[i].w; *nh = WIN_PRESETS[i].h;
  return i;
}
void pc_win_name(int w, int h, char *out, size_t n) {
  for (int k = 0; k < N_WIN_PRESETS; k++) if (WIN_PRESETS[k].w == w && WIN_PRESETS[k].h == h) { snprintf(out, n, "%s", WIN_PRESETS[k].n); return; }
  snprintf(out, n, "CUSTOM %dX%d", w, h);
}
const char *pc_fit_name(int fit) {
  static const char *const n[PC_FIT_COUNT] = {"FILL", "FIT", "INTEGER", "STRETCH"};
  return fit >= 0 && fit < PC_FIT_COUNT ? n[fit] : "?";
}
int pc_cfg_to_json(const struct pc_cfg *c, char *out, size_t n) {
  char rom[1100], core[1100], pack[1100];
  int r;
  jesc(rom, sizeof rom, c->rom); jesc(core, sizeof core, c->core); jesc(pack, sizeof pack, c->pack);
  r = snprintf(out, n,
    "{\n  \"version\": 1,\n  \"wide\": %d,\n  \"fit\": %d,\n  \"hud_hide\": %d,\n  \"screen\": %d,\n  \"win_w\": %d,\n  \"win_h\": %d,\n"
    "  \"vsync\": %d,\n  \"overlay\": %d,\n  \"corner\": %d,\n  \"names\": %d,\n  \"fire_on_press\": %d,\n  \"fallback_black\": %d,\n  \"volume\": %d,\n"
    "  \"shader\": %d,\n  \"state_slot\": %d,\n  \"excl_w\": %d,\n  \"excl_h\": %d,\n"
    "  \"rom\": \"%s\",\n  \"core\": \"%s\",\n  \"pack\": \"%s\"\n}\n",
    c->wide, c->fit, c->hud_hide, c->screen, c->win_w, c->win_h, c->vsync, c->overlay, c->corner, c->names, c->fire_on_press,
    c->fallback_black, c->volume, c->shader, c->state_slot, c->excl_w, c->excl_h, rom, core, pack);
  return r < 0 || (size_t)r >= n ? -1 : r;
}
int pc_cfg_from_json(struct pc_cfg *c, const char *t) {
  int v;
  const char *p = t;
  while (*p && isspace((unsigned char)*p)) p++;
  if (*p != '{') return -1;
  if (jint(t, "wide", &v) && pc_valid_wide(v)) c->wide = v;
  if (jint(t, "fit", &v) && v >= 0 && v < PC_FIT_COUNT) c->fit = v;
  if (jint(t, "hud_hide", &v)) c->hud_hide = v != 0;
  if (jint(t, "screen", &v) && v >= 0 && v < PC_SCREEN_COUNT) c->screen = v;
  if (jint(t, "win_w", &v) && v >= 320 && v <= 16384) c->win_w = v;
  if (jint(t, "win_h", &v) && v >= 240 && v <= 16384) c->win_h = v;
  if (jint(t, "vsync", &v)) c->vsync = v != 0;
  if (jint(t, "overlay", &v) && v >= 0 && v < PC_OV_COUNT) c->overlay = v;
  if (jint(t, "corner", &v) && v >= 0 && v < PC_CORNER_COUNT) c->corner = v;
  if (jint(t, "names", &v)) c->names = v != 0;
  if (jint(t, "fire_on_press", &v)) c->fire_on_press = v != 0;
  if (jint(t, "fallback_black", &v)) c->fallback_black = v != 0;
  if (jint(t, "volume", &v) && v >= 0 && v <= 100) c->volume = v;
  if (jint(t, "shader", &v) && v >= 0 && v < PC_SH_COUNT) c->shader = v;
  if (jint(t, "state_slot", &v) && v >= 0 && v < 10) c->state_slot = v;
  if (jint(t, "excl_w", &v) && v >= 320 && v <= 16384) c->excl_w = v;
  if (jint(t, "excl_h", &v) && v >= 240 && v <= 16384) c->excl_h = v;
  jstr(t, "rom", c->rom, sizeof c->rom);
  jstr(t, "core", c->core, sizeof c->core);
  jstr(t, "pack", c->pack, sizeof c->pack);
  return 0;
}

/* ---- profile (bindings of one ROM) ---- */
int pc_profile_to_json(const struct pc_profile *p, const char *variant, char *out, size_t n) {
  static const char *const names[SOM_UX_SHOULDERS] = {"L1", "R1", "L2", "R2"};
  size_t o = 0;
  int r = snprintf(out, n, "{\n  \"version\": 1,\n  \"variant\": \"%s\",\n  \"slots\": {\n", variant ? variant : "");
  if (r < 0 || (size_t)r >= n) return -1;
  o = (size_t)r;
  for (int i = 0; i < SOM_UX_SHOULDERS; i++) {
    r = snprintf(out + o, n - o, "    \"%s\": [%d, %d, %d]%s\n", names[i], p->slot[i].kind, p->slot[i].id, p->slot[i].policy, i + 1 < SOM_UX_SHOULDERS ? "," : "");
    if (r < 0 || (size_t)r >= n - o) return -1;
    o += (size_t)r;
  }
  r = snprintf(out + o, n - o, "  }\n}\n");
  if (r < 0 || (size_t)r >= n - o) return -1;
  return (int)(o + (size_t)r);
}
int pc_profile_from_json(struct pc_profile *p, const char *t) {
  static const char *const names[SOM_UX_SHOULDERS] = {"L1", "R1", "L2", "R2"};
  const char *s = strstr(t, "\"slots\"");
  if (!s) return -1;
  memset(p, 0, sizeof *p);
  for (int i = 0; i < SOM_UX_SHOULDERS; i++) {
    const char *q = jfind(s, names[i]);
    int k, id, pol;
    if (!q || *q != '[') continue;
    if (sscanf(q, "[%d , %d , %d", &k, &id, &pol) != 3) continue;
    if (k < 0 || k > SOM_UX_BK_LAST || id < 0 || id > 255 || pol < 0 || pol >= SOM_UX_POL_COUNT) continue;
    p->slot[i].kind = (uint8_t)k; p->slot[i].id = (uint8_t)(k ? id : 0); p->slot[i].policy = (uint8_t)pol;
  }
  return 0;
}

int pc_json_complete(const char *t) {
  int depth = 0, in = 0, seen = 0;
  while (*t && isspace((unsigned char)*t)) t++;
  if (*t != '{') return 0;
  for (; *t; t++) {
    if (in) { if (*t == '\\' && t[1]) t++; else if (*t == '"') in = 0; continue; }
    if (*t == '"') in = 1;
    else if (*t == '{' || *t == '[') { depth++; seen = 1; }
    else if (*t == '}' || *t == ']') { if (--depth < 0) return 0; if (depth == 0) { for (t++; *t; t++) if (!isspace((unsigned char)*t)) return 0; return 1; } }
  }
  return seen && depth == 0 && !in;
}

/* ---- picture fitting ---- */
double pc_aspect(int wide) { return 4.0 / 3.0 * (256 + 2 * wide) / 256.0; }
void pc_fit_rects(int out_w, int out_h, int frame_w, int frame_h, int wide, int fit, struct pc_rect *d) {
  int full_w = 256 + 2 * wide;
  double asp = pc_aspect(wide), sx, sy;
  if (frame_w < 1 || frame_h < 1 || out_w < 1 || out_h < 1) { d->x = d->y = 0; d->w = out_w; d->h = out_h; return; }
  if (frame_w > full_w) full_w = frame_w;       /* the core is wider than configured (state loaded with another option) */
  switch (fit) {
  case PC_FIT_STRETCH: sx = (double)out_w / full_w; sy = (double)out_h / frame_h; break;
  case PC_FIT_INTEGER: {
    double pxw = asp * frame_h / full_w;          /* pixel width in pixel heights, about 7/6 */
    int k = (int)((double)out_h / frame_h), kx;
    while (k > 1 && (int)(k * pxw + 0.5) * full_w > out_w) k--;
    if (k < 1) k = 1;
    kx = (int)(k * pxw + 0.5);
    if (kx < 1) kx = 1;
    sx = kx; sy = k;
    break;
  }
  case PC_FIT_FILL: {
    double w = out_w, h = w / asp;
    if (h < out_h) { h = out_h; w = h * asp; }
    sx = w / full_w; sy = h / frame_h;
    break;
  }
  default: {
    double w = out_w, h = w / asp;
    if (h > out_h) { h = out_h; w = h * asp; }
    sx = w / full_w; sy = h / frame_h;
    break;
  }
  }
  d->w = (int)(frame_w * sx + 0.5);
  d->h = (int)(frame_h * sy + 0.5);
  d->x = (out_w - (int)(full_w * sx + 0.5)) / 2 + (int)((full_w - frame_w) / 2 * sx + 0.5);
  d->y = (out_h - d->h) / 2;
}

/* ---- shoulder buttons ---- */
void pc_sh_init(struct pc_sh *s, int fire_on_press) { memset(s, 0, sizeof *s); s->fire_on_press = (uint8_t)(fire_on_press != 0); }
void pc_sh_update(struct pc_sh *s, uint8_t held, uint32_t now, struct pc_sh_out *o) {
  o->fire = -1; o->assign = -1;
  for (int i = 0; i < SOM_UX_SHOULDERS; i++) {
    int was = (s->held >> i) & 1, is = (held >> i) & 1;
    o->progress[i] = 0;
    if (is && !was) {
      s->t0[i] = now; s->consumed[i] = 0;
      if (s->fire_on_press) { if (o->fire < 0) o->fire = i; s->consumed[i] = 1; }
    }
    if (is && !s->consumed[i] && !s->fire_on_press) {
      uint32_t dt = now - s->t0[i];
      if (dt >= PC_HOLD_MS) { if (o->assign < 0) o->assign = i; s->consumed[i] = 1; }
      else if (dt >= PC_HOLD_SHOW_MS) o->progress[i] = (int)((dt - PC_HOLD_SHOW_MS) * 100 / (PC_HOLD_MS - PC_HOLD_SHOW_MS));
    }
    if (!is && was && !s->consumed[i]) { if (o->fire < 0) o->fire = i; }
  }
  s->held = held;
}

void pc_sh_cancel(struct pc_sh *s) {
  s->held = 0;
  for (int i = 0; i < SOM_UX_SHOULDERS; i++) s->consumed[i] = 1;
}
int pc_trigger_step(int prev, int axis) {
  if (axis > 19600) return 1;
  if (axis < 13100) return 0;
  return prev ? 1 : 0;
}
int pc_stick_dir(int axis) { return axis < -16000 ? -1 : axis > 16000 ? 1 : 0; }
int pc_screen_toggle(int cur, int shift) {
  if (shift) return cur == PC_EXCLUSIVE ? PC_WINDOWED : PC_EXCLUSIVE;
  return cur == PC_WINDOWED ? PC_BORDERLESS : PC_WINDOWED;
}
int pc_screen_fallback(int mode) { return mode == PC_EXCLUSIVE ? PC_BORDERLESS : PC_WINDOWED; }
void pc_audio_plan(double q, double bpf, struct pc_audio_plan *o) {
  o->frames = 0; o->underrun = 0; o->overflow = 0;
  if (bpf <= 0) return;
  if (q > bpf * PC_AUDIO_MAX) { o->overflow = 1; return; }
  if (q <= 0) { o->underrun = 1; o->frames = PC_AUDIO_BURST; return; }
  while (q < bpf * PC_AUDIO_TARGET && o->frames < 3) { o->frames++; q += bpf; }
}

/* ---- gamepad navigation ---- */
int pc_nav_pick(const struct som_ux_hitrect *h, int n, int from, int dx, int dy) {
  int best = from;
  double bs = 1e30, cx, cy;
  if (from < 0 || from >= n) {
    for (int i = 0; i < n; i++) if (h[i].enabled) return i;
    return from;
  }
  cx = h[from].x + h[from].w / 2.0; cy = h[from].y + h[from].h / 2.0;
  for (int i = 0; i < n; i++) {
    double x = h[i].x + h[i].w / 2.0, y = h[i].y + h[i].h / 2.0, ax, ay, along, across;
    if (i == from || !h[i].enabled) continue;
    ax = x - cx; ay = y - cy;
    along = dx ? ax * dx : ay * dy;
    across = dx ? ay : ax;
    if (across < 0) across = -across;
    if (along <= 0.5) continue;
    {
      /* a candidate must lie in a 45 degree cone, or overlap the current rectangle on the perpendicular axis */
      int lo_a = dx ? h[i].y : h[i].x, hi_a = lo_a + (dx ? h[i].h : h[i].w);
      int lo_f = dx ? h[from].y : h[from].x, hi_f = lo_f + (dx ? h[from].h : h[from].w);
      int overlap = lo_a < hi_f && lo_f < hi_a;
      if (!overlap && across > along) continue;
    }
    double sc = along + across * 2.0;
    if (sc < bs) { bs = sc; best = i; }
  }
  return best;
}

uint32_t pc_hash_pixels(const uint32_t *px, int stride, int w, int h) {
  uint32_t hs = 2166136261u;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      uint32_t v = px[(size_t)y * stride + x];
      for (int b = 0; b < 4; b++) { hs ^= (v >> (8 * b)) & 255u; hs *= 16777619u; }
    }
  return hs;
}
