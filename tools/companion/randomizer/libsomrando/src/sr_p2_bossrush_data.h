/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Boss rush mode (group w8): data tables, see sr_p2_bossrush_data.c.
 */
#ifndef SR_P2_BOSSRUSH_DATA_H
#define SR_P2_BOSSRUSH_DATA_H
#include <stdint.h>
#include "sr_p2_bossrush_int.h"

/* VanillaBossMaps.VanillaBossMap, the fields boss rush reads, in BY_VANILLA_BOSS_ID order */
struct sr_w8_boss_map {
  int id;
  const char *name;
  int original_map_num, original_obj_num;
  int entry_x, entry_y, boss_x, boss_y;
  int bg_exclusions[4];
  int n_bg_exclusions;
  int fg_exclusions[4];
  int n_fg_exclusions;
  int collide_layer2;
};
extern const struct sr_w8_boss_map sr_w8_boss_maps[W8_NUM_BOSS_ENTRIES];

extern const uint8_t sr_w8_rest_map_l1[3500], sr_w8_rest_map_l2[3500];

struct sr_w8_diff_row {
  const char *key;
  double v[3];   /* casual, hard, reallyhard */
};
extern const struct sr_w8_diff_row sr_w8_diff_rows[];
extern const int sr_w8_diff_row_count;
extern const struct sr_w8_diff_row sr_p3_ac_diff_rows[];   /* Ancient Cave presets, sr_p3_ac_diff.c */
extern const int sr_p3_ac_diff_row_count;
#endif
