/* som_ui_state: WRAM / ROM decoding for the companion UI (modes, actors, dialogue, Flammie flight, names, world map).
 * Everything here is read-only on the memory it is given.  Evidence for every address: docs/companion-ux-20261017.md. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "som_ui_priv.h"

/* ------------------------------------------------------------------ small helpers */
static unsigned u16(const uint8_t *w, unsigned a) { return w[a] | (w[a + 1] << 8); }

static char glyph_char(unsigned g) {
  if (g == 0x80) return ' ';
  if (g >= 0x81 && g <= 0x9A) return (char)('a' + (g - 0x81));
  if (g >= 0x9B && g <= 0xB4) return (char)('A' + (g - 0x9B));
  if (g >= 0xB5 && g <= 0xBE) return (char)('0' + (g - 0xB5));
  switch (g) {
  case 0xBF: return '.';
  case 0xC0: return ',';
  case 0xC1: return '/';
  case 0xC2: return '\'';
  case 0xC3: return '[';
  case 0xC4: return ']';
  case 0xC5: return ':';
  case 0xC6: return '-';
  case 0xC8: return '!';
  case 0xCA: return '?';
  default: return '*';
  }
}

const char *som_ux_mode_name(enum som_ux_mode m) {
  static const char *const n[SOM_UX_MODE_COUNT] = {"BOOT", "FRONTEND", "FIELD", "EVENT", "TRANSITION", "RING", "SHOP",
                                                    "FLAMMIE"};
  return (unsigned)m < SOM_UX_MODE_COUNT ? n[m] : "?";
}

enum som_ux_mode som_ux_classify(const uint8_t *w) {
  unsigned act0 = w[0xE000], e4 = w[0xE4], e6 = w[0xE6], d0 = w[0xD0];
  if (act0 != 1) return SOM_UX_BOOT;
  if (u16(w, 0xE184) == 0 && e4 == 0) return SOM_UX_FRONTEND;
  if (e4 == 4 && e6 == 15) return d0 ? SOM_UX_EVENT : SOM_UX_FLAMMIE;
  if (e6 != 15 || e4 != 1) return SOM_UX_TRANSITION;
  if (w[0x7D] == 1 && w[0x1847] >= 1 && w[0x1847] <= 3) return SOM_UX_SHOP;
  if (d0) return SOM_UX_EVENT;
  if (w[0x7D] == 1) return SOM_UX_RING;
  return SOM_UX_FIELD;
}

enum som_ux_tclass som_ux_item_class(int id) {
  switch (id) {
  case 0: case 1: case 2: case 3: case 8: case 9: case 10: return SOM_UX_TC_ALLY_ONE;
  case 4: return SOM_UX_TC_ALLY_STATUS_ONE;
  case 5: return SOM_UX_TC_ALLY_DEAD_ONE;
  default: return SOM_UX_TC_UNSUPPORTED;
  }
}

/* ------------------------------------------------------------------ graphics pack */
struct som_ux_pack {
  int aw, ah, n;
  const uint8_t *rgba;
  struct pk_rect *r;
};

static int pk_cmp(const void *a, const void *b) { return (int)((const struct pk_rect *)a)->id - (int)((const struct pk_rect *)b)->id; }
static unsigned le16(const uint8_t *p) { return p[0] | (p[1] << 8); }

