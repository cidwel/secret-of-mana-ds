/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Implements somrando_api.h.  The pipeline follows RomGenerator.initGeneration() and RomGenerator.generate() (Author: Moppleton):
 * header check, copy to the 4 MiB output, two random streams from the hashed seed, the common hacks, the mode hacks, then the
 * post-processing passes, then the file is written.  Logging is replaced by the progress callback and, for tests, SOMRANDO_TRACE.
 * Translated to C 2026-10-18.
 */
#include <stdlib.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p2_bossrush.h"
#include "sr_p2_chaos.h"
#include "sr_p3_ac_mode.h"
#include "sr_p4.h"

/* Debug aids, compiled only with -DSOMRANDO_DEBUG (test builds): SOMRANDO_SKIP=ClassA,ClassB skips those upstream hacks (the output then differs
 * from the C# generator on purpose); SOMRANDO_DUMP_DIR=dir writes the ROM after every step.  Release builds ignore both variables. */
#ifdef SOMRANDO_DEBUG
#define DBG_GETENV(n) getenv(n)
#else
#define DBG_GETENV(n) ((const char *)0)
#endif
static int debug_skipped(const char *upstream) {
  const char *e = DBG_GETENV("SOMRANDO_SKIP");
  size_t n = strlen(upstream);
  while (e && *e) {
    const char *q = strchr(e, ',');
    size_t len = q ? (size_t)(q - e) : strlen(e);
    if (len == n && !strncmp(e, upstream, n)) return 1;
    if (!q) break;
    e = q + 1;
  }
  return 0;
}

void sr_debug_dump_rom(struct sr_ctx *c, const char *name) {
  const char *dir = DBG_GETENV("SOMRANDO_DUMP_DIR");
  if (!dir || !dir[0]) return;
  char path[600];
  snprintf(path, sizeof path, "%s/%s.rom", dir, name);
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fwrite(c->out, 1, SR_ROM_OUT_SIZE, f);
  fclose(f);
}

static void debug_dump(struct sr_ctx *c, const char *prefix, const char *upstream) {
  char name[200];
  snprintf(name, sizeof name, "%s%d_%s", prefix, c->debug_dump_index++, upstream);
  sr_debug_dump_rom(c, name);
}

/* ------------------------------------------------------------------------------------------------ requirement checking */
static int value_in_list(const char *list, const char *v) {
  const char *p = list;
  size_t n = strlen(v);
  while (*p) {
    const char *q = strchr(p, '|');
    size_t len = q ? (size_t)(q - p) : strlen(p);
    if (len == n && !strncmp(p, v, n)) return 1;
    if (!q) break;
    p = q + 1;
  }
  return 0;
}

/* Returns 1 when every requirement of every hack holds; otherwise writes a message naming the first unmet options. */
static int check_requirements(const struct sr_settings *s, char *why, size_t why_len) {
  int missing = 0;
  const char *seen[96];
  int n_seen = 0;
  if (why_len) why[0] = 0;
  for (int pass = 0; pass < 2; pass++) {
    const struct sr_hack *list = pass ? sr_rando_hacks : sr_common_hacks;
    int count = pass ? sr_rando_hack_count : sr_common_hack_count;
    if (pass && strcmp(sr_settings_get(s, "mode"), "rando")) count = 0;
    for (int i = 0; i < count; i++) {
      if (debug_skipped(list[i].upstream)) continue;
      const char *const *rq = list[i].requires;
      for (; rq && rq[0]; rq += 2) {
        const char *cur = sr_settings_get(s, rq[0]);
        if (value_in_list(rq[1], cur)) continue;
        int dup = 0;
        for (int k = 0; k < n_seen; k++)
          if (!strcmp(seen[k], rq[0])) dup = 1;
        if (dup) continue;
        if (n_seen < 96) seen[n_seen++] = rq[0];
        if (missing < 3 && why_len) {
          size_t used = strlen(why);
          snprintf(why + used, why_len - used, "%s%s=%s is not reproduced yet (supported: %s; needed by '%s')",
                   missing ? "; " : "unsupported option: ", rq[0], cur, rq[1], list[i].name);
        }
        missing++;
      }
    }
  }
  if (missing > 3 && why_len) {
    size_t u = strlen(why);
    if (u + 24 < why_len) snprintf(why + u, why_len - u, "; and %d more", missing - 3);
  }
  return missing == 0;
}

