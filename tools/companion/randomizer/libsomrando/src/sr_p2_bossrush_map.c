/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Boss rush mode (group w8): generated maps and their injection into the ROM.  Derived from (Author of every file: Moppleton):
 * processing/common/structure/{Door,MapHeader,MapObject,FullMap*}.cs, processing/common/{VanillaMapUtil,SomMapCompressor,
 * GeneratedMapUtil}.cs, processing/common/mapgen/{ManaBeastMap,CreditsMap}.cs, processing/bossrush/RestMap.cs and
 * processing/hacks/common/util/{DoorReplacer,FullMapReplacer,MapPaletteSetReplacer,MapPieceExpander}.cs.
 * Translated to C 2026-10-18.
 */
#include <stdlib.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p2_bossrush.h"
#include "sr_p2_bossrush_int.h"
#include "sr_p3_ac.h"
#include "sr_p2_bossrush_data.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))

/* ------------------------------------------------------------------------------------------------------- VanillaStructure */
static int vs_shift(int bits) {
  for (int i = 0; i < 32; i++)
    if (bits & (1 << i)) return i;
  return 32;
}
int w8_vs_get(const uint8_t *v, int byte_index, int bits) { return (uint8_t)((v[byte_index] & bits) >> vs_shift(bits)); }
void w8_vs_set(uint8_t *v, int byte_index, int bits, int value) {
  uint8_t other = (uint8_t)(v[byte_index] & ~bits);
  v[byte_index] = (uint8_t)((uint8_t)value << vs_shift(bits));
  v[byte_index] |= other;
}

/* MapHeader setters used by boss rush */
void w8_hdr_set_animated_tiles(uint8_t *h, int v) { w8_vs_set(h, 0, 0x80, v ? 1 : 0); }
void w8_hdr_set_tileset8(uint8_t *h, int v) { w8_vs_set(h, 0, 0x3F, v); }
void w8_hdr_set_palette_animated(uint8_t *h, int v) { w8_vs_set(h, 1, 0x80, v ? 1 : 0); }
void w8_hdr_set_palette_set(uint8_t *h, int v) { w8_vs_set(h, 1, 0x7F, v); }
int w8_hdr_get_palette_set(const uint8_t *h) { return w8_vs_get(h, 1, 0x7F); }
void w8_hdr_set_layer2_fg(uint8_t *h, int v) { w8_vs_set(h, 2, 0x80, v ? 1 : 0); }
void w8_hdr_set_layer1_fg(uint8_t *h, int v) { w8_vs_set(h, 2, 0x40, v ? 1 : 0); }
void w8_hdr_set_tileset16(uint8_t *h, int v) { w8_vs_set(h, 2, 0x3F, v); }
int w8_hdr_get_tileset16(const uint8_t *h) { return w8_vs_get(h, 0, 0x3F); }   /* sic: the C# getter reads the tileset8 field */
void w8_hdr_set_walkon(uint8_t *h, int v) { w8_vs_set(h, 3, 0x04, v ? 1 : 0); }
void w8_hdr_set_flammie(uint8_t *h, int v) { w8_vs_set(h, 4, 0x80, v ? 1 : 0); }
void w8_hdr_set_magic_rope(uint8_t *h, int v) { w8_vs_set(h, 4, 0x40, v ? 1 : 0); }
void w8_hdr_set_display(uint8_t *h, int v) { w8_vs_set(h, 5, 0xFF, v); }
void w8_hdr_set_npc_palette(uint8_t *h, int v) { w8_vs_set(h, 7, 0xFF, v); }

/* MapObject setters */
void w8_obj_set_direction(uint8_t *o, int v) { w8_vs_set(o, 4, 0xC0, v); }
void w8_obj_set_xpos(uint8_t *o, int v) { w8_vs_set(o, 2, 0x7F, v); }
void w8_obj_set_ypos(uint8_t *o, int v) { w8_vs_set(o, 3, 0x7F, v); }
void w8_obj_set_species(uint8_t *o, int v) { w8_vs_set(o, 5, 0xFF, v); }
void w8_obj_set_event(uint8_t *o, int v) {
  w8_vs_set(o, 6, 0xFF, (uint8_t)v);
  w8_vs_set(o, 7, 0x07, (uint8_t)(v >> 8));
}
void w8_obj_set_vis_flag(uint8_t *o, int v) { w8_vs_set(o, 0, 0xFF, v); }
void w8_obj_set_vis_min(uint8_t *o, int v) { w8_vs_set(o, 1, 0xF0, v); }
void w8_obj_set_vis_max(uint8_t *o, int v) { w8_vs_set(o, 1, 0x0F, v); }
void w8_obj_set_unknown4a(uint8_t *o, int v) { w8_vs_set(o, 4, 0x20, v ? 1 : 0); }
void w8_obj_set_unknown_b7(uint8_t *o, int v) { w8_vs_set(o, 7, 0xF8, v); }
static void obj_always_visible(uint8_t *o) {
  w8_obj_set_vis_flag(o, 0x00);
  w8_obj_set_vis_min(o, 0x00);
  w8_obj_set_vis_max(o, 0x0F);
}

/* Door setters */
void w8_door_init(uint8_t d[4], int map, int x, int y, int transition) {
  memset(d, 0, 4);
  w8_vs_set(d, 0, 0xFF, (uint8_t)map);
  w8_vs_set(d, 1, 0x01, (uint8_t)(map >> 8));
  w8_vs_set(d, 1, 0xFE, x);
  w8_vs_set(d, 2, 0xFE, y);
  w8_vs_set(d, 3, 0xFF, transition);
}
static void door_set_layer2_collision(uint8_t d[4], int v) { w8_vs_set(d, 2, 0x01, v ? 1 : 0); }

/* ------------------------------------------------------------------------------------------------------- full map model */
static struct w8_map *new_generated_map(struct sr_ctx *c, int id) {
  struct w8_state *st = w8_state_get(c);
  for (int i = 0; i < st->n_maps; i++)
    if (st->maps[i]->id == id) {
      memset(st->maps[i], 0, sizeof *st->maps[i]);
      st->maps[i]->id = id;
      return st->maps[i];
    }
  if (st->n_maps >= W8_MAX_MAPS) sr_fail(c, SOMRANDO_ERR_FAILED, "too many generated maps");
  struct w8_map *mp = (struct w8_map *)sr_alloc(c, sizeof *mp);
  mp->id = id;
  st->maps[st->n_maps++] = mp;
  return mp;
}

