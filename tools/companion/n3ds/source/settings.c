#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"
#include "gen_data.h"

static const char *const FILTERS[N3_FILTER_COUNT] = {"None", "Smooth", "Scanlines"};

void n3_defaults(struct n3_settings *s) {
  memset(s, 0, sizeof *s);
  s->filter = N3_FILTER_NONE;
  s->slot = 1;
  sgp_defaults(&s->gp);
}
const char *n3_filter_name(int f) { return f >= 0 && f < N3_FILTER_COUNT ? FILTERS[f] : "?"; }

int n3_gp_count(void) { return GD_NOPTS; }
int n3_gp_sgp(int i) { return i >= 0 && i < GD_NOPTS ? sgp_find_key(GD_OPTS[i].key) : -1; }

/* A menu enum value is either a name (position = the C value) or a number (the number is the C value). */
static int is_number(const char *s) { if (!*s) return 0; for (; *s; s++) if (*s < '0' || *s > '9') return 0; return 1; }
static int enum_value(const struct gd_opt *o, int idx) { return is_number(o->vals[idx].v) ? atoi(o->vals[idx].v) : idx; }
static int enum_index(const struct gd_opt *o, int32_t v) {
  for (int i = 0; i < o->nvals; i++) if (enum_value(o, i) == v) return i;
  return 0;
}

void n3_gp_text(const struct n3_settings *s, int i, char *buf, size_t n) {
  const struct gd_opt *o = &GD_OPTS[i];
  int k = n3_gp_sgp(i);
  buf[0] = 0;
  if (k < 0) { snprintf(buf, n, "n/a"); return; }
  int32_t v = s->gp.v[k];
  if (o->type == 0) snprintf(buf, n, "%s", o->vals[enum_index(o, v)].l);
  else if (o->type == 1) { if (v < 0) snprintf(buf, n, "off"); else snprintf(buf, n, "%d", (int)v); }
  else snprintf(buf, n, "%s", v ? "on" : "off");
}

void n3_gp_step(struct n3_settings *s, int i, int dir, int big) {
  const struct gd_opt *o = &GD_OPTS[i];
  int k = n3_gp_sgp(i);
  if (k < 0) return;
  int32_t *v = &s->gp.v[k];
  if (o->type == 0) {
    int idx = (enum_index(o, *v) + dir + o->nvals) % o->nvals;
    *v = enum_value(o, idx);
  } else if (o->type == 1) {   /* optional number: unset, min..max, unset */
    int step = big ? 10 : 1;
    if (*v < 0) *v = dir > 0 ? o->min : o->max;
    else {
      long nv = *v + (long)dir * step;
      if (nv > o->max) *v = -1;
      else if (nv < o->min) *v = -1;
      else *v = (int32_t)nv;
    }
  } else *v = !*v;
}

int n3_gp_set(struct n3_settings *s, const char *key, const char *value) {
  for (int i = 0; i < GD_NOPTS; i++) {
    const struct gd_opt *o = &GD_OPTS[i];
    if (strcmp(o->key, key) != 0) continue;
    int k = n3_gp_sgp(i);
    if (k < 0) return -1;
    if (o->type == 0) {
      for (int j = 0; j < o->nvals; j++) if (strcmp(o->vals[j].v, value) == 0) { s->gp.v[k] = enum_value(o, j); return 0; }
      return -1;
    }
    if (o->type == 2) { s->gp.v[k] = (!strcmp(value, "yes") || !strcmp(value, "true") || !strcmp(value, "1")) ? 1 : 0; return 0; }
    if (!value[0]) { s->gp.v[k] = -1; return 0; }
    if (!is_number(value)) return -1;
    int v = atoi(value);
    if (v < o->min || v > o->max) return -1;
    s->gp.v[k] = v;
    return 0;
  }
  return -1;
}

int n3_gp_preset_count(void) { return GD_NPRESETS; }
const char *n3_gp_preset_name(int i) { return GD_PRESETS[i].name; }
const char *n3_gp_preset_desc(int i) { return GD_PRESETS[i].desc; }
void n3_gp_apply_preset(struct n3_settings *s, int i) {
  sgp_defaults(&s->gp);
  for (int j = 0; j < GD_PRESETS[i].n; j++) n3_gp_set(s, GD_PRESETS[i].keys[j], GD_PRESETS[i].vals[j]);
}
int n3_gp_preset_matches(const struct n3_settings *s, int i) {
  struct n3_settings t = *s;
  n3_gp_apply_preset(&t, i);
  return memcmp(&t.gp, &s->gp, sizeof t.gp) == 0;
}
int n3_gp_changed_count(const struct n3_settings *s) {
  struct sgp_opts d;
  int n = 0;
  sgp_defaults(&d);
  for (int i = 0; i < SGP_COUNT; i++) if (d.v[i] != s->gp.v[i]) n++;
  return n;
}

void n3_load(struct n3_settings *s, const char *path) {
  char line[96];
  n3_defaults(s);
  FILE *f = fopen(path, "rb");
  if (!f) return;
  while (fgets(line, sizeof line, f)) {
    char *eq = strchr(line, '='), *nl = strpbrk(line, "\r\n");
    if (nl) *nl = 0;
    if (!eq) continue;
    *eq = 0;
    const char *k = line, *v = eq + 1;
    if (!strcmp(k, "filter")) { int x = atoi(v); if (x >= 0 && x < N3_FILTER_COUNT) s->filter = x; }
    else if (!strcmp(k, "slot")) { int x = atoi(v); if (x >= 1 && x <= 8) s->slot = x; }
    else if (!strncmp(k, "gp.", 3)) {
      int idx = sgp_find_key(k + 3);
      if (idx >= 0 && (v[0] == '-' || (v[0] >= '0' && v[0] <= '9'))) { int32_t x = (int32_t)atoi(v); if (sgp_valid(idx, x)) s->gp.v[idx] = x; }
    }
  }
  fclose(f);
}

int n3_save(const struct n3_settings *s, const char *path) {
  char buf[1024];
  int n = snprintf(buf, sizeof buf, "filter=%d\nslot=%d\n", s->filter, s->slot);
  struct sgp_opts d;
  sgp_defaults(&d);
  for (int i = 0; i < SGP_COUNT; i++)
    if (s->gp.v[i] != d.v[i]) n += snprintf(buf + n, sizeof buf - (size_t)n, "gp.%s=%d\n", sgp_key(i), (int)s->gp.v[i]);
  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  int ok = fwrite(buf, 1, (size_t)n, f) == (size_t)n;
  return (fclose(f) == 0 && ok) ? 0 : -1;
}
