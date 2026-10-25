/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Derived from:
 *   SoMRandomizer/util/rng/DotNet110Random.cs - Copyright (c) .NET Foundation and Contributors, MIT license, taken upstream from
 *       https://github.com/dotnet/coreclr/blob/release/1.1.0/src/mscorlib/src/System/Random.cs (Knuth subtractive generator).
 *   SoMRandomizer/util/rng/HashcodeUtil.cs - Author: unknown ("came off the internet somewhere", upstream comment).
 * Translated to C 2026-10-18.  All C# int arithmetic is unchecked (32-bit wrap); it is done on uint32_t here.
 *
 * The C# class overrides Sample() to InternalSample() * (1.0 / Int32.MaxValue); the doubles are IEEE binary64, so the build must not
 * contract multiply-add (-ffp-contract=off) or the bounded draws could differ from the .NET results in rare cases.
 */
#include <math.h>
#include <stdlib.h>

#include "sr_core.h"

#define MBIG 2147483647
#define MSEED 161803398

static int32_t wrap_sub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
static int32_t wrap_add(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }

void sr_random_init(struct sr_random *r, int32_t seed) {
  int32_t subtraction = (seed == INT32_MIN) ? MBIG : (seed < 0 ? -seed : seed);
  int32_t mj = wrap_sub(MSEED, subtraction), mk = 1;
  for (int i = 0; i < 56; i++) r->seed_array[i] = 0;
  r->seed_array[55] = mj;
  for (int i = 1; i < 55; i++) {
    int ii = (21 * i) % 55;
    r->seed_array[ii] = mk;
    mk = wrap_sub(mj, mk);
    if (mk < 0) mk = wrap_add(mk, MBIG);
    mj = r->seed_array[ii];
  }
  for (int k = 1; k < 5; k++) {
    for (int i = 1; i < 56; i++) {
      r->seed_array[i] = wrap_sub(r->seed_array[i], r->seed_array[1 + (i + 30) % 55]);
      if (r->seed_array[i] < 0) r->seed_array[i] = wrap_add(r->seed_array[i], MBIG);
    }
  }
  r->inext = 0;
  r->inextp = 21;
  r->calls = 0;
}

static int32_t internal_sample(struct sr_random *r) {
  int loc_inext = r->inext, loc_inextp = r->inextp;
  r->calls++;
  if (++loc_inext >= 56) loc_inext = 1;
  if (++loc_inextp >= 56) loc_inextp = 1;
  int32_t ret = wrap_sub(r->seed_array[loc_inext], r->seed_array[loc_inextp]);
  if (ret == MBIG) ret--;
  if (ret < 0) ret = wrap_add(ret, MBIG);
  r->seed_array[loc_inext] = ret;
  r->inext = loc_inext;
  r->inextp = loc_inextp;
  return ret;
}

static double sample(struct sr_random *r) {
  /* InternalSample() * (1.0 / MBIG): the constant is folded to a double first, as the C# compiler does */
  static const double inv = 1.0 / (double)MBIG;
  return (double)internal_sample(r) * inv;
}

int32_t sr_random_next(struct sr_random *r) { return internal_sample(r); }

int32_t sr_random_next_max(struct sr_random *r, int32_t max) {
  if (max < 0) return -1;
  return (int32_t)(sample(r) * (double)max);
}

static double sample_for_large_range(struct sr_random *r) {
  int32_t result = internal_sample(r);
  int negative = (internal_sample(r) % 2 == 0);
  if (negative) result = -result;
  double d = (double)result;
  d += (double)(MBIG - 1);
  d /= (double)(2 * (uint32_t)MBIG - 1);   /* uint arithmetic: 4294967293 */
  return d;
}

int32_t sr_random_next_range(struct sr_random *r, int32_t min, int32_t max) {
  if (min > max) return min;
  int64_t range = (int64_t)max - (int64_t)min;
  if (range <= (int64_t)MBIG) return (int32_t)((int32_t)(sample(r) * (double)range) + min);
  /* (int)((long)(GetSampleForLargeRange() * range) + minValue): the cast to int wraps */
  double v = sample_for_large_range(r) * (double)range;
  int64_t l = (int64_t)v;
  return (int32_t)(uint32_t)((uint64_t)l + (uint64_t)(int64_t)min);
}

double sr_random_next_double(struct sr_random *r) { return sample(r); }

void sr_random_next_bytes(struct sr_random *r, uint8_t *buf, size_t n) {
  for (size_t i = 0; i < n; i++) buf[i] = (uint8_t)(internal_sample(r) % 256);
}

/* Decodes UTF-8 into UTF-16 code units as .NET does (Marshal.PtrToStringUTF8): invalid sequences become U+FFFD. */
static size_t utf8_to_utf16(const char *s, uint16_t *out) {
  const uint8_t *p = (const uint8_t *)s;
  size_t n = 0;
  while (*p) {
    uint32_t cp;
    int len;
    uint8_t b = *p;
    if (b < 0x80) { cp = b; len = 1; }
    else if (b >= 0xC2 && b <= 0xDF) { cp = b & 0x1F; len = 2; }
    else if (b >= 0xE0 && b <= 0xEF) { cp = b & 0x0F; len = 3; }
    else if (b >= 0xF0 && b <= 0xF4) { cp = b & 0x07; len = 4; }
    else { if (out) out[n] = 0xFFFD; n++; p++; continue; }
    int ok = 1;
    for (int i = 1; i < len; i++) {
      if ((p[i] & 0xC0) != 0x80) { ok = 0; break; }
      cp = (cp << 6) | (p[i] & 0x3F);
    }
    if (ok && ((len == 3 && (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))) || (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)))) ok = 0;
    if (!ok) {
      /* one replacement character per maximal invalid prefix; continuation bytes that do not fit are consumed one at a time */
      if (out) out[n] = 0xFFFD;
      n++;
      p++;
      continue;
    }
    p += len;
    if (cp >= 0x10000) {
      cp -= 0x10000;
      if (out) { out[n] = (uint16_t)(0xD800 + (cp >> 10)); out[n + 1] = (uint16_t)(0xDC00 + (cp & 0x3FF)); }
      n += 2;
    } else {
      if (out) out[n] = (uint16_t)cp;
      n++;
    }
  }
  return n;
}

int32_t sr_hash_string(const char *utf8) {
  size_t n = utf8_to_utf16(utf8, NULL);
  uint16_t *u = (uint16_t *)malloc((n + 1) * sizeof *u);
  if (!u) return 0;
  utf8_to_utf16(utf8, u);
  uint32_t hash1 = (5381u << 16) + 5381u, hash2 = hash1;
  for (size_t i = 0; i < n; i += 2) {
    hash1 = ((hash1 << 5) + hash1) ^ u[i];
    if (i == n - 1) break;
    hash2 = ((hash2 << 5) + hash2) ^ u[i + 1];
  }
  free(u);
  return (int32_t)(hash1 + hash2 * 1566083941u);
}
