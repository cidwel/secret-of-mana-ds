/* libsomrando: Open World preflight of the shared hacks (WeaponRandomizer, TitleScreenAddition, ElementSwaps, BossSwaps, EnemySwaps with the nested
 * NoEnemies / EnemyRandomizer, CustomMusic, ExperienceAdjust, GoldAdjust, FirePalaceDoorFix, Autosave, OrbRewardEventGenerator,
 * EnemyPositionAdjustments).  LGPL-2.1 or later.  Original work of this port.
 *
 * Every value of opWeapons, opMusic, opAutosave, opEnemies, opBosses, opStatusAilments, opGPEleRando, opFlammieDrumInLogic, oopsAllThis,
 * opExp and opGold is reproduced; what is refused is the plando setting, which ElementSwaps and BossSwaps read through the plando dictionary.
 */
#include <stdio.h>

#include "sr_p4.h"

int sr_p4_shared_supports(const struct sr_settings *s, char *why, size_t why_len) {
  if (sr_settings_get(s, "plandoSettings")[0]) {
    snprintf(why, why_len, "unsupported option: plandoSettings (boss and spell orb plando are not ported)");
    return 0;
  }
  return 1;
}
