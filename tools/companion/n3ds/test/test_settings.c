/* Host test of source/settings.c against the generated option table.  Usage: test_settings <scratch dir> */
#include <stdio.h>
#include <string.h>
#include "../source/settings.h"
#include "gen_data.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(int argc, char **argv) {
  const char *d = argc > 1 ? argv[1] : ".";
  char p[300], t[64];
  struct n3_settings s, r;
  n3_defaults(&s);
  CHECK(sgp_is_default(&s.gp) && s.slot == 1 && s.filter == 0);
  /* every option in the JSON table is known to the C patcher and steps through all its values and back */
  CHECK(n3_gp_count() == SGP_COUNT);
  for (int i = 0; i < n3_gp_count(); i++) {
    int k = n3_gp_sgp(i);
    CHECK(k >= 0);
    struct n3_settings w = s;
    int cycle = 0;
    do { n3_gp_step(&w, i, 1, 0); CHECK(sgp_valid(k, w.gp.v[k])); n3_gp_text(&w, i, t, sizeof t); CHECK(t[0]); cycle++; } while (memcmp(&w, &s, sizeof w) != 0 && cycle < 400);
    CHECK(cycle < 400);
    n3_gp_step(&w, i, -1, 0);
    n3_gp_step(&w, i, 1, 0);
    CHECK(memcmp(&w, &s, sizeof w) == 0);
  }
  CHECK(n3_gp_set(&s, "weapon_recharge", "instant") == 0 && s.gp.v[SGP_WEAPON_RECHARGE] == 3);
  CHECK(n3_gp_set(&s, "walk_speed", "3") == 0 && s.gp.v[SGP_WALK_SPEED] == 3);
  CHECK(n3_gp_set(&s, "charge_speed_up_bonus", "yes") == 0 && s.gp.v[SGP_CHARGE_SPEEDUP_BONUS] == 1);
  CHECK(n3_gp_set(&s, "hero_crit_bonus_girl", "30") == 0 && s.gp.v[SGP_CRIT_GIRL] == 30);
  CHECK(n3_gp_set(&s, "hero_crit_bonus_girl", "500") != 0 && s.gp.v[SGP_CRIT_GIRL] == 30);
  CHECK(n3_gp_set(&s, "weapon_recharge", "warp") != 0 && n3_gp_set(&s, "nope", "1") != 0);
  CHECK(n3_gp_set(&s, "hero_crit_bonus_girl", "") == 0 && s.gp.v[SGP_CRIT_GIRL] == -1);
  n3_gp_text(&s, 0, t, sizeof t); CHECK(!strcmp(t, "Instant"));
  /* numbers: unset -> min .. max -> unset, big step */
  n3_defaults(&s);
  int gi = -1;
  for (int i = 0; i < n3_gp_count(); i++) if (!strcmp(GD_OPTS[i].key, "hero_crit_bonus_girl")) gi = i;
  n3_gp_step(&s, gi, 1, 0); CHECK(s.gp.v[SGP_CRIT_GIRL] == 0);
  n3_gp_step(&s, gi, 1, 1); CHECK(s.gp.v[SGP_CRIT_GIRL] == 10);
  n3_gp_step(&s, gi, -1, 0); n3_gp_step(&s, gi, -1, 0); CHECK(s.gp.v[SGP_CRIT_GIRL] == 8);
  s.gp.v[SGP_CRIT_GIRL] = 120; n3_gp_step(&s, gi, 1, 0); CHECK(s.gp.v[SGP_CRIT_GIRL] == -1);
  n3_gp_step(&s, gi, -1, 0); CHECK(s.gp.v[SGP_CRIT_GIRL] == 120);
  /* presets */
  n3_defaults(&s);
  CHECK(n3_gp_preset_count() >= 3 && n3_gp_preset_matches(&s, 0));
  int arcade = -1;
  for (int i = 0; i < n3_gp_preset_count(); i++) if (!strcmp(GD_PRESETS[i].id, "arcade")) arcade = i;
  CHECK(arcade > 0);
  n3_gp_apply_preset(&s, arcade);
  CHECK(s.gp.v[SGP_WEAPON_RECHARGE] == 3 && s.gp.v[SGP_CRIT_MULTIPLIER] == 3 && s.gp.v[SGP_CRIT_GIRL] == 30 && s.gp.v[SGP_WALK_SPEED] == 3);
  CHECK(n3_gp_preset_matches(&s, arcade) && !n3_gp_preset_matches(&s, 1) && n3_gp_changed_count(&s) == 9);
  /* persistence */
  s.filter = N3_FILTER_SCANLINES; s.slot = 5;
  snprintf(p, sizeof p, "%s/settings.cfg", d);
  CHECK(n3_save(&s, p) == 0);
  n3_load(&r, p);
  CHECK(memcmp(&r, &s, sizeof r) == 0);
  FILE *f = fopen(p, "wb");
  fputs("filter=9\nslot=0\ngp.walk_speed=7\ngp.weapon_recharge=zzz\ngp.hero_crit_bonus_girl=-1\nnonsense\ngp.crit_multiplier=4\n", f);
  fclose(f);
  n3_load(&r, p);
  CHECK(r.filter == 0 && r.slot == 1 && r.gp.v[SGP_WALK_SPEED] == 2 && r.gp.v[SGP_CRIT_MULTIPLIER] == 4 && r.gp.v[SGP_CRIT_GIRL] == -1);
  n3_load(&r, "/nonexistent/settings.cfg");
  CHECK(sgp_is_default(&r.gp));
  printf(fails ? "test_settings: %d FAILED\n" : "test_settings: all passed\n", fails);
  return fails != 0;
}
