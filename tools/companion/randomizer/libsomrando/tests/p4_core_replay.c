/* Open World search replay: runs OpenWorldRandomizer (sr_p4_hack_open_randomizer) from the state the instrumented oracle dumped at its entry
 * (workingData, functional random state), so the search can be checked without the rest of the pipeline.  The C side writes the same dump
 * format as the oracle (SOM_CORE_DUMP).  Test tool, original work of this repository (LGPL-2.1+).
 *
 *   p4_core_replay ORACLE_DUMP OUT_DUMP [key=value]...      (the key=value pairs are the --set options of the oracle run)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p4.h"

int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage: p4_core_replay ORACLE_DUMP OUT_DUMP [k=v]...\n"); return 2; }
  FILE *f = fopen(argv[1], "r");
  if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
  setenv("SOM_CORE_DUMP", argv[2], 1);
  {
    FILE *t = fopen(argv[2], "w");
    if (t) fclose(t);
  }
  struct sr_ctx *c = (struct sr_ctx *)calloc(1, sizeof *c);
  if (setjmp(c->fatal)) {
    int ok = strstr(c->err, "not ported yet") != NULL;
    if (!ok) fprintf(stderr, "failed: %s\n", c->err);
    sr_arena_free(&c->arena);
    free(c);
    return ok ? 0 : 1;
  }
  c->settings = sr_settings_new(&c->arena, "open");
  char lines[4096] = "";
  for (int i = 3; i < argc; i++) {
    strncat(lines, argv[i], sizeof lines - strlen(lines) - 2);
    strcat(lines, "\n");
  }
  char err[256];
  if (sr_settings_apply_lines(c->settings, lines, err, sizeof err)) { fprintf(stderr, "settings: %s\n", err); return 2; }
  c->seed = "replay";
  static char line[1 << 20];
  while (fgets(line, 1 << 20, f)) {
    line[strcspn(line, "\r\n")] = 0;
    if (!strncmp(line, "work ", 5)) {
      char *eq = strchr(line + 5, '=');
      if (!eq) continue;
      *eq = 0;
      sr_work_set(c, line + 5, eq + 1);
    } else if (!strncmp(line, "rf-start ", 9)) {
      char *end;
      const char *p = line + 8;
      c->rf.inext = (int)strtol(p, &end, 10);
      c->rf.inextp = (int)strtol(end, &end, 10);
      for (int i = 0; i < 56; i++) c->rf.seed_array[i] = (int32_t)strtol(end, &end, 10);
    }
  }
  fclose(f);
  sr_p4_hack_open_randomizer(c);
  sr_arena_free(&c->arena);
  free(c);
  return 0;
}
