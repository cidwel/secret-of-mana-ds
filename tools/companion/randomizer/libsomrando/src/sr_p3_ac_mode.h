/* libsomrando: Ancient Cave mode declarations used by the pipeline (sr_generate.c).  LGPL-2.1 or later. */
#ifndef SR_P3_AC_MODE_H
#define SR_P3_AC_MODE_H
#include <stddef.h>
#include "sr_core.h"

/* 1 when the request can be reproduced; otherwise writes the reason to why */
int sr_p3_preflight(const struct sr_settings *s, const char *mode, const char *difficulty, char *why, size_t why_len);

/* AncientCaveGenerator: the mode specific hacks in registration order */
extern const struct sr_hack sr_p3_ac_hacks[];
extern const int sr_p3_ac_hack_count;

int sr_p3_ac_num_floors(const struct sr_settings *s);   /* AncientCaveGenerator.LENGTH_CONVERSIONS[acLength] */
int sr_p3_is_ac(struct sr_ctx *c);

/* progress step (also honours the cancel request); the name must outlive the call (arena or literal) */
void sr_next_step(struct sr_ctx *c, const char *name);

int sr_p3_hack_ancient_cave_randomizer(struct sr_ctx *c);   /* sr_p3_ac_rand.c */
int sr_p3_hack_island_tileset(struct sr_ctx *c);            /* sr_p3_ac_tileset.c */
int sr_p3_hack_cave_tileset(struct sr_ctx *c);
int sr_p3_hack_manafort_tileset(struct sr_ctx *c);
#endif
