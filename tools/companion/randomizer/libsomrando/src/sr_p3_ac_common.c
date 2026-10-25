/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ancient Cave (phase 3): shared model of the floor generators, see sr_p3_ac.h.  Derived from (Author of every file: Moppleton):
 * processing/ancientcave/mapgen/{AncientCaveMapGenerator,ProcGenMapNode,TileReplacer,ReplacementMapSegment,*PaletteSetSource}.cs,
 * processing/common/{GeneratedMapUtil,structure/{MapHeader,MapObject}}.cs, util/{XyPos,PerlinNoise}.cs.  Translated to C 2026-10-18.
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sr_p3_ac.h"

/* ---------------------------------------------------------------------------------------------- checked arithmetic */
int ac_div(struct sr_ctx *c, int a, int b) {
  if (b == 0) sr_fail(c, SOMRANDO_ERR_FAILED, "Attempted to divide by zero.");
  if (b == -1 && a == INT32_MIN) sr_fail(c, SOMRANDO_ERR_FAILED, "Arithmetic operation resulted in an overflow.");
  return a / b;
}

int ac_mod(struct sr_ctx *c, int a, int b) {
  if (b == 0) sr_fail(c, SOMRANDO_ERR_FAILED, "Attempted to divide by zero.");
  if (b == -1) {
    if (a == INT32_MIN) sr_fail(c, SOMRANDO_ERR_FAILED, "Arithmetic operation resulted in an overflow.");
    return 0;
  }
  return a % b;
}

/* ------------------------------------------------------------------------------------------------------- XyPos */
struct ac_xy *ac_xy_new(struct sr_ctx *c, int x, int y) {
  struct ac_xy *p = (struct ac_xy *)sr_alloc(c, sizeof *p);
  p->x = (int16_t)x;
  p->y = (int16_t)y;
  return p;
}

int ac_xy_equals(const struct ac_xy *a, const struct ac_xy *b) { return a && b && a->x == b->x && a->y == b->y; }

/* ----------------------------------------------------------------------------------------------------- layers */
struct ac_layer *ac_layer_new(struct sr_ctx *c, int w, int h) {
  if (w < 0 || h < 0) sr_fail(c, SOMRANDO_ERR_FAILED, "Arithmetic operation resulted in an overflow.");
  struct ac_layer *l = (struct ac_layer *)sr_alloc(c, sizeof *l);
  l->w = w;
  l->h = h;
  l->d = (uint8_t *)sr_alloc(c, (size_t)w * (size_t)h + 1);
  return l;
}

uint8_t *ac_at(struct sr_ctx *c, const struct ac_layer *l, int x, int y) {
  if (!l) sr_fail(c, SOMRANDO_ERR_FAILED, "Object reference not set to an instance of an object.");
  if (x < 0 || y < 0 || x >= l->w || y >= l->h) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was outside the bounds of the array.");
  return &l->d[(size_t)y * (size_t)l->w + (size_t)x];
}

/* ------------------------------------------------------------------------------------------------------ lists */
static void list_grow(struct sr_ctx *c, struct ac_list *l) {
  if (l->n < l->cap) return;
  int ncap = l->cap ? l->cap * 2 : 8;
  int64_t *np = (int64_t *)sr_alloc(c, (size_t)ncap * sizeof *np);
  if (l->n) memcpy(np, l->p, (size_t)l->n * sizeof *np);
  l->p = np;   /* the old block stays in the arena */
  l->cap = ncap;
}

void ac_list_add(struct sr_ctx *c, struct ac_list *l, int64_t v) {
  list_grow(c, l);
  l->p[l->n++] = v;
}

void ac_list_insert(struct sr_ctx *c, struct ac_list *l, int idx, int64_t v) {
  if (idx < 0 || idx > l->n) sr_fail(c, SOMRANDO_ERR_FAILED, "Index must be within the bounds of the List. (Parameter 'index')");
  list_grow(c, l);
  memmove(&l->p[idx + 1], &l->p[idx], (size_t)(l->n - idx) * sizeof *l->p);
  l->p[idx] = v;
  l->n++;
}

