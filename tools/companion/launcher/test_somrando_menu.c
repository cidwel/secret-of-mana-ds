/* Checks the menu glue (somrando_menu.c) against the generator: for every preset the menu's answer equals somrando_supports() on the same
 * request, and presets of unported modes are refused.  Build: see run_launcher_tests.sh (links libsomrando sources).  Prints one line per preset. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "somrando_api.h"
#include "somrando_menu.h"
#include "som_launcher.h"
#include "som_launcher_catalog.h"

int main(int argc, char **argv) {
  if (argc == 3 && !strcmp(argv[1], "--lines")) {   /* the exact request the menu would send for preset id argv[2]: mode, difficulty, then key=value lines */
    for (int i = 0; i < sl_npresets; i++) {
      if (strcmp(sl_presets[i].id, argv[2])) continue;
      struct sl_launcher *L = (struct sl_launcher *)calloc(1, sizeof *L);
      char lines[4096];
      const char *d = NULL;
      sl_init(L, 1);
      sl_apply_preset(L, i);
      sl_config_lines(L, lines, sizeof lines);
      printf("mode=%s\ndifficulty=%s\n%s", sl_modes[L->cfg.mode].key, sl_difficulty(L, &d) ? d : "", lines);
      free(L);
      return 0;
    }
    return 2;
  }
  int fails = 0, supported = 0;
  for (int i = 0; i < sl_npresets; i++) {
    char why[256] = "";
    int s = somrando_menu_preset_supported(i, why, sizeof why);
    printf("%-22s %s %s\n", sl_presets[i].id, s ? "supported" : "refused  ", s ? "" : why);
    if (s != 0 && s != 1) fails++;
    if (s && strcmp(sl_modes[sl_presets[i].mode].key, "rando") && strcmp(sl_modes[sl_presets[i].mode].key, "bossrush") &&
        strcmp(sl_modes[sl_presets[i].mode].key, "chaos") && strcmp(sl_modes[sl_presets[i].mode].key, "ancientcave") &&
        strcmp(sl_modes[sl_presets[i].mode].key, "open"))
      fails++;   /* a mode that is not ported must never be reported as supported */
    supported += s == 1;
  }
  if (somrando_menu_preset_supported(-1, NULL, 0) != -1 || somrando_menu_preset_supported(sl_npresets, NULL, 0) != -1) fails++;
  struct sl_launcher L;
  sl_init(&L, 7);
  sl_apply_preset(&L, 0);
  char why[128];
  int a = somrando_menu_supports(&L, why, sizeof why);
  int b = somrando_menu_preset_supported(0, why, sizeof why);
  if (a != b) fails++;
  printf("%d of %d presets supported, %d failures\n", supported, sl_npresets, fails);
  return fails != 0;
}
