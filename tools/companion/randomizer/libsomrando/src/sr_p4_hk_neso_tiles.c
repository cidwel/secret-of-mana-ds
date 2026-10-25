/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Nesoberi tileset step: the 16x16 tileset codec of processing/common/{VanillaTilesetUtil.cs,structure/Tileset16.cs} (Author: Moppleton; the same
 * code as the static copy in sr_p3_ac_tileset.c) and the palette change that Nesoberi.cs applies to vanilla tileset 15.  Translated to C 2026-10-18.
 */
#include <string.h>

#include "sr_core.h"
#include "sr_p4.h"

#define TILESET16_OFFSETS 0xB4000

/* ------------------------------------------------------------------------------------------------------ Tile8 / Tile16 */
struct tile8 {
  int vflip, hflip, altbg;
  int pal;       /* 3 bits */
  int tile_num;  /* 10 bits */
};

static struct tile8 tile8_new(int vflip, int hflip, int altbg, int pal, int tile_num) {
  struct tile8 t;
  t.vflip = vflip;
  t.hflip = hflip;
  t.altbg = altbg;
  t.pal = (uint8_t)pal & 0x7;
  t.tile_num = (uint16_t)tile_num & 0x3FF;
  return t;
}

static int tile8_to_binary(const struct tile8 *t) {
  return (uint16_t)((t->vflip ? 1 << 15 : 0) | (t->hflip ? 1 << 14 : 0) | (t->altbg ? 1 << 13 : 0) | (t->pal << 10) | t->tile_num);
}

struct tileset16 {
  struct tile8 t[384][4];
};

/* ------------------------------------------------------------------------------------------------------ bit streams */
struct bitpos {
  int byte_num, bit_num;
};

static int extract_bits(const uint8_t *comp, size_t len, int num_bits, struct bitpos *bp) {
  uint16_t result = 0;
  for (int i = 0; i < num_bits; i++) {
    result = (uint16_t)(result << 1);
    uint8_t mask = (uint8_t)(1 << (7 - bp->bit_num));
    if ((size_t)bp->byte_num < len && (comp[bp->byte_num] & mask) > 0) result |= 1;
    bp->bit_num++;
    if (bp->bit_num == 8) {
      bp->byte_num++;
      bp->bit_num = 0;
    }
  }
  return result;
}

/* VanillaTilesetUtil.DecodeTileset16 */
static void decode_tileset16(const uint8_t *comp, size_t len, struct tileset16 *out) {
  struct bitpos bp = {0, 0};
  uint16_t tile_num = 0;
  int vflip = 0, hflip = 0, altbg = 1;
  uint8_t pal = 4;
  for (int i = 0; i < 384; i++) {
    for (int j = 0; j < 4; j++) {
      int control1 = extract_bits(comp, len, 2, &bp);
      switch (control1) {
        case 0:
          break;
        case 1:
          if ((tile_num & 0xFF) == 0xFF) tile_num = (uint16_t)(tile_num - 0xFF);
          else tile_num++;
          break;
        case 2:
          if ((tile_num & 0xFF) == 0) tile_num = (uint16_t)(tile_num + 0xFF);
          else tile_num--;
          break;
        default: {
          int next_bit = extract_bits(comp, len, 1, &bp);
          if (next_bit == 1) {
            int fourth_bit = extract_bits(comp, len, 1, &bp);
            if (fourth_bit == 1) {
              vflip = extract_bits(comp, len, 1, &bp) > 0;
              hflip = extract_bits(comp, len, 1, &bp) > 0;
              altbg = extract_bits(comp, len, 1, &bp) > 0;
              pal = (uint8_t)extract_bits(comp, len, 3, &bp);
              tile_num = (uint16_t)extract_bits(comp, len, 10, &bp);
            } else {
              int flag_control_bit = extract_bits(comp, len, 1, &bp);
              if (flag_control_bit == 1) {
                int flags = extract_bits(comp, len, 3, &bp);
                vflip = (flags & 0x04) > 0;
                hflip = (flags & 0x02) > 0;
                altbg = (flags & 0x01) > 0;
              } else {
                pal = (uint8_t)extract_bits(comp, len, 3, &bp);
              }
              uint16_t six = (uint16_t)extract_bits(comp, len, 6, &bp);
              if (six & 0x20) six |= 0xFFC0;
              tile_num = (uint16_t)(tile_num + (int16_t)six);
            }
          } else {
            uint16_t seven = (uint16_t)extract_bits(comp, len, 7, &bp);
            if (seven & 0x40) seven |= 0xFF80;
            tile_num = (uint16_t)(tile_num + (int16_t)seven);
          }
          break;
        }
      }
      out->t[i][j] = tile8_new(vflip, hflip, altbg, pal, tile_num);
    }
  }
}

static void drop_bits(uint8_t *comp, int num_bits, int data, struct bitpos *bp) {
  for (int i = 0; i < num_bits; i++) {
    uint8_t mask = (uint8_t)(1 << (7 - bp->bit_num));
    uint16_t source_mask = (uint16_t)(1 << (num_bits - i - 1));
    if (((uint16_t)data & source_mask) > 0) comp[bp->byte_num] |= mask;
    else comp[bp->byte_num] &= (uint8_t)~mask;
    bp->bit_num++;
    if (bp->bit_num == 8) {
      bp->byte_num++;
      bp->bit_num = 0;
    }
  }
}

