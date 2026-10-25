/* Ancient Cave event maker replay: runs sr_p3_event_maker from the RNG state the instrumented oracle dumped before AncientCaveEventMaker.process
 * and prints the same lines the oracle hook writes (evrng, ev, wd, npc, evrom).  Test tool, original work of this repository (LGPL-2.1+).
 *
 *   ev_replay ORACLE_DUMP OUT_DUMP DIFFICULTY [key=value ...]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p3_ac.h"
#include "somrando_api.h"

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage: ev_replay ORACLE_DUMP OUT_DUMP DIFFICULTY [key=value ...]\n"); return 2; }
  FILE *f = fopen(argv[1], "r");
  if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
  char *line = (char *)malloc(1 << 20);
  struct sr_random rf;
  memset(&rf, 0, sizeof rf);
  int cmds[4] = {-1, -1, -1, -1}, have_state = 0;
  while (fgets(line, 1 << 20, f)) {
    if (!strncmp(line, "evstate ", 8)) {
      char *end;
      rf.inext = (int)strtol(line + 8, &end, 10);
      rf.inextp = (int)strtol(end, &end, 10);
      for (int i = 0; i < 56; i++) rf.seed_array[i] = (int32_t)strtol(end, &end, 10);
      have_state = 1;
    } else if (!strncmp(line, "evcmds ", 7)) {
      sscanf(line + 7, "%d %d %d %d", &cmds[0], &cmds[1], &cmds[2], &cmds[3]);
    }
  }
  fclose(f);
  free(line);
  if (!have_state) { fprintf(stderr, "no evstate line\n"); return 2; }

  { FILE *t = fopen(argv[2], "w"); if (t) fclose(t); }   /* ac_dump_line appends */
  setenv("SOM_AC_DUMP", argv[2], 1);
  struct sr_ctx *c = (struct sr_ctx *)calloc(1, sizeof *c);
  struct somrando_request req;
  memset(&req, 0, sizeof req);
  req.mode = "ancientcave";
  req.difficulty = argv[3];
  c->req = &req;
  if (setjmp(c->fatal)) {
    fprintf(stderr, "event maker failed: %s\n", c->err);
    ac_dump_line("error %s", c->err);
    return 1;
  }
  c->settings = sr_settings_new(&c->arena, "ancientcave");
  char err[256];
  for (int i = 4; i < argc; i++) {
    char one[256];
    snprintf(one, sizeof one, "%s\n", argv[i]);
    if (sr_settings_apply_lines(c->settings, one, err, sizeof err)) { fprintf(stderr, "bad setting %s: %s\n", argv[i], err); return 2; }
  }
  c->orig = (uint8_t *)sr_alloc(c, SR_ROM_OUT_SIZE);
  c->out = (uint8_t *)sr_alloc(c, SR_ROM_OUT_SIZE);
  c->rf = rf;
  static const char *const names[4] = {"print timer", "print 16 bit value", "print 32 bit value", "sell all"};
  for (int want = 3; want < 7; want++)
    for (int i = 0; i < 4; i++)
      if (cmds[i] == want) w8_register_event(c, names[i], 0);

  struct ac_npc_events *ne = (struct ac_npc_events *)sr_alloc(c, sizeof *ne);
  sr_p3_event_maker(c, ne);

  ac_dump_line("evrng %ld", c->rf.calls);
  for (int k = 0; k < c->n_events; k++) {
    char *hex = (char *)malloc(c->events[k].data->len * 3 + 2);
    size_t o = 0;
    for (size_t i = 0; i < c->events[k].data->len; i++) o += (size_t)sprintf(hex + o, "%s%02X", i ? "-" : "", c->events[k].data->p[i]);
    hex[o] = 0;
    ac_dump_line("ev %x %s", c->events[k].id, hex);
    free(hex);
  }
  for (int k = 0; k < c->n_work; k++) ac_dump_line("wd %s=%s", c->work[k].key, c->work[k].val);
  for (int i = 0; i < AC_MAX_FLOORS; i++) {
    if (!ne->has[i]) continue;
    char *buf = (char *)malloc(4096);
    size_t o = 0;
    buf[0] = 0;
    for (int k = 0; k < ne->floor[i].n; k++) {
      const struct ac_npc_event *e = (const struct ac_npc_event *)AC_PTR(ne->floor[i].p[k]);
      o += (size_t)snprintf(buf + o, 4096 - o, "%d,%d,%d,%d,%d,%d;", e->event_id, e->npc_id, e->flag_id, e->flag_min, e->flag_max, e->mobile);
    }
    ac_dump_line("npc %d %s", i, buf);
    free(buf);
  }
  ac_dump_line("evrom %d %d", c->out[0x19459], c->out[0x1945A]);
  sr_arena_free(&c->arena);
  free(c);
  return 0;
}