void w8_register_map(struct sr_ctx *c, int id, struct w8_map *m) {
  struct w8_state *st = w8_state_get(c);
  m->id = id;
  for (int i = 0; i < st->n_maps; i++)
    if (st->maps[i]->id == id) { st->maps[i] = m; return; }
  if (st->n_maps >= W8_MAX_MAPS) sr_fail(c, SOMRANDO_ERR_FAILED, "too many generated maps");
  st->maps[st->n_maps++] = m;
}

static uint8_t *map_add_object(struct sr_ctx *c, struct w8_map *m) {
  if (m->n_objs >= W8_MAX_OBJS) sr_fail(c, SOMRANDO_ERR_FAILED, "too many objects on a generated map");
  uint8_t *o = m->objs[m->n_objs++];
  memset(o, 0, 8);
  return o;
}

static struct w8_ref *map_add_ref(struct sr_ctx *c, struct w8_map *m, int fg) {
  if ((fg ? m->n_fg : m->n_bg) >= W8_MAX_REFS) sr_fail(c, SOMRANDO_ERR_FAILED, "too many map pieces on generated map %d (fg %d bg %d)", m->id, m->n_fg, m->n_bg);
  struct w8_ref *r = fg ? &m->fg[m->n_fg++] : &m->bg[m->n_bg++];
  memset(r, 0, sizeof *r);
  return r;
}

/* ------------------------------------------------------------------------------------------------------- VanillaMapUtil */
#define MAP_TRIGGER_OFFSETS 0x84000
#define MAP_PIECE_REFERENCE_OFFSETS 0x85000
#define MAP_OBJECT_OFFSETS 0x87000
#define MAP_PIECE_OFFSETS 0xD0004

static int bank_start(int off) { return off & 0xFF0000; }

static int object_offset(const uint8_t *rom, int map_num) {
  return bank_start(MAP_OBJECT_OFFSETS) + rom[MAP_OBJECT_OFFSETS + map_num * 2] + (rom[MAP_OBJECT_OFFSETS + map_num * 2 + 1] << 8);
}

static int piece_placement_offset(const uint8_t *rom, int map_num) {
  return bank_start(MAP_PIECE_REFERENCE_OFFSETS) + rom[MAP_PIECE_REFERENCE_OFFSETS + map_num * 2] +
         (rom[MAP_PIECE_REFERENCE_OFFSETS + map_num * 2 + 1] << 8);
}

static int map_piece_offset(const uint8_t *rom, int piece_index) {
  uint8_t o1 = rom[MAP_PIECE_OFFSETS + piece_index * 2];
  uint8_t o2 = rom[MAP_PIECE_OFFSETS + piece_index * 2 + 1];
  int bank = 0x0D;
  if (piece_index >= 0xA3 && piece_index < 0x206) bank = 0x0E;
  else if (piece_index >= 0x206 && piece_index < 0x300) bank = 0x0F;
  else if (piece_index >= 0x300) bank = 0x0C;
  return (bank << 16) + (o2 << 8) + o1;
}

/* FullMapMapPieces getMapPieceReference(rom, mapNum) */
static void get_map_piece_reference(struct sr_ctx *c, const uint8_t *rom, int map_num, struct w8_map *out) {
  int start = piece_placement_offset(rom, map_num);
  int end = piece_placement_offset(rom, map_num + 1);
  int is_fg = 0, offset = start, counter = 0;
  while (offset < end) {
    if (rom[offset] == 0xFE) {
      offset++;
      counter = 0;
      is_fg = 0;
    } else if (rom[offset] == 0xFF) {
      offset++;
      counter = 0;
      is_fg = 1;
    } else {
      struct w8_ref *r = map_add_ref(c, out, is_fg);
      r->vis_flag = rom[offset + 0];
      r->vis_low = rom[offset + 1] >> 4;
      r->vis_high = rom[offset + 1] & 0x0F;
      r->is_fg = is_fg;
      r->index_on_map = counter;
      r->x = rom[offset + 3] >> 1;
      r->y = rom[offset + 4] >> 1;
      r->piece_index = rom[offset + 2];
      if (rom[offset + 3] & 0x01) r->piece_index += 256;
      if (rom[offset + 4] & 0x01) r->piece_index += 512;
      counter++;
      offset += 5;
    }
  }
}

/* makeMapPieceReference: returns the number of bytes written to out */
static size_t make_map_piece_reference(const struct w8_map *m, uint8_t *out) {
  size_t n = 0;
  for (int pass = 0; pass < 2; pass++) {
    const struct w8_ref *refs = pass ? m->fg : m->bg;
    int count = pass ? m->n_fg : m->n_bg;
    for (int i = 0; i < count; i++) {
      if (i == 0) out[n++] = pass ? 0xFF : 0xFE;
      const struct w8_ref *r = &refs[i];
      out[n++] = (uint8_t)r->vis_flag;
      out[n++] = (uint8_t)(r->vis_low << 4 | r->vis_high);
      out[n++] = (uint8_t)r->piece_index;
      out[n++] = (uint8_t)((r->x << 1) | ((r->piece_index & 0x100) ? 1 : 0));
      out[n++] = (uint8_t)((r->y << 1) | ((r->piece_index & 0x200) ? 1 : 0));
    }
  }
  return n;
}

/* ------------------------------------------------------------------------------------------------------ SomMapCompressor */
struct w8_grid {
  int w, h;
  uint8_t *d;   /* row-major: d[y * w + x] is the C# [x, y] */
};

struct stack_info {
  int w, h;
  uint8_t data[256];
  int *positions;
  int n_pos, cap_pos;
};