int64_t ac_list_get(struct sr_ctx *c, const struct ac_list *l, int idx) {
  if (idx < 0 || idx >= l->n) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range. Must be non-negative and less than the size of the collection. (Parameter 'index')");
  return l->p[idx];
}

void ac_list_set(struct sr_ctx *c, struct ac_list *l, int idx, int64_t v) {
  if (idx < 0 || idx >= l->n) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range. Must be non-negative and less than the size of the collection. (Parameter 'index')");
  l->p[idx] = v;
}

void ac_list_remove_at(struct sr_ctx *c, struct ac_list *l, int idx) {
  if (idx < 0 || idx >= l->n) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range. Must be non-negative and less than the size of the collection. (Parameter 'index')");
  memmove(&l->p[idx], &l->p[idx + 1], (size_t)(l->n - idx - 1) * sizeof *l->p);
  l->n--;
}

int ac_list_index_of(const struct ac_list *l, int64_t v) {
  for (int i = 0; i < l->n; i++)
    if (l->p[i] == v) return i;
  return -1;
}

int ac_list_contains(const struct ac_list *l, int64_t v) { return ac_list_index_of(l, v) >= 0; }

int ac_list_remove(struct ac_list *l, int64_t v) {
  int i = ac_list_index_of(l, v);
  if (i < 0) return 0;
  memmove(&l->p[i], &l->p[i + 1], (size_t)(l->n - i - 1) * sizeof *l->p);
  l->n--;
  return 1;
}

void ac_list_clear(struct ac_list *l) { l->n = 0; }

int ac_xylist_index_of(const struct ac_list *l, int x, int y) {
  for (int i = 0; i < l->n; i++) {
    const struct ac_xy *p = (const struct ac_xy *)AC_PTR(l->p[i]);
    if (p && p->x == (int16_t)x && p->y == (int16_t)y) return i;
  }
  return -1;
}

/* ------------------------------------------------------------------------------------------------- dictionary */
void ac_dict_init(struct ac_dict *d, int kind) {
  memset(d, 0, sizeof *d);
  d->kind = kind;
  d->free_list = -1;
}

static int key_eq(const struct ac_dict *d, int64_t a, int64_t b) {
  if (d->kind == AC_DICT_STR) return strcmp((const char *)AC_PTR(a), (const char *)AC_PTR(b)) == 0;
  return a == b;
}

int ac_dict_find(const struct ac_dict *d, int64_t key) {
  for (int i = 0; i < d->count; i++)
    if (d->e[i].used && key_eq(d, d->e[i].key, key)) return i;
  return -1;
}

int ac_dict_has(const struct ac_dict *d, int64_t key) { return ac_dict_find(d, key) >= 0; }

static void dict_insert(struct sr_ctx *c, struct ac_dict *d, int64_t key, int64_t val) {
  int idx;
  if (d->free_count > 0) {
    idx = d->free_list;
    d->free_list = d->e[idx].next_free;
    d->free_count--;
  } else {
    if (d->count == d->cap) {
      int ncap = d->cap ? d->cap * 2 : 8;
      struct ac_dent *ne = (struct ac_dent *)sr_alloc(c, (size_t)ncap * sizeof *ne);
      if (d->count) memcpy(ne, d->e, (size_t)d->count * sizeof *ne);
      d->e = ne;
      d->cap = ncap;
    }
    idx = d->count++;
  }
  d->e[idx].key = key;
  d->e[idx].val = val;
  d->e[idx].used = 1;
  d->e[idx].next_free = -1;
}

void ac_dict_set(struct sr_ctx *c, struct ac_dict *d, int64_t key, int64_t val) {
  int i = ac_dict_find(d, key);
  if (i >= 0) d->e[i].val = val;
  else dict_insert(c, d, key, val);
}

void ac_dict_add(struct sr_ctx *c, struct ac_dict *d, int64_t key, int64_t val) {
  if (ac_dict_find(d, key) >= 0) sr_fail(c, SOMRANDO_ERR_FAILED, "An item with the same key has already been added.");
  dict_insert(c, d, key, val);
}

