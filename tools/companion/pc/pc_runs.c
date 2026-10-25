/* The list of generated runs, see pc_runs.h. */
#include "pc_runs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_logic.h"
#include "pc_store.h"

/* ---- tiny JSON string helpers: flat strings with \" \\ \n escapes ---- */
static const char *find_key(const char *t, const char *end, const char *key) {
  char pat[48];
  size_t kl;
  snprintf(pat, sizeof pat, "\"%s\"", key);
  kl = strlen(pat);
  for (const char *p = t; p && p + kl < end; p++) {
    if (*p == '"' && !strncmp(p, pat, kl)) {
      const char *q = p + kl;
      while (q < end && isspace((unsigned char)*q)) q++;
      if (q < end && *q == ':') { q++; while (q < end && isspace((unsigned char)*q)) q++; return q; }
    }
  }
  return NULL;
}
static int get_str(const char *t, const char *end, const char *key, char *out, size_t n) {
  const char *p = find_key(t, end, key);
  size_t i = 0;
  if (!p || *p != '"') return 0;
  for (p++; p < end && *p != '"'; p++) {
    char c = *p;
    if (c == '\\' && p + 1 < end) { p++; c = *p == 'n' ? '\n' : *p; }
    if (i + 1 < n) out[i++] = c;
  }
  out[i] = 0;
  return 1;
}
static int get_num(const char *t, const char *end, const char *key, int64_t *v) {
  const char *p = find_key(t, end, key);
  char *e;
  long long x;
  if (!p) return 0;
  x = strtoll(p, &e, 10);
  if (e == p) return 0;
  *v = x;
  return 1;
}
static void esc(char *out, size_t n, const char *s) {
  size_t i = 0;
  for (; *s && i + 3 < n; s++) {
    if (*s == '"' || *s == '\\') out[i++] = '\\', out[i++] = *s;
    else if (*s == '\n') out[i++] = '\\', out[i++] = 'n';
    else if ((unsigned char)*s >= 0x20) out[i++] = *s;
  }
  out[i] = 0;
}

void pr_init(struct pr_list *l) { memset(l, 0, sizeof *l); }

static int parse_list(const char *t, struct pr_list *l) {
  const char *arr = strstr(t, "\"runs\""), *p;
  pr_init(l);
  if (!arr || !(arr = strchr(arr, '['))) return -1;
  for (p = arr + 1; *p && *p != ']' && l->n < PR_MAX;) {
    const char *start, *end;
    struct pr_run *r;
    int64_t v;
    while (*p && *p != '{' && *p != ']') p++;
    if (*p != '{') break;
    start = p;
    end = strchr(p, '}');
    if (!end) return -1;
    p = end + 1;
    r = &l->run[l->n];
    memset(r, 0, sizeof *r);
    if (!get_str(start, end, "key", r->key, sizeof r->key) || !r->key[0]) continue;
    get_str(start, end, "preset", r->preset, sizeof r->preset);
    get_str(start, end, "mode", r->mode, sizeof r->mode);
    get_str(start, end, "seed", r->seed, sizeof r->seed);
    get_str(start, end, "diff", r->diff, sizeof r->diff);
    get_str(start, end, "path", r->path, sizeof r->path);
    get_str(start, end, "lines", r->lines, sizeof r->lines);
    if (get_num(start, end, "created", &v)) r->created = v;
    if (get_num(start, end, "last", &v)) r->last = v;
    if (get_num(start, end, "plays", &v) && v >= 0 && v < 1000000) r->plays = (int)v;
    l->n++;
  }
  return 0;
}
static int text_ok(const char *t) { struct pr_list *l = malloc(sizeof *l); int ok = l && pc_json_complete(t) && parse_list(t, l) == 0; free(l); return ok; }

int pr_load(const char *path, struct pr_list *l) {
  int src = 2;
  char *t = pc_store_load(path, text_ok, &src);
  pr_init(l);
  if (!t) return src == 2 ? 0 : -1;                       /* no file yet is an empty list; a damaged one with no backup is reported */
  parse_list(t, l);
  free(t);
  return 0;
}