struct som_ux_pack *som_ux_pack_load(const uint8_t *b, size_t len) {
  if (len < 12 || memcmp(b, "SUXP", 4) || le16(b + 4) != 1) return NULL;
  int aw = (int)le16(b + 6), ah = (int)le16(b + 8), n = (int)le16(b + 10);
  if ((size_t)12 + 16u * (unsigned)n + (size_t)aw * ah * 4 > len) return NULL;
  struct som_ux_pack *p = (struct som_ux_pack *)calloc(1, sizeof *p);
  if (!p) return NULL;
  p->r = (struct pk_rect *)malloc(sizeof(struct pk_rect) * (size_t)(n ? n : 1));
  if (!p->r) { free(p); return NULL; }
  for (int i = 0; i < n; i++) {
    const uint8_t *q = b + 12 + 16 * i;
    p->r[i].id = (uint16_t)le16(q); p->r[i].x = (uint16_t)le16(q + 2); p->r[i].y = (uint16_t)le16(q + 4);
    p->r[i].w = (uint16_t)le16(q + 6); p->r[i].h = (uint16_t)le16(q + 8);
    p->r[i].ox = (int16_t)le16(q + 10); p->r[i].oy = (int16_t)le16(q + 12); p->r[i].adv = (uint16_t)le16(q + 14);
  }
  qsort(p->r, (size_t)n, sizeof(struct pk_rect), pk_cmp);
  p->aw = aw; p->ah = ah; p->n = n; p->rgba = b + 12 + 16 * n;
  return p;
}
void som_ux_pack_free(struct som_ux_pack *p) { if (p) { free(p->r); free(p); } }

const struct pk_rect *pk_find(const struct som_ux_pack *p, int id) {
  if (!p) return NULL;
  struct pk_rect key; key.id = (uint16_t)id;
  return (const struct pk_rect *)bsearch(&key, p->r, (size_t)p->n, sizeof(struct pk_rect), pk_cmp);
}
int pk_atlas_w(const struct som_ux_pack *p) { return p->aw; }
uint32_t pk_pixel(const struct som_ux_pack *p, int x, int y) {
  const uint8_t *q = p->rgba + 4 * ((size_t)y * (size_t)p->aw + (size_t)x);
  return ((uint32_t)q[3] << 24) | ((uint32_t)q[0] << 16) | ((uint32_t)q[1] << 8) | q[2];
}

/* ------------------------------------------------------------------ ROM views */
const uint8_t *ux_rom(const struct som_ux *ux, size_t *len) {
  if (!ux->rom || ux->rom_len < 0x100000) return NULL;
  size_t off = (ux->rom_len % 0x8000 == 0x200) ? 0x200 : 0;
  *len = ux->rom_len - off;
  return ux->rom + off;
}

/* The pointer table is at 0xA0800; its words are offsets from the string bank.  Vanilla and the translations use 0xA0000, the Ancient
 * Cave generator moves every string into its expansion (bank file offset 0x210000, "EARTH SLIDE" at 0x2154F1) and keeps the table in place. */
static int decode_names_at(struct som_ux *ux, size_t base) {
  size_t len; const uint8_t *r = ux_rom(ux, &len);
  ux->names_ok = 0;
  if (!r || len < 0xA1000) return 0;
  for (int n = 0; n < SOM_UX_NAMES; n++) {
    size_t pa = 0xA0800u + 2u * (unsigned)n;
    size_t o = base + le16(r + pa);
    char *d = ux->names[n];
    int k = 0;
    while (o < len && r[o] != 0 && k < SOM_UX_NAME_LEN - 2) {
      unsigned g = r[o++];
      if (g == 0xE6) { d[k++] = 'A'; d[k++] = ' '; continue; }   /* Magno digraph tiles (found from "ESPADA MANA", "GUANTE") */
      if (g == 0xF3) { d[k++] = 'A'; d[k++] = 'N'; continue; }
      d[k] = (g >= 0x80 && g <= 0xD2) ? glyph_char(g) : '?';
      if (d[k] >= 'a' && d[k] <= 'z') d[k] = (char)(d[k] - 32);     /* one case: the outlined small caps read better at 1x */
      k++;
    }
    while (k > 0 && d[k - 1] == ' ') k--;
    d[k] = 0;
    /* a plausible name ends before the limit and has at most two unmapped glyphs (a translation may add one) */
    int bad = k >= SOM_UX_NAME_LEN - 2 || k == 0, q = 0;
    for (int i = 0; i < k; i++) if (d[i] == '?' || d[i] == '*') q++;
    if (bad || q > 2) d[0] = 0;
  }
  /* A modified ROM (for example the Ancient Cave generator) can move the name block: the sample entries must be sane. */
  static const int probe[3] = {NAME_WEAPON, NAME_ITEM, NAME_ENEMY};
  for (int i = 0; i < 3; i++) if (!ux->names[probe[i]][0]) return 0;
  ux->names_ok = 1;
  return 1;
}