static struct w8_grid decode_map(struct sr_ctx *c, const uint8_t *data, size_t data_len) {
  struct w8_grid g = {0, 0, NULL};
  if (data_len < 2) return g;
  struct stack_info *stack = (struct stack_info *)sr_alloc(c, 8 * sizeof *stack);
  int n_stack = 0;
  size_t in = 0;
  int width = data[in++];
  int height = data[in++];
  width = (uint8_t)(width / 2 + 1);
  height = (uint8_t)(height / 2 + 1);
  int total = width * height;
  uint8_t *d1 = (uint8_t *)sr_alloc(c, (size_t)total + 1);
  int so_far = 0;
#define NEED(n) do { if (in + (n) > data_len) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was outside the bounds of the array (map piece data)."); } while (0)
  while (so_far < total) {
    NEED(1);
    uint8_t cb = data[in++];
    if (cb >= 0xC0 && cb <= 0xC7) {
      for (int i = 0; i < cb - 0xBF; i++) {
        if (so_far < total && so_far - (1 + i) >= 0) d1[so_far] = d1[so_far - (1 + i)];
        so_far++;
      }
    } else if (cb >= 0xC8 && cb <= 0xCF) {
      for (int i = 0; i < cb - 0xC7; i++) {
        if (so_far < total && so_far - (2 + i) >= 0) d1[so_far] = d1[so_far - (2 + i)];
        so_far++;
      }
    } else if (cb >= 0xD0 && cb <= 0xD7) {
      for (int i = 0; i < cb - 0xCF; i++) {
        if (so_far < total && so_far - (3 + i) >= 0) d1[so_far] = d1[so_far - (3 + i)];
        so_far++;
      }
    } else if (cb >= 0xD8 && cb <= 0xDF) {
      for (int i = 0; i < cb - 0xD7; i++) {
        if (so_far < total && so_far - (4 + i) >= 0) d1[so_far] = d1[so_far - (4 + i)];
        so_far++;
      }
    } else if (cb == 0xE0) {
      NEED(1);
      uint8_t db = data[in++];
      if (db < 0x80) {
        for (int i = 0; i <= db; i++) {
          if (so_far < total && so_far - width >= 0) d1[so_far] = d1[so_far - width];
          so_far++;
        }
      } else {
        db %= 0x80;
        for (int i = 0; i <= db; i++) {
          if (so_far < total && so_far - (2 * width) >= 0) d1[so_far] = d1[so_far - (2 * width)];
          so_far++;
        }
      }
    } else if (cb >= 0xE1 && cb <= 0xE7) {
      for (int i = 0; i < cb - 0xDF; i++) {
        if (so_far < total && so_far - width >= 0) d1[so_far] = d1[so_far - width];
        so_far++;
      }
    } else if (cb >= 0xE8 && cb <= 0xEF) {
      NEED(1);
      uint8_t db = data[in++];
      uint8_t word[16];
      for (int i = 0; i < 16; i++) word[i] = (so_far - i - 1 >= 0) ? d1[so_far - i - 1] : 0;
      for (int i = 0; i <= (db % 0x80); i++) {
        if (cb == 0xE8 && db < 0x80) {
          if (so_far < total) d1[so_far] = word[0];
        } else {
          int numofprevs = (2 * (cb - 0xE8)) + (db / 0x80);
          int index = numofprevs - (i % (numofprevs + 1));
          if (so_far < total) d1[so_far] = word[index];
        }
        so_far++;
      }
    } else if (cb == 0xF0) {
      NEED(1);
      uint8_t db = data[in++];
      int iters = db;
      int upward = 1;
      if (iters >= 128) {
        iters -= 128;
        upward = 0;
      }
      for (int i = 0; i <= iters; i++) {
        if (upward) {
          if (so_far < total && so_far - (1 + i) >= 0) d1[so_far] = (uint8_t)((d1[so_far - (1 + i)] + i + 1) % 0x100);
        } else {
          if (so_far < total && so_far - (1 + i) >= 0) d1[so_far] = (uint8_t)((d1[so_far - (1 + i)] - i - 1) % 0x100);
        }
        so_far++;
      }
    } else if (cb >= 0xF1 && cb <= 0xF7) {
      for (int i = 0; i < cb - 0xEF; i++) {
        if (so_far < total && so_far - (1 + i) >= 0) d1[so_far] = (uint8_t)(d1[so_far - (1 + i)] + i + 1);
        so_far++;
      }
    } else if (cb >= 0xF8 && cb <= 0xFD) {
      int this_index = cb - 0xF8;
      if (this_index < n_stack) {
        struct stack_info *s = &stack[this_index];
        if (s->n_pos == s->cap_pos) {
          int nc = s->cap_pos ? s->cap_pos * 2 : 16;
          int *np = (int *)sr_alloc(c, (size_t)nc * sizeof *np);
          if (s->n_pos) memcpy(np, s->positions, (size_t)s->n_pos * sizeof *np);
          s->positions = np;
          s->cap_pos = nc;
        }
        s->positions[s->n_pos++] = so_far;
        so_far += 1;
      }
    } else if (cb == 0xFE || cb == 0xFF) {
      uint8_t settings = d1[0];
      int sw = (settings & 0x0F) + 1;
      int sh = ((settings & 0xF0) >> 4) + 1;
      if (n_stack >= 8) sr_fail(c, SOMRANDO_ERR_FAILED, "map piece stack overflow");
      struct stack_info *s = &stack[n_stack];
      memset(s, 0, sizeof *s);
      s->w = sw;
      s->h = sh;
      int sx = 0, sy = 0;
      while (sy < sh) {
        if (sx + sy * sw + 1 < total) s->data[sy * sw + sx] = d1[sx + sy * sw + 1];
        sx++;
        if (sx >= sw) {
          sx = 0;
          sy++;
        }
      }
      so_far -= (sw * sh + 1);
      if (so_far < 0) so_far = 0;
      n_stack++;
    } else {
      d1[so_far] = cb;
      so_far += 1;
    }
  }
#undef NEED
  for (int k = 0; k < n_stack; k++) {
    struct stack_info *s = &stack[k];
    for (int p = 0; p < s->n_pos; p++) {
      int pos = s->positions[p];
      for (int y = 0; y < s->h; y++)
        for (int x = 0; x < s->w; x++)
          if (pos + x + y * width < total) d1[pos + x + y * width] = s->data[y * s->w + x];
    }
  }
  g.w = width;
  g.h = height;
  g.d = d1;
  return g;
}

