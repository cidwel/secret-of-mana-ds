/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Derived from processing/hacks/openworld/TitleMenuSeedHashDisplay.cs (Author: Moppleton).  Translated to C 2026-10-18.
 * Writes the seed and a hash of the final random state over the "WELCOME TO SECRET OF MANA" text; it must be the last hack. */
#include <stdlib.h>
#include <string.h>

#include "sr_p4.h"
#include "sr_p4_sel.h"

#define SR_P4_VERSION "1.49"

/* VanillaEventUtil.getByte */
static uint8_t text_byte(int s) {
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

/* UTF-8 to UTF-16 code units; returns the unit count, -1 on malformed input */
static int utf8_to_utf16(const char *s, uint16_t *out, int cap) {
  const uint8_t *p = (const uint8_t *)s;
  int n = 0;
  while (*p) {
    uint32_t cp;
    int len;
    if (*p < 0x80) { cp = *p; len = 1; }
    else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; len = 2; }
    else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; len = 3; }
    else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; len = 4; }
    else return -1;
    for (int i = 1; i < len; i++) {
      if ((p[i] & 0xC0) != 0x80) return -1;
      cp = (cp << 6) | (p[i] & 0x3F);
    }
    if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) || (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)) || (cp >= 0xD800 && cp <= 0xDFFF)) return -1;
    p += len;
    if (cp >= 0x10000) {
      if (n + 2 > cap) return -1;
      cp -= 0x10000;
      out[n++] = (uint16_t)(0xD800 + (cp >> 10));
      out[n++] = (uint16_t)(0xDC00 + (cp & 0x3FF));
    } else {
      if (n + 1 > cap) return -1;
      out[n++] = (uint16_t)cp;
    }
  }
  return n;
}

/* string.Replace(pattern, repl) over UTF-16 units; the pattern and the replacement are ASCII/control codes */
static int replace_units(uint16_t *buf, int len, const char *pat, const uint16_t *repl, int repl_len, int cap) {
  int pl = (int)strlen(pat), o = 0;
  uint16_t *tmp = (uint16_t *)malloc((size_t)(cap + 1) * sizeof *tmp);
  if (!tmp) return len;
  for (int i = 0; i < len;) {
    int match = i + pl <= len;
    for (int k = 0; match && k < pl; k++) match = buf[i + k] == (uint16_t)(uint8_t)pat[k];
    if (match && o + repl_len <= cap) {
      for (int k = 0; k < repl_len; k++) tmp[o++] = repl[k];
      i += pl;
    } else {
      tmp[o++] = buf[i++];
    }
  }
  memcpy(buf, tmp, (size_t)o * sizeof *tmp);
  free(tmp);
  return o;
}

int sr_p4_hack_title_menu_seed_hash(struct sr_ctx *c) {
  struct sr_random *r = &c->rf;
  long long hash = 0;
  for (int i = 0; i < 8; i++) {
    hash <<= 4;
    hash |= (sr_random_next(r) % 16);
  }
  char hashString[24];
  snprintf(hashString, sizeof hashString, "%s%llX", sr_settings_get_bool(c->settings, "raceMode") ? "r" : "", (unsigned long long)hash);

  /* seedShortened = seed.Length > 20 ? seed.Substring(0, 20) + "..." : seed, in UTF-16 units */
  size_t seed_len = strlen(c->seed);
  int cap = (int)seed_len + 256;
  uint16_t *seed16 = (uint16_t *)sr_alloc(c, (size_t)(seed_len + 1) * sizeof *seed16);
  int sn = utf8_to_utf16(c->seed, seed16, (int)seed_len + 1);
  if (sn < 0) sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "unsupported option: seed is not valid UTF-8");
  if (sn > 20) {
    sn = 20;
    seed16[sn++] = '.';
    seed16[sn++] = '.';
    seed16[sn++] = '.';
  }
  uint16_t *text = (uint16_t *)sr_alloc(c, (size_t)cap * sizeof *text);
  int tn = 0;
  const char *pre = "Secret of Mana Open World v" SR_P4_VERSION "\n\nSeed: ";
  for (const char *q = pre; *q; q++) text[tn++] = (uint8_t)*q;
  for (int i = 0; i < sn; i++) text[tn++] = seed16[i];
  const char *post1 = "\n\nHash check: ";
  for (const char *q = post1; *q; q++) text[tn++] = (uint8_t)*q;
  for (const char *q = hashString; *q; q++) text[tn++] = (uint8_t)*q;
  const char *post2 = "\n\nEnjoy";
  for (const char *q = post2; *q; q++) text[tn++] = (uint8_t)*q;
  /* the indicators are replaced in this order, over the whole text including the seed */
  static const uint16_t boy[2] = {3, 0}, girl[2] = {3, 1}, sprite[2] = {3, 2};
  tn = replace_units(text, tn, "(boy)", boy, 2, cap);
  tn = replace_units(text, tn, "(girl)", girl, 2, cap);
  tn = replace_units(text, tn, "(sprite)", sprite, 2, cap);

  /* VanillaEventUtil.getBytes(text, -1): delay 0 and no weapon-description spacing */
  struct sr_bytes bytes = {0};
  int numNewlines = 0;
  for (int i = 0; i < tn; i++) {
    int ch = text[i];
    if (ch == '\n') {
      numNewlines++;
      if (numNewlines == 3 && i != tn - 1) {
        numNewlines = 0;
        sr_bytes_push(c, &bytes, 0x28);
        sr_bytes_push(c, &bytes, 0);
      }
    } else if (ch == '\t') {
      sr_bytes_push(c, &bytes, 0x10);
      sr_bytes_push(c, &bytes, 0x00);
    }
    if (ch > 3) sr_bytes_push(c, &bytes, text_byte(ch));
    else if (ch == 0x03) sr_bytes_push(c, &bytes, 0x57);
    else sr_bytes_push(c, &bytes, ch);
  }

  /* 0x33F0 is the "WELCOME TO SECRET OF MANA" text, through 0x348B */
  int introMenuSize = 0x348B - 0x33F0 + 1;
  int srcI = 0;
  for (int i = 0; i < introMenuSize; i++) {
    if (srcI < (int)bytes.len) {
      if (bytes.p[srcI] == 0x28) srcI += 2;
      if (srcI >= (int)bytes.len) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range.");
      sr_poke(c, 0x33F0 + i, bytes.p[srcI]);
    } else {
      sr_poke(c, 0x33F0 + i, i == introMenuSize - 1 ? 0 : 0x80);
    }
    srcI++;
  }
  return 1;
}
