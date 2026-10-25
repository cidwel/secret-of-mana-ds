/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Derived from SoMRandomizer/processing/hacks/openworld/{NoEnemies,EnemyRandomizer}.cs (Author: Moppleton), both run nested from EnemySwaps
 * in Open World mode.  Translated to C 2026-10-18.
 */
#include "sr_core.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))

/* NoEnemies: every normal-enemy object is hidden with MapObject.setNeverVisible (byte 0 = 0x00, byte 1 = 0xFF) */
int sr_p4_shared_no_enemies(struct sr_ctx *c) {
  for (int map_num = 16; map_num < 500; map_num++) {
    if (map_num == 51 || map_num == 32 || map_num == 33 || map_num == 34) continue;
    int start = sr_vanilla_object_offset(c->out, map_num) + 8;
    int end = sr_vanilla_object_offset(c->out, map_num + 1);
    for (int i = start; i < end; i += 8) {
      if (sr_peek(c, i + 5) < 84) {
        P(i, 0x00);
        P(i + 1, 0xFF);
      }
    }
  }
  return 1;
}

/* EnemyRandomizer: patches the map loader so every normal enemy species is rolled with the vanilla RNG at spawn time */
int sr_p4_shared_enemy_randomizer(struct sr_ctx *c) {
  P(0xDD69, 0x22);
  P(0xDD6A, c->working_offset);
  P(0xDD6B, c->working_offset >> 8);
  P(0xDD6C, (c->working_offset >> 16) + 0xC0);
  P(0xDD6D, 0xEA);

  static const uint8_t no_randomize[] = {0x3F, 0x2E, 0x20, 0x09, 0x0F, 0x1D, 0x30};
  const int n_no = (int)sizeof no_randomize;

  E(0xB7); E(0x0D);
  E(0x9D); E(0x0C); E(0xC8);
  E(0xEB);
  E(0xE2); E(0x20);
  E(0xC9); E(0x54);
  E(0x90); E(0x03);
  E(0xC2); E(0x20);
  E(0x6B);

  for (int k = 0; k < n_no; k++) {
    E(0xC9); E(no_randomize[k]);
    E(0xD0); E(0x03);
    E(0xC2); E(0x20);
    E(0x6B);
  }

  E(0x22); E(0x72); E(0x38); E(0xC0);
  E(0x29); E(0x7F);
  E(0xC9); E(0x54);
  E(0x90); E(0x02);
  E(0x80); E(0xF4);
  int iter = 0;
  for (int k = 0; k < n_no; k++) {
    E(0xC9); E(no_randomize[k]);
    E(0xD0); E(0x02);
    E(0x80); E(0xEE - iter * 6);
    iter++;
  }
  E(0x9D); E(0x0D); E(0xC8);
  E(0xC2); E(0x20);
  E(0x6B);
  return 1;
}
