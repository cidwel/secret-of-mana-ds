/* Open World injector/hints replay: loads the state the instrumented oracle dumped right before OpenWorldResultInjector.injectRandomization
 * (placements, simulation cycles, replacement events, workingData, functional RNG state, ROM) and compares what sr_p4_ow_inject +
 * sr_p4_ow_add_hints produce with what the oracle produced afterwards.  Test tool, original work of this repository (LGPL-2.1+).
 *
 *   p4_inject_replay DUMP [-v]      (DUMP.rom0/.rom1/.rom2 are the ROM before, after the injector and after the hints)
 * Exit status 0 when everything is identical.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p4_ow.h"
#include "somrando_api.h"

static uint8_t *read_file(const char *path, size_t *n) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *b = (uint8_t *)malloc((size_t)len);
  if (fread(b, 1, (size_t)len, f) != (size_t)len) { fclose(f); free(b); return NULL; }
  fclose(f);
  *n = (size_t)len;
  return b;
}

static char *strdup_ctx(struct sr_ctx *c, const char *s) {
  size_t n = strlen(s) + 1;
  char *p = (char *)sr_alloc(c, n);
  memcpy(p, s, n);
  return p;
}

/* splits s in place on ch; returns the number of fields (at most max) */
static int split(char *s, char ch, char **out, int max) {
  int n = 0;
  out[n++] = s;
  for (; *s && n < max; s++)
    if (*s == ch) { *s = 0; out[n++] = s + 1; }
  return n;
}

static int hexbytes(struct sr_ctx *c, const char *hex, uint8_t **out) {
  int n = 0;
  size_t cap = strlen(hex) / 2 + 1;
  uint8_t *b = (uint8_t *)sr_alloc(c, cap);
  while (*hex) {
    b[n++] = (uint8_t)strtol(hex, (char **)&hex, 16);
    if (*hex == '-') hex++;
  }
  *out = b;
  return n;
}

static void hexstr(const struct sr_bytes *e, char *out) {
  size_t o = 0;
  for (size_t i = 0; i < e->len; i++) o += (size_t)sprintf(out + o, "%s%02X", i ? "-" : "", e->p[i]);
  out[o] = 0;
}

struct expect_ev { int id; char *hex; };

static int check_events(struct sr_ctx *c, const char *tag, struct expect_ev *ex, int nex) {
  int bad = 0;
  if (c->n_events != nex) { printf("%s: event count C=%d oracle=%d\n", tag, c->n_events, nex); bad = 1; }
  char *buf = (char *)malloc(1 << 20);
  for (int i = 0; i < c->n_events && i < nex; i++) {
    hexstr(c->events[i].data, buf);
    if (c->events[i].id != ex[i].id || strcmp(buf, ex[i].hex)) {
      printf("%s: event #%d id C=%x oracle=%x differ\n  C:      %.200s\n  oracle: %.200s\n", tag, i, c->events[i].id, ex[i].id, buf, ex[i].hex);
      bad = 1;
      break;
    }
  }
  free(buf);
  return bad;
}