int64_t ac_dict_get(struct sr_ctx *c, const struct ac_dict *d, int64_t key) {
  int i = ac_dict_find(d, key);
  if (i < 0) sr_fail(c, SOMRANDO_ERR_FAILED, "The given key was not present in the dictionary.");
  return d->e[i].val;
}

int ac_dict_try_get(const struct ac_dict *d, int64_t key, int64_t *val) {
  int i = ac_dict_find(d, key);
  if (i < 0) return 0;
  if (val) *val = d->e[i].val;
  return 1;
}

int ac_dict_remove(struct ac_dict *d, int64_t key) {
  int i = ac_dict_find(d, key);
  if (i < 0) return 0;
  d->e[i].used = 0;
  d->e[i].next_free = d->free_list;
  d->free_list = i;
  d->free_count++;
  return 1;
}

int ac_dict_size(const struct ac_dict *d) { return d->count - d->free_count; }

/* ------------------------------------------------------------------------------------------------------- nodes */
struct ac_node *ac_node_new(struct sr_ctx *c) {
  struct ac_node *n = (struct ac_node *)sr_alloc(c, sizeof *n);
  ac_dict_init(&n->values, AC_DICT_STR);
  return n;
}

/* ------------------------------------------------------------------------------ MapHeader / MapObject accessors */
static int shift_of(int bits) {
  for (int i = 0; i < 32; i++)
    if (bits & (1 << i)) return i;
  return 32;
}
static int vget(const uint8_t *v, int byte_index, int bits) { return (uint8_t)((v[byte_index] & bits) >> shift_of(bits)); }
static void vset(uint8_t *v, int byte_index, int bits, int value) {
  uint8_t other = (uint8_t)(v[byte_index] & ~bits);
  v[byte_index] = (uint8_t)((uint8_t)value << shift_of(bits));
  v[byte_index] |= other;
}

uint8_t ac_hdr_get_tileset8(const uint8_t *h) { return (uint8_t)vget(h, 0, 0x3F); }
void ac_hdr_set_animated_tiles(uint8_t *h, int v) { vset(h, 0, 0x80, v ? 1 : 0); }
void ac_hdr_set_unknown_tile_flag(uint8_t *h, int v) { vset(h, 0, 0x40, v ? 1 : 0); }
void ac_hdr_set_tileset8(uint8_t *h, int v) { vset(h, 0, 0x3F, v); }
void ac_hdr_set_palette_animated(uint8_t *h, int v) { vset(h, 1, 0x80, v ? 1 : 0); }
int ac_hdr_get_palette_set(const uint8_t *h) { return vget(h, 1, 0x7F); }
void ac_hdr_set_palette_set(uint8_t *h, int v) { vset(h, 1, 0x7F, v); }
int ac_hdr_get_tileset16(const uint8_t *h) { return vget(h, 0, 0x3F); }
void ac_hdr_set_tileset16(uint8_t *h, int v) { vset(h, 2, 0x3F, v); }
void ac_hdr_set_layer1_fg(uint8_t *h, int v) { vset(h, 2, 0x40, v ? 1 : 0); }
void ac_hdr_set_layer2_fg(uint8_t *h, int v) { vset(h, 2, 0x80, v ? 1 : 0); }
void ac_hdr_set_mode7(uint8_t *h, int v) { vset(h, 3, 0xF8, v ? 0xF8 : 0); }
int ac_hdr_get_walkon(const uint8_t *h) { return vget(h, 3, 0x04) > 0; }
void ac_hdr_set_walkon(uint8_t *h, int v) { vset(h, 3, 0x04, v ? 1 : 0); }
void ac_hdr_set_special_event(uint8_t *h, int v) { vset(h, 3, 0x01, v ? 1 : 0); }
void ac_hdr_set_flammie(uint8_t *h, int v) { vset(h, 4, 0x80, v ? 1 : 0); }
void ac_hdr_set_magic_rope(uint8_t *h, int v) { vset(h, 4, 0x40, v ? 1 : 0); }
void ac_hdr_set_bg_scrolling(uint8_t *h, int v) { vset(h, 4, 0x3F, v); }
void ac_hdr_set_display(uint8_t *h, int v) { vset(h, 5, 0xFF, v); }
int ac_hdr_get_display(const uint8_t *h) { return vget(h, 5, 0xFF); }
void ac_hdr_set_npc_palette(uint8_t *h, int v) { vset(h, 7, 0xFF, v); }

