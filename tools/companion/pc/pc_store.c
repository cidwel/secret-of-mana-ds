/* File helpers of the SoM PC frontend, see pc_store.h. */
#define _POSIX_C_SOURCE 200809L
#include "pc_store.h"
#include "pc_compat.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int pc_mkdirs(const char *path) {
  char t[1024];
  snprintf(t, sizeof t, "%s", path);
  for (char *p = t + 1; *p; p++) if (*p == '/') { *p = 0; PC_MKDIR(t); *p = '/'; }
  return PC_MKDIR(t) == 0 || errno == EEXIST ? 0 : -1;
}

uint8_t *pc_slurp(const char *path, size_t *n) {
  FILE *f = fopen(path, "rb");
  uint8_t *b;
  long sz;
  if (!f) return NULL;
  if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0) { fclose(f); return NULL; }
  rewind(f);
  b = malloc(sz > 0 ? (size_t)sz + 1 : 1);
  if (!b || fread(b, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(b); return NULL; }
  fclose(f);
  b[sz] = 0;
  *n = (size_t)sz;
  return b;
}

int pc_spit(const char *path, const void *d, size_t n) {
  char tmp[1100];
  FILE *f;
  snprintf(tmp, sizeof tmp, "%s.part", path);
  f = fopen(tmp, "wb");
  if (!f) return -1;
  if (fwrite(d, 1, n, f) != n || fflush(f) != 0) { fclose(f); unlink(tmp); return -1; }
  PC_FSYNC(fileno(f));
  if (fclose(f) != 0) { unlink(tmp); return -1; }
  if (pc_rename(tmp, path) != 0) { unlink(tmp); return -1; }
  return 0;
}

static char *load_text(const char *path) {
  size_t n;
  return (char *)pc_slurp(path, &n);
}

int pc_store_save(const char *path, const char *text) {
  char bak[1100];
  char *old = load_text(path);
  snprintf(bak, sizeof bak, "%s.bak", path);
  if (old && old[0] && strcmp(old, text) != 0) pc_spit(bak, old, strlen(old));
  free(old);
  return pc_spit(path, text, strlen(text));
}

char *pc_store_load(const char *path, int (*valid)(const char *), int *source) {
  char bak[1100], bad[1100];
  int src = 2;
  char *t = load_text(path);
  snprintf(bak, sizeof bak, "%s.bak", path);
  snprintf(bad, sizeof bad, "%s.bad", path);
  if (t && (!valid || valid(t))) { if (source) *source = 0; return t; }
  if (t) { pc_rename(path, bad); src = 3; }
  free(t);
  t = load_text(bak);
  if (t && (!valid || valid(t))) { if (source) *source = 1; return t; }
  free(t);
  if (source) *source = src == 3 ? 3 : 2;
  return NULL;
}