static void decode_names(struct som_ux *ux) {
  size_t len; const uint8_t *r = ux_rom(ux, &len);
  if (!r) { ux->names_ok = 0; return; }
  if (decode_names_at(ux, 0xA0000u)) return;
  if (len >= 0x220000u && decode_names_at(ux, 0x210000u)) return;
  ux->names_ok = 0;
}

static void decode_spell_classes(struct som_ux *ux) {
  size_t len; const uint8_t *r = ux_rom(ux, &len);
  for (int i = 0; i < 42; i++) ux->spell_class[i] = SOM_UX_TC_UNSUPPORTED;
  if (!r || len < 0x102AD0u + 64u * 42u) return;
  for (int i = 0; i < 42; i++) {
    unsigned t = r[0x102AD0u + 64u * (unsigned)i + 8];
    enum som_ux_tclass c = SOM_UX_TC_UNSUPPORTED;
    if (i == 0x21) c = SOM_UX_TC_ALLY_DEAD_ONE;
    else if (t == 0x81) c = SOM_UX_TC_ENEMY_ONE_OR_ALL;
    else if (t == 0x01) c = SOM_UX_TC_ALLY_ONE_OR_ALL;
    else if (t == 0x00) c = SOM_UX_TC_ALLY_ONE;
    else if (t == 0x40) c = SOM_UX_TC_BOY_ONLY;
    else if (t == 0xA0) c = SOM_UX_TC_ENEMY_ONE;
    ux->spell_class[i] = (uint8_t)c;
  }
}

/* World map: 512 rows (word pointers at 0x68000, row data from 0x60000, +0x10000 each time the pointer wraps), each row
 * is 512 palette indices as run-length bytes (< 0xC0 literal, otherwise (b - 0xC0 + 1) copies of the next byte).
 * Palette: 256 BGR555 words at 0x67D00.  Measured on the US ROM: every row decodes to exactly 512 pixels. */
void ux_build_world(struct som_ux *ux) {
  size_t len; const uint8_t *r = ux_rom(ux, &len);
  if (ux->world_ok || !r || len < 0x80000) return;
  uint32_t *img = (uint32_t *)malloc(sizeof(uint32_t) * SOM_UX_WORLD * SOM_UX_WORLD);
  if (!img) return;
  uint32_t pal[256];
  for (int i = 0; i < 256; i++) {
    unsigned c = le16(r + 0x67D00 + 2 * i), r5 = c & 31, g5 = (c >> 5) & 31, b5 = (c >> 10) & 31;
    pal[i] = 0xFF000000u | (((r5 << 3) | (r5 >> 2)) << 16) | (((g5 << 3) | (g5 >> 2)) << 8) | ((b5 << 3) | (b5 >> 2));
  }
  unsigned prev = 0, add = 0;
  for (int row = 0; row < SOM_UX_WORLD; row++) {
    unsigned p = le16(r + 0x68000 + 2 * row);
    if (p < prev) add += 0x10000;
    prev = p;
    size_t i = 0x60000u + add + p;
    int n = 0;
    while (n < SOM_UX_WORLD) {
      if (i + 1 >= len) { free(img); return; }
      unsigned b = r[i++];
      if (b < 0xC0) img[row * SOM_UX_WORLD + n++] = pal[b];
      else {
        unsigned cnt = b - 0xC0 + 1, v = r[i++];
        while (cnt-- && n < SOM_UX_WORLD) img[row * SOM_UX_WORLD + n++] = pal[v];
      }
    }
  }
  ux->world = img;
  ux->world_ok = 1;
}

/* ------------------------------------------------------------------ area names
 * The ROM has no map-name table.  This table gives a name only where the evidence is good (see area_names.json, which is the source of
 * truth and is compared with this table by test_area_names.py): "doc" = the maps documented and visited in this repository, "visual" =
 * every map of the range checked on the warp thumbnails.  Every other map has no name and the header shows none. */
