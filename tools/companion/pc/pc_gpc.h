/* Gameplay tweaks of the PC frontend through the C patcher (tools/companion/gameplay_c): no Python at run time.  The page of pc_gameplay.h
 * holds the choices; this module turns them into sgp_opts and patches a copy of the ROM.  Pure C99, no SDL. */
#ifndef PC_GPC_H
#define PC_GPC_H

#include <stddef.h>
#include "../gameplay_c/som_gameplay.h"
#include "pc_gameplay.h"

/* Maps the page values to patcher options by key.  Returns the number of options that differ from vanilla, or -1 when the menu has a key
 * the patcher does not know and the player changed it (`why` names it). */
int pgc_from_pg(const struct pg *g, struct sgp_opts *o, char *why, size_t why_len);
/* Reads `in` (US v1.0, with or without a copier header), applies `o`, writes the result to `out` (temp file + rename).  0 ok; -1 with `msg`.
 * `skipped` (may be NULL) receives the names of the options dropped because their bytes conflict with other hacks, comma separated. */
int pgc_apply_file(const char *in, const char *out, const struct sgp_opts *o, char *msg, size_t msg_len, char *skipped, size_t skipped_len);

#endif
