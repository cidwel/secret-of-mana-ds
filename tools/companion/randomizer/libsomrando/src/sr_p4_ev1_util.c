/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Event script builders for the Open World event modules.  Derived from processing/common/structure/EventScript.cs,
 * processing/common/VanillaEventUtil.cs, processing/openworld/events/{PrizeEvents,HintEvents}.cs and processing/common/ElementSwaps.cs
 * (Author: Moppleton).  Translated to C 2026-10-18.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "sr_p4_ev1.h"

const uint8_t sr_p4_ev1_event_injection_pattern[4] = {0x12, 0x34, 0x56, 0x78};
const uint8_t sr_p4_ev1_hint_injection_pattern[4] = {0x23, 0x45, 0x67, 0x89};

void sr_p4_ev1_add(struct sr_ctx *c, struct sr_bytes *e, int b) { sr_bytes_push(c, e, b); }
void sr_p4_ev1_return(struct sr_ctx *c, struct sr_bytes *e) { sr_bytes_push(c, e, EVC_RETURN); }
void sr_p4_ev1_open_dialogue(struct sr_ctx *c, struct sr_bytes *e) { sr_bytes_push(c, e, EVC_OPEN_DIALOGUE); }
void sr_p4_ev1_close_dialogue(struct sr_ctx *c, struct sr_bytes *e) { sr_bytes_push(c, e, EVC_CLOSE_DIALOGUE); }
void sr_p4_ev1_dialogue_wait(struct sr_ctx *c, struct sr_bytes *e) { sr_ev_sleep(c, e, 0); }

/* VanillaEventUtil.getByte */
static uint8_t event_byte(int s) {
  if (s == '\n') return 0x7F;
  if (s == ' ') return 0x80;
  if (s >= 0x61 && s <= 0x7A) return (uint8_t)(s + 0x20);
  if (s >= 0x41 && s <= 0x5A) return (uint8_t)(s + 0x5A);
  if (s >= 0x30 && s <= 0x39) return (uint8_t)(s + 0x85);
  switch (s) {
    case '.': return 0xBF;
    case ',': return 0xC0;
    case '/': return 0xC1;
    case '\'': return 0xC2;
    case '[': return 0xC3;
    case ']': return 0xC4;
    case ':': return 0xC5;
    case '-': return 0xC6;
    case '%': return 0xC7;
    case '!': return 0xC8;
    case '&': return 0xC9;
    case '?': return 0xCA;
    case '(': return 0xCB;
    case ')': return 0xCC;
    case '*': return 0xCD;
    case '^': return 0xCE;
    case 149: return 0xCF;
    case 150: return 0xD0;
    case 151: return 0xD1;
    case 152: return 0xD2;
    case '>': return 0x57;
  }
  return 0x80;
}

/* string.Replace(pattern, repl) on a counted buffer (the replacement may contain NULs); returns the new length */
static size_t replace_all(char *buf, size_t len, const char *pat, const char *repl, size_t repl_len) {
  size_t pl = strlen(pat), i = 0, o = 0;
  char *tmp = (char *)malloc(len + 1);
  if (!tmp) return len;
  while (i < len) {
    if (i + pl <= len && !memcmp(buf + i, pat, pl)) {
      memcpy(tmp + o, repl, repl_len);
      o += repl_len;
      i += pl;
    } else {
      tmp[o++] = buf[i++];
    }
  }
  memcpy(buf, tmp, o);
  free(tmp);
  return o;
}

void sr_p4_ev1_text(struct sr_ctx *c, struct sr_bytes *e, const char *text, int delay, int weapon_index) {
  size_t len = strlen(text);
  char *s = (char *)sr_alloc(c, len + 1);
  memcpy(s, text, len);
  len = replace_all(s, len, "(boy)", "\x03\x00", 2);
  len = replace_all(s, len, "(girl)", "\x03\x01", 2);
  len = replace_all(s, len, "(sprite)", "\x03\x02", 2);
  int num_newlines = 0, num_chars = 0;
  for (size_t i = 0; i < len; i++) {
    int ch = (uint8_t)s[i];
    if (ch == '\n') {
      num_newlines++;
      if (num_newlines == 3 && i != len - 1) {
        num_newlines = 0;
        sr_bytes_push(c, e, 0x28);
        sr_bytes_push(c, e, delay);
      }
      num_chars = 0;
    } else if (ch == '\t') {
      sr_bytes_push(c, e, 0x10);
      sr_bytes_push(c, e, 0x00);
    } else {
      num_chars++;
      if ((num_chars % 64) == weapon_index) {
        sr_bytes_push(c, e, event_byte(' '));
        sr_bytes_push(c, e, event_byte(' '));
        num_chars += 2;
      }
    }
    if (ch > 3) sr_bytes_push(c, e, event_byte(ch));
    else if (ch == 0x03) sr_bytes_push(c, e, 0x57);
    else sr_bytes_push(c, e, ch);
  }
}