static int check_rom(const char *tag, const uint8_t *got, const char *path) {
  size_t n;
  uint8_t *want = read_file(path, &n);
  if (!want) { printf("%s: cannot read %s\n", tag, path); return 1; }
  int bad = 0;
  if (n != SR_ROM_OUT_SIZE) { printf("%s: oracle ROM size %zu\n", tag, n); bad = 1; }
  for (size_t i = 0; i < n && i < SR_ROM_OUT_SIZE && !bad; i++)
    if (got[i] != want[i]) { printf("%s: ROM differs at 0x%zX: C=%02X oracle=%02X\n", tag, i, got[i], want[i]); bad = 1; }
  free(want);
  return bad;
}

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: p4_inject_replay DUMP [-v]\n"); return 2; }
  const char *dump = argv[1];
  FILE *f = fopen(dump, "r");
  if (!f) { fprintf(stderr, "cannot open %s\n", dump); return 2; }

  struct sr_ctx *c = (struct sr_ctx *)calloc(1, sizeof *c);
  struct somrando_request req;
  memset(&req, 0, sizeof req);
  req.mode = "open";
  c->req = &req;
  int fatal = 0;
  if (setjmp(c->fatal)) {
    fatal = 1;
    printf("C failed: %s\n", c->err);
    return 1;
  }
  (void)fatal;
  c->settings = sr_settings_new(&c->arena, "open");
  c->orig = (uint8_t *)sr_alloc(c, SR_ROM_OUT_SIZE);
  c->out = (uint8_t *)sr_alloc(c, SR_ROM_OUT_SIZE);

  struct ow_placement *pv = (struct ow_placement *)sr_alloc(c, 512 * sizeof *pv);
  struct ow_placements pl = {pv, 0};
  struct ow_sim_result sim;
  memset(&sim, 0, sizeof sim);
  sim.cycle_len = (int *)sr_alloc(c, 256 * sizeof(int));
  sim.cycles = (const char ***)sr_alloc(c, 256 * sizeof(char **));
  struct expect_ev *mid = (struct expect_ev *)calloc(2048, sizeof *mid), *aft = (struct expect_ev *)calloc(2048, sizeof *aft);
  int nmid = 0, naft = 0, nbev = 0;
  struct expect_ev *bef = (struct expect_ev *)calloc(2048, sizeof *bef);
  char *arng = NULL;
  char **awd = (char **)calloc(4096, sizeof *awd);
  int nawd = 0;
  char *line = (char *)malloc(1 << 20);
  while (fgets(line, 1 << 20, f)) {
    size_t L = strlen(line);
    while (L && (line[L - 1] == '\n' || line[L - 1] == '\r')) line[--L] = 0;
    if (!strncmp(line, "set ", 4)) {
      char one[300], err[256];
      snprintf(one, sizeof one, "%s\n", line + 4);
      if (sr_settings_apply_lines(c->settings, one, err, sizeof err)) { fprintf(stderr, "bad setting %s: %s\n", line, err); return 2; }
    } else if (!strncmp(line, "pl\t", 3)) {
      char *fld[16];
      int nf = split(line + 3, '\t', fld, 16);
      if (nf < 13) { fprintf(stderr, "bad pl line\n"); return 2; }
      struct ow_location *loc = (struct ow_location *)sr_alloc(c, sizeof *loc);
      struct ow_prize *z = (struct ow_prize *)sr_alloc(c, sizeof *z);
      loc->name = strdup_ctx(c, fld[0]);
      loc->map_num = atoi(fld[1]);
      loc->obj_num = atoi(fld[2]);
      loc->event_num = atoi(fld[3]);
      loc->event_replacement_index = atoi(fld[4]);
      for (int pass = 0; pass < 2; pass++) {
        char *lst = strdup_ctx(c, fld[5 + pass]);
        const char **items = (const char **)sr_alloc(c, 32 * sizeof *items);
        int n = 0;
        if (*lst) {
          char *fs[32];
          n = split(lst, '\x1f', fs, 32);
          for (int i = 0; i < n; i++) items[i] = fs[i];
        }
        if (pass == 0) { loc->location_hints = items; loc->n_location_hints = n; }
        else { loc->locked_by_prizes = items; loc->n_locked_by_prizes = n; }
      }
      z->name = strdup_ctx(c, fld[7]);
      z->type = strdup_ctx(c, fld[8]);
      z->hint_name = strdup_ctx(c, fld[9]);
      z->got_item_event_flag = atoi(fld[10]);
      z->value = atof(fld[11]);
      z->event_len = hexbytes(c, fld[12], &z->event_data);
      pv[pl.n].loc = loc;
      pv[pl.n].prize = z;
      pl.n++;
    } else if (!strncmp(line, "cyc\t", 4)) {
      char *lst = strdup_ctx(c, line + 4);
      const char **items = (const char **)sr_alloc(c, 512 * sizeof *items);
      int n = 0;
      if (*lst) {
        char *fs[512];
        n = split(lst, '\x1f', fs, 512);
        for (int i = 0; i < n; i++) items[i] = fs[i];
      }
      sim.cycles[sim.n_cycles] = items;
      sim.cycle_len[sim.n_cycles] = n;
      sim.n_cycles++;
    } else if (!strncmp(line, "brng ", 5)) {
      char *end;
      c->rf.inext = (int)strtol(line + 5, &end, 10);
      c->rf.inextp = (int)strtol(end, &end, 10);
      for (int i = 0; i < 56; i++) c->rf.seed_array[i] = (int32_t)strtol(end, &end, 10);
    } else if (!strncmp(line, "bev ", 4)) {
      char *sp = strchr(line + 4, ' ');
      *sp = 0;
      int id = (int)strtol(line + 4, NULL, 16);
      uint8_t *b;
      int n = hexbytes(c, sp + 1, &b);
      struct sr_bytes *e = sr_event_new(c, id);
      sr_bytes_push_n(c, e, b, (size_t)n);
      nbev++;
    } else if (!strncmp(line, "bwd ", 4)) {
      char *eq = strchr(line + 4, '=');
      *eq = 0;
      sr_work_set(c, line + 4, eq + 1);
    } else if (!strncmp(line, "mev ", 4) || !strncmp(line, "aev ", 4)) {
      struct expect_ev *dst = line[0] == 'm' ? &mid[nmid++] : &aft[naft++];
      char *sp = strchr(line + 4, ' ');
      *sp = 0;
      dst->id = (int)strtol(line + 4, NULL, 16);
      dst->hex = strdup(sp + 1);
    } else if (!strncmp(line, "arng ", 5)) {
      arng = strdup(line + 5);
    } else if (!strncmp(line, "awd ", 4)) {
      awd[nawd++] = strdup(line + 4);
    }
  }
  fclose(f);
  free(line);

  size_t n;
  char path[1024];
  snprintf(path, sizeof path, "%s.rom0", dump);
  uint8_t *rom0 = read_file(path, &n);
  if (!rom0 || n != SR_ROM_OUT_SIZE) { fprintf(stderr, "bad %s\n", path); return 2; }
  memcpy(c->out, rom0, n);
  memcpy(c->orig, rom0, n);

  sr_p4_ow_inject(c, &pl);
  int bad = check_events(c, "inject events", mid, nmid);
  snprintf(path, sizeof path, "%s.rom1", dump);
  bad |= check_rom("inject rom", c->out, path);

  sr_p4_ow_add_hints(c, &pl, &sim);
  sr_p4_ow_log_spoilers(c, &pl, &sim);
  bad |= check_events(c, "hint events", aft, naft);
  snprintf(path, sizeof path, "%s.rom2", dump);
  bad |= check_rom("hints rom", c->out, path);

  /* functional RNG state */
  {
    char got[1024];
    size_t o = (size_t)snprintf(got, sizeof got, "%d %d", c->rf.inext, c->rf.inextp);
    for (int i = 0; i < 56; i++) o += (size_t)snprintf(got + o, sizeof got - o, " %d", c->rf.seed_array[i]);
    if (!arng || strcmp(got, arng)) { printf("functional RNG state differs\n"); bad = 1; }
  }
  /* workingData: every oracle entry must hold the same value in C, and C must not hold extra keys */
  for (int i = 0; i < nawd; i++) {
    char *eq = strchr(awd[i], '=');
    *eq = 0;
    const char *v = sr_work_get(c, awd[i]);
    if (!v || strcmp(v, eq + 1)) { printf("workingData %s: C=%s oracle=%s\n", awd[i], v ? v : "(missing)", eq + 1); bad = 1; }
  }
  if (c->n_work != nawd) { printf("workingData count C=%d oracle=%d\n", c->n_work, nawd); bad = 1; }
  printf(bad ? "FAIL %s\n" : "OK %s\n", dump);
  return bad;
}
