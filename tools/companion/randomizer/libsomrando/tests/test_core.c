/* Unit tests of libsomrando against .NET reference values (tests/oracle/OracleVectors) and round trips.
 *
 *   test_core <vec_rng.txt> <vec_pow.txt> [rom]
 *
 * RNG: hash codes and streams of DotNet110Random for the seeds the generator uses.  Math.Pow: every reachable call site of the C#
 * (see docs/randomizer-c-port-plan-20261018.md section 3) is swept and compared bit for bit with libm pow(), and the results are checked
 * for stability under a one-ulp change, which says how exposed the port would be to a libm that rounds differently.
 * Original work of this repository (LGPL-2.1+).
 */
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sr_core.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; if (failures < 40) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

static int b64v(int c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}
static size_t b64_decode(const char *s, char *out) {
  size_t n = 0;
  uint32_t acc = 0;
  int bits = 0;
  for (; *s && *s != '='; s++) {
    int v = b64v(*s);
    if (v < 0) continue;
    acc = (acc << 6) | (uint32_t)v;
    bits += 6;
    if (bits >= 8) { bits -= 8; out[n++] = (char)((acc >> bits) & 0xFF); }
  }
  out[n] = 0;
  return n;
}

static uint64_t bits_of(double d) { uint64_t u; memcpy(&u, &d, 8); return u; }
static double dbl_of(uint64_t u) { double d; memcpy(&d, &u, 8); return d; }

static char *next_tok(char **p) {
  while (**p == ' ') (*p)++;
  if (!**p) return NULL;
  char *t = *p;
  while (**p && **p != ' ' && **p != '\n') (*p)++;
  if (**p) { **p = 0; (*p)++; }
  return t;
}

static void test_rng(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) { printf("cannot open %s\n", path); failures++; return; }
  static char line[1 << 16];
  struct sr_random r, r2;
  char seed[256] = "";
  int32_t hash = 0;
  int in_seed = 0, n_seeds = 0;
  while (fgets(line, sizeof line, f)) {
    char *p = line;
    char *tag = next_tok(&p);
    if (!tag) continue;
    if (!strcmp(tag, "seed")) {
      char *b = next_tok(&p), *h = next_tok(&p);
      b64_decode(b + 2, seed);   /* "b:" prefix keeps the empty seed a token */
      hash = (int32_t)atoll(h);
      CHECK(sr_hash_string(seed) == hash, "hash of '%s': C %d, .NET %d", seed, sr_hash_string(seed), hash);
      sr_random_init(&r, hash);
      sr_random_init(&r2, hash);
      in_seed = 1;
      n_seeds++;
    } else if (!strcmp(tag, "next") && in_seed) {
      char *t; int i = 0;
      while ((t = next_tok(&p))) { int32_t want = (int32_t)atoll(t), got = sr_random_next(&r); CHECK(got == want, "seed '%s' Next #%d: C %d, .NET %d", seed, i, got, want); i++; }
    } else if (!strcmp(tag, "nextmax8") || !strcmp(tag, "nextmax1000003")) {
      int mx = !strcmp(tag, "nextmax8") ? 8 : 1000003;
      char *t; int i = 0;
      while ((t = next_tok(&p))) { int32_t want = (int32_t)atoll(t), got = sr_random_next_max(&r2, mx); CHECK(got == want, "seed '%s' Next(%d) #%d: C %d, .NET %d", seed, mx, i, got, want); i++; }
    } else if (!strcmp(tag, "nextrange")) {
      char *t; int i = 0;
      while ((t = next_tok(&p))) { int32_t want = (int32_t)atoll(t), got = sr_random_next_range(&r2, -50, 77); CHECK(got == want, "seed '%s' Next(-50,77) #%d: C %d, .NET %d", seed, i, got, want); i++; }
    } else if (!strcmp(tag, "nextrangebig") || !strcmp(tag, "nextrangeedge")) {
      int edge = !strcmp(tag, "nextrangeedge");
      char *t; int i = 0;
      while ((t = next_tok(&p))) {
        int32_t want = (int32_t)atoll(t), got = edge ? sr_random_next_range(&r2, INT32_MIN + 5, INT32_MAX - 5) : sr_random_next_range(&r2, INT32_MIN, INT32_MAX);
        CHECK(got == want, "seed '%s' large-range Next #%d: C %d, .NET %d", seed, i, got, want); i++;
      }
    } else if (!strcmp(tag, "double")) {
      char *t; int i = 0;
      while ((t = next_tok(&p))) { uint64_t want = strtoull(t, NULL, 16), got = bits_of(sr_random_next_double(&r2)); CHECK(got == want, "seed '%s' NextDouble #%d: C %016" PRIx64 ", .NET %016" PRIx64, seed, i, got, want); i++; }
    } else if (!strcmp(tag, "bytes")) {
      uint8_t buf[64];
      sr_random_next_bytes(&r2, buf, 64);
      char *t; int i = 0;
      while ((t = next_tok(&p))) { CHECK(buf[i] == atoi(t), "seed '%s' NextBytes[%d]: C %d, .NET %s", seed, i, buf[i], t); i++; }
    } else if (!strcmp(tag, "cosmetic")) {
      char *h = next_tok(&p);
      char cos[300];
      snprintf(cos, sizeof cos, "%s_cosmetic", seed);
      int32_t hc = (int32_t)atoll(h);
      CHECK(sr_hash_string(cos) == hc, "cosmetic hash of '%s'", seed);
      struct sr_random rc; sr_random_init(&rc, hc);
      char *t; int i = 0;
      while ((t = next_tok(&p))) { int32_t want = (int32_t)atoll(t), got = sr_random_next(&rc); CHECK(got == want, "cosmetic '%s' #%d", seed, i); i++; }
    } else if (!strcmp(tag, "race")) {
      struct sr_random a, b; sr_random_init(&a, hash); sr_random_init(&b, sr_random_next(&a));
      char *t; int i = 0;
      while ((t = next_tok(&p))) { int32_t want = (int32_t)atoll(t), got = sr_random_next(&b); CHECK(got == want, "race '%s' #%d", seed, i); i++; }
    } else if (!strcmp(tag, "iseed")) {
      int32_t sd = (int32_t)atoll(next_tok(&p));
      struct sr_random q; sr_random_init(&q, sd);
      char *t; int i = 0;
      while ((t = next_tok(&p))) { int32_t want = (int32_t)atoll(t), got = sr_random_next(&q); CHECK(got == want, "iseed %d #%d: C %d, .NET %d", sd, i, got, want); i++; }
      n_seeds++;
    }
  }
  fclose(f);
  printf("rng: %d seeds, checks so far %d\n", n_seeds, checks);
}

