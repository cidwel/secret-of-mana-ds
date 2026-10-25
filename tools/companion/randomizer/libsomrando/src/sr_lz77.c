/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Derived from SoMRandomizer/util/SomLz77.cs (Author: Moppleton).  Translated to C 2026-10-18.
 * The two Math.Pow calls of the encoder are Math.Pow(2, n) for integers n between 2 and 15: exact in IEEE doubles, done with shifts.
 */
#include "sr_core.h"

void sr_lz77_decode(struct sr_ctx *c, const uint8_t *rom, int location, struct sr_bytes *out) {
  int window_size = rom[location];
  location += 2;
  int size = (rom[location] << 8) + rom[location + 1];   /* big endian */
  location += 2;
  int pos = 0;
  while (pos < size) {
    uint8_t b = rom[location++];
    if (b & 0x80) {
      uint8_t mask1 = 0x1F;
      for (int i = 0; i < window_size; i++) mask1 >>= 1;
      uint8_t dist_msb = (uint8_t)(b & mask1);
      uint8_t num_bytes = (uint8_t)(((b & 0x7F) >> (5 - window_size)) + 3);
      uint16_t dist = (uint16_t)(rom[location++] + 1 + (dist_msb * 256));
      for (int i = 0; pos < size && i < num_bytes; i++) {
        if (pos - dist < 0 || (size_t)(pos - dist) >= out->len) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range (LZ77 seek back).");
        sr_bytes_push(c, out, out->p[pos - dist]);
        pos++;
      }
    } else {
      for (int i = 0; pos < size && i <= b; i++) {
        sr_bytes_push(c, out, rom[location++]);
        pos++;
      }
    }
  }
}

static int min_int(int a, int b) { return a < b ? a : b; }

void sr_lz77_encode(struct sr_ctx *c, const uint8_t *block, size_t length, int window_size, struct sr_bytes *out) {
  const int block_len = (int)length;
  sr_bytes_push(c, out, window_size);
  sr_bytes_push(c, out, 0);
  sr_bytes_push(c, out, block_len >> 8);   /* big endian */
  sr_bytes_push(c, out, block_len);

  int decomp_index = 0, rom_bytes = 0;
  const int max_len_comp = (1 << (window_size + 2)) + 2;
  const int max_dist_comp = 1 << (13 - window_size);

  while (decomp_index < block_len) {
    int best_match_len = 0, best_match_dist = 0;
    int max_len = min_int(max_len_comp, block_len - decomp_index);
    int max_dist = min_int(max_dist_comp, decomp_index);
    for (int dist = 1; dist <= max_dist; dist++) {
      int pos = decomp_index - dist;
      for (int i = 0; i < max_len; i++) {
        if (block[pos + i] != block[decomp_index + i]) {
          if (i > best_match_len) {
            best_match_len = i;
            best_match_dist = dist;
          }
          break;
        }
        if (i == max_len - 1) {
          best_match_len = i + 1;
          best_match_dist = dist;
        }
      }
      if (best_match_len == max_len) break;
    }

    if (best_match_len >= 3) {
      int len = best_match_len, dist = best_match_dist;
      if (rom_bytes > 0) {
        sr_bytes_push(c, out, rom_bytes - 1);
        for (int i = 0; i < rom_bytes; i++) sr_bytes_push(c, out, block[decomp_index - (rom_bytes - i)]);
      }
      rom_bytes = 0;
      uint8_t ctrl = 0x80;
      uint8_t length_field = (uint8_t)((len - 3) << (5 - window_size));
      ctrl |= length_field;
      uint8_t distance_msb = (uint8_t)((dist - 1) >> 8);
      ctrl |= distance_msb;
      uint8_t distance_lsb = (uint8_t)(dist - 1);
      sr_bytes_push(c, out, ctrl);
      sr_bytes_push(c, out, distance_lsb);
      decomp_index += len;
    } else {
      rom_bytes++;
      decomp_index++;
    }

    if (rom_bytes == 128) {
      sr_bytes_push(c, out, rom_bytes - 1);
      for (int i = 0; i < rom_bytes; i++) sr_bytes_push(c, out, block[decomp_index - (rom_bytes - i)]);
      rom_bytes = 0;
    }
  }

  if (rom_bytes > 0) {
    sr_bytes_push(c, out, rom_bytes - 1);
    for (int i = 0; i < rom_bytes; i++) sr_bytes_push(c, out, block[decomp_index - (rom_bytes - i)]);
  }
}
