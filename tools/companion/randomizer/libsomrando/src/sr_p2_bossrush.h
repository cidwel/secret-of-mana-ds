/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Boss rush mode (group w8): declarations shared with the pipeline in sr_generate.c.
 */
#ifndef SR_P2_BOSSRUSH_H
#define SR_P2_BOSSRUSH_H
#include <stddef.h>
#include "sr_core.h"

extern const struct sr_hack sr_w8_bossrush_hacks[];
extern const int sr_w8_bossrush_hack_count;

/* 1 when the request can be reproduced; otherwise writes the reason to why. */
int sr_w8_preflight(const char *mode, const char *difficulty, char *why, size_t why_len);
/* RomGenerator.generate() post-processing after the event expander: map pieces, doors, maps, palettes, custom event commands. */
void sr_w8_post_process(struct sr_ctx *c);
/* debug: writes the current ROM to $SOMRANDO_DUMP_DIR/name.rom (sr_generate.c) */
void sr_debug_dump_rom(struct sr_ctx *c, const char *name);
#endif
