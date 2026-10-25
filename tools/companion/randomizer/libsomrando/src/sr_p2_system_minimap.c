/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ff6StyleMiniMap (processing/hacks/common/enhancement/Ff6StyleMiniMap.cs): mini-map on the sprite layer when flying on flammie.
 * Author: Moppleton.  Translated to C 2026-10-18.
 */
#include <string.h>

#include "sr_core.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))

/* 16 pointer sprites, row-major 8x8: n = transparent, m = 1, y = 14 */
static const char *const sr_w4_pointer_tiles[16] = {
  "nnnmymnnnnnmymnnnnnmymnnnnmyyymnnnmyyymnnnmyyymnnnnmmmnnnnnnnnnn",
  "nnynnnnnnmymnnnnnnmymmnnnnmyyymnnnmyyymnnnmyyymnnnnmmmnnnnnnnnnn",
  "nmnnnnnnmymnnnnnnmymmmnnnnmyyymnnnmyyymnnnmyyymnnnnmmmnnnnnnnnnn",
  "nnnnnnnnnmnnnnnnyymmmmnnnmyyyymnnnmyyymnnnmyyymnnnnmmmnnnnnnnnnn",
  "nnnnnnnnnnnnnnnnnnnmmmnnmmmyyymnyyyyyymnmmmyyymnnnnmmmnnnnnnnnnn",
  "nnnnnnnnnnnmmmnnnnmyyymnnnmyyymnnmyyyymnyymmmmnnnmnnnnnnnnnnnnnn",
  "nnnnnnnnnnnmmmnnnnmyyymnnnmyyymnnnmyyymnnmymmmnnmymnnnnnnmnnnnnn",
  "nnnnnnnnnnnmmmnnnnmyyymnnnmyyymnnnmyyymnnnmymmnnnmymnnnnnnynnnnn",
  "nnnnnnnnnnnmmmnnnnmyyymnnnmyyymnnnmyyymnnnnmymnnnnnmymnnnnnmymnn",
  "nnnnnnnnnnmmmnnnnmyyymnnnmyyymnnnmyyymnnnnmmymnnnnnnmymnnnnnnynn",
  "nnnnnnnnnnmmmnnnnmyyymnnnmyyymnnnmyyymnnnnmmmymnnnnnnmymnnnnnnmn",
  "nnnnnnnnnnmmmnnnnmyyymnnnmyyymnnnmyyyymnnnmmmmyynnnnnnmnnnnnnnnn",
  "nnnnnnnnnnnnnnnnnnmmmnnnnmyyymmmnmyyyyyynmyyymmmnnmmmnnnnnnnnnnn",
  "nnnnnnnnnnnnnnmnnnmmmmyynmyyyymnnmyyymnnnmyyymnnnnmmmnnnnnnnnnnn",
  "nnnnnnmnnnnnnmymnnmmmymnnmyyymnnnmyyymnnnmyyymnnnnmmmnnnnnnnnnnn",
  "nnnnnynnnnnnmymnnnmmymnnnmyyymnnnmyyymnnnmyyymnnnnmmmnnnnnnnnnnn",
};

/* SnesTile.WriteTile4bpp into the output ROM */
static void sr_w4_write_tile_4bpp(struct sr_ctx *c, const uint8_t *data, int address) {
  for (int b = 0; b < 32; b++) {
    int enc = 0;
    for (int bit = 0; bit < 8; bit++) {
      int y_loc = (b / 2) % 8;
      int bit_num = (b / 16) * 2 + (b % 2);
      if (data[y_loc * 8 + bit] & (1 << bit_num)) enc |= 1 << (7 - bit);
    }
    P(address + b, enc);
  }
}

static const uint8_t sr_w4_mountain[] = {0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 28, 30, 31, 32, 33, 34, 35, 36, 37, 44, 45, 46, 47,
                                         49, 50, 63, 66, 78, 79, 54, 55, 56, 70, 71, 72, 86, 87, 88, 141};