static long fin_value(int formula, double c, double d, double p) {
  if (formula == 0) return (int)p;
  if (formula == 1) return (long)rint(c + p * d);   /* Math.Round: ties to even, the default rounding mode */
  return (int)(c + p * d);
}

static void test_pow(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) { printf("cannot open %s\n", path); failures++; return; }
  static char line[512];
  struct site { char name[24]; long n, mismatch, fin_mismatch, inexact, fragile; } sites[16];
  int ns = 0;
  while (fgets(line, sizeof line, f)) {
    char *tok[8]; int nt = 0;
    for (char *p = strtok(line, "|\n"); p && nt < 8; p = strtok(NULL, "|\n")) tok[nt++] = p;
    if (nt != 8) continue;
    int si = -1;
    for (int i = 0; i < ns; i++) if (!strcmp(sites[i].name, tok[0])) si = i;
    if (si < 0) { si = ns++; memset(&sites[si], 0, sizeof sites[si]); snprintf(sites[si].name, sizeof sites[si].name, "%s", tok[0]); }
    double a = dbl_of(strtoull(tok[1], NULL, 16)), b = dbl_of(strtoull(tok[2], NULL, 16));
    double c = dbl_of(strtoull(tok[3], NULL, 16)), d = dbl_of(strtoull(tok[4], NULL, 16));
    int formula = atoi(tok[5]);
    uint64_t want = strtoull(tok[6], NULL, 16);
    long want_fin = atol(tok[7]);
    double p = pow(a, b);
    sites[si].n++;
    if (bits_of(p) != want) {
      sites[si].mismatch++;
      /* "grid" is a coarse sweep of free-form exponents (custom difficulty values): informational, the shipped exponents are in "table" */
      if (strcmp(tok[0], "grid")) CHECK(0, "pow(%g,%g) site %s: C %016" PRIx64 ", .NET %016" PRIx64, a, b, tok[0], bits_of(p), want);
      else if (!getenv("SR_QUIET")) printf("note: pow(%g,%g) differs in the last bit from .NET on this libm (free-form exponent)\n", a, b);
    }
    long got_fin = fin_value(formula, c, d, p);
    if (got_fin != want_fin) { sites[si].fin_mismatch++; CHECK(0, "pow site %s final value: C %ld, .NET %ld", tok[0], got_fin, want_fin); }
    checks += 2;
    /* Integer exponents with results below 2^53 are exact in every libm.  For the others, a libm that is one ulp off would change the
     * integer result only for the arguments counted as fragile. */
    if (b == floor(b) && fabs(p) < 9007199254740992.0) continue;
    sites[si].inexact++;
    if (fin_value(formula, c, d, nextafter(p, INFINITY)) != got_fin || fin_value(formula, c, d, nextafter(p, -INFINITY)) != got_fin) sites[si].fragile++;
  }
  fclose(f);
  printf("pow: %-10s %8s %9s %9s %8s %12s\n", "site", "vectors", "bit diff", "int diff", "inexact", "1ulp-fragile");
  for (int i = 0; i < ns; i++) printf("pow: %-10s %8ld %9ld %9ld %8ld %12ld\n", sites[i].name, sites[i].n, sites[i].mismatch, sites[i].fin_mismatch, sites[i].inexact, sites[i].fragile);
}

