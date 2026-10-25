/* See som_credits.h. */
#include "som_credits.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Copies the JSON string value at p (just after its opening quote) into out, resolving escapes; non ASCII becomes '?'. Returns the pointer after the closing quote. */
static const char *read_str(const char *p, char *out, size_t n) {
  size_t i = 0;
  while (*p && *p != '"') {
    char c = *p++;
    if (c == '\\' && *p) {
      c = *p++;
      if (c == 'n') c = '\n';
      else if (c == 'u') { for (int k = 0; k < 4 && isxdigit((unsigned char)*p); k++) p++; c = '?'; }
    } else if ((unsigned char)c >= 0x80) {
      while (((unsigned char)*p & 0xC0) == 0x80) p++;       /* rest of a UTF-8 sequence */
      c = '?';
    }
    if (i + 1 < n) out[i++] = c;
  }
  if (*p == '"') p++;
  out[i] = 0;
  return p;
}

/* Finds "key" inside [obj, end) at nesting depth 1 of that object and reads its string value; 1 when found. */
static int obj_str(const char *obj, const char *end, const char *key, char *out, size_t n) {
  char pat[48];
  size_t kl;
  int depth = 0, in_str = 0;
  snprintf(pat, sizeof pat, "\"%s\"", key);
  kl = strlen(pat);
  out[0] = 0;
  for (const char *p = obj; p < end; p++) {
    if (in_str) { if (*p == '\\') p++; else if (*p == '"') in_str = 0; continue; }
    if (*p == '{' || *p == '[') depth++;
    else if (*p == '}' || *p == ']') depth--;
    else if (*p == '"') {
      if (depth == 1 && (size_t)(end - p) > kl && !strncmp(p, pat, kl)) {
        const char *q = p + kl;
        while (q < end && isspace((unsigned char)*q)) q++;
        if (q < end && *q == ':') {
          q++;
          while (q < end && isspace((unsigned char)*q)) q++;
          if (q < end && *q == '"') { read_str(q + 1, out, n); return 1; }
        }
      }
      in_str = 1;
    }
  }
  return 0;
}

static const char *skip_obj(const char *p) {             /* p at '{': returns just after the matching '}' (or the end) */
  int depth = 0, in_str = 0;
  for (; *p; p++) {
    if (in_str) { if (*p == '\\' && p[1]) p++; else if (*p == '"') in_str = 0; continue; }
    if (*p == '"') in_str = 1;
    else if (*p == '{') depth++;
    else if (*p == '}' && --depth == 0) return p + 1;
  }
  return p;
}

static int meaningful(const char *s) { return s[0] && strcmp(s, "n/a") && strcmp(s, "see survey") && strcmp(s, "unknown"); }

int sc_format(const char *json, char *out, size_t n) {
  char title[96], who[256], what[512], lic[96], url[256], block[1500];
  const char *p, *arr;
  size_t o = 0;
  int count = 0;
  if (!json || !out || n < 16) return -1;
  out[0] = 0;
  arr = strstr(json, "\"entries\"");
  if (!arr) return -1;
  arr = strchr(arr, '[');
  if (!arr) return -1;
  if (!obj_str(json, json + strlen(json), "title", title, sizeof title)) snprintf(title, sizeof title, "Special thanks");
  o = (size_t)snprintf(out, n, "%s\n\n", title);
  if (o >= n) { out[0] = 0; return -1; }
  for (p = arr + 1; *p && *p != ']';) {
    const char *start, *end;
    size_t bl;
    while (*p && *p != '{' && *p != ']') p++;
    if (*p != '{') break;
    start = p; end = skip_obj(p); p = end;
    if (!obj_str(start, end, "who", who, sizeof who) && !obj_str(start, end, "author", who, sizeof who)) who[0] = 0;
    if (!obj_str(start, end, "what", what, sizeof what) && !obj_str(start, end, "patch", what, sizeof what)) what[0] = 0;
    if (!obj_str(start, end, "license", lic, sizeof lic)) lic[0] = 0;
    if (!obj_str(start, end, "url", url, sizeof url)) url[0] = 0;
    if (!who[0] && !what[0]) continue;
    bl = 0;
    block[0] = 0;
    if (who[0]) bl += (size_t)snprintf(block + bl, sizeof block - bl, "%s\n", who);
    if (what[0]) bl += (size_t)snprintf(block + bl, sizeof block - bl, "%s\n", what);
    if (meaningful(lic) && bl < sizeof block) bl += (size_t)snprintf(block + bl, sizeof block - bl, "License: %s\n", lic);
    if (url[0] && bl < sizeof block) bl += (size_t)snprintf(block + bl, sizeof block - bl, "%s\n", url);
    if (bl < sizeof block) bl += (size_t)snprintf(block + bl, sizeof block - bl, "\n");
    if (bl >= sizeof block) bl = sizeof block - 1;
    if (o + bl >= n) break;                                 /* whole entries only */
    memcpy(out + o, block, bl);
    o += bl;
    out[o] = 0;
    count++;
  }
  return count ? (int)o : -1;
}

const char *sc_fallback(void) {
  return "Special thanks\n\n"
         "Moppleton and contributors\nSecret of Mana Randomizer and its Ancient Cave mode. libsomrando is a C port of their generator.\nLicense: LGPL-2.1\nhttps://github.com/Moppu/SecretOfManaRandomizer\n\n"
         "The authors of the Secret of Mana QOL and bug-fix patches on romhacking.net\nhttps://www.romhacking.net/\n\n"
         "The Secret of Mana reverse-engineering and documentation community\n";
}