/* EncodeMap(byte[] mapData, width, height) */
static void encode_map(struct sr_ctx *c, const uint8_t *map, int width, int height, struct sr_bytes *dest) {
  int idx = 0;
  const int len = width * height;
  sr_bytes_push(c, dest, (width - 1) * 2);
  sr_bytes_push(c, dest, (height - 1) * 2);
  while (idx < len) {
    int cmd = 0, arg = -1, best = 0, src_len = 0;
    int matches;
    /* C0-C7 .. D8-DF: repeat the Nth previous decoded byte */
    for (int back = 1; back <= 4; back++) {
      if (idx > back - 1) {
        matches = 0;
        for (int i = 0; i < 8; i++) {
          if (idx + i < len && map[idx + i] == map[idx - back]) matches++;
          else break;
        }
        if (matches > 1) {
          int saved = matches - 1;
          if (saved > best) {
            best = saved;
            cmd = (back == 1 ? 0xBF : back == 2 ? 0xC7 : back == 3 ? 0xCF : 0xD7) + matches;
            arg = -1;
            src_len = matches;
          }
        }
      }
    }
    /* E0-E7: copy the tile "above" (-width) */
    if (idx >= width) {
      matches = 0;
      for (int i = 0; i < 8; i++) {
        if (idx + i < len && map[idx + i] == map[idx - width + i]) matches++;
        else break;
      }
      if (matches > 1) {
        int saved = matches - 1;
        if (saved > best) {
          best = saved;
          cmd = 0xDF + matches;
          arg = -1;
          src_len = matches;
        }
      }
    }
    /* F0: next tile (param & 0x80 == 0) or previous tile in the tileset */
    if (idx > 0) {
      matches = 0;
      for (int i = 0; i < 0x7f; i++) {
        if (idx + i < len && map[idx + i] == map[idx - 1] + i + 1) matches++;
        else break;
      }
      if (matches > 1) {
        int saved = matches - 2;
        if (saved > best) {
          best = saved;
          cmd = 0xF0;
          arg = matches - 1;
          src_len = matches;
        }
      }
      matches = 0;
      for (int i = 0; i < 0x7f; i++) {
        if (idx + i < len && map[idx + i] == map[idx - 1] - i - 1) matches++;
        else break;
      }
      if (matches > 1) {
        int saved = matches - 2;
        if (saved > best) {
          best = saved;
          cmd = 0xF0;
          arg = 0x80 | (matches - 1);
          src_len = matches;
        }
      }
    }
    /* F1-F7: F0, but a fixed number of positives */
    if (idx > 0) {
      matches = 0;
      for (int i = 0; i < 8; i++) {
        if (idx + i < len && map[idx + i] == map[idx - 1] + i + 1) matches++;
        else break;
      }
      if (matches > 1) {
        int saved = matches - 1;
        if (saved > best) {
          best = saved;
          cmd = 0xEF + matches;
          arg = -1;
          src_len = matches;
        }
      }
    }
    if (cmd == 0) {
      sr_bytes_push(c, dest, map[idx]);
      idx++;
    } else {
      sr_bytes_push(c, dest, cmd);
      if (arg != -1) sr_bytes_push(c, dest, arg);
      idx += src_len;
    }
  }
}

/* loadVanillaMapPiece(rom, pieceIndex) */
static struct w8_grid load_vanilla_map_piece(struct sr_ctx *c, const uint8_t *rom, int piece_index) {
  int po = map_piece_offset(rom, piece_index);
  int next = map_piece_offset(rom, piece_index + 1);
  if ((next & 0xFF0000) != (po & 0xFF0000)) next = (po & 0xFF0000) + 0x10000;
  if (next - po < 0) sr_fail(c, SOMRANDO_ERR_FAILED, "Unable to load vanilla map piece %d", piece_index);
  return decode_map(c, rom + po, (size_t)(next - po));
}

struct door_tile { uint8_t door, repl; };
static const struct door_tile tiles_1[] = {{0xB2, 0x94}};
static const struct door_tile tiles_9[] = {{0xB1, 0xB0}, {0xBD, 0x5E}};
static const struct door_tile tiles_10[] = {{0x77, 0x00}, {0x64, 0x00}, {0x74, 0x00}};
static const struct door_tile tiles_14[] = {{0x66, 0x0F}};
static const struct door_tile tiles_16[] = {{0x35, 0x12}, {0x4A, 0x49}};
static const struct door_tile tiles_17[] = {{0x18, 0x5D}, {0x7C, 0x5D}};
static const struct door_tile tiles_19[] = {{0xAF, 0xAE}, {0x89, 0x00}};
static const struct door_tile tiles_21[] = {{0x36, 0x27}, {0x3D, 0x27}};
static const struct door_tile tiles_23[] = {{0x80, 0x43}, {0x81, 0x43}, {0xBF, 0xBA}, {0x97, 0x36}, {0xAA, 0x00}};
static const struct door_tile tiles_26[] = {{0x4D, 0x00}};
static const struct door_tile tiles_27[] = {{0xBD, 0x00}, {0xBA, 0xA2}};
static const struct door_tile tiles_30[] = {{0x5D, 0x54}};

static struct w8_grid load_vanilla_map_piece_remove_doors(struct sr_ctx *c, const uint8_t *rom, int piece_index, int composite_map) {
  struct w8_grid g = load_vanilla_map_piece(c, rom, piece_index);
  int tileset16 = w8_hdr_get_tileset16(rom + object_offset(rom, composite_map));
  const struct door_tile *t = NULL;
  int n = 0;
#define TS(id, arr) case id: t = arr; n = (int)(sizeof arr / sizeof *arr); break;
  switch (tileset16) {
    TS(1, tiles_1) TS(9, tiles_9) TS(10, tiles_10) TS(14, tiles_14) TS(16, tiles_16) TS(17, tiles_17) TS(19, tiles_19)
    TS(21, tiles_21) TS(23, tiles_23) TS(26, tiles_26) TS(27, tiles_27) TS(30, tiles_30)
    default: break;
  }
#undef TS
  for (int k = 0; k < n; k++)
    for (int i = 0; i < g.w * g.h; i++)
      if (g.d[i] == t[k].door) g.d[i] = t[k].repl;
  return g;
}

/* copyMapPieceData(source, dest, destX, destY) */
static void copy_map_piece_data(const struct w8_grid *src, struct w8_grid *dst, int dx, int dy) {
  for (int y = 0; y < src->h; y++)
    for (int x = 0; x < src->w; x++) {
      int tx = x + dx, ty = y + dy;
      if (tx >= 0 && tx < dst->w && ty >= 0 && ty < dst->h) dst->d[ty * dst->w + tx] = src->d[y * src->w + x];
    }
}

/* ------------------------------------------------------------------------------------------------- GeneratedMapUtil */
struct w8_generated {
  struct w8_map *map;
  struct w8_grid l1, l2;
  int layer2_collision;
};

static int generated_palette_index_next(struct sr_ctx *c) {
  int v = w8_wget_int(c, "GeneratedPaletteIndex");
  sr_work_set_int(c, "GeneratedPaletteIndex", v + 1);
  return v;
}

/* GeneratedMapUtil.importVanillaBossArena; the new map is registered by the caller */
static struct w8_generated import_vanilla_boss_arena(struct sr_ctx *c, int boss_id) {
  const struct sr_w8_boss_map *vm = NULL;
  for (int i = 0; i < W8_NUM_BOSS_ENTRIES; i++)
    if (sr_w8_boss_maps[i].id == boss_id) vm = &sr_w8_boss_maps[i];
  if (!vm) sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%d' was not present in the dictionary.", boss_id);

