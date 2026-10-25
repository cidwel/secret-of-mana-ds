/* som_launcher: start menu model.  See som_launcher.h.  Option names and values are the generator's own
 * (Moppu/SecretOfManaRandomizer v1.49, LGPL-2.1). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "som_launcher.h"

/* ------------------------------------------------------------------ SHA-256 (FIPS 180-4) */
struct sha { uint32_t h[8]; uint64_t len; uint8_t buf[64]; size_t n; };
static const uint32_t K256[64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
  0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
  0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
  0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
  0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
  0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_block(struct sha *s, const uint8_t *p) {
  uint32_t w[64], a, b, c, d, e, f, g, h;
  for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
  for (int i = 16; i < 64; i++) {
    uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
  for (int i = 0; i < 64; i++) {
    uint32_t t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
    uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
    h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
  }
  s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}
static void sha_init(struct sha *s) {
  static const uint32_t iv[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  memcpy(s->h, iv, sizeof iv); s->len = 0; s->n = 0;
}
static void sha_update(struct sha *s, const uint8_t *p, size_t n) {
  s->len += n;
  while (n) {
    size_t k = 64 - s->n < n ? 64 - s->n : n;
    memcpy(s->buf + s->n, p, k); s->n += k; p += k; n -= k;
    if (s->n == 64) { sha_block(s, s->buf); s->n = 0; }
  }
}
static void sha_final(struct sha *s, uint8_t out[32]) {
  uint64_t bits = s->len * 8;
  uint8_t pad = 0x80;
  sha_update(s, &pad, 1);
  pad = 0;
  while (s->n != 56) sha_update(s, &pad, 1);
  uint8_t l[8];
  for (int i = 0; i < 8; i++) l[i] = (uint8_t)(bits >> (56 - 8 * i));
  sha_update(s, l, 8);
  for (int i = 0; i < 8; i++) { out[4 * i] = (uint8_t)(s->h[i] >> 24); out[4 * i + 1] = (uint8_t)(s->h[i] >> 16); out[4 * i + 2] = (uint8_t)(s->h[i] >> 8); out[4 * i + 3] = (uint8_t)s->h[i]; }
}

void sl_sha256(const void *p, size_t n, uint8_t out[32]) {
  struct sha s;
  sha_init(&s); sha_update(&s, (const uint8_t *)p, n); sha_final(&s, out);
}

/* ------------------------------------------------------------------ small helpers */
static void cpy(char *d, size_t n, const char *s) { snprintf(d, n, "%s", s ? s : ""); }

struct out { char *b; size_t n, len; int over; };
static void put(struct out *o, const char *s) {
  size_t k = strlen(s);
  if (o->len + k + 1 > o->n) { o->over = 1; return; }
  memcpy(o->b + o->len, s, k + 1);
  o->len += k;
}
static void put_json_str(struct out *o, const char *s) {
  put(o, "\"");
  for (; *s; s++) {
    char t[8];
    if (*s == '"' || *s == '\\') { t[0] = '\\'; t[1] = *s; t[2] = 0; }
    else if ((unsigned char)*s < 32) snprintf(t, sizeof t, "\\u%04x", *s);
    else { t[0] = *s; t[1] = 0; }
    put(o, t);
  }
  put(o, "\"");
}

static const struct sl_mode *mode_of(const struct sl_launcher *L) { return &sl_modes[L->cfg.mode]; }
int sl_nopts(const struct sl_launcher *L) { return mode_of(L)->nopts + sl_ncommon; }
const struct sl_option *sl_opt(const struct sl_launcher *L, int i) {
  const struct sl_mode *m = mode_of(L);
  return i < m->nopts ? &m->opts[i] : &sl_common[i - m->nopts];
}
int sl_find_opt(const struct sl_launcher *L, const char *key) {
  int n = sl_nopts(L);
  for (int i = 0; i < n; i++) if (!strcmp(sl_opt(L, i)->key, key)) return i;
  return -1;
}

/* ------------------------------------------------------------------ values */
static int pow10i(int d) { int r = 1; while (d-- > 0) r *= 10; return r; }

static void num_text(const struct sl_option *o, int32_t v, char *buf, size_t n) {
  int p = pow10i(o->decimals);
  if (!o->decimals) { snprintf(buf, n, "%d", (int)v); return; }
  char f[32];
  snprintf(f, sizeof f, "%0*d", o->decimals > 9 ? 9 : o->decimals, (int)(v % p));
  size_t k = strlen(f);
  while (k && f[k - 1] == '0') f[--k] = 0;
  if (k) snprintf(buf, n, "%d.%s", (int)(v / p), f); else snprintf(buf, n, "%d", (int)(v / p));
}

static int parse_value(const struct sl_option *o, const char *s, int32_t *out) {
  switch (o->type) {
    case SL_O_BOOL:
      if (!strcmp(s, "yes")) { *out = 1; return 0; }
      if (!strcmp(s, "no")) { *out = 0; return 0; }
      return -1;
    case SL_O_ENUM:
      for (int i = 0; i < o->nvalues; i++) if (!strcmp(o->values[i].v, s)) { *out = i; return 0; }
      return -1;
    case SL_O_NUM: {
      int ip = 0, fp = 0, fd = 0, any = 0;
      const char *q = s;
      for (; *q >= '0' && *q <= '9'; q++) { ip = ip * 10 + (*q - '0'); any = 1; if (ip > 10000000) return -1; }
      if (*q == '.') for (q++; *q >= '0' && *q <= '9'; q++) { if (fd < o->decimals) { fp = fp * 10 + (*q - '0'); fd++; } else if (*q != '0') return -1; any = 1; }
      if (!any || *q) return -1;
      while (fd < o->decimals) { fp *= 10; fd++; }
      int32_t v = ip * pow10i(o->decimals) + fp;
      if (v < o->min || v > o->max) return -1;
      *out = v;
      return 0;
    }
    case SL_O_MULTI: {
      int32_t mask = 0;
      char tmp[SL_TEXT];
      cpy(tmp, sizeof tmp, s);
      for (char *t = tmp; t && *t;) {
        char *c = strchr(t, ',');
        if (c) *c = 0;
        int f = -1;
        for (int i = 0; i < o->nvalues; i++) if (!strcmp(o->values[i].v, t)) f = i;
        if (f < 0) return -1;
        mask |= 1 << f;
        t = c ? c + 1 : NULL;
      }
      if (!mask) return -1;
      *out = mask;
      return 0;
    }
  }
  return -1;
}

void sl_value_string(const struct sl_launcher *L, int i, char *buf, size_t n) {
  const struct sl_option *o = sl_opt(L, i);
  int32_t v = L->cfg.val[i];
  switch (o->type) {
    case SL_O_BOOL: case SL_O_ENUM: cpy(buf, n, o->values[v].v); break;
    case SL_O_NUM: num_text(o, v, buf, n); break;
    case SL_O_MULTI: {
      struct out w = {buf, n, 0, 0};
      if (n) buf[0] = 0;
      for (int k = 0; k < o->nvalues; k++) if (v & (1 << k)) { if (w.len) put(&w, ","); put(&w, o->values[k].v); }
      break;
    }
  }
}

void sl_value_text(const struct sl_launcher *L, int i, char *buf, size_t n) {
  const struct sl_option *o = sl_opt(L, i);
  int32_t v = L->cfg.val[i];
  switch (o->type) {
    case SL_O_BOOL: case SL_O_ENUM: cpy(buf, n, o->values[v].label); break;
    case SL_O_NUM: num_text(o, v, buf, n); break;
    case SL_O_MULTI: {
      int on = 0;
      for (int k = 0; k < o->nvalues; k++) if (v & (1 << k)) on++;
      snprintf(buf, n, "%d of %d", on, o->nvalues);
      break;
    }
  }
}

int sl_opt_visible(const struct sl_launcher *L, int i) {
  const struct sl_option *o = sl_opt(L, i);
  for (int r = 0; r < o->nreq; r++) {
    int j = sl_find_opt(L, o->req[r].key);
    if (j < 0) return 0;
    char cur[SL_TEXT], pat[SL_TEXT + 3];
    sl_value_string(L, j, cur, sizeof cur);
    snprintf(pat, sizeof pat, "|%s|", cur);
    if (!strstr(o->req[r].vals, pat)) return 0;
  }
  return 1;
}

const char *sl_diff_label(const char *d) {
  if (!strcmp(d, "casual")) return "Casual";
  if (!strcmp(d, "hard")) return "Hard";
  if (!strcmp(d, "reallyhard")) return "Really hard";
  return d;
}
int sl_difficulty(const struct sl_launcher *L, const char **name) {
  const struct sl_mode *m = mode_of(L);
  if (!m->ndiff || L->cfg.diff < 0) return 0;
  if (name) *name = m->diff[L->cfg.diff];
  return 1;
}

/* ------------------------------------------------------------------ rng */
static uint32_t rnd(struct sl_launcher *L, uint32_t n) {
  uint64_t x = L->rng;
  x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
  L->rng = x;
  return n ? (uint32_t)(((x * 0x2545F4914F6CDD1DULL) >> 32) % n) : 0;
}
void sl_seed_rng(struct sl_launcher *L, uint64_t s) {
  s += 0x9E3779B97F4A7C15ULL;                      /* splitmix64: nearby entropy values (clock ticks) give unrelated seeds */
  s = (s ^ (s >> 30)) * 0xBF58476D1CE4E5B9ULL; s = (s ^ (s >> 27)) * 0x94D049BB133111EBULL; s ^= s >> 31;
  L->rng = s ? s : 0x9E3779B97F4A7C15ULL;
}

/* ------------------------------------------------------------------ configuration */
static void touch(struct sl_launcher *L) { L->rev++; }

static void set_defaults(struct sl_launcher *L) {
  int n = sl_nopts(L);
  for (int i = 0; i < n; i++) L->cfg.val[i] = sl_opt(L, i)->def;
}

static void apply_constraints(struct sl_launcher *L) {
  const struct sl_mode *m = mode_of(L);
  for (int c = 0; c < m->ncons; c++) {
    const struct sl_cons *k = &m->cons[c];
    if (k->kind == SL_C_ANY) {
      int any = 0, first = -1;
      for (int i = 0; i < k->n; i++) { int j = sl_find_opt(L, k->key[i]); if (j >= 0) { if (first < 0) first = j; if (L->cfg.val[j]) any = 1; } }
      if (!any && first >= 0) L->cfg.val[first] = 1;
    } else {
      int a = sl_find_opt(L, k->key[0]), b = sl_find_opt(L, k->key[1]);
      if (a >= 0 && b >= 0) {
        int va = atoi(sl_opt(L, a)->values[L->cfg.val[a]].v), vb = atoi(sl_opt(L, b)->values[L->cfg.val[b]].v);
        if (va > vb) { int32_t t = L->cfg.val[a]; L->cfg.val[a] = L->cfg.val[b]; L->cfg.val[b] = t; }
      }
    }
  }
}

int sl_set_option(struct sl_launcher *L, const char *key, const char *value) {
  int j = sl_find_opt(L, key);
  int32_t v;
  if (j < 0 || parse_value(sl_opt(L, j), value, &v)) return -1;
  L->cfg.val[j] = v;
  touch(L);
  return 0;
}

int sl_apply_preset(struct sl_launcher *L, int p) {
  if (p < 0 || p >= sl_npresets) return -1;
  const struct sl_preset *ps = &sl_presets[p];
  L->cfg.mode = ps->mode;
  L->cfg.preset = p;
  set_defaults(L);
  const struct sl_mode *m = mode_of(L);
  L->cfg.diff = !m->ndiff ? -1 : (ps->diff >= 0 ? ps->diff : m->def_diff);
  int rc = 0;
  for (int i = 0; i < ps->nset; i++) if (sl_set_option(L, ps->set[i].key, ps->set[i].val)) rc = -1;
  apply_constraints(L);
  touch(L);
  return rc;
}

void sl_randomize_all(struct sl_launcher *L) {
  int n = sl_nopts(L);
  const struct sl_mode *m = mode_of(L);
  if (m->ndiff) L->cfg.diff = (int)rnd(L, m->ndiff);
  for (int i = 0; i < n; i++) {
    const struct sl_option *o = sl_opt(L, i);
    if (!o->randomize || !sl_opt_visible(L, i)) continue;
    if (o->npool) { int32_t v; if (!parse_value(o, o->pool[rnd(L, o->npool)], &v)) L->cfg.val[i] = v; continue; }
    switch (o->type) {
      case SL_O_BOOL: L->cfg.val[i] = (int32_t)rnd(L, 2); break;
      case SL_O_ENUM: L->cfg.val[i] = (int32_t)rnd(L, o->nvalues); break;
      case SL_O_NUM: L->cfg.val[i] = o->min + (int32_t)rnd(L, (uint32_t)((o->max - o->min) / o->step + 1)) * o->step; break;
      default: break;
    }
  }
  apply_constraints(L);
  touch(L);
}

int sl_set_seed(struct sl_launcher *L, const char *d) {
  size_t n = strlen(d);
  if (!n || n > SL_SEED_DIGITS) return -1;
  long v = 0;
  for (size_t i = 0; i < n; i++) { if (d[i] < '0' || d[i] > '9') return -1; v = v * 10 + (d[i] - '0'); }
  snprintf(L->cfg.seed, sizeof L->cfg.seed, "%ld", v);
  touch(L);
  return 0;
}
static long seed_val(const struct sl_launcher *L) { return atol(L->cfg.seed); }
static void seed_put(struct sl_launcher *L, long v) { snprintf(L->cfg.seed, sizeof L->cfg.seed, "%ld", v); touch(L); }
void sl_random_seed(struct sl_launcher *L) { seed_put(L, 1 + (long)rnd(L, 99999999)); }

/* ------------------------------------------------------------------ canonical outputs */
static int delta_list(const struct sl_launcher *L, int *idx) {
  int n = sl_nopts(L), k = 0;
  for (int i = 0; i < n; i++) if (L->cfg.val[i] != sl_opt(L, i)->def && sl_opt_visible(L, i)) idx[k++] = i;
  for (int a = 1; a < k; a++) {                    /* insertion sort by key: byte order, same as Python sorted() on ASCII */
    int t = idx[a], b = a - 1;
    while (b >= 0 && strcmp(sl_opt(L, idx[b])->key, sl_opt(L, t)->key) > 0) { idx[b + 1] = idx[b]; b--; }
    idx[b + 1] = t;
  }
  return k;
}

static int finish(struct out *o) { return o->over ? -1 : (int)o->len; }

int sl_config_keystring(const struct sl_launcher *L, char *buf, size_t n) {
  struct out o = {buf, n, 0, 0};
  int idx[SL_MAX_OPTS];
  int k = delta_list(L, idx);
  const char *dn = NULL;
  if (n) buf[0] = 0;
  put(&o, "somrando/"); put(&o, sl_generator_version); put(&o, "|"); put(&o, mode_of(L)->key); put(&o, "|"); put(&o, L->cfg.seed);
  put(&o, "|"); put(&o, sl_difficulty(L, &dn) ? dn : "-"); put(&o, "|");
  for (int i = 0; i < k; i++) {
    char v[SL_TEXT];
    sl_value_string(L, idx[i], v, sizeof v);
    if (i) put(&o, ";");
    put(&o, sl_opt(L, idx[i])->key); put(&o, "="); put(&o, v);
  }
  return finish(&o);
}

void sl_config_key(const struct sl_launcher *L, char out[17]) {
  char ks[1024];
  uint8_t d[32];
  struct sha s;
  if (sl_config_keystring(L, ks, sizeof ks) < 0) { strcpy(out, "0000000000000000"); return; }
  sha_init(&s); sha_update(&s, (const uint8_t *)ks, strlen(ks)); sha_final(&s, d);
  for (int i = 0; i < 8; i++) snprintf(out + 2 * i, 3, "%02x", d[i]);
}

int sl_config_json(const struct sl_launcher *L, char *buf, size_t n) {
  struct out o = {buf, n, 0, 0};
  int idx[SL_MAX_OPTS];
  int k = delta_list(L, idx);
  const char *dn = NULL;
  char key[17];
  sl_config_key(L, key);
  if (n) buf[0] = 0;
  put(&o, "{\"difficulty\":");
  if (sl_difficulty(L, &dn)) put_json_str(&o, dn); else put(&o, "null");
  put(&o, ",\"generator\":"); put_json_str(&o, sl_generator_version);
  put(&o, ",\"key\":"); put_json_str(&o, key);
  put(&o, ",\"mode\":"); put_json_str(&o, mode_of(L)->key);
  put(&o, ",\"options\":{");
  for (int i = 0; i < k; i++) {
    char v[SL_TEXT];
    sl_value_string(L, idx[i], v, sizeof v);
    if (i) put(&o, ",");
    put_json_str(&o, sl_opt(L, idx[i])->key); put(&o, ":"); put_json_str(&o, v);
  }
  put(&o, "},\"preset\":");
  put_json_str(&o, L->cfg.preset >= 0 ? sl_presets[L->cfg.preset].id : "custom");
  put(&o, ",\"schema\":1,\"seed\":"); put_json_str(&o, L->cfg.seed);
  put(&o, "}");
  return finish(&o);
}

int sl_config_cli(const struct sl_launcher *L, char *buf, size_t n) {
  struct out o = {buf, n, 0, 0};
  int idx[SL_MAX_OPTS];
  int k = delta_list(L, idx);
  const char *dn = NULL;
  if (n) buf[0] = 0;
  put(&o, "--mode "); put(&o, mode_of(L)->key); put(&o, " --seed "); put(&o, L->cfg.seed);
  if (sl_difficulty(L, &dn)) { put(&o, " --difficulty "); put(&o, dn); }
  for (int i = 0; i < k; i++) {
    char v[SL_TEXT];
    sl_value_string(L, idx[i], v, sizeof v);
    put(&o, " --option "); put(&o, sl_opt(L, idx[i])->key); put(&o, "="); put(&o, v);
  }
  return finish(&o);
}

int sl_config_lines(const struct sl_launcher *L, char *buf, size_t n) {
  struct out o = {buf, n, 0, 0};
  int idx[SL_MAX_OPTS];
  int k = delta_list(L, idx);
  if (n) buf[0] = 0;
  for (int i = 0; i < k; i++) {
    char v[SL_TEXT];
    sl_value_string(L, idx[i], v, sizeof v);
    put(&o, sl_opt(L, idx[i])->key); put(&o, "="); put(&o, v); put(&o, "\n");
  }
  return finish(&o);
}

/* ------------------------------------------------------------------ rows */
static void add_row(struct sl_launcher *L, int kind, int a) {
  if (L->nrows < SL_MAX_ROWS) { L->rows[L->nrows].kind = (uint8_t)kind; L->rows[L->nrows].a = (int16_t)a; L->nrows++; }
}

static void add_option_groups(struct sl_launcher *L, int advanced) {
  int n = sl_nopts(L);
  const char *seen[24];
  int ns = 0;
  for (int i = 0; i < n; i++) {
    const struct sl_option *o = sl_opt(L, i);
    if ((o->advanced != 0) != (advanced != 0) || !sl_opt_visible(L, i)) continue;
    int g = 0;
    for (; g < ns; g++) if (!strcmp(seen[g], o->group)) break;
    if (g < ns || ns >= 24) continue;
    seen[ns++] = o->group;
    add_row(L, SR_HEADER, i);
    for (int j = i; j < n; j++) {
      const struct sl_option *p = sl_opt(L, j);
      if (!strcmp(p->group, o->group) && (p->advanced != 0) == (advanced != 0) && sl_opt_visible(L, j)) add_row(L, SR_OPT, j);
    }
  }
}

static void build_rows(struct sl_launcher *L) {
  L->nrows = 0;
  if (L->screen == SL_PRESETS) {
    add_row(L, SR_PRESET, -1);
    int last = -1;
    for (int p = 0; p < sl_npresets; p++) {
      if (sl_presets[p].mode != last) { last = sl_presets[p].mode; add_row(L, SR_HEADER, 1000 + last); }
      add_row(L, SR_PRESET, p);
    }
  } else if (L->screen == SL_TWEAK) {
    add_row(L, SR_ACTION, SA_GENERATE);
    add_row(L, SR_ACTION, SA_SEED);
    add_row(L, SR_ACTION, SA_RANDOM_ALL);
    add_row(L, SR_ACTION, SA_RESET);
    if (mode_of(L)->ndiff) add_row(L, SR_DIFF, 0);
    add_option_groups(L, 0);
    add_row(L, SR_TOGGLE_ADV, 0);
    if (L->show_adv) add_option_groups(L, 1);
  } else if (L->screen == SL_MULTI) {
    const struct sl_option *o = sl_opt(L, L->multi_opt);
    for (int k = 0; k < o->nvalues; k++) add_row(L, SR_MULTI_ITEM, k);
  }
  if (L->cursor >= L->nrows) L->cursor = L->nrows ? L->nrows - 1 : 0;
  while (L->cursor < L->nrows - 1 && L->rows[L->cursor].kind == SR_HEADER) L->cursor++;
}

static void ensure_visible(struct sl_launcher *L) {
  int vis = L->vis_rows > 0 ? L->vis_rows : 8;
  if (L->cursor < L->scroll) {
    L->scroll = L->cursor;
    if (L->scroll > 0 && L->rows[L->scroll - 1].kind == SR_HEADER) L->scroll--;
  }
  if (L->cursor >= L->scroll + vis) L->scroll = L->cursor - vis + 1;
  int max = L->nrows > vis ? L->nrows - vis : 0;
  if (L->scroll > max) L->scroll = max;
  if (L->scroll < 0) L->scroll = 0;
}

static void goto_screen(struct sl_launcher *L, enum sl_screen s, int keep_cursor) {
  L->screen = s;
  L->focus = SF_LIST; L->del_confirm = 0;
  if (!keep_cursor) { L->cursor = s == SL_PRESETS ? L->preset_row : 0; L->scroll = 0; }
  build_rows(L);
  ensure_visible(L);
  touch(L);
}

static int is_list(const struct sl_launcher *L) { return L->screen == SL_PRESETS || L->screen == SL_TWEAK || L->screen == SL_MULTI; }

int sl_focus_has(const struct sl_launcher *L, int f) {
  if (f == SF_LIST) return 1;
  if (is_list(L)) return f == SF_BACK || f == SF_UP || f == SF_DOWN || (f == SF_GO && L->screen == SL_TWEAK);
  if (L->screen == SL_SAVED) return f == SF_BACK || (f == SF_DEL && L->cursor < SL_SAVED_MAX && L->saved[L->cursor].used);
  if (L->screen == SL_CREDITS) return f == SF_BACK || f == SF_UP || f == SF_DOWN;
  return 0;
}

/* One step over the selectable rows without wrapping; returns 0 at the end of the list. */
static int step_cursor(struct sl_launcher *L, int dir) {
  int c = L->cursor;
  do { c += dir; } while (c >= 0 && c < L->nrows && L->rows[c].kind == SR_HEADER);
  if (c < 0 || c >= L->nrows) return 0;
  L->cursor = c;
  ensure_visible(L);
  touch(L);
  return 1;
}

/* Up and down walk a ring: the rows, then the footer buttons, then the first row again. */
static void move_cursor(struct sl_launcher *L, int dir) {
  if (L->focus != SF_LIST) {                      /* leave the footer: down wraps to the first row, up goes back to the row we left */
    L->focus = SF_LIST;
    if (dir > 0) {
      L->cursor = -1; L->scroll = 0;
      if (!step_cursor(L, 1)) L->cursor = 0;
    }
    ensure_visible(L); touch(L);
    return;
  }
  if (step_cursor(L, dir)) return;
  L->focus = SF_BACK;                             /* past either end of the list: the footer */
  if (dir < 0 && sl_focus_has(L, SF_GO)) L->focus = SF_GO;
  touch(L);
}

static void page_cursor(struct sl_launcher *L, int dir) {
  int n = L->vis_rows > 1 ? L->vis_rows - 1 : 4;
  L->focus = SF_LIST;
  for (int i = 0; i < n; i++) if (!step_cursor(L, dir)) break;
  touch(L);
}

static void focus_step(struct sl_launcher *L, int dir) {          /* left / right inside the footer */
  static const int ring[4] = {SF_BACK, SF_GO, SF_UP, SF_DOWN};
  int at = 0;
  for (int i = 0; i < 4; i++) if (ring[i] == L->focus) at = i;
  for (int k = 1; k <= 4; k++) {
    int f = ring[((at + dir * k) % 4 + 4) % 4];
    if (sl_focus_has(L, f)) { L->focus = f; break; }
  }
  touch(L);
}

const char *sl_screen_name(int s) {
  static const char *const n[SL_SCREEN_COUNT] = {"MAIN", "PRESETS", "TWEAK", "MULTI", "SEED", "GENERATE", "PLAY", "SAVED", "CREDITS"};
  return s >= 0 && s < SL_SCREEN_COUNT ? n[s] : "?";
}

void sl_row_texts(const struct sl_launcher *L, const struct sl_row *r, char *label, size_t ln, char *value, size_t vn) {
  if (ln) label[0] = 0;
  if (vn) value[0] = 0;
  switch (r->kind) {
    case SR_HEADER:
      cpy(label, ln, r->a >= 1000 ? sl_modes[r->a - 1000].name : sl_opt(L, r->a)->group);
      break;
    case SR_ACTION:
      switch (r->a) {
        case SA_GENERATE: cpy(label, ln, "Generate and play"); break;
        case SA_SEED: cpy(label, ln, "Seed"); cpy(value, vn, L->cfg.seed); break;
        case SA_RANDOM_ALL: cpy(label, ln, "Randomize all"); break;
        case SA_RESET: cpy(label, ln, "Reset to preset"); break;
        default: break;
      }
      break;
    case SR_DIFF: {
      const char *d = NULL;
      cpy(label, ln, "Difficulty");
      if (sl_difficulty(L, &d)) cpy(value, vn, sl_diff_label(d));
      break;
    }
    case SR_OPT:
      cpy(label, ln, sl_opt(L, r->a)->label);
      sl_value_text(L, r->a, value, vn);
      break;
    case SR_TOGGLE_ADV: cpy(label, ln, L->show_adv ? "Fewer options" : "More options"); break;
    case SR_PRESET:
      cpy(label, ln, r->a < 0 ? "Surprise me" : sl_presets[r->a].name);
      if (r->a >= 0 && L->preset_ok[r->a] == 0) cpy(value, vn, "PC only");
      break;
    case SR_MULTI_ITEM: {
      const struct sl_option *o = sl_opt(L, L->multi_opt);
      cpy(label, ln, o->values[r->a].label);
      cpy(value, vn, (L->cfg.val[L->multi_opt] >> r->a) & 1 ? "Yes" : "No");
      break;
    }
  }
}

/* ------------------------------------------------------------------ editing */
static void step_option(struct sl_launcher *L, int i, int dir, int activate) {
  const struct sl_option *o = sl_opt(L, i);
  int32_t *v = &L->cfg.val[i];
  switch (o->type) {
    case SL_O_BOOL: *v = !*v; break;
    case SL_O_ENUM: *v = (*v + dir + o->nvalues) % o->nvalues; break;
    case SL_O_NUM: {
      int32_t t = *v + dir * o->step;
      *v = t < o->min ? o->min : (t > o->max ? o->max : t);
      break;
    }
    case SL_O_MULTI:
      if (activate) { L->ret_cursor = L->cursor; L->ret_scroll = L->scroll; L->multi_opt = i; L->back_to = SL_TWEAK; L->screen = SL_MULTI; L->cursor = 0; L->scroll = 0; }
      break;
  }
  apply_constraints(L);
  build_rows(L);
  if (L->screen == SL_MULTI) { ensure_visible(L); }
  touch(L);
}

static void request_generate(struct sl_launcher *L) {
  L->gen = SL_GEN_ASK; L->gen_step = L->gen_total = 0; L->gen_name[0] = L->gen_msg[0] = 0;
  L->screen = SL_GENERATE;
  L->act = SL_ACT_GENERATE; L->act_pending = 1;
  touch(L);
}

static void notice(struct sl_launcher *L, const char *t) { cpy(L->notice, sizeof L->notice, t); L->notice_t = 90; touch(L); }

static void surprise(struct sl_launcher *L) {
  sl_apply_preset(L, (int)rnd(L, (uint32_t)sl_npresets));
  sl_randomize_all(L);
  sl_random_seed(L);
  goto_screen(L, SL_TWEAK, 0);
  notice(L, "Surprise");
}

static void activate_row(struct sl_launcher *L, int delta) {
  if (L->cursor >= L->nrows) return;
  const struct sl_row r = L->rows[L->cursor];
  if (L->screen == SL_PRESETS) {
    if (delta < 0) return;
    L->preset_row = L->cursor;
    if (r.a < 0) surprise(L);
    else { sl_apply_preset(L, r.a); goto_screen(L, SL_TWEAK, 0); }
    return;
  }
  if (L->screen == SL_MULTI) {
    if (r.kind != SR_MULTI_ITEM) return;
    int32_t m = L->cfg.val[L->multi_opt] ^ (1 << r.a);
    if (m) L->cfg.val[L->multi_opt] = m; else notice(L, "Keep at least one");
    touch(L);
    return;
  }
  switch (r.kind) {
    case SR_ACTION:
      if (delta < 0) return;
      if (r.a == SA_GENERATE) request_generate(L);
      else if (r.a == SA_SEED) { L->back_to = SL_TWEAK; L->seed_pos = SL_SEED_DIGITS - 1; L->screen = SL_SEED; touch(L); }
      else if (r.a == SA_RANDOM_ALL) { sl_randomize_all(L); build_rows(L); ensure_visible(L); notice(L, "Randomized"); }
      else if (r.a == SA_RESET) { sl_apply_preset(L, L->cfg.preset >= 0 ? L->cfg.preset : 0); build_rows(L); ensure_visible(L); notice(L, "Reset"); }
      break;
    case SR_DIFF: {
      const struct sl_mode *m = mode_of(L);
      L->cfg.diff = (L->cfg.diff + (delta < 0 ? -1 : 1) + m->ndiff) % m->ndiff;
      touch(L);
      break;
    }
    case SR_OPT: step_option(L, r.a, delta < 0 ? -1 : 1, delta >= 0); break;
    case SR_TOGGLE_ADV:
      if (delta < 0) return;
      L->show_adv = !L->show_adv;
      build_rows(L);
      ensure_visible(L);
      touch(L);
      break;
    default: break;
  }
}

/* ------------------------------------------------------------------ metrics and layout */
void sl_metrics_for(int w, int h, struct sl_metrics *m) {
  int mn = w < h ? w : h;
  m->s = (mn + 125) / 250;
  if (m->s < 1) m->s = 1;
  m->W = w / m->s; m->H = h / m->s;
  m->col_w = m->W - 8 < 360 ? m->W - 8 : 360;
  m->col_x = (m->W - m->col_w) / 2;
  m->head_h = 30; m->foot_h = 20; m->row_h = 20;
  m->rows = (m->H - m->head_h - m->foot_h - 28) / m->row_h;
  if (m->rows < 3) m->rows = 3;
}

void sl_set_view(struct sl_launcher *L, int w, int h) {
  struct sl_metrics m;
  int cv;
  sl_metrics_for(w, h, &m);
  cv = (m.H - m.head_h - m.foot_h - 8) / 11;
  if (cv < 3) cv = 3;
  if (cv != L->cred_vis) { L->cred_vis = cv; touch(L); }
  if (m.rows != L->vis_rows) { L->vis_rows = m.rows; ensure_visible(L); touch(L); }
}

/* ------------------------------------------------------------------ extras and credits */
void sl_set_extras(struct sl_launcher *L, unsigned flags, const char *credits_text) {
  L->extras = flags & (SL_X_TWEAKS | SL_X_DISPLAY | SL_X_CREDITS);
  if (credits_text) L->credits = credits_text;
  if (L->cursor >= sl_main_count(L) && L->screen == SL_MAIN) L->cursor = 0;
  touch(L);
}
void sl_set_tweaks_status(struct sl_launcher *L, const char *text) { cpy(L->status_tweaks, sizeof L->status_tweaks, text); touch(L); }
int sl_main_count(const struct sl_launcher *L) {
  return 3 + ((L->extras & SL_X_TWEAKS) != 0) + ((L->extras & SL_X_DISPLAY) != 0) + ((L->extras & SL_X_CREDITS) != 0);
}
int sl_main_kind(const struct sl_launcher *L, int i) {
  int k = 3;
  if (i == 0) return SH_MAIN_ORIGINAL;
  if (i == 1) return SH_MAIN_RANDOM;
  if (i == 2) return SH_MAIN_SAVED;
  if (L->extras & SL_X_TWEAKS) { if (i == k) return SH_MAIN_TWEAKS; k++; }
  if (L->extras & SL_X_DISPLAY) { if (i == k) return SH_MAIN_DISPLAY; k++; }
  if ((L->extras & SL_X_CREDITS) && i == k) return SH_MAIN_CREDITS;
  return SH_MAIN_ORIGINAL;
}
/* Splits the credits text into lines of at most SL_CREDITS_COLS characters, at spaces when it can.  Returns `want` after copying that line
 * into out, or -(line count) - 1 when the text ends first. */
static int credits_walk(const char *s, int want, char *out, size_t n) {
  int line = 0;
  while (s && *s) {
    const char *e = strchr(s, '\n');
    int len = e ? (int)(e - s) : (int)strlen(s), done = 0;
    if (len == 0) { if (line == want && out) out[0] = 0; if (line == want) return line; line++; }
    while (done < len) {
      int rest = len - done, cut = rest > SL_CREDITS_COLS ? SL_CREDITS_COLS : rest;
      if (rest > SL_CREDITS_COLS) { int k = cut; while (k > 0 && s[done + k] != ' ') k--; if (k > 0) cut = k; }
      if (line == want && out) { size_t c = (size_t)cut < n - 1 ? (size_t)cut : n - 1; memcpy(out, s + done, c); out[c] = 0; return line; }
      line++;
      done += cut;
      while (done < len && s[done] == ' ') done++;
    }
    s += len;
    if (*s == '\n') s++;
  }
  return -line - 1;
}
int sl_credits_count(const struct sl_launcher *L) { return -credits_walk(L->credits, -1, NULL, 0) - 1; }
int sl_credits_line(const struct sl_launcher *L, int i, char *out, size_t n) {
  if (i < 0 || n < 2) return 0;
  return credits_walk(L->credits, i, out, n) == i;
}

static int add_hit(struct sl_hit *hits, int n, int max, int x, int y, int w, int h, int kind, int a) {
  if (n < max) { hits[n].r.x = x; hits[n].r.y = y; hits[n].r.w = w; hits[n].r.h = h; hits[n].kind = kind; hits[n].a = a; }
  return n + 1;
}

int sl_layout(const struct sl_launcher *L, const struct sl_metrics *m, struct sl_hit *hits, int max) {
  int n = 0, fy = m->H - m->foot_h + 2, fh = m->foot_h - 4;
  switch (L->screen) {
    case SL_MAIN: {
      int bw = m->col_w < 280 ? m->col_w : 280, bx = m->col_x + (m->col_w - bw) / 2, y = m->head_h + 10, gap = 8;
      int cnt = sl_main_count(L), bh = (m->H - m->head_h - 10 - m->foot_h - 6 - (cnt - 1) * gap) / cnt;
      if (bh > 50) bh = 50;
      for (int i = 0; i < cnt; i++) n = add_hit(hits, n, max, bx, y + i * (bh + gap), bw, bh, sl_main_kind(L, i), 0);
      break;
    }
    case SL_CREDITS:
      n = add_hit(hits, n, max, m->col_x, fy, 44, fh, SH_BACK, 0);
      n = add_hit(hits, n, max, m->col_x + m->col_w - 52, fy, 24, fh, SH_SCROLL_UP, 0);
      n = add_hit(hits, n, max, m->col_x + m->col_w - 26, fy, 24, fh, SH_SCROLL_DOWN, 0);
      break;
    case SL_SAVED: {
      int y = m->head_h + 4, gap = 3, avail = m->H - m->head_h - m->foot_h - 8, sh = (avail - (SL_SAVED_MAX - 1) * gap) / SL_SAVED_MAX;
      if (sh > 38) sh = 38;
      for (int i = 0; i < SL_SAVED_MAX; i++) {
        int yy = y + i * (sh + gap);
        n = add_hit(hits, n, max, m->col_x, yy, m->col_w - 56, sh, SH_SAVED_SLOT, i);
        if (L->saved[i].used) n = add_hit(hits, n, max, m->col_x + m->col_w - 54, yy, 54, sh, SH_SAVED_DEL, i);
      }
      n = add_hit(hits, n, max, m->col_x, fy, 44, fh, SH_BACK, 0);
      break;
    }
    case SL_PRESETS: case SL_TWEAK: case SL_MULTI: {
      int rows = L->vis_rows > 0 ? L->vis_rows : m->rows;
      for (int i = 0; i < rows && L->scroll + i < L->nrows; i++) {
        int ri = L->scroll + i, y = m->head_h + i * m->row_h;
        const struct sl_row *r = &L->rows[ri];
        if (r->kind == SR_HEADER) continue;
        if (L->screen == SL_TWEAK && (r->kind == SR_OPT || r->kind == SR_DIFF)) {
          const struct sl_option *o = r->kind == SR_OPT ? sl_opt(L, r->a) : NULL;
          if (!o || o->type != SL_O_MULTI) {
            n = add_hit(hits, n, max, m->col_x + m->col_w - 52, y, 26, m->row_h - 2, SH_ROW_DEC, ri);
            n = add_hit(hits, n, max, m->col_x + m->col_w - 26, y, 26, m->row_h - 2, SH_ROW_INC, ri);
          }
        }
        n = add_hit(hits, n, max, m->col_x, y, m->col_w, m->row_h - 2, SH_ROW, ri);
      }
      n = add_hit(hits, n, max, m->col_x, fy, 44, fh, SH_BACK, 0);
      if (L->screen == SL_TWEAK) n = add_hit(hits, n, max, m->col_x + m->col_w - 98, fy, 40, fh, SH_GO, 0);
      n = add_hit(hits, n, max, m->col_x + m->col_w - 52, fy, 24, fh, SH_SCROLL_UP, 0);
      n = add_hit(hits, n, max, m->col_x + m->col_w - 26, fy, 24, fh, SH_SCROLL_DOWN, 0);
      break;
    }
    case SL_SEED: {
      int bw = 26, gap = 4, total = SL_SEED_DIGITS * bw + (SL_SEED_DIGITS - 1) * gap, x0 = m->col_x + (m->col_w - total) / 2, y0 = m->head_h + 8;
      for (int i = 0; i < SL_SEED_DIGITS; i++) n = add_hit(hits, n, max, x0 + i * (bw + gap), y0, bw, 30, SH_SEED_DIGIT, i);
      int kw = 52, kh = 22, kg = 4, kx = m->col_x + (m->col_w - (3 * kw + 2 * kg)) / 2, ky = y0 + 30 + 10;
      static const int order[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 12};
      for (int i = 0; i < 12; i++) n = add_hit(hits, n, max, kx + (i % 3) * (kw + kg), ky + (i / 3) * (kh + kg), kw, kh, SH_KEY, order[i]);
      n = add_hit(hits, n, max, kx, ky + 4 * (kh + kg), 3 * kw + 2 * kg, kh, SH_KEY, 11);
      n = add_hit(hits, n, max, m->col_x, fy, 44, fh, SH_BACK, 0);
      break;
    }
    case SL_GENERATE: {
      int bw = 120, bx = m->col_x + (m->col_w - bw) / 2, by = m->H - m->foot_h - 40;
      if (L->gen == SL_GEN_RUNNING || L->gen == SL_GEN_ASK) n = add_hit(hits, n, max, bx, by, bw, 28, SH_CANCEL, 0);
      else {
        if (L->gen == SL_GEN_ERROR) n = add_hit(hits, n, max, m->col_x + m->col_w / 2 - bw - 6, by, bw, 28, SH_RETRY, 0);
        n = add_hit(hits, n, max, L->gen == SL_GEN_ERROR ? m->col_x + m->col_w / 2 + 6 : bx, by, bw, 28, SH_BACK, 0);
      }
      break;
    }
    default: break;
  }
  return n < max ? n : max;
}

/* ------------------------------------------------------------------ input */
void sl_init(struct sl_launcher *L, uint64_t entropy) {
  memset(L, 0, sizeof *L);
  memset(L->preset_ok, -1, sizeof L->preset_ok); L->gen_ok = -1;
  sl_seed_rng(L, entropy);
  L->screen = SL_MAIN;
  sl_apply_preset(L, 0);
  sl_random_seed(L);
  cpy(L->status_original, sizeof L->status_original, "");
  cpy(L->status_generator, sizeof L->status_generator, "");
  L->rev = 1;
}

void sl_show(struct sl_launcher *L, enum sl_screen s) { goto_screen(L, s, 0); }

void sl_set_status(struct sl_launcher *L, const char *o, const char *g) {
  cpy(L->status_original, sizeof L->status_original, o);
  cpy(L->status_generator, sizeof L->status_generator, g);
  touch(L);
}

/* ------------------------------------------------------------------ generator support */
void sl_set_preset_ok(struct sl_launcher *L, int preset, int ok) {
  if (preset < 0 || preset >= SL_MAX_PRESETS || L->preset_ok[preset] == ok) return;
  L->preset_ok[preset] = (int8_t)ok; touch(L);
}
void sl_set_gen_support(struct sl_launcher *L, int ok, const char *why) {
  if (L->gen_ok == ok && !strcmp(L->gen_why, why ? why : "")) return;
  L->gen_ok = (int8_t)ok; cpy(L->gen_why, sizeof L->gen_why, why); touch(L);
}

/* ------------------------------------------------------------------ saved games */
void sl_saved_clear(struct sl_launcher *L) { memset(L->saved, 0, sizeof L->saved); L->saved_hidden = 0; L->del_confirm = 0; touch(L); }

int sl_saved_add(struct sl_launcher *L, const char *key, const char *preset, const char *mode, const char *seed, const char *diff,
                 const char *date, const char *path, const char *lines) {
  for (int i = 0; i < SL_SAVED_MAX; i++) {
    struct sl_saved *v = &L->saved[i];
    if (v->used) continue;
    v->used = 1;
    cpy(v->key, sizeof v->key, key); cpy(v->preset, sizeof v->preset, preset); cpy(v->mode, sizeof v->mode, mode);
    cpy(v->seed, sizeof v->seed, seed); cpy(v->diff, sizeof v->diff, diff); cpy(v->date, sizeof v->date, date);
    cpy(v->path, sizeof v->path, path); cpy(v->lines, sizeof v->lines, lines);
    touch(L);
    return i;
  }
  return -1;
}

void sl_saved_set_hidden(struct sl_launcher *L, int n) { L->saved_hidden = n < 0 ? 0 : n; touch(L); }

int sl_saved_count(const struct sl_launcher *L) {
  int k = 0;
  for (int i = 0; i < SL_SAVED_MAX; i++) k += L->saved[i].used;
  return k;
}

int sl_load_config(struct sl_launcher *L, const char *preset, const char *mode, const char *seed, const char *diff, const char *lines) {
  int mi = -1;
  for (int i = 0; i < sl_nmodes; i++) if (mode && !strcmp(sl_modes[i].key, mode)) mi = i;
  if (mi < 0) return -1;
  int pi = -1;
  for (int i = 0; i < sl_npresets; i++) if (preset && preset[0] && !strcmp(sl_presets[i].id, preset) && sl_presets[i].mode == mi) pi = i;
  L->cfg.mode = mi; L->cfg.preset = pi;
  set_defaults(L);
  const struct sl_mode *m = &sl_modes[mi];
  L->cfg.diff = !m->ndiff ? -1 : m->def_diff;
  if (m->ndiff && diff && diff[0]) for (int d = 0; d < m->ndiff; d++) if (!strcmp(m->diff[d], diff)) L->cfg.diff = d;
  if (seed && seed[0]) sl_set_seed(L, seed);
  for (const char *q = lines ? lines : ""; *q;) {
    char key[SL_TEXT], val[SL_TEXT];
    const char *e = strchr(q, '\n'), *eq = strchr(q, '=');
    size_t len = e ? (size_t)(e - q) : strlen(q);
    if (eq && eq < q + len) {
      size_t kl = (size_t)(eq - q), vl = len - kl - 1;
      if (kl < sizeof key && vl < sizeof val) { memcpy(key, q, kl); key[kl] = 0; memcpy(val, eq + 1, vl); val[vl] = 0; sl_set_option(L, key, val); }
    }
    q += len; if (*q == '\n') q++;
  }
  apply_constraints(L);
  touch(L);
  return 0;
}

int sl_act_arg(const struct sl_launcher *L) { return L->act_arg; }

const char *sl_pad_hint(const struct sl_launcher *L) {
  switch (L->screen) {
    case SL_MAIN: return "D-PAD choose   A open   B quit";
    case SL_PRESETS: return "D-PAD move   A choose   START choose   L1 R1 page   B back";
    case SL_TWEAK: return "D-PAD move   LEFT RIGHT change   A select   X random   SELECT reset   L1 R1 page   START generate   B back";
    case SL_MULTI: return "D-PAD move   A toggle   L1 R1 page   B back";
    case SL_SEED: return "LEFT RIGHT digit   UP DOWN change   X random   Y delete   A done";
    case SL_GENERATE: return L->gen == SL_GEN_ERROR ? "A retry   B back" : "B cancel";
    case SL_SAVED: return "D-PAD move   A play   RIGHT then A or X delete   B back";
    case SL_CREDITS: return "UP DOWN scroll   L1 R1 page   B back";
    default: return "";
  }
}

int sl_poll(struct sl_launcher *L, int *action) {
  if (!L->act_pending) return 0;
  *action = L->act;
  L->act_pending = 0;
  return 1;
}

void sl_tick(struct sl_launcher *L) {
  if (L->notice_t > 0 && --L->notice_t == 0) { L->notice[0] = 0; L->del_confirm = 0; touch(L); }
}

static void post(struct sl_launcher *L, int act) { L->act = act; L->act_pending = 1; }

static void go_back(struct sl_launcher *L) {
  switch (L->screen) {
    case SL_MAIN: post(L, SL_ACT_QUIT); break;
    case SL_PRESETS: goto_screen(L, SL_MAIN, 0); L->cursor = 1; break;       /* the cursor returns to the entry we came from */
    case SL_SAVED: goto_screen(L, SL_MAIN, 0); L->cursor = 2; break;
    case SL_CREDITS: goto_screen(L, SL_MAIN, 0); for (int k = 0; k < sl_main_count(L); k++) if (sl_main_kind(L, k) == SH_MAIN_CREDITS) L->cursor = k; break;
    case SL_TWEAK: goto_screen(L, SL_PRESETS, 0); break;
    case SL_MULTI: L->cursor = L->ret_cursor; L->scroll = L->ret_scroll; goto_screen(L, SL_TWEAK, 1); break;
    case SL_SEED: goto_screen(L, SL_TWEAK, 1); break;
    case SL_GENERATE:
      if (L->gen == SL_GEN_RUNNING || L->gen == SL_GEN_ASK) { post(L, SL_ACT_CANCEL); notice(L, "Cancelled"); }
      goto_screen(L, SL_TWEAK, 1);
      break;
    default: break;
  }
}

static void main_activate(struct sl_launcher *L, int kind) {
  switch (kind) {
    case SH_MAIN_ORIGINAL: post(L, SL_ACT_PLAY_ORIGINAL); break;
    case SH_MAIN_RANDOM: goto_screen(L, SL_PRESETS, 0); break;
    case SH_MAIN_SAVED: goto_screen(L, SL_SAVED, 0); break;
    case SH_MAIN_TWEAKS: post(L, SL_ACT_TWEAKS); break;
    case SH_MAIN_DISPLAY: post(L, SL_ACT_DISPLAY); break;
    case SH_MAIN_CREDITS: goto_screen(L, SL_CREDITS, 0); L->cred_scroll = 0; break;
    default: break;
  }
}

static void seed_digit_step(struct sl_launcher *L, int dir) {
  int k = SL_SEED_DIGITS - 1 - L->seed_pos, p = pow10i(k);
  long v = seed_val(L);
  int d = (int)((v / p) % 10), nd = (d + dir + 10) % 10;
  seed_put(L, v + (long)(nd - d) * p);
}
static void seed_type(struct sl_launcher *L, int d) {
  long v = seed_val(L);
  if (v < 10000000L) seed_put(L, v * 10 + d);
}

static void do_reset(struct sl_launcher *L) { sl_apply_preset(L, L->cfg.preset >= 0 ? L->cfg.preset : 0); build_rows(L); ensure_visible(L); notice(L, "Reset"); }
static void do_random_all(struct sl_launcher *L) { sl_randomize_all(L); build_rows(L); ensure_visible(L); notice(L, "Randomized"); }

static void saved_play(struct sl_launcher *L, int i) {
  const struct sl_saved *v = &L->saved[i];
  if (!v->used) { notice(L, "Empty slot"); return; }
  sl_load_config(L, v->preset, v->mode, v->seed, v->diff, v->lines);
  sl_gen_done(L, v->path);
}

static void saved_delete(struct sl_launcher *L, int i) {
  if (!L->saved[i].used) { notice(L, "Empty slot"); return; }
  if (L->del_confirm != i + 1) { L->del_confirm = i + 1; notice(L, "Press again to delete"); return; }
  L->del_confirm = 0;
  L->act_arg = i;
  post(L, SL_ACT_DELETE_SAVED);
  touch(L);
}

/* Activates the focused footer button. */
static void footer_activate(struct sl_launcher *L) {
  switch (L->focus) {
    case SF_BACK: go_back(L); break;
    case SF_GO: request_generate(L); break;
    case SF_UP: page_cursor(L, -1); L->focus = SF_UP; break;
    case SF_DOWN: page_cursor(L, 1); L->focus = SF_DOWN; break;
    default: break;
  }
}

void sl_key(struct sl_launcher *L, int b) {
  if (b != SL_X && b != SL_A && L->del_confirm) L->del_confirm = 0;
  switch (L->screen) {
    case SL_MAIN:
      if (b == SL_UP) { int cnt = sl_main_count(L); L->cursor = (L->cursor + cnt - 1) % cnt; touch(L); }
      else if (b == SL_DOWN) { L->cursor = (L->cursor + 1) % sl_main_count(L); touch(L); }
      else if (b == SL_A || b == SL_START) main_activate(L, sl_main_kind(L, L->cursor));
      else if (b == SL_B) go_back(L);
      break;
    case SL_SAVED:
      if (b == SL_UP || b == SL_DOWN) {                   /* the four slots, then BACK, then the first slot again */
        int n = SL_SAVED_MAX + 1, c = L->focus == SF_BACK ? SL_SAVED_MAX : L->cursor;
        c = (c + (b == SL_UP ? n - 1 : 1)) % n;
        L->focus = c == SL_SAVED_MAX ? SF_BACK : SF_LIST;
        if (c < SL_SAVED_MAX) L->cursor = c;
        touch(L);
      }
      else if (b == SL_RIGHT && sl_focus_has(L, SF_DEL) && L->focus == SF_LIST) { L->focus = SF_DEL; touch(L); }
      else if (b == SL_LEFT && L->focus == SF_DEL) { L->focus = SF_LIST; touch(L); }
      else if (b == SL_A) {
        if (L->focus == SF_BACK) go_back(L);
        else if (L->focus == SF_DEL) saved_delete(L, L->cursor);
        else saved_play(L, L->cursor);
      }
      else if (b == SL_X || b == SL_Y || b == SL_SELECT) { if (L->focus != SF_BACK) saved_delete(L, L->cursor); }
      else if (b == SL_START) { if (L->focus != SF_BACK) saved_play(L, L->cursor); }
      else if (b == SL_B) go_back(L);
      break;
    case SL_PRESETS: case SL_TWEAK: case SL_MULTI:
      if (b == SL_B) { go_back(L); break; }
      if (b == SL_UP) move_cursor(L, -1);
      else if (b == SL_DOWN) move_cursor(L, 1);
      else if (b == SL_L) page_cursor(L, -1);
      else if (b == SL_R) page_cursor(L, 1);
      else if (L->focus != SF_LIST) {                     /* a footer button has the focus */
        if (b == SL_LEFT) focus_step(L, -1);
        else if (b == SL_RIGHT) focus_step(L, 1);
        else if (b == SL_A) footer_activate(L);
        else if (b == SL_START && L->screen == SL_TWEAK) request_generate(L);
        else if (b == SL_SELECT && L->screen == SL_TWEAK) do_reset(L);
        else if (b == SL_X && L->screen == SL_TWEAK) do_random_all(L);
        else if (b == SL_Y && L->screen == SL_TWEAK) do_reset(L);
      }
      else if (b == SL_A) activate_row(L, 1);
      else if (b == SL_LEFT && L->screen == SL_TWEAK) activate_row(L, -1);
      else if (b == SL_RIGHT && L->screen == SL_TWEAK) activate_row(L, 1);
      else if (b == SL_X && L->screen == SL_TWEAK) do_random_all(L);
      else if (b == SL_Y && L->screen == SL_TWEAK) do_reset(L);
      else if (b == SL_SELECT && L->screen == SL_TWEAK) do_reset(L);
      else if (b == SL_START && L->screen == SL_TWEAK) request_generate(L);
      else if (b == SL_START && L->screen == SL_PRESETS) activate_row(L, 1);
      break;
    case SL_SEED:
      if (b == SL_LEFT && L->seed_pos > 0) { L->seed_pos--; touch(L); }
      else if (b == SL_RIGHT && L->seed_pos < SL_SEED_DIGITS - 1) { L->seed_pos++; touch(L); }
      else if (b == SL_UP) seed_digit_step(L, 1);
      else if (b == SL_DOWN) seed_digit_step(L, -1);
      else if (b == SL_X || b == SL_SELECT) sl_random_seed(L);
      else if (b == SL_Y) seed_put(L, seed_val(L) / 10);
      else if (b == SL_L) { if (L->seed_pos > 0) L->seed_pos = 0; else L->seed_pos = SL_SEED_DIGITS - 1; touch(L); }
      else if (b == SL_R) { if (L->seed_pos < SL_SEED_DIGITS - 1) L->seed_pos = SL_SEED_DIGITS - 1; else L->seed_pos = 0; touch(L); }
      else if (b == SL_A || b == SL_START || b == SL_B) go_back(L);
      break;
    case SL_GENERATE:
      if (b == SL_B) go_back(L);
      else if ((b == SL_A || b == SL_START) && L->gen == SL_GEN_ERROR) request_generate(L);
      break;
    case SL_CREDITS:
      if (b == SL_B || b == SL_A || b == SL_START) go_back(L);
      else if (b == SL_UP) sl_scroll(L, -1);
      else if (b == SL_DOWN) sl_scroll(L, 1);
      else if (b == SL_L || b == SL_LEFT) sl_scroll(L, -(L->cred_vis > 2 ? L->cred_vis - 1 : 1));
      else if (b == SL_R || b == SL_RIGHT) sl_scroll(L, L->cred_vis > 2 ? L->cred_vis - 1 : 1);
      break;
    default: break;
  }
}

void sl_scroll(struct sl_launcher *L, int rows) {
  if (L->screen == SL_CREDITS) {
    int vis = L->cred_vis > 0 ? L->cred_vis : 8, max = sl_credits_count(L) - vis;
    L->cred_scroll += rows;
    if (L->cred_scroll > max) L->cred_scroll = max;
    if (L->cred_scroll < 0) L->cred_scroll = 0;
    touch(L);
    return;
  }
  if (L->screen != SL_PRESETS && L->screen != SL_TWEAK && L->screen != SL_MULTI) return;
  int vis = L->vis_rows > 0 ? L->vis_rows : 8, max = L->nrows > vis ? L->nrows - vis : 0;
  L->scroll += rows;
  if (L->scroll < 0) L->scroll = 0;
  if (L->scroll > max) L->scroll = max;
  touch(L);
}

void sl_tap(struct sl_launcher *L, int x, int y, int w, int h) {
  struct sl_metrics m;
  struct sl_hit hits[SL_MAX_HITS];
  sl_metrics_for(w, h, &m);
  int n = sl_layout(L, &m, hits, SL_MAX_HITS), vx = x / m.s, vy = y / m.s;
  for (int i = 0; i < n; i++) {
    const struct sl_hit *t = &hits[i];
    if (vx < t->r.x || vy < t->r.y || vx >= t->r.x + t->r.w || vy >= t->r.y + t->r.h) continue;
    L->focus = SF_LIST;
    switch (t->kind) {
      case SH_MAIN_ORIGINAL: L->cursor = 0; post(L, SL_ACT_PLAY_ORIGINAL); break;
      case SH_MAIN_RANDOM: L->cursor = 1; goto_screen(L, SL_PRESETS, 0); break;
      case SH_MAIN_SAVED: L->cursor = 2; goto_screen(L, SL_SAVED, 0); break;
      case SH_MAIN_TWEAKS: case SH_MAIN_DISPLAY: case SH_MAIN_CREDITS:
        for (int k = 0; k < sl_main_count(L); k++) if (sl_main_kind(L, k) == t->kind) L->cursor = k;
        main_activate(L, t->kind);
        break;
      case SH_SAVED_SLOT: L->cursor = t->a; L->focus = SF_LIST; saved_play(L, t->a); break;
      case SH_SAVED_DEL: L->cursor = t->a; L->focus = SF_LIST; saved_delete(L, t->a); break;
      case SH_ROW: L->focus = SF_LIST; L->cursor = t->a; activate_row(L, 1); break;
      case SH_ROW_DEC: L->cursor = t->a; activate_row(L, -1); break;
      case SH_ROW_INC: L->cursor = t->a; activate_row(L, 1); break;
      case SH_BACK: go_back(L); break;
      case SH_GO: request_generate(L); break;
      case SH_SCROLL_UP: sl_scroll(L, -(m.rows > 2 ? m.rows - 1 : 1)); break;
      case SH_SCROLL_DOWN: sl_scroll(L, m.rows > 2 ? m.rows - 1 : 1); break;
      case SH_SEED_DIGIT: L->seed_pos = t->a; touch(L); break;
      case SH_KEY:
        if (t->a <= 9) seed_type(L, t->a);
        else if (t->a == 10) seed_put(L, seed_val(L) / 10);
        else if (t->a == 11) sl_random_seed(L);
        else go_back(L);
        break;
      case SH_CANCEL: go_back(L); break;
      case SH_RETRY: request_generate(L); break;
      default: break;
    }
    return;
  }
}

/* ------------------------------------------------------------------ generation seam */
void sl_gen_cached(struct sl_launcher *L, const char *path) { sl_gen_done(L, path); }

void sl_gen_begin(struct sl_launcher *L) {
  L->screen = SL_GENERATE; L->gen = SL_GEN_RUNNING; L->gen_step = 0; L->gen_total = 0; L->gen_name[0] = 0;
  touch(L);
}

void sl_gen_progress(struct sl_launcher *L, int step, int total, const char *name) {
  L->gen = SL_GEN_RUNNING; L->gen_step = step; L->gen_total = total; cpy(L->gen_name, sizeof L->gen_name, name);
  touch(L);
}

void sl_gen_done(struct sl_launcher *L, const char *path) {
  cpy(L->rom_path, sizeof L->rom_path, path);
  L->gen = SL_GEN_DONE; L->screen = SL_PLAY;
  post(L, SL_ACT_PLAY);
  touch(L);
}

void sl_gen_fail(struct sl_launcher *L, const char *msg) {
  L->screen = SL_GENERATE; L->gen = SL_GEN_ERROR; cpy(L->gen_msg, sizeof L->gen_msg, msg);
  touch(L);
}

void sl_gen_unavailable(struct sl_launcher *L, const char *msg) {
  L->screen = SL_GENERATE; L->gen = SL_GEN_UNAVAILABLE; cpy(L->gen_msg, sizeof L->gen_msg, msg);
  touch(L);
}

const char *sl_rom_path(const struct sl_launcher *L) { return L->rom_path; }
