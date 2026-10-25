/* libsomrando: Ancient Cave floor generation shared by the five biomes (sr_p3_ac_floor.c).  LGPL-2.1 or later. */
#ifndef SR_P3_AC_FLOOR_H
#define SR_P3_AC_FLOOR_H
#include "sr_p3_ac.h"

struct ac_floor {   /* AncientCaveFloor */
  struct ac_map *outside, *inside, *boss;
  const uint8_t *song;
  int song_len;
};

/* AncientCaveMapGenerator.generate(context) for the given biome: reads workingData FloorNumber, registers the three maps and the six map
 * pieces, writes the generated palettes. */
void sr_p3_generate_floor(struct sr_ctx *c, const struct ac_biome *b, struct ac_floor *out);
#endif
