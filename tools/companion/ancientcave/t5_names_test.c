/* T5: the UX layer's name block decode (som_ux_name) on a ROM given on the command line.
 *   t5_names_test ROM   prints names_ok-dependent samples and exits 0 when weapon, item, enemy and spell names resolve.
 * Build: gcc -std=gnu99 -O2 -I../ux -I.. -o OUT t5_names_test.c ../ux/som_ui_state.c ../ux/som_ui_draw.c ../ux/som_ui_sel.c ../ux/som_ui_zone.c ../som_companion.c ../direct/som_direct.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include "som_ui_draw.h"
#include "som_ui_priv.h"

int main(int argc, char **argv) {
  if (argc < 2) return 2;
  FILE *f = fopen(argv[1], "rb");
  if (!f) return 2;
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  uint8_t *rom = malloc((size_t)n);
  if (fread(rom, 1, (size_t)n, f) != (size_t)n) return 2;
  fclose(f);
  struct som_ux *ux = som_ux_new();
  som_ux_set_rom(ux, rom, (size_t)n);
  int ids[] = {NAME_SPELL + 0, NAME_SPELL + 12, NAME_WEAPON, NAME_WEAPON + 9, NAME_ITEM, NAME_ITEM + 6, NAME_ITEM + 7, NAME_ENEMY, NAME_ENEMY + 17, NAME_ENEMY + 105};
  int ok = ux->names_ok;
  for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
    const char *s = som_ux_name(ux, ids[i]);
    printf("name[%d]=%s\n", ids[i], s ? s : "(null)");
    if (!s) ok = 0;
  }
  printf("names_ok=%d\n", ux->names_ok);
  som_ux_delete(ux);
  return ok ? 0 : 1;
}
