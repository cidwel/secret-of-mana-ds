/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Preflight of the supporting event modules (OpenWorldSupportingEvents, OpenWorldSupportingMapChanges, MiscDialogue); Author of the upstream
 * files: Moppleton.  Translated to C 2026-10-18.
 */
#include <stdio.h>

#include "sr_p4.h"

int sr_p4_ev2_supports(const struct sr_settings *s, char *why, size_t why_len) {
  (void)s;
  (void)why;
  (void)why_len;
  return 1;
}
