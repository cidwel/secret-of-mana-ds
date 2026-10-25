/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Chaos mode (group w9): declarations shared with the pipeline in sr_generate.c.
 */
#ifndef SR_P2_CHAOS_H
#define SR_P2_CHAOS_H
#include <stddef.h>
#include "sr_core.h"

extern const struct sr_hack sr_w9_chaos_hacks[];
extern const int sr_w9_chaos_hack_count;

/* 1 when the request can be reproduced; otherwise writes the reason to why. */
int sr_w9_preflight(const struct sr_settings *s, const char *mode, const char *difficulty, char *why, size_t why_len);
/* RomGenerator.generate() post-processing for chaos mode. */
void sr_w9_post_process(struct sr_ctx *c);
/* 1 when the C# hack returns false at once for this mode (nothing written, no random draws), so run_hack skips it */
int sr_w9_noop_in_mode(const char *upstream, const char *mode);
#endif