int ac_obj_get_species(const uint8_t *o) { return vget(o, 5, 0xFF); }
int ac_obj_get_xpos(const uint8_t *o) { return vget(o, 2, 0x7F); }
int ac_obj_get_ypos(const uint8_t *o) { return vget(o, 3, 0x7F); }
int ac_obj_get_event(const uint8_t *o) { return vget(o, 6, 0xFF) + (vget(o, 7, 0x07) << 8); }
int ac_obj_get_vis_flag(const uint8_t *o) { return vget(o, 0, 0xFF); }
void ac_obj_set_vis_flag(uint8_t *o, int v) { vset(o, 0, 0xFF, v); }
void ac_obj_set_vis_min(uint8_t *o, int v) { vset(o, 1, 0xF0, v); }
void ac_obj_set_vis_max(uint8_t *o, int v) { vset(o, 1, 0x0F, v); }
void ac_obj_set_always_visible(uint8_t *o) {
  ac_obj_set_vis_flag(o, 0x00);
  ac_obj_set_vis_min(o, 0x00);
  ac_obj_set_vis_max(o, 0x0F);
}
void ac_obj_set_never_visible(uint8_t *o) {
  ac_obj_set_vis_flag(o, 0x00);
  ac_obj_set_vis_min(o, 0x0F);
  ac_obj_set_vis_max(o, 0x0F);
}
void ac_obj_set_xpos(uint8_t *o, int v) { vset(o, 2, 0x7F, v); }
void ac_obj_set_ypos(uint8_t *o, int v) { vset(o, 3, 0x7F, v); }
void ac_obj_set_spawn_priority(uint8_t *o, int v) { vset(o, 2, 0x80, v ? 1 : 0); }
void ac_obj_set_direction(uint8_t *o, int v) { vset(o, 4, 0xC0, v); }
void ac_obj_set_unknown4a(uint8_t *o, int v) { vset(o, 4, 0x20, v ? 1 : 0); }
void ac_obj_set_layer2_collision(uint8_t *o, int v) { vset(o, 4, 0x08, v ? 1 : 0); }
void ac_obj_set_frozen(uint8_t *o, int v) { vset(o, 3, 0x80, v ? 1 : 0); }
void ac_obj_set_species(uint8_t *o, int v) { vset(o, 5, 0xFF, v); }
void ac_obj_set_event(uint8_t *o, int v) {
  vset(o, 6, 0xFF, (uint8_t)v);
  vset(o, 7, 0x07, (uint8_t)(v >> 8));
}
void ac_obj_set_unknown_b7(uint8_t *o, int v) { vset(o, 7, 0xF8, v); }

uint8_t *ac_map_add_object(struct sr_ctx *c, struct w8_map *m) {
  if (m->n_objs >= W8_MAX_OBJS) sr_fail(c, SOMRANDO_ERR_FAILED, "too many objects on a generated map");
  uint8_t *o = m->objs[m->n_objs++];
  memset(o, 0, 8);
  return o;
}

void ac_map_add_trigger(struct sr_ctx *c, struct w8_map *m, int v) {
  if (m->n_triggers >= W8_MAX_TRIGGERS) sr_fail(c, SOMRANDO_ERR_FAILED, "too many triggers on a generated map");
  m->triggers[m->n_triggers++] = (uint16_t)v;
}