/* ------------------------------------------------------------------------------------------------------------- helpers */
static int read_file(const char *path, uint8_t **buf, size_t *len) {
  FILE *f = fopen(path, "rb");
  if (!f) return 0;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n < 0) { fclose(f); return 0; }
  *buf = (uint8_t *)malloc((size_t)n + 1);
  if (!*buf) { fclose(f); return 0; }
  if (fread(*buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(*buf); *buf = NULL; return 0; }
  fclose(f);
  *len = (size_t)n;
  return 1;
}

static void set_err(char *err, size_t err_len, const char *msg) {
  if (err && err_len) snprintf(err, err_len, "%s", msg);
}

static int check_request_shape(const struct somrando_request *req, char *err, size_t err_len) {
  if (!req || !req->mode || !req->seed) {
    set_err(err, err_len, "missing mode or seed");
    return SOMRANDO_ERR_FAILED;
  }
  return 0;
}

/* Builds the settings of a request.  Returns 0, or a SOMRANDO_* code with err set. */
static int build_settings(struct sr_arena *a, const struct somrando_request *req, struct sr_settings **out, char *err, size_t err_len) {
  if (!sr_settings_known_mode(req->mode)) {
    if (!strcmp(req->mode, "ancientcave") || !strcmp(req->mode, "open") || !strcmp(req->mode, "bossrush") || !strcmp(req->mode, "chaos"))
      snprintf(err, err_len, "unsupported option: mode '%s' is not ported to C yet", req->mode);
    else
      snprintf(err, err_len, "unknown mode '%s'", req->mode);
    return SOMRANDO_ERR_UNSUPPORTED;
  }
  struct sr_settings *s = sr_settings_new(a, req->mode);
  if (!s) {
    set_err(err, err_len, "out of memory");
    return SOMRANDO_ERR_FAILED;
  }
  int rc = sr_settings_apply_lines(s, req->settings, err, err_len);
  if (rc) return rc;
  *out = s;
  return 0;
}

int somrando_available(void) {
#ifdef SOMRANDO_ADVERTISE
  return 1;
#else
  return 0;   /* build without SOMRANDO_ADVERTISE: behave like the stub for the menu */
#endif
}

const char *somrando_version(void) { return "1.49"; }

int somrando_supports(const struct somrando_request *req, char *why, size_t why_len) {
  char err[512] = "";
  if (check_request_shape(req, err, sizeof err)) { set_err(why, why_len, err); return 0; }
  struct sr_arena a = {0};
  struct sr_settings *s = NULL;
  int rc = build_settings(&a, req, &s, err, sizeof err);
  int ok = 0;
  if (!rc) {
    char msg[512];
    ok = check_requirements(s, msg, sizeof msg) && sr_w8_preflight(sr_settings_get(s, "mode"), req->difficulty, msg, sizeof msg) && sr_w9_preflight(s, sr_settings_get(s, "mode"), req->difficulty, msg, sizeof msg) && sr_p3_preflight(s, sr_settings_get(s, "mode"), req->difficulty, msg, sizeof msg) && sr_p4_preflight(s, sr_settings_get(s, "mode"), req->difficulty, msg, sizeof msg);
    if (!ok) snprintf(err, sizeof err, "%s", msg);
  }
  sr_arena_free(&a);
  if (!ok) set_err(why, why_len, err);
  else if (why && why_len) why[0] = 0;
  return ok;
}

/* ------------------------------------------------------------------------------------------------------------ trace */
static void trace_step(struct sr_ctx *c, const char *name, int applied, int wo_before) {
  if (!c->trace) return;
  fprintf(c->trace, "{\"step\":%d,\"name\":\"%s\",\"applied\":%d,\"wo_before\":%d,\"wo_after\":%d,\"ranges\":[", c->step, name, applied, wo_before,
          c->working_offset);
  int nr = 0;
  long total = 0;
  for (int i = 0; i < SR_ROM_OUT_SIZE;) {
    if (c->out[i] != c->trace_snapshot[i]) {
      int j = i;
      while (j < SR_ROM_OUT_SIZE && c->out[j] != c->trace_snapshot[j]) j++;
      if (nr < 200) fprintf(c->trace, "%s[%d,%d]", nr ? "," : "", i, j - i);
      nr++;
      total += j - i;
      i = j;
    } else {
      i++;
    }
  }
  fprintf(c->trace, "],\"nranges\":%d,\"changed\":%ld}\n", nr, total);
  memcpy(c->trace_snapshot, c->out, SR_ROM_OUT_SIZE);
}

void sr_next_step(struct sr_ctx *c, const char *name) {
  c->step++;
  c->step_name = name;
  if (c->progress && c->progress(c->progress_user, c->step, c->total_steps, name)) sr_fail(c, SOMRANDO_ERR_CANCELLED, "cancelled");
}
#define next_step sr_next_step

