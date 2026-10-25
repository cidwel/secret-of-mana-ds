/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Open World prize locations.  Derived from processing/openworld/randomization/OpenWorldLocations.cs (Author: Moppleton).  Translated to C
 * 2026-10-18.  The location table itself is generated (tools/gen_p4_core.py -> sr_p4_ow_core_loc_body.inc). */
#include <stdlib.h>
#include <string.h>

#include "sr_p4.h"
#include "sr_p4_ow.h"
#include "sr_p4_ow_core_int.h"

#include "sr_p4_ow_core_map.inc"

#define DEPENDENCY_GIRL_SPELLS "girlCaster"
#define DEPENDENCY_SPRITE_SPELLS "spriteCaster"
#define DEPENDENCY_ELINEE_ENTRY "elinee"
#define DEPENDENCY_CUTTING_WEAPON "cuttingWeapon"
#define DEPENDENCY_MATANGO_ENTRY "matango"

/* OpenWorldLocations.populateDependencies */
static void populate_dependencies(struct sr_ctx *c, struct ow_slist *grand_palace_deps, struct ow_slist *manafort_deps) {
  int randomize_gp = sr_settings_get_bool(c->settings, "opGPEleRando");
  int girl_spells = sr_p4_core_w_bool(c, "girlMagicExists");
  int sprite_spells = sr_p4_core_w_bool(c, "spriteMagicExists");
  sr_p4_core_sl_add(c, grand_palace_deps, "whip");
  sr_p4_core_sl_add(c, manafort_deps, DEPENDENCY_CUTTING_WEAPON);
  sr_p4_core_sl_add(c, manafort_deps, "whip");
  if (randomize_gp) {
    int any_valid = 0;
    if (girl_spells || sprite_spells) {
      /* crystalOrbColorMap.Keys: the workingData entries "orbElement<n>" in insertion order */
      for (int i = 0; i < c->n_work; i++) {
        if (strncmp(c->work[i].key, "orbElement", 10)) continue;
        int map_num = atoi(c->work[i].key + 10);
        if (map_num >= SR_P4_ORBMAP_GRANDPALACE_FIRST && map_num <= SR_P4_ORBMAP_GRANDPALACE_LAST) {
          int ele = sr_p4_core_w_int(c, c->work[i].key) & 0xFF;
          const char *item;
          switch (ele) {
            case 0x81: item = "gnome spells"; break;
            case 0x82: item = "undine spells"; break;
            case 0x83: item = "salamando spells"; break;
            case 0x84: item = "lumina spells"; break;
            case 0x85: item = "sylphid spells"; break;
            case 0x86: item = "shade spells"; break;
            case 0x87: item = "luna spells"; break;
            case 0x88: item = "dryad spells"; break;
            case 0xFF: item = "no"; break;
            default: sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%d' was not present in the dictionary.", ele);
          }
          if (strcmp(item, "no")) {
            if (!sr_p4_core_sl_contains(grand_palace_deps, item)) {
              sr_p4_core_sl_add(c, grand_palace_deps, item);
              sr_p4_core_sl_add(c, manafort_deps, item);
            }
            any_valid = 1;
          }
        }
      }
    }
    if (any_valid) {
      if (girl_spells && sprite_spells) {
        sr_p4_core_sl_add(c, grand_palace_deps, DEPENDENCY_GIRL_SPELLS);
        sr_p4_core_sl_add(c, grand_palace_deps, DEPENDENCY_SPRITE_SPELLS);
        sr_p4_core_sl_add(c, manafort_deps, DEPENDENCY_GIRL_SPELLS);
        sr_p4_core_sl_add(c, manafort_deps, DEPENDENCY_SPRITE_SPELLS);
      } else if (girl_spells) {
        sr_p4_core_sl_add(c, grand_palace_deps, DEPENDENCY_GIRL_SPELLS);
        sr_p4_core_sl_add(c, manafort_deps, DEPENDENCY_GIRL_SPELLS);
      } else if (sprite_spells) {
        sr_p4_core_sl_add(c, grand_palace_deps, DEPENDENCY_SPRITE_SPELLS);
        sr_p4_core_sl_add(c, manafort_deps, DEPENDENCY_SPRITE_SPELLS);
      }
    }
  } else {
    static const char *const both[] = {"gnome spells", "undine spells", "salamando spells", "sylphid spells", "lumina spells", "shade spells", "luna spells"};
    static const char *const girl_only[] = {"salamando spells", "sylphid spells", "lumina spells"};
    static const char *const sprite_only[] = {"gnome spells", "undine spells", "salamando spells", "sylphid spells", "shade spells", "luna spells"};
    const char *const *set = NULL;
    int n = 0;
    const char *caster = NULL;
    if (girl_spells && sprite_spells) {
      set = both; n = 7;
    } else if (girl_spells) {
      set = girl_only; n = 3; caster = "girlCaster";
    } else if (sprite_spells) {
      set = sprite_only; n = 6; caster = "spriteCaster";
    }
    if (set) {
      for (int i = 0; i < n; i++) {
        sr_p4_core_sl_add(c, grand_palace_deps, set[i]);
        sr_p4_core_sl_add(c, manafort_deps, set[i]);
      }
      if (caster) {
        sr_p4_core_sl_add(c, grand_palace_deps, caster);
        sr_p4_core_sl_add(c, manafort_deps, caster);
      } else {
        sr_p4_core_sl_add(c, grand_palace_deps, "girlCaster");
        sr_p4_core_sl_add(c, grand_palace_deps, "spriteCaster");
        sr_p4_core_sl_add(c, manafort_deps, "girlCaster");
        sr_p4_core_sl_add(c, manafort_deps, "spriteCaster");
      }
    }
  }
}