/* ------------------------------------------------------------------------------------------------- maps, context */
struct ac_map *ac_map_new(struct sr_ctx *c) {
  struct ac_map *m = (struct ac_map *)sr_alloc(c, sizeof *m);
  m->md = (struct w8_map *)sr_alloc(c, sizeof *m->md);
  m->is_ac = 1;
  strcpy(m->map_name, "?");
  ac_dict_init(&m->same_exit, AC_DICT_INT);
  ac_dict_init(&m->same_entry, AC_DICT_INT);
  ac_dict_init(&m->alt_exit, AC_DICT_INT);
  ac_dict_init(&m->alt_entry, AC_DICT_INT);
  return m;
}

struct ac_gen_ctx *ac_gen_ctx_new(struct sr_ctx *c) {
  struct ac_gen_ctx *g = (struct ac_gen_ctx *)sr_alloc(c, sizeof *g);
  ac_dict_init(&g->special_nodes, AC_DICT_STR);
  return g;
}

struct ac_node *ac_gen_ctx_shared_node(const struct ac_gen_ctx *g, const char *name) {
  for (int i = 0; i < g->shared_nodes.n; i++) {
    struct ac_node *n = (struct ac_node *)AC_PTR(g->shared_nodes.p[i]);
    if (n->name && !strcmp(n->name, name)) return n;
  }
  return NULL;
}

void ac_gen_ctx_nodes_by_name(struct sr_ctx *c, const struct ac_gen_ctx *g, struct ac_dict *out) {
  ac_dict_init(out, AC_DICT_STR);
  for (int i = 0; i < g->shared_nodes.n; i++) {
    struct ac_node *n = (struct ac_node *)AC_PTR(g->shared_nodes.p[i]);
    if (n->name) ac_dict_set(c, out, AC_STRKEY(n->name), AC_VAL(n));
  }
}

/* ------------------------------------------------------------------------------------------------------ palettes */
void ac_palsrc_get(struct sr_ctx *c, const struct ac_palsrc *s, uint8_t out[210]) {
  if (s->resource) {
    size_t len = 0;
    const uint8_t *d = sr_rom_res(c, s->resource, &len);
    if (!d || len != 210) sr_fail(c, SOMRANDO_ERR_FAILED, "Could not find resource %s", s->resource);
    memcpy(out, d, 210);
  } else {
    memcpy(out, c->orig + 0xC8000 + s->vanilla * 30 * 7, 210);
  }
}

void ac_apply_palette_source(struct sr_ctx *c, struct ac_map *map, const struct ac_palsrc *s) {
  uint8_t pal[210];
  ac_palsrc_get(c, s, pal);
  int pal_index = w8_wget_int(c, "GeneratedPaletteIndex");
  sr_work_set_int(c, "GeneratedPaletteIndex", pal_index + 1);
  sr_w8_copy_palette_animation_settings(c, s->vanilla, pal_index);
  w8_set_pal(c, pal_index, pal);
  ac_hdr_set_palette_set(map->md->header, (uint8_t)pal_index);
}

const uint8_t *ac_custom_map(struct sr_ctx *c, const char *name, size_t *len) { return sr_rom_res(c, name, len); }

/* ---------------------------------------------------------------------------------------------------- Perlin noise */
static const int perm[256] = {
    151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225, 140, 36,  103, 30,  69,  142, 8,   99,  37,  240, 21,  10,  23,
    190, 6,   148, 247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117, 35,  11,  32,  57,  177, 33,  88,  237, 149, 56,  87,  174, 20,
    125, 136, 171, 168, 68,  175, 74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158, 231, 83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,
    41,  55,  46,  245, 40,  244, 102, 143, 54,  65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,  18,  169, 200, 196, 135, 130,
    116, 188, 159, 86,  164, 100, 109, 198, 173, 186, 3,   64,  52,  217, 226, 250, 124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212, 207,
    206, 59,  227, 47,  16,  58,  17,  182, 189, 28,  42,  223, 183, 170, 213, 119, 248, 152, 2,   44,  154, 163, 70,  221, 153, 101, 155, 167, 43,
    172, 9,   129, 22,  39,  253, 19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104, 218, 246, 97,  228, 251, 34,  242, 193, 238, 210, 144,
    12,  191, 179, 162, 241, 81,  51,  145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157, 184, 84,  204, 176, 115, 121, 50,  45,
    127, 4,   150, 254, 138, 236, 205, 93,  222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156, 180};