int pr_save(const char *path, const struct pr_list *l) {
  size_t cap = 4096 + (size_t)l->n * 4096, o = 0;
  char *buf = malloc(cap);
  int r;
  if (!buf) return -1;
  o += (size_t)snprintf(buf + o, cap - o, "{\n  \"version\": 1,\n  \"runs\": [\n");
  for (int i = 0; i < l->n; i++) {
    const struct pr_run *x = &l->run[i];
    char preset[80], seed[40], diff[48], mode[64], path_[1100], lines[2200], key[48];
    esc(key, sizeof key, x->key); esc(preset, sizeof preset, x->preset); esc(mode, sizeof mode, x->mode); esc(seed, sizeof seed, x->seed);
    esc(diff, sizeof diff, x->diff); esc(path_, sizeof path_, x->path); esc(lines, sizeof lines, x->lines);
    o += (size_t)snprintf(buf + o, cap - o,
      "    {\"key\": \"%s\", \"preset\": \"%s\", \"mode\": \"%s\", \"seed\": \"%s\", \"diff\": \"%s\", \"path\": \"%s\", \"lines\": \"%s\", \"created\": %lld, \"last\": %lld, \"plays\": %d}%s\n",
      key, preset, mode, seed, diff, path_, lines, (long long)x->created, (long long)x->last, x->plays, i + 1 < l->n ? "," : "");
  }
  o += (size_t)snprintf(buf + o, cap - o, "  ]\n}\n");
  r = o < cap ? pc_store_save(path, buf) : -1;
  free(buf);
  return r;
}

int pr_find(const struct pr_list *l, const char *key) {
  for (int i = 0; i < l->n; i++) if (!strcmp(l->run[i].key, key)) return i;
  return -1;
}
int pr_add(struct pr_list *l, const struct pr_run *r) {
  int i = pr_find(l, r->key);
  if (i >= 0) {
    struct pr_run old = l->run[i];
    l->run[i] = *r;
    l->run[i].created = old.created ? old.created : r->created;
    l->run[i].last = old.last > r->last ? old.last : r->last;
    l->run[i].plays = old.plays > r->plays ? old.plays : r->plays;
    return i;
  }
  if (l->n >= PR_MAX) return -1;
  l->run[l->n] = *r;
  return l->n++;
}
void pr_played(struct pr_list *l, const char *key, int64_t now) {
  int i = pr_find(l, key);
  if (i >= 0) { l->run[i].last = now; l->run[i].plays++; }
}
int pr_remove(struct pr_list *l, const char *key) {
  int i = pr_find(l, key);
  if (i < 0) return -1;
  memmove(&l->run[i], &l->run[i + 1], (size_t)(l->n - i - 1) * sizeof l->run[0]);
  l->n--;
  return 0;
}
static int cmp(const void *a, const void *b) {
  const struct pr_run *x = a, *y = b;
  int64_t kx = x->last ? x->last : x->created, ky = y->last ? y->last : y->created;
  return kx < ky ? 1 : kx > ky ? -1 : strcmp(x->key, y->key);
}
void pr_sort(struct pr_list *l) { qsort(l->run, (size_t)l->n, sizeof l->run[0], cmp); }

int pr_from_sidecar(const char *json, const char *rom_path, struct pr_run *r) {
  const char *end = json + strlen(json), *cfg, *opts;
  char tmp[64];
  memset(r, 0, sizeof *r);
  if (!get_str(json, end, "tool", tmp, sizeof tmp) || !get_str(json, end, "config_key", r->key, sizeof r->key) || !r->key[0]) return -1;
  get_str(json, end, "mode", r->mode, sizeof r->mode);
  get_str(json, end, "seed", r->seed, sizeof r->seed);
  get_str(json, end, "difficulty", r->diff, sizeof r->diff);
  cfg = find_key(json, end, "config");
  if (cfg) get_str(cfg, end, "preset", r->preset, sizeof r->preset);
  if (cfg && (opts = find_key(cfg, end, "options")) && *opts == '{') {
    const char *oe = strchr(opts, '}'), *p = opts + 1;
    size_t o = 0;
    while (oe && p < oe) {
      char k[48], v[80];
      const char *q1 = strchr(p, '"'), *q2, *q3, *q4;
      if (!q1 || q1 >= oe) break;
      q2 = strchr(q1 + 1, '"'); if (!q2 || q2 >= oe) break;
      q3 = strchr(q2 + 1, '"'); if (!q3 || q3 >= oe) break;
      q4 = strchr(q3 + 1, '"'); if (!q4 || q4 > oe) break;
      snprintf(k, sizeof k, "%.*s", (int)(q2 - q1 - 1), q1 + 1);
      snprintf(v, sizeof v, "%.*s", (int)(q4 - q3 - 1), q3 + 1);
      o += (size_t)snprintf(r->lines + o, sizeof r->lines - o, "%s=%s\n", k, v);
      if (o >= sizeof r->lines) { r->lines[0] = 0; break; }
      p = q4 + 1;
    }
  }
  snprintf(r->path, sizeof r->path, "%s", rom_path);
  return 0;
}
