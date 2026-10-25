/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Preflight of the Open World character hacks (CharacterClasses, CharacterJoinChanges, InvincibleRabite, FastWhipPosts).  Translated to C 2026-10-18.
 */
#include "sr_p4.h"

int sr_p4_hchar_supports(const struct sr_settings *s, char *why, size_t why_len) {
  (void)s;
  (void)why;
  (void)why_len;
  return 1;
}
