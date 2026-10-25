/* Host driver of som_gameplay_apply.c for gameplay_parity.py: sga_cli ROM OPTIONS.json OUT.sfc */
#include "../../som_gameplay_apply.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
  if (argc < 4) return 2;
  FILE *f = fopen(argv[1], "rb");
  if (!f) return 2;
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  uint8_t *rom = malloc((size_t)n);
  if (fread(rom, 1, (size_t)n, f) != (size_t)n) return 2;
  fclose(f);
  f = fopen(argv[2], "rb");
  if (!f) return 2;
  char js[8192]; size_t jn = fread(js, 1, sizeof js - 1, f); js[jn] = 0; fclose(f);
  struct sga_opts o; char err[256], sk[256];
  if (sga_parse(&o, js, err, sizeof err)) { fprintf(stderr, "error: %s\n", err); return 1; }
  long r = sga_apply(rom, (size_t)n, &o, err, sizeof err, sk, sizeof sk);
  if (r < 0) { fprintf(stderr, "error: %s\n", err); return 1; }
  f = fopen(argv[3], "wb"); fwrite(rom, 1, (size_t)n, f); fclose(f);
  printf("changed %ld skipped [%s]\n", r, sk);
  free(rom);
  return 0;
}