  struct w8_generated res;
  memset(&res, 0, sizeof res);
  res.map = (struct w8_map *)sr_alloc(c, sizeof *res.map);
  uint8_t *hdr = res.map->header;
  memcpy(hdr, c->orig + object_offset(c->orig, vm->original_map_num), 8);
  if (sr_work_get(c, "GeneratedPaletteIndex")) {
    int pal_index = generated_palette_index_next(c);
    sr_w8_copy_palette_animation_settings(c, w8_hdr_get_palette_set(hdr), pal_index);
    uint8_t pal[210];
    memcpy(pal, c->orig + 0xC8000 + w8_hdr_get_palette_set(hdr) * 30 * 7, 210);
    w8_set_pal(c, pal_index, pal);
    w8_hdr_set_palette_set(hdr, pal_index);
  }

  struct w8_map *og = (struct w8_map *)sr_alloc(c, sizeof *og);
  get_map_piece_reference(c, c->orig, vm->original_map_num, og);
  struct w8_ref *f_ref[2 * W8_MAX_REFS];
  struct w8_grid f_data[2 * W8_MAX_REFS];
  int nf = 0;
  for (int i = 0; i < og->n_bg; i++) {
    int excluded = 0;
    for (int k = 0; k < vm->n_bg_exclusions; k++)
      if (vm->bg_exclusions[k] == og->bg[i].index_on_map) excluded = 1;
    if (!excluded) {
      f_ref[nf] = &og->bg[i];
      f_data[nf++] = load_vanilla_map_piece_remove_doors(c, c->orig, og->bg[i].piece_index, vm->original_map_num);
    }
  }
  for (int i = 0; i < og->n_fg; i++) {
    int excluded = 0;
    for (int k = 0; k < vm->n_fg_exclusions; k++)
      if (vm->fg_exclusions[k] == og->fg[i].index_on_map) excluded = 1;
    if (!excluded) {
      f_ref[nf] = &og->fg[i];
      f_data[nf++] = load_vanilla_map_piece_remove_doors(c, c->orig, og->fg[i].piece_index, vm->original_map_num);
    }
  }
  int max_w = 0, max_h = 0;
  for (int i = 0; i < nf; i++) {
    int wn = f_ref[i]->x + f_data[i].w;
    int hn = f_ref[i]->y + f_data[i].h;
    if (wn > max_w) max_w = wn;
    if (hn > max_h) max_h = hn;
  }
  res.l1.w = res.l2.w = max_w;
  res.l1.h = res.l2.h = max_h;
  res.l1.d = (uint8_t *)sr_alloc(c, (size_t)max_w * max_h + 1);
  res.l2.d = (uint8_t *)sr_alloc(c, (size_t)max_w * max_h + 1);
  for (int i = 0; i < nf; i++) {
    if (!f_ref[i]->is_fg) copy_map_piece_data(&f_data[i], &res.l1, f_ref[i]->x, f_ref[i]->y);
    else copy_map_piece_data(&f_data[i], &res.l2, f_ref[i]->x, f_ref[i]->y);
  }
  res.layer2_collision = vm->collide_layer2;

  if (boss_id == 0x80) {
    /* put the other two tonpoles in */
    const struct sr_w8_boss_map *tp = NULL;
    for (int i = 0; i < W8_NUM_BOSS_ENTRIES; i++)
      if (sr_w8_boss_maps[i].id == 0x71) tp = &sr_w8_boss_maps[i];
    int objs = object_offset(c->orig, tp->original_map_num) + 8 + tp->original_obj_num * 8 + 5;
    boss_id = c->orig[objs];
    uint8_t *t1 = map_add_object(c, res.map);
    w8_obj_set_species(t1, boss_id);
    w8_obj_set_xpos(t1, (uint8_t)(vm->boss_x - 5));
    w8_obj_set_ypos(t1, (uint8_t)(vm->boss_y + 4));
    obj_always_visible(t1);
    uint8_t *t2 = map_add_object(c, res.map);
    w8_obj_set_species(t2, boss_id);
    w8_obj_set_xpos(t2, (uint8_t)(vm->boss_x + 6));
    w8_obj_set_ypos(t2, (uint8_t)(vm->boss_y + 4));
    obj_always_visible(t2);
  }
  uint8_t *bo = map_add_object(c, res.map);
  w8_obj_set_species(bo, boss_id);
  w8_obj_set_xpos(bo, (uint8_t)vm->boss_x);
  w8_obj_set_ypos(bo, (uint8_t)vm->boss_y);
  obj_always_visible(bo);
  return res;
}

/* Ancient Cave: GeneratedMapUtil.importVanillaBossArena as a GeneratedMap (not an AncientCaveMap) */
struct ac_map *ac_import_vanilla_boss_arena(struct sr_ctx *c, int boss_id) {
  struct w8_generated g = import_vanilla_boss_arena(c, boss_id);
  struct ac_map *m = (struct ac_map *)sr_alloc(c, sizeof *m);
  const struct sr_w8_boss_map *vm = NULL;
  for (int i = 0; i < W8_NUM_BOSS_ENTRIES; i++)
    if (sr_w8_boss_maps[i].id == boss_id) vm = &sr_w8_boss_maps[i];
  m->md = g.map;
  m->layer1 = (struct ac_layer *)sr_alloc(c, sizeof *m->layer1);
  m->layer1->w = g.l1.w; m->layer1->h = g.l1.h; m->layer1->d = g.l1.d;
  m->layer2 = (struct ac_layer *)sr_alloc(c, sizeof *m->layer2);
  m->layer2->w = g.l2.w; m->layer2->h = g.l2.h; m->layer2->d = g.l2.d;
  m->layer2_collision = g.layer2_collision;
  m->entry_pos = ac_xy_new(c, vm->entry_x, vm->entry_y);
  strcpy(m->map_name, "?");
  return m;
}

