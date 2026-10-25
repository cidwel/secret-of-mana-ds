/* See somrando_menu.h.  Original work of this repository (LGPL-2.1+). */
#include <stdio.h>
#include <stdlib.h>

#include "somrando_api.h"
#include "somrando_menu.h"
#include "som_launcher.h"
#include "som_launcher_catalog.h"

int somrando_menu_supports(const struct sl_launcher *L, char *why, size_t why_len) {
  char lines[4096];
  const char *diff = NULL;
  if (sl_config_lines(L, lines, sizeof lines) < 0) {
    if (why && why_len) snprintf(why, why_len, "configuration too long");
    return 0;
  }
  struct somrando_request req = {0};
  req.mode = sl_modes[L->cfg.mode].key;
  req.seed = L->cfg.seed;
  req.difficulty = sl_difficulty(L, &diff) ? diff : NULL;
  req.settings = lines;
  return somrando_supports(&req, why, why_len);
}

int somrando_menu_preset_supported(int preset, char *why, size_t why_len) {
  struct sl_launcher *L = (struct sl_launcher *)calloc(1, sizeof *L);
  if (!L) {
    if (why && why_len) snprintf(why, why_len, "out of memory");
    return 0;
  }
  sl_init(L, 1);
  int ok = -1;
  if (sl_apply_preset(L, preset) == 0) ok = somrando_menu_supports(L, why, why_len);
  else if (why && why_len) snprintf(why, why_len, "bad preset");
  free(L);
  return ok;
}
