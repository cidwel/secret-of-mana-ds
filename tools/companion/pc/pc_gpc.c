/* See pc_gpc.h. */
#include "pc_gpc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_store.h"

int pgc_from_pg(const struct pg *g, struct sgp_opts *o, char *why, size_t why_len) {
  int changed = 0;
  sgp_defaults(o);
  if (why && why_len) why[0] = 0;
  for (int i = 0; i < g->n; i++) {
    const struct pg_opt *p = &g->o[i];
    int k = sgp_find_key(p->key);
    int32_t v;
    if (k < 0) {                          /* a menu entry the patcher does not know: harmless while it stays at its default */
      if (p->cur == p->def) continue;
      if (why && why_len) snprintf(why, why_len, "unknown option %s", p->key);
      return -1;
    }
    if (p->type == PG_ENUM) v = p->integer ? (int32_t)atoi(p->val[p->cur].v) : (int32_t)p->cur;
    else v = (int32_t)p->cur;               /* bool 0 / 1; number: -1 = unset */
    if (!sgp_valid(k, v)) { if (why && why_len) snprintf(why, why_len, "bad value %d for %s", (int)v, p->key); return -1; }
    o->v[k] = v;
  }
  {
    struct sgp_opts d;
    sgp_defaults(&d);
    for (int k = 0; k < SGP_COUNT; k++) changed += o->v[k] != d.v[k];
  }
  return changed;
}

int pgc_apply_file(const char *in, const char *out, const struct sgp_opts *o, char *msg, size_t msg_len, char *skipped, size_t skipped_len) {
  size_t n;
  uint8_t *rom = pc_slurp(in, &n);
  struct sgp_report rep;
  int rc;
  if (skipped && skipped_len) skipped[0] = 0;
  if (!rom) { snprintf(msg, msg_len, "cannot read %s", in); return -1; }
  rc = sgp_apply(rom, n, o, &rep);
  if (rc != 0) { snprintf(msg, msg_len, "%s", rep.why[0] ? rep.why : "the ROM is not a US v1.0 image"); free(rom); return -1; }
  if (skipped && skipped_len) {
    size_t len = 0;
    for (int k = 0; k < SGP_COUNT; k++)
      if (rep.skipped & (1u << k)) len += (size_t)snprintf(skipped + len, skipped_len - len, "%s%s", len ? "," : "", sgp_key(k));
  }
  if (pc_spit(out, rom, n) != 0) { snprintf(msg, msg_len, "cannot write %s", out); free(rom); return -1; }
  free(rom);
  msg[0] = 0;
  return 0;
}