/* ------------------------------------------------------------------------------------------------------- ManaBeastMap */
static void make_mana_beast_map(struct sr_ctx *c) {
  int death_event = w8_wget_int(c, "ManaBeastDeathEvent");
  int entry_event = w8_wget_int(c, "ManaBeastWalkonEvent");
  struct w8_map *m = new_generated_map(c, 0xFD);
  memcpy(m->header, c->orig + object_offset(c->orig, 0xFD), 8);
  uint8_t *o = map_add_object(c, m);
  w8_obj_set_direction(o, 1);
  w8_obj_set_xpos(o, 18);
  w8_obj_set_ypos(o, 18);
  w8_obj_set_species(o, 0x7F);
  w8_obj_set_event(o, death_event);
  obj_always_visible(o);
  /* a single layer 1 piece for collision, with no graphics - vanilla 753 */
  struct w8_grid g = load_vanilla_map_piece(c, c->orig, 753);
  struct sr_bytes enc = {0};
  encode_map(c, g.d, g.w, g.h, &enc);
  w8_set_piece(c, 753, enc.p, enc.len);
  struct w8_ref *r = map_add_ref(c, m, 0);
  r->vis_flag = 0x00;
  r->vis_low = 0x00;
  r->vis_high = 0x0F;
  r->piece_index = 753;
  if (entry_event >= 0) {
    w8_hdr_set_walkon(m->header, 1);
    if (m->n_triggers >= W8_MAX_TRIGGERS) sr_fail(c, SOMRANDO_ERR_FAILED, "too many triggers");
    m->triggers[m->n_triggers++] = (uint16_t)entry_event;
  }
}

/* ------------------------------------------------------------------------------------------------------- CreditsMap */
static void make_credits_map(struct sr_ctx *c) {
  const int map_id = 0xB4;
  struct w8_map *m = new_generated_map(c, map_id);
  memcpy(m->header, c->orig + object_offset(c->orig, map_id), 8);
  for (int layer = 0; layer < 2; layer++) {
    int piece = layer ? 898 : 897;
    struct w8_grid g = load_vanilla_map_piece(c, c->orig, piece);
    struct sr_bytes enc = {0};
    encode_map(c, g.d, g.w, g.h, &enc);
    w8_set_piece(c, piece, enc.p, enc.len);
    struct w8_ref *r = map_add_ref(c, m, layer);
    r->vis_flag = 0x00;
    r->vis_low = 0x00;
    r->vis_high = 0x0F;
    r->piece_index = piece;
  }
}

/* ------------------------------------------------------------------------------------------------------------ RestMap */
static void make_rest_map_common(struct sr_ctx *c) {
  uint8_t layer1[3500], layer2[3500];
  memcpy(layer1, sr_w8_rest_map_l1, sizeof layer1);
  memcpy(layer2, sr_w8_rest_map_l2, sizeof layer2);
  for (int i = 0; i < (int)sizeof layer1; i++) {
    uint8_t v = layer2[i];
    if (v == 0x2A || v == 0x19 || v == 0x1A || v == 0x1B || v == 0x63) layer1[i] = 0x84;   /* solid wall */
    if (v == 0x57 || v == 0x2B) layer1[i] = 0x95;                                           /* wall in upper left */
    if (v == 0x29 || v == 0x61) layer1[i] = 0x93;                                           /* wall in upper right */
    if (v == 0x0B) layer1[i] = 0x72;                                                        /* wall in lower left */
    if (v == 0x09 || v == 0x41) layer1[i] = 0x70;                                           /* wall in lower right */
    if (v == 0x5D || v == 0x5E || v == 0x5F || v == 0x6D || v == 0x6E || v == 0x6F || v == 0x7D || v == 0x7E || v == 0x7F || v == 0x9F ||
        v == 0xAC || v == 0xAD || v == 0xAE || v == 0xAF)
      layer1[i] = 0x84;   /* waterfall->solid wall */
  }
  layer1[46 * 50 + 27] = 0x84;
  layer1[46 * 50 + 28] = 0x84;
  /* door */
  layer1[8 * 50 + 33] = 182;
  layer2[8 * 50 + 33] = 0x1A;
  layer2[8 * 50 + 32] = 175;
  layer2[7 * 50 + 33] = 0x1A;
  layer2[7 * 50 + 32] = 159;
  layer2[6 * 50 + 32] = 0x6e;
  layer2[6 * 50 + 33] = 174;

  struct sr_bytes enc = {0};
  encode_map(c, layer1, 50, 70, &enc);
  w8_set_piece(c, 128, enc.p, enc.len);
  memset(&enc, 0, sizeof enc);
  encode_map(c, layer2, 50, 70, &enc);
  w8_set_piece(c, 129, enc.p, enc.len);

  /* palette index 0 for rest map palette, animation settings from vanilla palette set 54 */
  sr_w8_copy_palette_animation_settings(c, 54, 0);
  {
    size_t pal_len = 0;
    const uint8_t *pal = sr_rom_res(c, "bossRushRestMapPal.bin", &pal_len);
    if (!pal || pal_len != 210) sr_fail(c, SOMRANDO_ERR_FAILED, "resource bossRushRestMapPal.bin is missing");
    w8_set_pal(c, 0, pal);
  }
}

static void rest_npc(struct sr_ctx *c, struct w8_map *m, int event, int x, int y, int species) {
  uint8_t *o = map_add_object(c, m);
  w8_obj_set_direction(o, 1);
  w8_obj_set_event(o, event);
  obj_always_visible(o);
  w8_obj_set_xpos(o, x);
  w8_obj_set_ypos(o, y);
  w8_obj_set_species(o, species);
  w8_obj_set_unknown_b7(o, 0x08);
  w8_obj_set_unknown4a(o, 1);
}

static void make_rest_map(struct sr_ctx *c) {
  int neko_event = w8_wget_int(c, "nekoEventNum");
  int watts_event = w8_wget_int(c, "wattsEventNum");
  int phanna_event = w8_wget_int(c, "phannaEventNum");
  int floor_num = w8_wget_int(c, "FloorNumber");
  struct w8_map *m = new_generated_map(c, floor_num * 2);
  rest_npc(c, m, neko_event, 30, 9, 0x99);
  rest_npc(c, m, watts_event, 29, 9, 0x9C);
  rest_npc(c, m, phanna_event, 29, 11, 0xA6);
  struct w8_ref *bg = map_add_ref(c, m, 0);
  bg->vis_flag = 0x00;
  bg->vis_low = 0x00;
  bg->vis_high = 0x0F;
  bg->piece_index = 128;
  struct w8_ref *fg = map_add_ref(c, m, 1);
  fg->vis_flag = 0x00;
  fg->vis_low = 0x00;
  fg->vis_high = 0x0F;
  fg->piece_index = 129;
  char key[48];
  if (floor_num == 0) {
    snprintf(key, sizeof key, "restMapEntryEventId%d", floor_num);
    int walkon = w8_wget_int(c, key);
    m->triggers[m->n_triggers++] = (uint16_t)walkon;
  }
  snprintf(key, sizeof key, "nextFloorEventId%d", floor_num);
  int boss_entry = w8_wget_int(c, key);
  m->triggers[m->n_triggers++] = (uint16_t)boss_entry;

  uint8_t *h = m->header;
  w8_hdr_set_palette_set(h, 0);
  w8_hdr_set_display(h, 5);
  w8_hdr_set_tileset16(h, 30);
  w8_hdr_set_tileset8(h, 30);
  w8_hdr_set_walkon(h, floor_num == 0);
  w8_hdr_set_npc_palette(h, 0xFF);   /* hostile map */
  w8_hdr_set_magic_rope(h, 0);
  w8_hdr_set_flammie(h, 1);
  w8_hdr_set_layer1_fg(h, 0);
  w8_hdr_set_layer2_fg(h, 1);
  w8_hdr_set_animated_tiles(h, 1);
  w8_hdr_set_palette_animated(h, 1);
}