static double pn_fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static double pn_lerp(double t, double a, double b) { return a + t * (b - a); }
static double pn_grad(int hash, double x, double y, double z) {
  int h = hash & 15;
  double u = h < 8 ? x : y;
  double v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
  return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}
#define PP(i) perm[(i) & 255]   /* p[256 + i] = p[i] = permutation[i]; the largest index used is below 512 */

double ac_perlin_noise(double x, double y, double z) {
  int X = (int)floor(x) & 255, Y = (int)floor(y) & 255, Z = (int)floor(z) & 255;
  x -= floor(x);
  y -= floor(y);
  z -= floor(z);
  double u = pn_fade(x), v = pn_fade(y), w = pn_fade(z);
  int A = PP(X) + Y, AA = PP(A) + Z, AB = PP(A + 1) + Z;
  int B = PP(X + 1) + Y, BA = PP(B) + Z, BB = PP(B + 1) + Z;
  return pn_lerp(w, pn_lerp(v, pn_lerp(u, pn_grad(PP(AA), x, y, z), pn_grad(PP(BA), x - 1, y, z)),
                            pn_lerp(u, pn_grad(PP(AB), x, y - 1, z), pn_grad(PP(BB), x - 1, y - 1, z))),
                 pn_lerp(v, pn_lerp(u, pn_grad(PP(AA + 1), x, y, z - 1), pn_grad(PP(BA + 1), x - 1, y, z - 1)),
                         pn_lerp(u, pn_grad(PP(AB + 1), x, y - 1, z - 1), pn_grad(PP(BB + 1), x - 1, y - 1, z - 1))));
}

/* ----------------------------------------------------------------------------------------------- TileReplacer */
enum { PT_ANY, PT_NUM, PT_NOTNUM, PT_SET, PT_NOTSET };
struct pat_tok {
  int kind, num;
  int set[32], nset;
};

static int parse_int(const char *s, int *out) {
  char *end;
  while (*s == ' ') s++;
  if (!*s) return 0;
  long v = strtol(s, &end, 10);
  while (*end == ' ') end++;
  if (*end) return 0;
  *out = (int)v;
  return 1;
}

static void parse_set(struct sr_ctx *c, const char *body, size_t len, struct pat_tok *t) {
  char buf[128];
  if (len >= sizeof buf) sr_fail(c, SOMRANDO_ERR_FAILED, "pattern set too long");
  memcpy(buf, body, len);
  buf[len] = 0;
  t->nset = 0;
  char *p = buf;
  for (;;) {
    char *comma = strchr(p, ',');
    if (comma) *comma = 0;
    int v;
    if (!parse_int(p, &v)) sr_fail(c, SOMRANDO_ERR_FAILED, "Input string was not in a correct format.");
    if (t->nset >= 32) sr_fail(c, SOMRANDO_ERR_FAILED, "pattern set too long");
    t->set[t->nset++] = v;
    if (!comma) break;
    p = comma + 1;
  }
}

static void parse_tok(struct sr_ctx *c, const char *s, struct pat_tok *t) {
  memset(t, 0, sizeof *t);
  int v;
  size_t n = strlen(s);
  if (parse_int(s, &v)) { t->kind = PT_NUM; t->num = v; return; }
  if (s[0] == '!') {
    const char *rest = s + 1;
    size_t rn = n - 1;
    if (parse_int(rest, &v)) { t->kind = PT_NOTNUM; t->num = v; return; }
    if (rn >= 2 && rest[0] == '{' && rest[rn - 1] == '}') { t->kind = PT_NOTSET; parse_set(c, rest + 1, rn - 2, t); return; }
    t->kind = PT_ANY;   /* a "!" with anything else adds no constraint */
    return;
  }
  if (n >= 2 && s[0] == '{' && s[n - 1] == '}') { t->kind = PT_SET; parse_set(c, s + 1, n - 2, t); return; }
  t->kind = PT_ANY;
}