void sr_p4_ev1_dialogue(struct sr_ctx *c, struct sr_bytes *e, const char *text, int ticks) {
  sr_p4_ev1_text(c, e, text, ticks & 0xFF, 31);
  sr_ev_sleep(c, e, ticks);
}
void sr_p4_ev1_dialogue_nowait(struct sr_ctx *c, struct sr_bytes *e, const char *text) { sr_p4_ev1_text(c, e, text, 0, 31); }
void sr_p4_ev1_dialogue_box(struct sr_ctx *c, struct sr_bytes *e, const char *text) {
  sr_p4_ev1_open_dialogue(c, e);
  sr_p4_ev1_dialogue(c, e, text, 0);
  sr_p4_ev1_close_dialogue(c, e);
}
void sr_p4_ev1_auto_dialogue_box(struct sr_ctx *c, struct sr_bytes *e, const char *text, int ticks) {
  sr_p4_ev1_open_dialogue(c, e);
  sr_p4_ev1_dialogue(c, e, text, ticks);
  sr_p4_ev1_close_dialogue(c, e);
}

void sr_p4_ev1_dialogue_choices(struct sr_ctx *c, struct sr_bytes *e, const char *text, int n, const uint8_t *x_positions, const char *const *choice_text,
                                const uint8_t (*commands)[2], const uint8_t cancel[2]) {
  sr_p4_ev1_text(c, e, text, 0, 31);
  sr_bytes_push(c, e, EVC_OPTION_SELECTION);
  for (int i = 0; i < n; i++) {
    sr_bytes_push(c, e, EVC_OPTION_AT);
    sr_bytes_push(c, e, x_positions[i]);
    sr_p4_ev1_text(c, e, choice_text[i], 0, 31);
  }
  sr_bytes_push(c, e, EVC_OPTION_SELECTION_END);
  sr_bytes_push(c, e, cancel[0]);
  sr_bytes_push(c, e, cancel[1]);
  for (int i = 0; i < n; i++) {
    sr_bytes_push(c, e, commands[i][0]);
    sr_bytes_push(c, e, commands[i][1]);
  }
}

const char *sr_p4_ev1_concat(struct sr_ctx *c, const char *a, const char *b) {
  size_t la = strlen(a), lb = strlen(b);
  char *r = (char *)sr_alloc(c, la + lb + 1);
  memcpy(r, a, la);
  memcpy(r + la, b, lb);
  return r;
}

const char *sr_p4_ev1_cat(struct sr_ctx *c, int n, ...) {
  va_list ap;
  size_t total = 0;
  va_start(ap, n);
  for (int i = 0; i < n; i++) total += strlen(va_arg(ap, const char *));
  va_end(ap);
  char *r = (char *)sr_alloc(c, total + 1);
  size_t o = 0;
  va_start(ap, n);
  for (int i = 0; i < n; i++) {
    const char *p = va_arg(ap, const char *);
    size_t l = strlen(p);
    memcpy(r + o, p, l);
    o += l;
  }
  va_end(ap);
  return r;
}

const char *sr_p4_ev1_word_wrap(struct sr_ctx *c, const char *text) {
  if (strchr(text, '\n')) return text;
  size_t len = strlen(text);
  char *out = (char *)sr_alloc(c, 2 * len + 8);
  size_t o = 0;
  int width = 0;
  const char *p = text;
  for (;;) {                                      /* string.Split(' ') yields empty tokens too */
    const char *q = strchr(p, ' ');
    size_t tl = q ? (size_t)(q - p) : strlen(p);
    if ((int)tl + width < 27) {
      width += (int)tl + 1;
    } else {
      out[o++] = '\n';
      width = (int)tl + 1;
    }
    memcpy(out + o, p, tl);
    o += tl;
    out[o++] = ' ';
    if (!q) break;
    p = q + 1;
  }
  return out;
}

struct sr_bytes *sr_p4_ev1_event_get(struct sr_ctx *c, int id) {
  for (int i = 0; i < c->n_events; i++)
    if (c->events[i].id == id) return c->events[i].data;
  sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%d' was not present in the dictionary.", id);
}

