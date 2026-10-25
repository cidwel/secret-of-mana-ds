/* Ancient Cave floor replay: regenerates one floor from the state the instrumented oracle dumped before it (RNG state, palette counter), so a
 * floor generator can be checked against the C# without the rest of the pipeline.  Test tool, original work of this repository (LGPL-2.1+).
 *
 *   ac_replay ROM ORACLE_DUMP FLOOR_INDEX OUT_DUMP
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p3_ac.h"
#include "sr_p3_ac_floor.h"
#include "sr_p3_quotes.h"

static int read_state(const char *line, struct sr_random *r) {
  const char *p = strchr(line, ' ');
  if (!p) return 0;
  char *end;
  r->inext = (int)strtol(p, &end, 10);
  r->inextp = (int)strtol(end, &end, 10);
  for (int i = 0; i < 56; i++) r->seed_array[i] = (int32_t)strtol(end, &end, 10);
  r->calls = 0;
  return 1;
}

int main(int argc, char **argv) {
  if (argc != 5) { fprintf(stderr, "usage: ac_replay ROM ORACLE_DUMP FLOOR_INDEX OUT_DUMP\n"); return 2; }
  int want = atoi(argv[3]);
  FILE *f = fopen(argv[2], "r");
  if (!f) { fprintf(stderr, "cannot open %s\n", argv[2]); return 2; }
  char *line = (char *)malloc(1 << 20);
  char type[64] = "";
  int found = 0, pal = -1;
  struct sr_random rf, rc;
  memset(&rf, 0, sizeof rf);
  memset(&rc, 0, sizeof rc);
  long rf_calls = 0, rc_calls = 0;
  while (fgets(line, 1 << 20, f)) {
    if (!strncmp(line, "floor ", 6)) {
      int n = atoi(line + 6);
      if (n == want) { found = 1; const char *t = strstr(line, "type="); if (t) { strncpy(type, t + 5, sizeof type - 1); type[strcspn(type, "\r\n")] = 0; } }
      else if (found) break;
    } else if (found && !strncmp(line, "floor-start", 11)) {
      sscanf(line, "floor-start rf=%ld rc=%ld", &rf_calls, &rc_calls);
    } else if (found && !strncmp(line, "state ", 6)) read_state(line, &rf);
    else if (found && !strncmp(line, "statec ", 7)) read_state(line, &rc);
    else if (found && !strncmp(line, "work pal=", 9)) pal = atoi(line + 9);
  }
  fclose(f);
  free(line);
  if (!found) { fprintf(stderr, "floor %d not in dump\n", want); return 2; }
  const struct ac_biome *b = NULL;
  for (int i = 0; i < 6; i++)
    if (sr_p3_biomes[i] && !strcmp(sr_p3_biomes[i]->floor_type, type)) b = sr_p3_biomes[i];
  if (!b) { fprintf(stderr, "no biome for '%s'\n", type); return 2; }

  FILE *rf_ = fopen(argv[1], "rb");
  if (!rf_) { fprintf(stderr, "cannot open ROM\n"); return 2; }
  uint8_t *rom = (uint8_t *)malloc(SR_ROM_IN_SIZE + 0x200);
  size_t n = fread(rom, 1, SR_ROM_IN_SIZE + 0x200, rf_);
  fclose(rf_);
  size_t hdr = n == SR_ROM_IN_SIZE + 0x200 ? 0x200 : 0;

  struct sr_ctx *c = (struct sr_ctx *)calloc(1, sizeof *c);
  setenv("SOM_AC_DUMP", argv[4], 1);
  if (setjmp(c->fatal)) {
    fprintf(stderr, "floor %d (%s) failed: %s\n", want, type, c->err);
    ac_dump_line("error %s", c->err);
    return 1;
  }
  c->settings = sr_settings_new(&c->arena, "ancientcave");
  c->orig = (uint8_t *)sr_alloc(c, SR_ROM_OUT_SIZE);
  c->out = (uint8_t *)sr_alloc(c, SR_ROM_OUT_SIZE);
  memcpy(c->orig, rom + hdr, SR_ROM_IN_SIZE);
  memcpy(c->out, c->orig, SR_ROM_IN_SIZE);
  c->rf = rf;
  c->rc = rc;
  c->rf.calls = rf_calls;
  c->rc.calls = rc_calls;
  sr_work_set_int(c, "numDialogueEvents", getenv("SOM_AC_NUMDLG") ? atoi(getenv("SOM_AC_NUMDLG")) : sr_quotes_mitch_count);
  sr_work_set_int(c, "FloorNumber", want);
  sr_work_set_int(c, "GeneratedPaletteIndex", pal);
  sr_work_set_int(c, "AnimatedPaletteOffset", 0x300000);
  struct ac_floor fl;
  sr_p3_generate_floor(c, b, &fl);
  sr_arena_free(&c->arena);
  free(c);
  free(rom);
  return 0;
}