void ac_apply_pattern(struct sr_ctx *c, const struct ac_pattern *p, const struct ac_layer *old_l, struct ac_layer *new_l, struct ac_layer *alt,
                      const struct ac_bounds *rect) {
  int width = old_l->w, height = old_l->h, x_start = 0, y_start = 0;
  if (rect) {
    x_start = rect->x1;
    y_start = rect->y1;
    width = rect->x2;
    height = rect->y2;
  }
  int block_w = p->width;
  if (block_w == 0) sr_fail(c, SOMRANDO_ERR_FAILED, "Attempted to divide by zero.");
  int block_h = p->n / block_w;
  struct pat_tok *toks = (struct pat_tok *)sr_alloc(c, (size_t)p->n * sizeof *toks + 1);
  for (int i = 0; i < p->n; i++) parse_tok(c, p->search[i], &toks[i]);
  for (int y = y_start; y < height; y++) {
    for (int x = x_start; x < width; x++) {
      int match = 1, i = 0;
      for (int py = 0; match && py < block_h; py++) {
        for (int px = 0; match && px < block_w; px++) {
          int xp = px + x, yp = py + y;
          if (xp >= 0 && yp >= 0 && xp < width && yp < height) {
            const struct pat_tok *t = &toks[i];
            if (t->kind != PT_ANY) {
              int cur = AC_AT(c, old_l, xp, yp);
              if (t->kind == PT_NUM) {
                if (t->num != cur) match = 0;
              } else if (t->kind == PT_NOTNUM) {
                if (t->num == cur) match = 0;
              } else {
                int any = 0;
                for (int k = 0; k < t->nset; k++)
                  if (cur == t->set[k]) any = 1;
                if (t->kind == PT_SET) match &= any;
                else match &= !any;
              }
            }
          } else {
            match = 0;
          }
          i++;
        }
      }
      if (match) {
        i = 0;
        for (int py = 0; py < block_h; py++) {
          for (int px = 0; px < block_w; px++) {
            int xp = px + x, yp = py + y;
            if (xp >= 0 && yp >= 0 && xp < width && yp < height) {
              const char *inj = p->inject[i];
              if (strcmp(inj, "-")) {
                int nt;
                if (!parse_int(inj, &nt)) sr_fail(c, SOMRANDO_ERR_FAILED, "Input string was not in a correct format.");
                AC_AT(c, new_l, xp, yp) = (uint8_t)nt;
              }
              if (p->alt_inject && alt) {
                const char *ai = p->alt_inject[i];
                if (strcmp(ai, "-")) {
                  int nt;
                  if (!parse_int(ai, &nt)) sr_fail(c, SOMRANDO_ERR_FAILED, "Input string was not in a correct format.");
                  AC_AT(c, alt, xp, yp) = (uint8_t)nt;
                }
              }
            }
            i++;
          }
        }
      }
    }
  }
}

void ac_apply_segment(struct sr_ctx *c, struct ac_layer *l, int base_x, int base_y, const struct ac_segment *r) {
  for (int y = 0; y < r->h; y++)
    for (int x = 0; x < r->w; x++) AC_AT(c, l, base_x + r->x + x, base_y + r->y + y) = r->data[y * r->w + x];
}

/* ------------------------------------------------------------------------------------------------------- dumps */
#ifdef SOMRANDO_DEBUG
static FILE *dump_file(void) {
  const char *p = getenv("SOM_AC_DUMP");
  return (p && p[0]) ? fopen(p, "a") : NULL;
}
#endif

void ac_dump_line(const char *fmt, ...) {
#ifdef SOMRANDO_DEBUG
  FILE *f = dump_file();
  if (!f) return;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(f, fmt, ap);
  va_end(ap);
  fputc('\n', f);
  fclose(f);
#else
  (void)fmt;
#endif
}

void ac_dump_rng(struct sr_ctx *c, const char *tag) {
#ifdef SOMRANDO_DEBUG
  ac_dump_line("%s rf=%ld rc=%ld", tag, c->rf.calls, c->rc.calls);
#else
  (void)c; (void)tag;
#endif
}

