/* The direct-control module (direct/som_direct.c) checks signatures of the ROM; the gameplay patches must not break them.
 * Usage: test_direct_ok ROM  (patches a copy in memory with every option on, then runs somd_init). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "som_gameplay.h"
#include "../direct/som_direct.h"

int main(int argc, char **argv) {
  if (argc < 2) return 2;
  FILE *f = fopen(argv[1], "rb");
  if (!f) return 2;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *rom = malloc((size_t)n);
  if (fread(rom, 1, (size_t)n, f) != (size_t)n) return 2;
  fclose(f);
  static struct somd d;
  somd_init(&d, rom, (size_t)n);
  int before = d.rom_ok;
  struct sgp_opts o;
  sgp_defaults(&o);
  o.v[SGP_WEAPON_RECHARGE] = 3; o.v[SGP_RUN_RECHARGE] = 2; o.v[SGP_CHARGE_SPEED] = 2; o.v[SGP_CHARGE_SPEEDUP_BONUS] = 1; o.v[SGP_MONSTER_FLINCH] = 2;
  o.v[SGP_HERO_FLINCH] = 1; o.v[SGP_CRIT_GIRL] = 40; o.v[SGP_CRIT_MULTIPLIER] = 4; o.v[SGP_WALK_SPEED] = 4; o.v[SGP_WEAPON_LEVEL_RATE] = 3;
  o.v[SGP_QOL_CHEST_FAST] = 1; o.v[SGP_QOL_FAST_TRANSITIONS] = 1; o.v[SGP_QOL_FAST_FADES] = 1; o.v[SGP_QOL_ITEM_LIMIT] = 7;
  o.v[SGP_QOL_LEVELUP_MP] = 1; o.v[SGP_QOL_DEAD_EXP] = 1; o.v[SGP_QOL_MAGIC_RATE] = 3; o.v[SGP_QOL_SCROLL_DEADZONE] = 2;
  struct sgp_report rep;
  int rc = sgp_apply(rom, (size_t)n, &o, &rep);
  somd_init(&d, rom, (size_t)n);
  printf("rc=%d skipped=%u rom_ok before=%d after=%d\n", rc, rep.skipped, before, d.rom_ok);
  free(rom);
  return (rc == 0 && before && d.rom_ok) ? 0 : 1;
}