void sr_p4_ev1_inject_pattern(struct sr_ctx *c, struct sr_bytes *e, int index, int dialogue) {
  if (dialogue) sr_bytes_push(c, e, EVC_OPEN_DIALOGUE);
  for (int i = 0; i < 4; i++) sr_bytes_push(c, e, sr_p4_ev1_event_injection_pattern[i]);
  sr_bytes_push(c, e, index);
  if (dialogue) {
    sr_bytes_push(c, e, EVC_SLEEP_FOR);
    sr_bytes_push(c, e, 0x00);
    sr_bytes_push(c, e, EVC_CLOSE_DIALOGUE);
  }
}

void sr_p4_ev1_inject_pattern_auto(struct sr_ctx *c, struct sr_bytes *e, int index, int dialogue, int wait) {
  if (dialogue) sr_bytes_push(c, e, EVC_OPEN_DIALOGUE);
  for (int i = 0; i < 4; i++) sr_bytes_push(c, e, sr_p4_ev1_event_injection_pattern[i]);
  sr_bytes_push(c, e, index);
  if (dialogue) {
    sr_bytes_push(c, e, EVC_SLEEP_FOR);
    sr_bytes_push(c, e, wait);
    sr_bytes_push(c, e, EVC_CLOSE_DIALOGUE);
  }
}

void sr_p4_ev1_inject_hint(struct sr_ctx *c, struct sr_bytes *e) {
  sr_p4_ev1_open_dialogue(c, e);
  for (int i = 0; i < 4; i++) sr_bytes_push(c, e, sr_p4_ev1_hint_injection_pattern[i]);
  sr_p4_ev1_dialogue_wait(c, e);
  sr_p4_ev1_close_dialogue(c, e);
}

/* ---- workingData */
const char *sr_p4_ev1_wstr(struct sr_ctx *c, const char *key) {
  const char *v = sr_work_get(c, key);
  return v ? v : "";
}

int sr_p4_ev1_wbool(struct sr_ctx *c, const char *key) {
  const char *v = sr_p4_ev1_wstr(c, key);
  while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
  size_t n = strlen(v);
  while (n && (v[n - 1] == ' ' || v[n - 1] == '\t' || v[n - 1] == '\n' || v[n - 1] == '\r')) n--;
  char lo[8];
  if (n > 4) return 0;
  for (size_t i = 0; i < n; i++) lo[i] = (v[i] >= 'A' && v[i] <= 'Z') ? (char)(v[i] + 32) : v[i];
  lo[n] = 0;
  return !strcmp(lo, "1") || !strcmp(lo, "yes") || !strcmp(lo, "true");
}

int sr_p4_ev1_wint(struct sr_ctx *c, const char *key) {
  const char *v = sr_p4_ev1_wstr(c, key);
  char *end;
  long x = strtol(v, &end, 10);
  if (!*v || *end) sr_fail(c, SOMRANDO_ERR_FAILED, "The input string '%s' was not in a correct format.", v);
  return (int)x;
}

/* ---- ElementSwaps.getCrystalOrbElementMap */
void sr_p4_ev1_orbmap_load(struct sr_ctx *c, struct sr_p4_ev1_orbmap *m) {
  static const char prefix[] = "orbElement";
  m->n = 0;
  for (int i = 0; i < c->n_work; i++) {
    const char *k = c->work[i].key;
    if (strncmp(k, prefix, sizeof prefix - 1)) continue;
    int map_num = atoi(k + sizeof prefix - 1);
    int v = sr_p4_ev1_wint(c, k) & 0xFF;
    int j;
    for (j = 0; j < m->n; j++)
      if (m->map[j] == map_num) break;
    if (j == m->n) {
      if (m->n == 64) sr_fail(c, SOMRANDO_ERR_FAILED, "too many crystal orbs");
      m->n++;
    }
    m->map[j] = map_num;
    m->val[j] = v;
  }
}

int sr_p4_ev1_orbmap_has(const struct sr_p4_ev1_orbmap *m, int map_num) {
  for (int j = 0; j < m->n; j++)
    if (m->map[j] == map_num) return 1;
  return 0;
}

int sr_p4_ev1_orbmap_get(struct sr_ctx *c, const struct sr_p4_ev1_orbmap *m, int map_num) {
  for (int j = 0; j < m->n; j++)
    if (m->map[j] == map_num) return m->val[j];
  sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%d' was not present in the dictionary.", map_num);
}
