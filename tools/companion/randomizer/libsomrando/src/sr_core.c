/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Runtime helpers.  Derived in part from SoMRandomizer/util/DataUtil.cs and SoMRandomizer/util/CodeGenerationUtils.cs
 * (Author: Moppleton), and from the RandoContext working data (processing/common/RandoContext.cs).  Translated to C 2026-10-18.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "sr_core.h"

struct sr_block {
  struct sr_block *next;
};

void *sr_arena_alloc(struct sr_arena *a, size_t n) {
  struct sr_block *b = (struct sr_block *)calloc(1, sizeof *b + n + 16);
  if (!b) return NULL;
  b->next = a->head;
  a->head = b;
  return (void *)(b + 1);
}

void sr_arena_free(struct sr_arena *a) {
  struct sr_block *b = a->head;
  while (b) {
    struct sr_block *n = b->next;
    free(b);
    b = n;
  }
  a->head = NULL;
}

void sr_fail(struct sr_ctx *c, int code, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(c->err, sizeof c->err, fmt, ap);
  va_end(ap);
  c->fail_code = code;
  longjmp(c->fatal, 1);
}

void *sr_alloc(struct sr_ctx *c, size_t n) {
  void *p = sr_arena_alloc(&c->arena, n);
  if (!p) sr_fail(c, SOMRANDO_ERR_FAILED, "out of memory");
  return p;
}

void sr_bytes_reserve(struct sr_ctx *c, struct sr_bytes *b, size_t n) {
  if (b->len + n <= b->cap) return;
  size_t nc = b->cap ? b->cap * 2 : 64;
  while (nc < b->len + n) nc *= 2;
  uint8_t *np = (uint8_t *)sr_alloc(c, nc);
  if (b->len) memcpy(np, b->p, b->len);
  b->p = np;
  b->cap = nc;
}

void sr_bytes_push(struct sr_ctx *c, struct sr_bytes *b, int v) {
  sr_bytes_reserve(c, b, 1);
  b->p[b->len++] = (uint8_t)v;
}

void sr_bytes_push_n(struct sr_ctx *c, struct sr_bytes *b, const uint8_t *p, size_t n) {
  sr_bytes_reserve(c, b, n);
  if (n) memcpy(b->p + b->len, p, n);
  b->len += n;
}

void sr_poke(struct sr_ctx *c, int off, int v) {
  if (off < 0 || off >= SR_ROM_OUT_SIZE) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was outside the bounds of the array (ROM offset 0x%X).", off);
  c->out[off] = (uint8_t)v;
}

void sr_emit(struct sr_ctx *c, int v) {
  int o = c->working_offset++;
  sr_poke(c, o, v);
}

uint8_t sr_peek(struct sr_ctx *c, int off) {
  if (off < 0 || off >= SR_ROM_OUT_SIZE) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was outside the bounds of the array (ROM offset 0x%X).", off);
  return c->out[off];
}

uint8_t sr_peek_orig(struct sr_ctx *c, int off) {
  if (off < 0 || off >= SR_ROM_OUT_SIZE) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was outside the bounds of the array (ROM offset 0x%X).", off);
  return c->orig[off];
}

int sr_ushort_from_bytes(const uint8_t *rom, int index) { return rom[index] + (rom[index + 1] << 8); }

void sr_ushort_to_bytes(uint8_t *rom, int index, int value) {
  rom[index] = (uint8_t)value;
  rom[index + 1] = (uint8_t)(value >> 8);
}

int sr_clamp_to_endpoints(int value, int min_inclusive, int max_inclusive) {
  if (value > max_inclusive) return max_inclusive;
  if (value < min_inclusive) return min_inclusive;
  return value;
}

int sr_ensure_space_in_bank(int *new_code_offset, int num_bytes) {
  uint8_t bank1 = (uint8_t)(*new_code_offset >> 16);
  uint8_t bank2 = (uint8_t)((*new_code_offset + num_bytes) >> 16);
  if (bank1 != bank2) {
    *new_code_offset = ((int)bank2 << 16);
    return 1;
  }
  return 0;
}

/* ---- workingData */
static char *dup_str(struct sr_ctx *c, const char *s) {
  size_t n = strlen(s) + 1;
  char *p = (char *)sr_alloc(c, n);
  memcpy(p, s, n);
  return p;
}

void sr_work_set(struct sr_ctx *c, const char *key, const char *val) {
  for (int i = 0; i < c->n_work; i++)
    if (!strcmp(c->work[i].key, key)) { c->work[i].val = dup_str(c, val); return; }
  if (c->n_work == c->cap_work) {
    int nc = c->cap_work ? c->cap_work * 2 : 16;
    struct sr_kv *n = (struct sr_kv *)sr_alloc(c, (size_t)nc * sizeof *n);
    if (c->n_work) memcpy(n, c->work, (size_t)c->n_work * sizeof *n);
    c->work = n;
    c->cap_work = nc;
  }
  c->work[c->n_work].key = dup_str(c, key);
  c->work[c->n_work].val = dup_str(c, val);
  c->n_work++;
}

const char *sr_work_get(const struct sr_ctx *c, const char *key) {
  for (int i = 0; i < c->n_work; i++)
    if (!strcmp(c->work[i].key, key)) return c->work[i].val;
  return NULL;
}

void sr_work_set_int(struct sr_ctx *c, const char *key, int v) {
  char b[16];
  snprintf(b, sizeof b, "%d", v);
  sr_work_set(c, key, b);
}

void sr_work_set_int_array(struct sr_ctx *c, const char *key, const int *v, int n) {
  char b[256];
  size_t len = 0;
  b[0] = 0;
  for (int i = 0; i < n && len + 12 < sizeof b; i++) len += (size_t)snprintf(b + len, sizeof b - len, "%s%d", i ? "," : "", v[i]);
  sr_work_set(c, key, b);
}

int sr_work_get_int_array(const struct sr_ctx *c, const char *key, int *out, int max) {
  const char *s = sr_work_get(c, key);
  if (!s) return -1;
  int n = 0;
  while (*s && n < max) {
    out[n++] = (int)strtol(s, (char **)&s, 10);
    if (*s == ',') s++;
  }
  return n;
}

struct sr_bytes *sr_event_new(struct sr_ctx *c, int id) {
  for (int i = 0; i < c->n_events; i++)
    if (c->events[i].id == id) {
      memset(c->events[i].data, 0, sizeof *c->events[i].data);
      return c->events[i].data;
    }
  if (c->n_events == c->cap_events) {
    int nc = c->cap_events ? c->cap_events * 2 : 32;
    struct sr_event *n = (struct sr_event *)sr_alloc(c, (size_t)nc * sizeof *n);
    if (c->n_events) memcpy(n, c->events, (size_t)c->n_events * sizeof *n);
    c->events = n;
    c->cap_events = nc;
  }
  c->events[c->n_events].id = id;
  c->events[c->n_events].data = (struct sr_bytes *)sr_alloc(c, sizeof(struct sr_bytes));
  return c->events[c->n_events++].data;
}