/* Development aid (SOMRANDO_DEBUG builds): one line per hack, same format as the instrumented oracle's SOM_HACK_LOG
 * (index, name, success, workingOffset, FNV-1a of the ROM, hashes of both random states), so the first diverging hack is found by diff. */
__attribute__((unused)) static uint64_t fnv_step(uint64_t h, uint32_t v) { return (h ^ v) * 1099511628211ULL; }
__attribute__((unused)) static uint64_t rng_state_hash(const struct sr_random *r) {
  uint64_t h = 14695981039346656037ULL;
  h = fnv_step(h, (uint32_t)r->inext);
  h = fnv_step(h, (uint32_t)r->inextp);
  for (int i = 0; i < 56; i++) h = fnv_step(h, (uint32_t)r->seed_array[i]);
  return h;
}
void sr_hacklog(struct sr_ctx *c, const char *name, int success) {
#ifdef SOMRANDO_DEBUG
  const char *path = getenv("SOMRANDO_HACKLOG");
  if (!path || !path[0]) return;
  uint64_t h = 14695981039346656037ULL;
  for (int i = 0; i < SR_ROM_OUT_SIZE; i++) h = (h ^ c->out[i]) * 1099511628211ULL;
  FILE *f = fopen(path, "a");
  if (f) {
    fprintf(f, "%d\t%s\t%d\t%d\t%016llx\t%016llx\t%016llx\n", c->hacklog_idx, name, success ? 1 : 0, c->working_offset, (unsigned long long)h,
            (unsigned long long)rng_state_hash(&c->rf), (unsigned long long)rng_state_hash(&c->rc));
    fclose(f);
  }
  const char *dir = getenv("SOMRANDO_HACKLOG_DUMP_DIR"), *which = getenv("SOMRANDO_HACKLOG_DUMP_IDX");
  if (dir && which) {
    char key[32], list[512];
    snprintf(key, sizeof key, ",%d,", c->hacklog_idx);
    snprintf(list, sizeof list, ",%s,", which);
    if (strstr(list, key)) {
      char pth[700];
      snprintf(pth, sizeof pth, "%s/c%d.rom", dir, c->hacklog_idx);
      FILE *g = fopen(pth, "wb");
      if (g) { fwrite(c->out, 1, SR_ROM_OUT_SIZE, g); fclose(g); }
    }
  }
  c->hacklog_idx++;
#else
  (void)c; (void)name; (void)success;
#endif
}

static void run_hack(struct sr_ctx *c, const struct sr_hack *h, const char *dump_prefix) {
  next_step(c, h->name);
  int before = c->working_offset;
  int applied = 0;
  if (debug_skipped(h->upstream) || sr_p4_name_skipped(h->name) || sr_w9_noop_in_mode(h->upstream, sr_settings_get(c->settings, "mode"))) {
    trace_step(c, h->name, 0, before);
    debug_dump(c, dump_prefix, h->upstream);
    sr_hacklog(c, h->name, 0);
    return;
  }
  if (h->fn) {
    applied = h->fn(c);
  } else {
    /* not ported: check_requirements() already proved the C# skips this hack with the current options */
    const char *const *rq = h->requires;
    for (; rq && rq[0]; rq += 2)
      if (!value_in_list(rq[1], sr_settings_get(c->settings, rq[0])))
        sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "unsupported option: %s=%s (%s)", rq[0], sr_settings_get(c->settings, rq[0]), h->name);
  }
  trace_step(c, h->name, applied, before);
  debug_dump(c, dump_prefix, h->upstream);
  sr_hacklog(c, h->name, applied);
}