/* ------------------------------------------------------------------------------------------------- BossRushRandomizer */
int sr_w8_hack_bossrush_randomizer(struct sr_ctx *c) {
  int boss_in_order[W8_NUM_BOSS_ENTRIES + 1];
  const int num_maps = W8_NUM_BOSS_ENTRIES;
  char key[48];
  for (int i = 0; i <= num_maps; i++) {
    snprintf(key, sizeof key, "bossNum%d", i);
    boss_in_order[i] = w8_wget_int(c, key);
  }
  int door_num = 10;
  int entry_doors[W8_NUM_BOSS_ENTRIES * 2 + 8];
  int n_entry = 0;
  entry_doors[n_entry++] = door_num;

  /* make rest map pieces and palette shared by every floor */
  make_rest_map_common(c);
  sr_work_set_int(c, "GeneratedPaletteIndex", 16);
  for (int i = 0; i <= num_maps; i++) {
    sr_work_set_int(c, "FloorNumber", i);
    make_rest_map(c);

    entry_doors[n_entry++] = door_num;
    uint8_t d[4];
    w8_door_init(d, i * 2, 34, 13, 0);
    w8_set_door(c, door_num++, d);

    int boss_num = (uint8_t)boss_in_order[i];
    if (boss_num != 0x7F) {
      const struct sr_w8_boss_map *vm = NULL;
      for (int k = 0; k < W8_NUM_BOSS_ENTRIES; k++)
        if (sr_w8_boss_maps[k].id == boss_num) vm = &sr_w8_boss_maps[k];
      struct w8_generated gm = import_vanilla_boss_arena(c, boss_num);
      /* clear any walkon flag from vanilla so not to execute random events that aren't present anymore */
      w8_hdr_set_walkon(gm.map->header, 0);
      for (int k = 0; k < gm.map->n_objs; k++) {
        snprintf(key, sizeof key, "bossDeathEventId%d", i);
        w8_obj_set_event(gm.map->objs[k], w8_wget_int(c, key));
      }
      /* generatedMaps[i * 2 + 256] = vanillaBossMap.mapData */
      struct w8_state *st = w8_state_get(c);
      int slot = -1;
      for (int k = 0; k < st->n_maps; k++)
        if (st->maps[k]->id == i * 2 + 256) slot = k;
      gm.map->id = i * 2 + 256;
      if (slot >= 0) st->maps[slot] = gm.map;
      else {
        if (st->n_maps >= W8_MAX_MAPS) sr_fail(c, SOMRANDO_ERR_FAILED, "too many generated maps");
        st->maps[st->n_maps++] = gm.map;
      }
      struct w8_ref *bg = map_add_ref(c, gm.map, 0);
      bg->piece_index = i * 2;
      bg->vis_flag = 0;
      bg->vis_low = 0;
      bg->vis_high = 0x0F;
      struct w8_ref *fg = map_add_ref(c, gm.map, 1);
      fg->piece_index = i * 2 + 1;
      fg->is_fg = 1;
      fg->vis_flag = 0;
      fg->vis_low = 0;
      fg->vis_high = 0x0F;
      struct sr_bytes enc = {0};
      encode_map(c, gm.l1.d, gm.l1.w, gm.l1.h, &enc);
      w8_set_piece(c, i * 2, enc.p, enc.len);
      memset(&enc, 0, sizeof enc);
      encode_map(c, gm.l2.d, gm.l2.w, gm.l2.h, &enc);
      w8_set_piece(c, i * 2 + 1, enc.p, enc.len);

      w8_door_init(d, i * 2 + 256, vm->entry_x, vm->entry_y, 0);
      door_set_layer2_collision(d, gm.layer2_collision);
      w8_set_door(c, door_num++, d);
    } else {
      /* mana beast entry */
      w8_door_init(d, 0xFD, 18, 22, 0x81);
      w8_set_door(c, 0xBC, d);
    }
  }

  make_mana_beast_map(c);
  make_credits_map(c);

  int start_door_offset = w8_wget_int(c, W8_KEY_START_DOOR_OFFSET);
  /* fill in the doors array for starting each map - this is used for magic rope and continues */
  for (int i = 0; i < num_maps; i++) {
    P(start_door_offset + i * 2, entry_doors[i]);
    P(start_door_offset + i * 2 + 1, entry_doors[i] >> 8);
  }
  return 1;
}


/* ------------------------------------------------------------------------------------------------------ MapPieceExpander */
void w8_pieces_expand(struct sr_ctx *c) {
  struct w8_state *st = w8_state_get(c);
  if (st->n_pieces == 0) return;
  uint8_t *rom = c->out;
  int new_offsets[0x400];
  for (int i = 0; i < 0x400; i++) new_offsets[i] = map_piece_offset(rom, i);
  for (int k = 0; k < st->n_pieces; k++) {
    const struct sr_bytes *d = &st->pieces[k].data;
    sr_ensure_space_in_bank(&c->working_offset, (int)d->len);
    new_offsets[st->pieces[k].id] = c->working_offset;
    for (size_t i = 0; i < d->len; i++) E(d->p[i]);
  }
  /* dump list of offsets, 24-bit hirom, out to rom in one contiguous block */
  sr_ensure_space_in_bank(&c->working_offset, 0x400 * 3);
  int offsets_offset = c->working_offset + 0xC00000;
  int offsets_offset_p2 = offsets_offset + 2;
  for (int i = 0; i < 0x400; i++) {
    E(new_offsets[i]);
    E(new_offsets[i] >> 8);
    E(0xC0 + (new_offsets[i] >> 16));
  }
  /* custom map loader that grabs the pieces from 24-bit locations */
  for (int o = 0xCC79; o <= 0xCC9C; o++) rom[o] = 0xEA;
  int off = 0xCC79;
  rom[off++] = 0xC2;
  rom[off++] = 0x30;
  rom[off++] = 0x85;
  rom[off++] = 0x1D;
  rom[off++] = 0x0A;
  rom[off++] = 0x18;
  rom[off++] = 0x65;
  rom[off++] = 0x1D;
  rom[off++] = 0xAA;
  rom[off++] = 0xBF;
  rom[off++] = (uint8_t)offsets_offset;
  rom[off++] = (uint8_t)(offsets_offset >> 8);
  rom[off++] = (uint8_t)(offsets_offset >> 16);
  rom[off++] = 0x85;
  rom[off++] = 0x1D;
  rom[off++] = 0xE2;
  rom[off++] = 0x20;
  rom[off++] = 0xBF;
  rom[off++] = (uint8_t)offsets_offset_p2;
  rom[off++] = (uint8_t)(offsets_offset_p2 >> 8);
  rom[off++] = (uint8_t)(offsets_offset_p2 >> 16);
  rom[off++] = 0x85;
  rom[off++] = 0x1F;
}