#ifdef SOMRANDO_DEBUG
static uint32_t fnv_bytes(const uint8_t *b, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
  return h;
}
static uint32_t fnv_layer(const struct ac_layer *l) {
  uint32_t h = 2166136261u;
  if (!l) return 0;
  for (int x = 0; x < l->w; x++)
    for (int y = 0; y < l->h; y++) { h ^= LAYER(l, x, y); h *= 16777619u; }
  return h;
}
static void put_pos(FILE *f, const struct ac_xy *p) {
  if (!p) fputs("null", f);
  else fprintf(f, "%d,%d", p->x, p->y);
}
static void put_hex_dash(FILE *f, const uint8_t *b, int n) {
  for (int i = 0; i < n; i++) fprintf(f, "%s%02X", i ? "-" : "", b[i]);
}
#endif

void ac_dump_map(struct sr_ctx *c, const char *tag, const struct ac_map *m) {
#ifdef SOMRANDO_DEBUG
  FILE *f = dump_file();
  if (!f) return;
  fprintf(f, "%s id=%d w=%d h=%d", tag, m->map_id, m->layer1 ? m->layer1->w : -1, m->layer1 ? m->layer1->h : -1);
  fprintf(f, " l1=%08x l2=%08x l2c=%s", fnv_layer(m->layer1), fnv_layer(m->layer2), m->layer2_collision ? "True" : "False");
  fputs(" entry=", f);
  put_pos(f, m->entry_pos);
  fputs(" exits=", f);
  for (int i = 0; i < m->exit_locations.n; i++) { put_pos(f, (const struct ac_xy *)AC_PTR(m->exit_locations.p[i])); fputc(';', f); }
  fputs(" hdr=", f);
  put_hex_dash(f, m->md->header, 8);
  fprintf(f, " nobj=%d objs=", m->md->n_objs);
  for (int i = 0; i < m->md->n_objs; i++) { put_hex_dash(f, m->md->objs[i], 8); fputc(';', f); }
  fputs(" trig=", f);
  for (int i = 0; i < m->md->n_triggers; i++) fprintf(f, "%x;", m->md->triggers[i]);
  if (m->is_ac) {
    fputs(" same_exit=", f);
    for (int i = 0; i < m->same_exit.count; i++)
      if (m->same_exit.e[i].used) { fprintf(f, "%d,%d:%d;", AC_KEY_X(m->same_exit.e[i].key), AC_KEY_Y(m->same_exit.e[i].key), (int)m->same_exit.e[i].val); }
    fputs(" same_entry=", f);
    for (int i = 0; i < m->same_entry.count; i++)
      if (m->same_entry.e[i].used) { fprintf(f, "%d:", (int)m->same_entry.e[i].key); put_pos(f, (const struct ac_xy *)AC_PTR(m->same_entry.e[i].val)); fputc(';', f); }
    fputs(" alt_exit=", f);
    for (int i = 0; i < m->alt_exit.count; i++)
      if (m->alt_exit.e[i].used) { fprintf(f, "%d,%d:%d;", AC_KEY_X(m->alt_exit.e[i].key), AC_KEY_Y(m->alt_exit.e[i].key), (int)m->alt_exit.e[i].val); }
    fputs(" alt_entry=", f);
    for (int i = 0; i < m->alt_entry.count; i++)
      if (m->alt_entry.e[i].used) { fprintf(f, "%d:", (int)m->alt_entry.e[i].key); put_pos(f, (const struct ac_xy *)AC_PTR(m->alt_entry.e[i].val)); fputc(';', f); }
    fputs(" ret=", f);
    for (int i = 0; i < m->return_doors.n; i++) fprintf(f, "%d;", (int)m->return_doors.p[i]);
  }
  fputc('\n', f);
  fclose(f);
  {
    char t[96];
    snprintf(t, sizeof t, "%s-rng", tag);
    ac_dump_rng(c, t);
  }
  (void)fnv_bytes;
#else
  (void)c; (void)tag; (void)m;
#endif
}