static const struct { uint16_t lo, hi; const char *name; } AREA[] = {
  {128, 128, "POTOS FALLS"}, {131, 131, "POTOS VILLAGE"}, {256, 256, "POTOS FALLS"}, {257, 259, "POTOS VILLAGE"},
  {16, 16, "PATH TO PANDORA"}, {19, 19, "PATH TO PANDORA"}, {144, 144, "PATH TO PANDORA"}, {198, 235, "PURE LAND"},
};
const char *som_ux_area_name(int map_id) {
  for (unsigned i = 0; i < sizeof AREA / sizeof AREA[0]; i++)
    if (map_id >= AREA[i].lo && map_id <= AREA[i].hi) return AREA[i].name;
  return NULL;
}

/* ------------------------------------------------------------------ lifecycle */
size_t som_ux_sizeof(void) { return sizeof(struct som_ux); }

void som_ux_init(struct som_ux *ux) {
  memset(ux, 0, sizeof *ux);
  ux->tab = SOM_UX_TAB_WEAPON; ux->spirit = 0xFF; ux->boss = -1; ux->name_cursor = 0xFF; ux->name_hero = 0xFF;
  for (int i = 0; i < 42; i++) ux->spell_class[i] = SOM_UX_TC_UNSUPPORTED;
  som_ux_set_shoulder_labels(ux, NULL, NULL, NULL, NULL);
}
struct som_ux *som_ux_new(void) {
  struct som_ux *u = (struct som_ux *)malloc(sizeof *u);
  if (u) som_ux_init(u);
  return u;
}
void som_ux_free(struct som_ux *ux) {
  free(ux->world); free(ux->world_scaled); free(ux->zone);
  ux->world = NULL; ux->world_scaled = NULL; ux->world_ok = 0; ux->world_scaled_n = 0; ux->zone = NULL; ux->zone_ok = 0;
}
void som_ux_delete(struct som_ux *ux) { if (ux) { som_ux_free(ux); free(ux); } }
void som_ux_set_pack(struct som_ux *ux, struct som_ux_pack *p) { ux->pack = p; }

void som_ux_set_rom(struct som_ux *ux, const uint8_t *rom, size_t len) {
  free(ux->world); free(ux->world_scaled);
  ux->world = NULL; ux->world_scaled = NULL; ux->world_ok = 0; ux->world_scaled_n = 0;
  ux->rom = rom; ux->rom_len = len;
  decode_names(ux);
  decode_spell_classes(ux);
  ux_build_world(ux);
}

const char *som_ux_name(const struct som_ux *ux, int i) {
  if (!ux->names_ok || i < 0 || i >= SOM_UX_NAMES || !ux->names[i][0]) return NULL;
  return ux->names[i];
}

/* ------------------------------------------------------------------ per-frame decoding */
static int decode_line(const uint8_t *w, char *out) {
  int n = 0;
  unsigned g0 = w[0xA1A4];
  out[0] = 0;
  if (g0 < 0x80 || g0 > 0xD2) return 0;
  for (int i = 0; i < SOM_UX_LINE_CHARS; i++) {
    unsigned g = w[0xA1A4 + i];
    if (g < 0x80 || g > 0xD2) break;
    out[n++] = glyph_char(g);
  }
  while (n > 0 && out[n - 1] == ' ') n--;
  out[n] = 0;
  return n;
}