/* ------------------------------------------------------------------------------------------------------- generation */
int somrando_generate(const struct somrando_request *req, somrando_progress_fn progress, void *user, char *err, size_t err_len) {
  char msg[512] = "";
  int rc = check_request_shape(req, msg, sizeof msg);
  if (rc) { set_err(err, err_len, msg); return rc; }

  struct sr_ctx *c = (struct sr_ctx *)calloc(1, sizeof *c);
  if (!c) { set_err(err, err_len, "out of memory"); return SOMRANDO_ERR_FAILED; }
  c->req = req;
  c->progress = progress;
  c->progress_user = user;
  c->seed = req->seed;
  uint8_t *volatile src = NULL;
  size_t src_len = 0;
  char tmp_path[1100] = "";
  volatile int result = 0;

  if (setjmp(c->fatal)) {
    result = c->fail_code ? c->fail_code : SOMRANDO_ERR_FAILED;
    set_err(err, err_len, c->err);
    goto done;
  }

  rc = build_settings(&c->arena, req, &c->settings, msg, sizeof msg);
  if (rc) { result = rc; set_err(err, err_len, msg); goto done; }
  if (!check_requirements(c->settings, msg, sizeof msg)) { result = SOMRANDO_ERR_UNSUPPORTED; set_err(err, err_len, msg); goto done; }
  if (!sr_w8_preflight(sr_settings_get(c->settings, "mode"), req->difficulty, msg, sizeof msg)) { result = SOMRANDO_ERR_UNSUPPORTED; set_err(err, err_len, msg); goto done; }
  if (!sr_w9_preflight(c->settings, sr_settings_get(c->settings, "mode"), req->difficulty, msg, sizeof msg)) { result = SOMRANDO_ERR_UNSUPPORTED; set_err(err, err_len, msg); goto done; }
  if (!sr_p3_preflight(c->settings, sr_settings_get(c->settings, "mode"), req->difficulty, msg, sizeof msg)) { result = SOMRANDO_ERR_UNSUPPORTED; set_err(err, err_len, msg); goto done; }
  if (!sr_p4_preflight(c->settings, sr_settings_get(c->settings, "mode"), req->difficulty, msg, sizeof msg)) { result = SOMRANDO_ERR_UNSUPPORTED; set_err(err, err_len, msg); goto done; }

  if (!req->src_rom || !req->dst_rom || !req->src_rom[0] || !req->dst_rom[0]) {
    result = SOMRANDO_ERR_FAILED;
    set_err(err, err_len, "No ROM selected.");
    goto done;
  }
  if (!strcmp(req->src_rom, req->dst_rom)) {
    result = SOMRANDO_ERR_FAILED;
    set_err(err, err_len, "Failed - source and destination file are the same!");
    goto done;
  }
  /* C# creates the destination before it reads the source: fail early (without touching an existing destination) when it can't be written */
  if (strlen(req->dst_rom) + 8 > sizeof tmp_path) {
    result = SOMRANDO_ERR_FAILED;
    set_err(err, err_len, "Can't write to destination file.");
    goto done;
  }
  snprintf(tmp_path, sizeof tmp_path, "%s.part", req->dst_rom);
  {
    FILE *probe = fopen(tmp_path, "wb");
    if (!probe) {
      tmp_path[0] = 0;
      result = SOMRANDO_ERR_FAILED;
      set_err(err, err_len, "Can't write to destination file.");
      goto done;
    }
    fclose(probe);
  }
  uint8_t *loaded = NULL;
  int have_src = read_file(req->src_rom, &loaded, &src_len);
  src = loaded;
  if (!have_src) {
    result = SOMRANDO_ERR_FAILED;
    set_err(err, err_len, "Cannot open source file");
    goto done;
  }
  if (src_len != 2 * 1024 * 1024 && src_len != 2 * 1024 * 1024 + 0x200) {
    result = SOMRANDO_ERR_BAD_ROM;
    set_err(err, err_len, "Source file had unexpected size; should be 16 Megabit ROM");
    goto done;
  }
  const size_t header = src_len == 2 * 1024 * 1024 ? 0 : 0x200;

  c->out = (uint8_t *)sr_alloc(c, SR_ROM_OUT_SIZE);
  c->orig = (uint8_t *)sr_alloc(c, SR_ROM_OUT_SIZE);
  memcpy(c->out, src + header, SR_ROM_IN_SIZE);
  memcpy(c->orig, c->out, SR_ROM_IN_SIZE);
  free(src);
  src = NULL;

  if (memcmp(c->orig + 0xFFC0, "Secret of MANA", 14) != 0) {
    result = SOMRANDO_ERR_BAD_ROM;
    set_err(err, err_len, "Name of game did not match the expected \"Secret of MANA\"");
    goto done;
  }
  if (c->orig[0xFFD9] != 1) {
    result = SOMRANDO_ERR_BAD_ROM;
    set_err(err, err_len, "Please use US ROM only.");
    goto done;
  }
  if (c->orig[0xFFDB] != 0) {
    result = SOMRANDO_ERR_BAD_ROM;
    set_err(err, err_len, "Please use version 1.0 only.");
    goto done;
  }

  /* RomGenerator.generate(): two streams from the hashed seed; race mode re-seeds the functional one from its own first draw */
  c->working_offset = 0x200000;
  sr_random_init(&c->rf, sr_hash_string(req->seed));
  {
    size_t n = strlen(req->seed);
    char *cos = (char *)sr_alloc(c, n + 16);
    memcpy(cos, req->seed, n);
    memcpy(cos + n, "_cosmetic", 10);
    sr_random_init(&c->rc, sr_hash_string(cos));
  }
  if (sr_settings_get_bool(c->settings, "raceMode")) sr_random_init(&c->rf, sr_random_next(&c->rf));

  const char *trace_path = getenv("SOMRANDO_TRACE");
  if (trace_path && trace_path[0]) {
    c->trace = fopen(trace_path, "w");
    if (c->trace) {
      c->trace_snapshot = (uint8_t *)sr_alloc(c, SR_ROM_OUT_SIZE);
      memcpy(c->trace_snapshot, c->out, SR_ROM_OUT_SIZE);
    }
  }

  {
    const char *mode = sr_settings_get(c->settings, "mode");
    const struct sr_hack *mode_hacks = sr_rando_hacks;
    int n_mode = sr_rando_hack_count;
    if (!strcmp(mode, "bossrush")) { mode_hacks = sr_w8_bossrush_hacks; n_mode = sr_w8_bossrush_hack_count; }
    else if (!strcmp(mode, "chaos")) { mode_hacks = sr_w9_chaos_hacks; n_mode = sr_w9_chaos_hack_count; }
    else if (!strcmp(mode, "ancientcave")) { mode_hacks = sr_p3_ac_hacks; n_mode = sr_p3_ac_hack_count; }
    else if (!strcmp(mode, "open")) { mode_hacks = sr_p4_open_hacks; n_mode = sr_p4_open_hack_count; }
    /* Ancient Cave reports one more step per floor, between the floor generations */
    c->total_steps = sr_common_hack_count + n_mode + 3 + (!strcmp(mode, "ancientcave") ? sr_p3_ac_num_floors(c->settings) : 0);
    if (!strcmp(mode, "open")) {
      /* OpenWorldGenerator.generate(): ROM marker, then the plando setting is split for the hacks */
      const char *ver = sr_settings_get(c->settings, "version");
      const char *pre = "open world v";
      for (const char *q = pre; *q; q++) sr_emit(c, (unsigned char)*q);
      for (const char *q = ver; *q; q++) sr_emit(c, (unsigned char)*q);
      sr_emit(c, 0);
      sr_p4_process_plando(c);
    }
    for (int i = 0; i < sr_common_hack_count; i++) run_hack(c, &sr_common_hacks[i], "c");
    for (int i = 0; i < n_mode; i++) run_hack(c, &mode_hacks[i], "m");
  }

  /* a few post-processing steps with data defined by the above */
  next_step(c, "Names of things");
  {
    int before = c->working_offset;
    sr_names_set_all(c);
    trace_step(c, "Names of things", c->working_offset != before, before);
  }
  next_step(c, "Event table");
  {
    int before = c->working_offset;
    sr_event_expander_process(c);
    trace_step(c, "Event table", c->n_events > 0, before);
  }
  sr_debug_dump_rom(c, "p100_names");
  sr_debug_dump_rom(c, "p101_events");
  next_step(c, "Custom event commands");
  if (!strcmp(sr_settings_get(c->settings, "mode"), "chaos") || !strcmp(sr_settings_get(c->settings, "mode"), "ancientcave")) sr_w9_post_process(c);
  else sr_w8_post_process(c);
  trace_step(c, "Custom event commands", 0, c->working_offset);

  {
    /* the ROM is written next to the destination and renamed over it, so a failure while writing never leaves a truncated file there */
    FILE *f = fopen(tmp_path, "wb");
    if (!f) sr_fail(c, SOMRANDO_ERR_FAILED, "Can't write to destination file.");
    size_t w = fwrite(c->out, 1, SR_ROM_OUT_SIZE, f);
    int fl = fflush(f);
    int cl = fclose(f);
    if (w != SR_ROM_OUT_SIZE || fl != 0 || cl != 0) sr_fail(c, SOMRANDO_ERR_FAILED, "Can't write to destination file.");
    if (rename(tmp_path, req->dst_rom) != 0) {
      remove(req->dst_rom);   /* platforms where rename does not replace an existing file */
      if (rename(tmp_path, req->dst_rom) != 0) sr_fail(c, SOMRANDO_ERR_FAILED, "Can't write to destination file.");
    }
    tmp_path[0] = 0;
  }
  result = SOMRANDO_OK;
  if (err && err_len) err[0] = 0;

done:
  if (tmp_path[0]) remove(tmp_path);   /* failed or cancelled: no partial file */
  if (src) free(src);
  if (c->trace) fclose(c->trace);
  sr_arena_free(&c->arena);
  free(c);
  return result;
}