static void test_lz77(const char *rom_path) {
  struct sr_ctx *c = (struct sr_ctx *)calloc(1, sizeof *c);
  if (setjmp(c->fatal)) { CHECK(0, "lz77 fatal: %s", c->err); free(c); return; }
  uint32_t x = 12345;
  for (int ws = 1; ws <= 4; ws++) {
    for (int trial = 0; trial < 30; trial++) {
      size_t n = 1 + (x = x * 1103515245u + 12345u) % 6000;
      uint8_t *blk = (uint8_t *)sr_alloc(c, n);
      int mode = trial % 3;
      for (size_t i = 0; i < n; i++) {
        x = x * 1103515245u + 12345u;
        blk[i] = mode == 0 ? (uint8_t)(x >> 16) : mode == 1 ? (uint8_t)((x >> 16) & 3) : (uint8_t)(i % 7 + ((x >> 24) == 0));
      }
      struct sr_bytes enc = {0}, dec = {0};
      sr_lz77_encode(c, blk, n, ws, &enc);
      sr_lz77_decode(c, enc.p, 0, &dec);
      CHECK(dec.len == n && !memcmp(dec.p, blk, n), "lz77 round trip ws=%d n=%zu", ws, n);
    }
  }
  if (rom_path) {
    FILE *f = fopen(rom_path, "rb");
    if (f) {
      uint8_t *rom = (uint8_t *)sr_alloc(c, SR_ROM_OUT_SIZE);
      size_t got = fread(rom, 1, SR_ROM_OUT_SIZE, f);
      fclose(f);
      size_t off = got == 2 * 1024 * 1024 + 512 ? 512 : 0;
      memmove(rom, rom + off, SR_ROM_IN_SIZE);
      static const int blocks[2] = {0x77C00, 0x7B480};
      for (int b = 0; b < 2; b++) {
        struct sr_bytes dec = {0}, enc = {0}, dec2 = {0};
        sr_lz77_decode(c, rom, blocks[b], &dec);
        sr_lz77_encode(c, dec.p, dec.len, b == 0 ? 1 : 4, &enc);
        sr_lz77_decode(c, enc.p, 0, &dec2);
        CHECK(dec2.len == dec.len && !memcmp(dec2.p, dec.p, dec.len), "title block %d re-encode round trip", b);
        printf("lz77: title block %d: %zu bytes decoded, re-encoded %zu\n", b, dec.len, enc.len);
      }
    }
  }
  sr_arena_free(&c->arena);
  free(c);
  printf("lz77: round trips done\n");
}

int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage: test_core vec_rng.txt vec_pow.txt [rom]\n"); return 2; }
  test_rng(argv[1]);
  test_pow(argv[2]);
  test_lz77(argc > 3 ? argv[3] : NULL);
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
