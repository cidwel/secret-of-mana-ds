/* sgp_cli IN.sfc OUT.sfc [key=int ...]: host tool for the comparison test against tools/gameplay/som_gameplay.py. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "som_gameplay.h"

int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage: sgp_cli IN OUT [key=int ...]\n"); return 2; }
  struct sgp_opts o;
  sgp_defaults(&o);
  for (int i = 3; i < argc; i++) {
    char *eq = strchr(argv[i], '=');
    if (!eq) return 2;
    *eq = 0;
    int k = sgp_find_key(argv[i]);
    if (k < 0) { fprintf(stderr, "unknown key %s\n", argv[i]); return 2; }
    o.v[k] = atoi(eq + 1);
  }
  FILE *f = fopen(argv[1], "rb");
  if (!f) return 2;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *rom = malloc((size_t)n);
  if (fread(rom, 1, (size_t)n, f) != (size_t)n) return 2;
  fclose(f);
  struct sgp_report rep;
  int rc = sgp_apply(rom, (size_t)n, &o, &rep);
  if (rc) { fprintf(stderr, "error: %s\n", rep.why); return 1; }
  f = fopen(argv[2], "wb");
  fwrite(rom, 1, (size_t)n, f);
  fclose(f);
  free(rom);
  printf("recs=%d bytes=%d skipped=%u checksum=%d marks=%d%s%s\n", rep.nrecs, rep.nchanged_bytes, rep.skipped, rep.checksum_updated, rep.mark_use_hits, rep.why[0] ? " why=" : "", rep.why);
  return 0;
}