void som_ux_update(struct som_ux *ux, const uint8_t *w) {
  som_read_state(w, &ux->st);
  ux->act0 = w[0xE000]; ux->e4 = w[0xE4]; ux->e6 = w[0xE6]; ux->d0 = w[0xD0]; ux->shop_mode = w[0x1847];
  ux->prev_mode = ux->mode;
  ux->mode = som_ux_classify(w);
  ux->frame_no++;
  for (int i = 0; i < 3; i++) {                                   /* player-given names: $CC00 + 12 * slot */
    char *n = ux->hero_name[i]; int k = 0;
    for (int j = 0; j < 9; j++) {
      unsigned g = w[0xCC00 + 12 * i + j];
      if (g == 0 || g == 0xFF) break;
      char ch = glyph_char(g);
      if (ch == '*') continue;
      if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
      n[k++] = ch;
    }
    while (k > 0 && n[k - 1] == ' ') k--;
    n[k] = 0;
  }
  if (ux->notice_t) ux->notice_t--;
  for (int i = 0; i < SOM_UX_QUICK_N; i++) if (ux->flash_t[i]) ux->flash_t[i]--;
  if (ux->sflash_t) ux->sflash_t--;

  ux->n_enemies = 0; ux->boss = -1;
  for (int s = 0; s < 16; s++) {
    unsigned b = 0xE000u + 0x200u * (unsigned)s;
    struct som_ux_actor *a = &ux->actor[s];
    a->slot = (uint8_t)s;
    a->id = w[b + 0x180];
    a->hp = (uint16_t)u16(w, b + 0x182); a->hp_max = (uint16_t)u16(w, b + 0x184);
    a->sx = (uint16_t)u16(w, b + 0x20); a->sy = (uint16_t)u16(w, b + 0x22);
    a->status = (uint16_t)u16(w, b + 0x190);
    a->boss = (w[b + 0x1FB] & 0x40) != 0;
    a->on_screen = a->sx >= 8 && a->sx < 0xF8 && a->sy < 0xD8;
    if (s < SOM_PARTY) {
      a->present = ux->st.hero[s].present;
      a->alive = ux->st.hero[s].alive;
    } else {
      a->present = w[b] == 1 && a->id < 0x80 && a->hp_max > 0;
      a->alive = a->present && a->hp > 0 && !(a->status & 0x8000);
    }
  }
  /* enemies: boss first, then the ones on screen, then the rest */
  for (int pass = 0; pass < 3; pass++)
    for (int s = SOM_PARTY; s < 16 && ux->n_enemies < SOM_UX_ENEMIES; s++) {
      struct som_ux_actor *a = &ux->actor[s];
      if (!a->present || !a->alive) continue;
      int cls = a->boss ? 0 : (a->on_screen ? 1 : 2);
      if (cls != pass) continue;
      ux->enemy_idx[ux->n_enemies++] = (uint8_t)s;
      if (a->boss && ux->boss < 0) ux->boss = (int8_t)s;
    }

  /* Analyzer: the caster's record holds the spell ($E170) and the targets as actor index + 1 ($E178..$E17A) */
  for (int h = 0; h < SOM_PARTY; h++) {
    const uint8_t *cr = w + 0xE170u + 0x200u * (unsigned)h;
    if (!memcmp(ux->cast_prev[h], cr, sizeof ux->cast_prev[h])) continue;
    memcpy(ux->cast_prev[h], cr, sizeof ux->cast_prev[h]);
    if (ux->cast_primed && cr[0] == 0x17)
      for (int t = 0; t < 3; t++) {
        int s = cr[8 + t] - 1;
        if (s >= SOM_PARTY && s < 16 && ux->actor[s].present) ux->analyzed[ux->actor[s].id >> 3] |= (uint8_t)(1u << (ux->actor[s].id & 7));
      }
  }
  ux->cast_primed = 1;                               /* a record already there when the game is loaded is not a new cast */

  ux_decode_selection(ux, w);
  ux_zone_update(ux, w);
  if (ux->rev_wait && (ux->rev_hero >= SOM_PARTY || !ux->st.hero[ux->rev_hero].present || ux->st.hero[ux->rev_hero].alive)) ux->rev_wait = 0;   /* revived (or gone): the chooser closes */

  /* Flammie flight */
  ux->fl_x = (uint16_t)u16(w, 0xFA); ux->fl_y = (uint16_t)u16(w, 0xFC); ux->fl_heading = (uint16_t)(u16(w, 0xF6) & 0x3FF);
  ux->fl_world_ok = ux->fl_x < 4096 && ux->fl_y < 4096;
  if (ux->mode == SOM_UX_FLAMMIE) {
    int n = ux->trail_n;
    int dx = n ? (int)ux->fl_x - (int)ux->trail[n - 1][0] : 99, dy = n ? (int)ux->fl_y - (int)ux->trail[n - 1][1] : 99;
    if (dx * dx + dy * dy >= 16 * 16) {
      if (n == 32) { memmove(ux->trail[0], ux->trail[1], sizeof(ux->trail[0]) * 31); n = 31; }
      ux->trail[n][0] = ux->fl_x; ux->trail[n][1] = ux->fl_y; ux->trail_n = (uint8_t)(n + 1);
    }
  } else if (ux->trail_n) ux->trail_n = 0;

  /* dialogue line buffer */
  char cur[SOM_UX_LINE_CHARS + 1];
  int n = decode_line(w, cur);
  if (n && ux->mode == SOM_UX_EVENT && strcmp(cur, ux->line) != 0) {
    memmove(ux->log[1], ux->log[0], sizeof(ux->log[0]) * (SOM_UX_LOG_LINES - 1));
    memcpy(ux->log[0], cur, sizeof cur);
    if (ux->log_n < SOM_UX_LOG_LINES) ux->log_n++;
  }
  if (n) memcpy(ux->line, cur, sizeof cur); else if (ux->mode != SOM_UX_EVENT) ux->line[0] = 0;

  /* name entry: sprite 0 of the OAM shadow is the letter cursor (x = 19 + 8 * letter) */
  int was_hero = ux->fe_name_entry ? ux->name_hero : 0xFF;
  ux->fe_name_entry = 0; ux->name_cursor = 0xFF; ux->name_hero = 0xFF;
  /* The title and game-select menus also put a cursor in sprite 0, so the sprite alone is not enough (round 3c): the name-entry flag $A20A bit 4 is set and no screen transition ($E4, $E184) is running. */
  int ne = w[0x9C00] != 0xF8 && w[0x9C01] < 0xE0 && (w[0xA20A] & 0x10);
  int fe = ux->mode == SOM_UX_FRONTEND && ne && w[0xE4] == 0 && !(w[0xE184] | w[0xE185]);
  /* In game (the girl and sprite join, every Ancient Cave start) an event opens the native menu on the name page (C1:EB71): page $A202 = 3, its
   * close bit $A20A.0 clear, the hero being named in $A22B.  $A202 and bit 4 stay behind after the menu closes, bit 0 does not. */
  int ev = ux->mode == SOM_UX_EVENT && ne && !(w[0xA20A] & 1) && w[0xA202] == 3 && w[0xA22B] < 3;
  if (fe || ev) {
    int x = w[0x9C00] - 19;
    ux->fe_name_entry = 1;
    if (x >= 0 && x % 8 == 0 && x / 8 < 26) ux->name_cursor = (uint8_t)(x / 8);
    if (w[0xA22B] < 3) ux->name_hero = w[0xA22B];
  }
  /* confirmed name: the game stores it at $CC00 + 12 * hero on the START frame; looked for during a few frames after the page closes */
  if (!ux->fe_name_entry && was_hero < 3) { ux->name_wait_hero = (uint8_t)was_hero; ux->name_wait_t = 8; }
  if (ux->fe_name_entry) ux->name_wait_t = 0;
  else if (ux->name_wait_t) {
    ux->name_wait_t--;
    char nm[SOM_UX_LAST_NAME]; int k = 0;
    const uint8_t *g = w + 0xCC00 + 12 * ux->name_wait_hero;
    while (k < SOM_UX_LAST_NAME - 1 && g[k] >= 0x9B && g[k] <= 0xB4) { nm[k] = (char)('A' + g[k] - 0x9B); k++; }
    nm[k] = 0;
    if (k && (g[k] == 0 || g[k] == 0xFF || k == SOM_UX_LAST_NAME - 1)) {     /* A..Z only: other glyphs cannot be typed back */
      memcpy(ux->name_done_text, nm, sizeof nm); ux->name_done = (uint8_t)(ux->name_wait_hero + 1); ux->name_wait_t = 0;
    }
  }

  /* round 3: playthrough bookkeeping */
  ux->had_dialogue = ux->mode == SOM_UX_EVENT && ux->line[0];
  if (ux->mode == SOM_UX_BOOT || ux->mode == SOM_UX_FRONTEND) {            /* title, game select, name entry: nothing is owned yet */
    if (ux->mode == SOM_UX_BOOT) ux->fe_saw_name = 0;
    memset(ux->analyzed, 0, sizeof ux->analyzed);                   /* what Analyzer revealed belongs to one game */
    if (ux->fe_name_entry) ux->fe_saw_name = 1;
    ux->armed = 0; ux->seen_w = ux->seen_s = 0; ux->seen_i = 0; ux->game_key = 0;
  } else {
    if (ux->prev_mode == SOM_UX_FRONTEND) { ux->game_event = ux->fe_saw_name ? SOM_UX_GAME_NEW : SOM_UX_GAME_LOAD; ux->fe_saw_name = 0; }
    for (int t = 0; t < SOM_WEAPON_TYPES; t++) if (ux->st.weapon_slot[t] != 0xFF) ux->seen_w |= (uint8_t)(1u << t);
    for (int i = 0; i < SOM_ITEM_RECORDS; i++) if (ux->st.item[i].qty && ux->st.item[i].id < 11) ux->seen_i |= (uint16_t)(1u << ux->st.item[i].id);
    for (int p = 0; p < SOM_PARTY; p++) for (int b = 0; b < SOM_SPIRITS; b++) if (ux->st.hero[p].present && ux->st.hero[p].spirit_mask[b]) ux->seen_s |= (uint8_t)(1u << b);
    if (w[0xCF00]) ux->armed = 1;                       /* story flag set by the event in which the boy takes the first sword: ring menus enabled (docs/wram-companion-map.md) */
    uint32_t k = 2166136261u; int any = 0;
    for (int i = 0; i < 36; i++) { unsigned g = w[0xCC00 + i]; if (g && g != 0xFF) any = 1; k = (k ^ g) * 16777619u; }
    uint32_t nk = any && ux->st.hero[0].present ? (k ? k : 1) : 0;
    if (nk && ux->game_key && nk != ux->game_key && !ux->game_event) {   /* names that were set stay as they were, a blank one got filled: same game */
      int same = 1;
      for (int i = 0; i < 36 && same; i += 12) {
        int blank = 1; for (int j = 0; j < 12; j++) if (ux->key_names[i + j] && ux->key_names[i + j] != 0xFF) blank = 0;
        if (!blank && memcmp(ux->key_names + i, w + 0xCC00 + i, 12)) same = 0;
      }
      if (same) ux->game_event = SOM_UX_GAME_RENAME;
    }
    if (nk) memcpy(ux->key_names, w + 0xCC00, 36);
    ux->game_key = nk;
  }
}