static const uint8_t sr_w4_desert[] = {6, 7, 8, 22, 24, 38, 39, 40, 89, 90, 106, 107, 108, 111, 122, 123, 124};
static const uint8_t sr_w4_desert_edge[] = {144, 145, 146, 147, 148, 149, 150, 151};
static const uint8_t sr_w4_forest[] = {9, 10, 11, 25, 26, 27, 41, 42, 43, 51, 52, 53, 67, 68, 69, 83, 84, 85};
static const uint8_t sr_w4_grass[] = {23, 29, 60, 61, 62, 104, 105, 120, 121, 127, 136, 137, 138, 139, 140, 142, 143, 179, 180, 181, 182, 183, 185};
static const uint8_t sr_w4_snow[] = {48, 64, 65, 80, 81, 96, 97, 98, 109, 110, 112, 113, 114, 125, 126, 128, 129, 130, 175, 186, 187};
static const uint8_t sr_w4_purple[] = {160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174};

#define SR_W4_FILL(arr, col) \
  for (size_t k_ = 0; k_ < sizeof arr; k_++) color[arr[k_]] = (col)

int sr_hack_minimap(struct sr_ctx *c) {
  if (!sr_settings_get_bool(c->settings, "showMinimap")) return 0;

  sr_ensure_space_in_bank(&c->working_offset, 3000);

  uint8_t *map = (uint8_t *)sr_alloc(c, 512 * 512);
  int first_bank = 0x68000 & 0xFF0000;
  int bank = first_bank, last_pos = 0;
  for (int i = 0; i < 512; i++) {
    int pos16 = sr_ushort_from_bytes(c->out, 0x68000 + i * 2);
    if (pos16 < last_pos && bank == first_bank) bank += 0x10000;
    last_pos = pos16;
    int rom_pos = pos16 + bank, row = 0;
    while (row < 512) {
      int b = sr_peek(c, rom_pos++);
      if (b >= 0xC0) {
        int num = b - 0xBF;
        int b2 = sr_peek(c, rom_pos++);
        for (int j = 0; j < num && row < 512; j++) map[i * 512 + row++] = (uint8_t)b2;
      } else {
        map[i * 512 + row++] = (uint8_t)b;
      }
    }
  }

  uint8_t color[256];
  memset(color, 0, sizeof color);
  SR_W4_FILL(sr_w4_mountain, 1);
  SR_W4_FILL(sr_w4_desert, 5);
  SR_W4_FILL(sr_w4_desert_edge, 7);
  SR_W4_FILL(sr_w4_forest, 10);
  SR_W4_FILL(sr_w4_grass, 9);
  SR_W4_FILL(sr_w4_snow, 2);
  SR_W4_FILL(sr_w4_purple, 4);

  uint8_t all_tiles[64][64];
  for (int ty = 0; ty < 8; ty++)
    for (int tx = 0; tx < 8; tx++) {
      uint8_t *tile = all_tiles[ty * 8 + tx];
      for (int py = 0; py < 8; py++)
        for (int px = 0; px < 8; px++) {
          int ys = ty * 64 + py * 8, xs = tx * 64 + px * 8;
          int nums[16] = {0};
          for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) nums[color[map[(ys + y) * 512 + xs + x]]]++;
          int max_index = 0, max_value = 0;
          for (int i = 1; i < 16; i++)
            if (nums[i] > max_value) { max_index = i; max_value = nums[i]; }
          tile[py * 8 + px] = (uint8_t)max_index;
        }
    }

  int tile_data_start = c->working_offset + 0xC00000;
  static const int vram_order[64] = {0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29, 30, 31,
                                     32, 33, 34, 35, 36, 37, 38, 39, 48, 49, 50, 51, 52, 53, 54, 55, 40, 41, 42, 43, 44, 45, 46, 47, 56, 57, 58, 59, 60, 61, 62, 63};
  for (int t = 0; t < 64; t++) {
    sr_w4_write_tile_4bpp(c, all_tiles[vram_order[t]], c->working_offset);
    c->working_offset += 32;
  }
  for (int p = 0; p < 16; p++) {
    uint8_t d[64];
    for (int k = 0; k < 64; k++) {
      char ch = sr_w4_pointer_tiles[p][k];
      d[k] = (uint8_t)(ch == 'y' ? 14 : ch == 'm' ? 1 : 0);
    }
    sr_w4_write_tile_4bpp(c, d, c->working_offset);
    c->working_offset += 32;
  }

  P(0x1F8D8, 0x22);
  P(0x1F8D9, c->working_offset);
  P(0x1F8DA, c->working_offset >> 8);
  P(0x1F8DB, (c->working_offset >> 16) + 0xC0);
  P(0x1F8DC, 0xEA);

  static const uint8_t dma_a[] = {0xA9, 0x80, 0x8D, 0x0B, 0x42, 0xA9, 0x01, 0x8D, 0x70, 0x43, 0xA9, 0x18, 0x8D, 0x71, 0x43};
  for (size_t k = 0; k < sizeof dma_a; k++) E(dma_a[k]);
  E(0xA9); E(tile_data_start); E(0x8D); E(0x72); E(0x43);
  E(0xA9); E(tile_data_start >> 8); E(0x8D); E(0x73); E(0x43);
  E(0xA9); E(tile_data_start >> 16); E(0x8D); E(0x74); E(0x43);
  E(0xA9); E(0x00); E(0x8D); E(0x75); E(0x43);
  E(0xA9); E(0x0A); E(0x8D); E(0x76); E(0x43);
  E(0xA9); E(0x80); E(0x8D); E(0x0B); E(0x42);
  E(0x6B);

  int frame_subr = c->working_offset;
  int map_x = 0xB0, map_y = 0x90, ram = 0x0970;
  for (int i = 0; i < 16; i++) {
    int this_x = map_x + (i % 4) * 16, this_y = map_y + (i / 4) * 16;
    E(0xA9); E(this_x); E(this_y);
    E(0x8D); E(ram); E(ram >> 8);
    ram += 2;
    int tile_index = i * 2;
    if (i >= 8) tile_index += 0x10;
    int ctrl = 0x80 + tile_index + 0x2000;
    E(0xA9); E(ctrl); E(ctrl >> 8);
    E(0x8D); E(ram); E(ram >> 8);
    ram += 2;
  }

  ram = 0x0960;
  E(0xA9); E(0xAA); E(0xAA);
  E(0x8D); E(0x17); E(0x0A);
  E(0x8D); E(0x19); E(0x0A);
  E(0xA5); E(0xFA);
  E(0x29); E(0xFF); E(0x0F);
  for (int k = 0; k < 6; k++) E(0x4A);
  E(0x18);
  int mx = map_x - 3;
  E(0x69); E(mx); E(mx >> 8);
  E(0x48);
  E(0xA5); E(0xFC);
  E(0x29); E(0xFF); E(0x0F);
  E(0x0A); E(0x0A);
  E(0x29); E(0x00); E(0xFF);
  E(0x18);
  int my = map_y - 3;
  E(0x69); E(0x00); E(my);
  E(0x63); E(0x01);
  E(0x8D); E(ram); E(ram >> 8);
  E(0x68);
  ram += 2;
  E(0xA5); E(0xF6);
  E(0x18);
  E(0x69); E(0x20); E(0x00);
  E(0x29); E(0xFF); E(0x03);
  for (int k = 0; k < 6; k++) E(0x4A);
  E(0x18);
  E(0x69); E(0xC0); E(0x30);
  E(0x8D); E(ram); E(ram >> 8);
  E(0x6B);

  P(0x9BF7, 0x22);
  P(0x9BF8, c->working_offset);
  P(0x9BF9, c->working_offset >> 8);
  P(0x9BFA, (c->working_offset >> 16) + 0xC0);
  P(0x9BFB, 0xEA);
  P(0x9BFC, 0xEA);

  E(0x22); E(frame_subr); E(frame_subr >> 8); E((frame_subr >> 16) + 0xC0);
  E(0xAE); E(0x45); E(0x0A);
  E(0xBD); E(0x1B); E(0x9C);
  E(0x6B);

  P(0x9FA8, 0x22);
  P(0x9FA9, c->working_offset);
  P(0x9FAA, c->working_offset >> 8);
  P(0x9FAB, (c->working_offset >> 16) + 0xC0);
  P(0x9FAC, 0xEA);

  E(0x22); E(frame_subr); E(frame_subr >> 8); E((frame_subr >> 16) + 0xC0);
  E(0xA9); E(0xF0); E(0x00);
  E(0xA6); E(0x5F);
  E(0x6B);
  return 1;
}