#define BUF_SIZE 10000

/* VanillaTilesetUtil.EncodeTileset16: returns the number of complete bytes (a trailing partial byte is dropped, like the C#) */
static int encode_tileset16(const struct tileset16 *ts, uint8_t *buf) {
  memset(buf, 0, BUF_SIZE);
  struct bitpos bp = {0, 0};
  int tile_num = 0, vflip = 0, hflip = 0, altbg = 1, pal = 4;
  for (int i = 0; i < 384; i++) {
    for (int j = 0; j < 4; j++) {
      const struct tile8 *st = &ts->t[i][j];
      int data = tile8_to_binary(st);
      int this_num = st->tile_num, this_v = st->vflip, this_h = st->hflip, this_alt = st->altbg, this_pal = st->pal;
      if (bp.byte_num >= BUF_SIZE - 4) return -1;   /* the C# buffer is 10000 bytes (IndexOutOfRange); never reached with 384 tiles */
      int full = 0;
      if (i + j == 0) {
        full = 1;
      } else if (this_pal == pal && this_v == vflip && this_h == hflip && this_alt == altbg) {
        if (this_num == tile_num) {
          drop_bits(buf, 2, 0, &bp);
        } else if (this_num == tile_num + 1) {
          drop_bits(buf, 2, 1, &bp);
        } else if ((this_num % 256) == ((tile_num - 1) % 256)) {
          drop_bits(buf, 2, 2, &bp);
        } else if (this_num < tile_num && tile_num - this_num <= 64) {
          drop_bits(buf, 3, 6, &bp);
          drop_bits(buf, 7, this_num - tile_num, &bp);
        } else if (this_num > tile_num && this_num - tile_num <= 63) {
          drop_bits(buf, 3, 6, &bp);
          drop_bits(buf, 7, this_num - tile_num, &bp);
        } else {
          full = 1;
        }
      } else if (this_pal == pal) {
        if ((this_num < tile_num && tile_num - this_num <= 32) || (this_num > tile_num && this_num - tile_num <= 31)) {
          drop_bits(buf, 4, 14, &bp);
          drop_bits(buf, 1, 1, &bp);
          drop_bits(buf, 1, this_v ? 1 : 0, &bp);
          drop_bits(buf, 1, this_h ? 1 : 0, &bp);
          drop_bits(buf, 1, this_alt ? 1 : 0, &bp);
          drop_bits(buf, 6, this_num - tile_num, &bp);
        } else {
          full = 1;
        }
      } else if (this_v == vflip && this_h == hflip && this_alt == altbg) {
        if ((this_num < tile_num && tile_num - this_num <= 32) || (this_num > tile_num && this_num - tile_num <= 31)) {
          drop_bits(buf, 4, 14, &bp);
          drop_bits(buf, 1, 0, &bp);
          drop_bits(buf, 3, this_pal, &bp);
          drop_bits(buf, 6, this_num - tile_num, &bp);
        } else {
          full = 1;
        }
      } else {
        full = 1;
      }
      if (full) {
        drop_bits(buf, 4, 0x0F, &bp);
        drop_bits(buf, 16, data, &bp);
      }
      tile_num = this_num;
      pal = this_pal;
      vflip = this_v;
      hflip = this_h;
      altbg = this_alt;
    }
  }
  return bp.byte_num;
}


/* Decodes vanilla tileset 15, moves palette 4 to palette 5 (tiles 192+133 and 192+149 become palette 4) and re-encodes it into buf
 * (BUF_SIZE bytes).  Returns the encoded length. */
int sr_p4_hmisc_neso_tileset(struct sr_ctx *c, uint8_t *buf) {
  const int tileset_num = 15;
  int offset = sr_ushort_from_bytes(c->orig, TILESET16_OFFSETS + tileset_num * 3) + (c->orig[TILESET16_OFFSETS + tileset_num * 3 + 2] << 16);
  int next_offset = sr_ushort_from_bytes(c->orig, TILESET16_OFFSETS + tileset_num * 3 + 3) + (c->orig[TILESET16_OFFSETS + tileset_num * 3 + 5] << 16);
  if (next_offset < offset) sr_fail(c, SOMRANDO_ERR_FAILED, "Arithmetic operation resulted in an overflow.");
  if (offset < 0 || next_offset > SR_ROM_OUT_SIZE) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was outside the bounds of the array.");
  struct tileset16 *ts = (struct tileset16 *)sr_alloc(c, sizeof *ts);
  decode_tileset16(c->orig + offset, (size_t)(next_offset - offset), ts);
  for (int i = 0; i < 384; i++) {
    for (int j = 0; j < 4; j++) {
      struct tile8 *tile = &ts->t[i][j];
      if (tile->pal == 4) tile->pal = 5;
      if (i == 192 + 133 || i == 192 + 149) tile->pal = 4;
    }
  }
  int n = encode_tileset16(ts, buf);
  if (n < 0) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was outside the bounds of the array.");
  return n;
}

int sr_p4_hmisc_neso_buf_size(void) { return BUF_SIZE; }
