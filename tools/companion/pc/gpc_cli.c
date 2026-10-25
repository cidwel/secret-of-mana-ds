/* gpc_cli MENU.json OPTIONS.json IN.sfc OUT.sfc: the PC frontend's gameplay path (options file -> page values -> patcher options -> patched ROM)
 * as a command, for the parity test against tools/gameplay/som_gameplay.py.  Prints "changed=<options differing from vanilla> skipped=<names>". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_gpc.h"
#include "pc_store.h"

int main(int argc, char **argv) {
  static struct pg g;
  struct sgp_opts o;
  char why[160], msg[200], skipped[200];
  size_t n;
  uint8_t *menu, *opt;
  int ch;
  if (argc < 5) { fprintf(stderr, "usage: gpc_cli MENU OPTIONS IN OUT\n"); return 2; }
  menu = pc_slurp(argv[1], &n);
  if (!menu || pg_load_menu(&g, (char *)menu) != 0) { fprintf(stderr, "bad menu file\n"); return 2; }
  free(menu);
  opt = pc_slurp(argv[2], &n);
  if (!opt || pg_load_values(&g, (char *)opt) != 0) { fprintf(stderr, "bad options file\n"); return 2; }
  free(opt);
  ch = pgc_from_pg(&g, &o, why, sizeof why);
  if (ch < 0) { fprintf(stderr, "%s\n", why); return 2; }
  if (pgc_apply_file(argv[3], argv[4], &o, msg, sizeof msg, skipped, sizeof skipped) != 0) { fprintf(stderr, "%s\n", msg); return 1; }
  printf("changed=%d skipped=%s\n", ch, skipped);
  return 0;
}