int som_ux_game_event(struct som_ux *ux) { int e = ux ? ux->game_event : 0; if (ux) ux->game_event = 0; return e; }
uint32_t som_ux_game_key(const struct som_ux *ux) { return ux ? ux->game_key : 0; }
uint32_t som_ux_seen_get(const struct som_ux *ux) { return ux ? (uint32_t)ux->seen_w | (uint32_t)ux->seen_s << 8 | (uint32_t)ux->seen_i << 16 : 0; }
void som_ux_seen_set(struct som_ux *ux, uint32_t p) {
  if (!ux) return;
  ux->seen_w |= (uint8_t)p; ux->seen_s |= (uint8_t)(p >> 8); ux->seen_i |= (uint16_t)((p >> 16) & 0x7FF);
}
int som_ux_is_armed(const struct som_ux *ux) { return ux && ux->armed; }

void som_ux_flammie_map_pos(const struct som_ux *ux, int *mx, int *my, int *bearing) {
  if (mx) *mx = ux->fl_x / 8;
  if (my) *my = ux->fl_y / 8;
  if (bearing) *bearing = (360 - (int)ux->fl_heading * 360 / 1024) % 360;
}

int som_ux_name_hero(const struct som_ux *ux) { return ux && ux->fe_name_entry && ux->name_hero < 3 ? ux->name_hero : -1; }
void som_ux_set_last_name(struct som_ux *ux, int hero, const char *name) {
  if (!ux || hero < 0 || hero > 2) return;
  int k = 0;
  for (; name && name[k] && k < SOM_UX_LAST_NAME - 1; k++) {
    char ch = name[k];
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
    if (ch < 'A' || ch > 'Z') { k = 0; break; }
    ux->name_last[hero][k] = ch;
  }
  ux->name_last[hero][k] = 0;
}
const char *som_ux_last_name(const struct som_ux *ux, int hero) { return ux && hero >= 0 && hero < 3 ? ux->name_last[hero] : ""; }
int som_ux_name_confirmed(struct som_ux *ux, char *out, int n) {
  if (!ux || !ux->name_done) return -1;
  int h = ux->name_done - 1;
  ux->name_done = 0;
  if (out && n > 0) snprintf(out, (size_t)n, "%s", ux->name_done_text);
  return h;
}