/* ---------------------------------------------------------------------------------------------------------- DoorReplacer */
void w8_doors_replace(struct sr_ctx *c) {
  struct w8_state *st = w8_state_get(c);
  if (st->n_doors == 0) return;
  int doors_offset = 0x83000;
  if (sr_work_get(c, W8_KEY_DOOR_LOCATION)) doors_offset = w8_wget_int(c, W8_KEY_DOOR_LOCATION);
  for (int i = 0; i < st->n_doors; i++)
    for (int k = 0; k < 4; k++) P(doors_offset + st->doors[i].id * 4 + k, st->doors[i].d[k]);
}

/* ------------------------------------------------------------------------------------------------------- FullMapReplacer */
static void write16(struct sr_ctx *c, int off, int v) {
  P(off, v);
  P(off + 1, v >> 8);
}

void w8_maps_replace(struct sr_ctx *c) {
  struct w8_state *st = w8_state_get(c);
  if (st->n_maps == 0) return;
  const int trigger_offsets = 0x84000, piece_offsets = 0x85000, object_offsets = 0x87000;
  int offset = 0x87400;
  /* SortedSet<int> of the map numbers */
  int order[W8_MAX_MAPS];
  for (int i = 0; i < st->n_maps; i++) order[i] = i;
  for (int i = 1; i < st->n_maps; i++) {
    int v = order[i], j = i - 1;
    while (j >= 0 && st->maps[order[j]]->id > st->maps[v]->id) {
      order[j + 1] = order[j];
      j--;
    }
    order[j + 1] = v;
  }
  for (int k = 0; k < st->n_maps; k++) {
    struct w8_map *m = st->maps[order[k]];
    int objects_size = 8 + m->n_objs * 8;
    write16(c, object_offsets + m->id * 2, offset);
    for (int i = 0; i < 8; i++) P(offset + i, m->header[i]);
    for (int i = 0; i < m->n_objs; i++)
      for (int b = 0; b < 8; b++) P(offset + 8 + 8 * i + b, m->objs[i][b]);
    offset += objects_size;
    write16(c, object_offsets + (m->id + 1) * 2, offset);
  }
  for (int k = 0; k < st->n_maps; k++) {
    struct w8_map *m = st->maps[order[k]];
    uint8_t ref[W8_MAX_REFS * 10 + 4];
    size_t n = make_map_piece_reference(m, ref);
    write16(c, piece_offsets + m->id * 2, offset);
    for (size_t i = 0; i < n; i++) P(offset + (int)i, ref[i]);
    offset += (int)n;
    write16(c, piece_offsets + (m->id + 1) * 2, offset);
  }
  for (int k = 0; k < st->n_maps; k++) {
    struct w8_map *m = st->maps[order[k]];
    write16(c, trigger_offsets + m->id * 2, offset);
    for (int i = 0; i < m->n_triggers; i++) {
      P(offset + i * 2, m->triggers[i]);
      P(offset + i * 2 + 1, m->triggers[i] >> 8);
    }
    offset += m->n_triggers * 2;
    write16(c, trigger_offsets + (m->id + 1) * 2, offset);
  }
}

/* ------------------------------------------------------------------------------------------------ MapPaletteSetReplacer */
void w8_pals_replace(struct sr_ctx *c) {
  struct w8_state *st = w8_state_get(c);
  for (int i = 0; i < st->n_pals; i++)
    for (int k = 0; k < 210; k++) P(0xC8000 + st->pals[i].id * 30 * 7 + k, st->pals[i].d[k]);
}

void w8_make_mana_beast_map(struct sr_ctx *c) { make_mana_beast_map(c); }
void w8_make_credits_map(struct sr_ctx *c) { make_credits_map(c); }

/* ------------------------------------------------------------------------------------------ exports for chaos mode (w9) */
struct w8_map *w9_map_new(struct sr_ctx *c, int id) { return new_generated_map(c, id); }
uint8_t *w9_map_add_object(struct sr_ctx *c, struct w8_map *m) { return map_add_object(c, m); }
struct w8_ref *w9_map_add_ref(struct sr_ctx *c, struct w8_map *m, int fg) { return map_add_ref(c, m, fg); }
int w9_object_offset(const uint8_t *rom, int map_num) { return object_offset(rom, map_num); }
int w9_trigger_offset(const uint8_t *rom, int map_num) {
  return bank_start(MAP_TRIGGER_OFFSETS) + rom[MAP_TRIGGER_OFFSETS + map_num * 2] + (rom[MAP_TRIGGER_OFFSETS + map_num * 2 + 1] << 8);
}
void w9_get_map_piece_reference(struct sr_ctx *c, int map_num, struct w8_map *out) { get_map_piece_reference(c, c->orig, map_num, out); }
/* replacementMapPieces[piece_index] = EncodeMap(loadVanillaMapPiece(origRom, piece_index)) */
/* SomMapCompressor.EncodeMap(byte[,]) of a row-major layer */
void w8_encode_layer(struct sr_ctx *c, const uint8_t *d, int w, int h, struct sr_bytes *dest) { encode_map(c, d, w, h, dest); }

void w9_copy_piece_encoded(struct sr_ctx *c, int piece_index) {
  struct w8_grid g = load_vanilla_map_piece(c, c->orig, piece_index);
  struct sr_bytes enc = {0};
  encode_map(c, g.d, g.w, g.h, &enc);
  w8_set_piece(c, piece_index, enc.p, enc.len);
}