/* OpenWorldLocations.getForSelectedOptions */
void sr_p4_core_locations(struct sr_ctx *c, struct ow_locs *L) {
  int restrictiveLogic = !strcmp(sr_settings_get(c->settings, "opLogic"), "restrictive");
  const char *goal = sr_p4_core_w_str(c, "openWorldGoalShortName");
  int fastManaFort = sr_p4_core_w_bool(c, "fastManaFort");
  int anySpellTriggers = sr_p4_core_w_bool(c, "anyMagicExists");
  int flammieDrumInLogic = sr_settings_get_bool(c->settings, "opFlammieDrumInLogic");
  int anyCharsAdded = !sr_p4_core_w_bool(c, "startSolo");
  struct ow_slist grandPalaceBossDependencies = {0}, manafortDependencies = {0};

#define ELEMENT(map) (!anySpellTriggers ? "no" : sr_p4_core_element_name(sr_p4_core_orb_element(c, (map))))
  const char *_earthPalaceElement = ELEMENT(SR_P4_ORBMAP_EARTHPALACE);
  const char *_firePalaceElement1 = ELEMENT(SR_P4_ORBMAP_FIREPALACE1);
  const char *_firePalaceElement2 = ELEMENT(SR_P4_ORBMAP_FIREPALACE2);
  const char *_firePalaceElement3 = ELEMENT(SR_P4_ORBMAP_FIREPALACE3);
  const char *_lunaPalaceElement = ELEMENT(SR_P4_ORBMAP_LUNAPALACE);
  const char *_matangoCaveElement = ELEMENT(SR_P4_ORBMAP_MATANGO);
  const char *_upperLandElement = "no";
  if (flammieDrumInLogic) _upperLandElement = ELEMENT(SR_P4_ORBMAP_UPPERLAND);
#undef ELEMENT
  (void)_upperLandElement;
  (void)anyCharsAdded;

  populate_dependencies(c, &grandPalaceBossDependencies, &manafortDependencies);

#include "sr_p4_ow_core_loc_body.inc"
}