uint16_t som_ux_name_next_pad(const struct som_ux *ux, int letter) {
  if (ux->name_cursor == 0xFF || letter < 0 || letter > 25) return 0;
  int d = (letter - ux->name_cursor + 26) % 26;
  if (d == 0) return 0;
  return (uint16_t)(1u << (d <= 13 ? SOM_PAD_RIGHT : SOM_PAD_LEFT));
}

int som_ux_scale(int w, int h) {
  int m = w < h ? w : h;
  int s = (m + 125) / 250;
  return s < 1 ? 1 : s;
}

/* ---- enemy sprites for the target selector ---- */
int som_ux_set_enemy_sprite(struct som_ux *ux, int enemy_id, const uint32_t *argb, int w, int h) {
  if (!ux || enemy_id < 0 || enemy_id > 255) return -1;
  int slot = -1;
  for (int i = 0; i < SOM_UX_SPRITES; i++) if (ux->spr_ok[i] && ux->spr_id[i] == enemy_id) slot = i;
  if (!argb) { if (slot >= 0) ux->spr_ok[slot] = 0; return 0; }
  if (w < 1 || h < 1 || w > SOM_UX_SPRITE_MAX || h > SOM_UX_SPRITE_MAX) return -1;
  if (slot < 0) {
    for (int i = 0; i < SOM_UX_SPRITES && slot < 0; i++) if (!ux->spr_ok[i]) slot = i;
    if (slot < 0) { slot = ux->spr_next % SOM_UX_SPRITES; ux->spr_next++; }
  }
  for (int j = 0; j < h; j++) memcpy(&ux->sprite[slot][j * SOM_UX_SPRITE_MAX], argb + (size_t)j * w, (size_t)w * 4);
  ux->spr_id[slot] = (uint8_t)enemy_id; ux->spr_w[slot] = (uint8_t)w; ux->spr_h[slot] = (uint8_t)h; ux->spr_ok[slot] = 1;
  return 0;
}

void som_ux_clear_enemy_sprites(struct som_ux *ux) {
  if (ux) memset(ux->spr_ok, 0, sizeof ux->spr_ok);
}

int som_ux_enemy_sprites_wanted(const struct som_ux *ux, uint8_t *ids, int max) {
  int n = 0;
  if (!ux || !ids) return 0;
  for (int i = 0; i < ux->n_enemies && n < max; i++) {
    int id = ux->actor[ux->enemy_idx[i]].id, have = 0, dup = 0;
    for (int k = 0; k < SOM_UX_SPRITES; k++) if (ux->spr_ok[k] && ux->spr_id[k] == id) have = 1;
    for (int k = 0; k < n; k++) if (ids[k] == id) dup = 1;
    if (!have && !dup) ids[n++] = (uint8_t)id;
  }
  return n;
}
