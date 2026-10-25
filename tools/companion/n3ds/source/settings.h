/* Persistent settings of the 3DS frontend: video filter, state slot and the gameplay options of tools/gameplay/gameplay_menu.json.
 * File: sdmc:/som/settings.cfg, "key=value" lines (gp.<option key>=<number>), unknown lines ignored.  Pure C99; the option table
 * comes from build/gen_data.h (tools/companion/n3ds/gen_data.py). */
#ifndef SOM_N3DS_SETTINGS_H
#define SOM_N3DS_SETTINGS_H
#include <stddef.h>
#include <stdint.h>
#include "gameplay_c/som_gameplay.h"

enum n3_filter { N3_FILTER_NONE = 0, N3_FILTER_SMOOTH, N3_FILTER_SCANLINES, N3_FILTER_COUNT };

struct n3_settings {
  int filter;               /* enum n3_filter */
  int slot;                 /* save state slot 1..8 */
  struct sgp_opts gp;
};

void n3_defaults(struct n3_settings *s);
void n3_load(struct n3_settings *s, const char *path);
int n3_save(const struct n3_settings *s, const char *path);
const char *n3_filter_name(int f);

/* gameplay rows: index into GD_OPTS */
int n3_gp_count(void);
int n3_gp_sgp(int gd_index);                                   /* enum sgp_opt of the option, -1 when the C patcher does not know it */
void n3_gp_text(const struct n3_settings *s, int gd_index, char *buf, size_t n);   /* label of the current value */
void n3_gp_step(struct n3_settings *s, int gd_index, int dir, int big);            /* dir -1/+1; big: x10 for numbers */
int n3_gp_set(struct n3_settings *s, const char *key, const char *value);          /* by menu string ("fast", "3", "yes"); 0 ok */
int n3_gp_preset_count(void);
const char *n3_gp_preset_name(int i);
const char *n3_gp_preset_desc(int i);
void n3_gp_apply_preset(struct n3_settings *s, int i);
int n3_gp_preset_matches(const struct n3_settings *s, int i);
int n3_gp_changed_count(const struct n3_settings *s);          /* options that differ from vanilla */

#endif
