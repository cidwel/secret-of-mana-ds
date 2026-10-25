#include "som_states.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAGIC 0x53534D53u   /* "SMSS" little endian */
#define VERSION 1u

struct hdr {
  uint32_t magic, version, core_size, tw, th;
  int64_t time;
  uint32_t state_size;
};

static int mkdirs(const char *path) {
  char tmp[700];
  snprintf(tmp, sizeof tmp, "%s", path);
  for (char *p = tmp + 1; *p; p++)
    if (*p == '/') { *p = 0; if (mkdir(tmp, 0775) != 0 && errno != EEXIST) return -1; *p = '/'; }
  return mkdir(tmp, 0775) != 0 && errno != EEXIST ? -1 : 0;
}

int sst_dir(const char *root, const char *key, char *dir, size_t n) {
  if (!root || !key || !key[0] || strchr(key, '/')) return SST_ERR_ARG;
  snprintf(dir, n, "%s/%s", root, key);
  return mkdirs(dir) == 0 ? SST_OK : SST_ERR_IO;
}

void sst_make_thumb(const uint32_t *f, int stride, int w, int h, uint32_t out[SST_TW * SST_TH]) {
  if (w <= 0 || h <= 0) { memset(out, 0, sizeof(uint32_t) * SST_TW * SST_TH); return; }
  for (int y = 0; y < SST_TH; y++) {
    int y0 = y * h / SST_TH, y1 = (y + 1) * h / SST_TH;
    if (y1 <= y0) y1 = y0 + 1;
    for (int x = 0; x < SST_TW; x++) {
      int x0 = x * w / SST_TW, x1 = (x + 1) * w / SST_TW;
      unsigned r = 0, g = 0, b = 0, cnt = 0;
      if (x1 <= x0) x1 = x0 + 1;
      for (int j = y0; j < y1 && j < h; j++)
        for (int i = x0; i < x1 && i < w; i++) {
          uint32_t p = f[(size_t)j * stride + i];
          r += p >> 16 & 255; g += p >> 8 & 255; b += p & 255; cnt++;
        }
      out[y * SST_TW + x] = cnt ? 0xFF000000u | (r / cnt) << 16 | (g / cnt) << 8 | (b / cnt) : 0xFF000000u;
    }
  }
}

static void slot_path(const char *dir, int slot, char *out, size_t n) { snprintf(out, n, "%s/slot%d.sst", dir, slot); }

int sst_save(const char *dir, int slot, const struct sst_ops *ops, const uint32_t thumb[SST_TW * SST_TH], int64_t now) {
  char path[760], tmp[780];
  struct hdr h;
  size_t n;
  uint8_t *buf;
  FILE *f;
  int ok;
  if (slot < 0 || slot >= SST_SLOTS || !ops || !ops->size || !ops->save) return SST_ERR_ARG;
  n = ops->size(ops->u);
  if (!n) return SST_ERR_CORE;
  buf = malloc(n);
  if (!buf) return SST_ERR_IO;
  if (!ops->save(ops->u, buf, n)) { free(buf); return SST_ERR_CORE; }
  memset(&h, 0, sizeof h);
  h.magic = MAGIC; h.version = VERSION; h.core_size = (uint32_t)n; h.tw = SST_TW; h.th = SST_TH; h.time = now; h.state_size = (uint32_t)n;
  slot_path(dir, slot, path, sizeof path);
  snprintf(tmp, sizeof tmp, "%s.part", path);
  f = fopen(tmp, "wb");
  if (!f) { free(buf); return SST_ERR_IO; }
  ok = fwrite(&h, sizeof h, 1, f) == 1 && fwrite(thumb, sizeof(uint32_t), SST_TW * SST_TH, f) == (size_t)SST_TW * SST_TH && fwrite(buf, 1, n, f) == n;
  ok = (fflush(f) == 0) && ok;
  fsync(fileno(f));
  ok = (fclose(f) == 0) && ok;
  free(buf);
  if (!ok || rename(tmp, path) != 0) return SST_ERR_IO;
  return SST_OK;
}

static int read_hdr(FILE *f, struct hdr *h, size_t size) {
  if (fread(h, sizeof *h, 1, f) != 1) return 0;
  if (h->magic != MAGIC || h->version != VERSION || h->tw != SST_TW || h->th != SST_TH) return 0;
  if (size && h->core_size != size) return 0;
  return 1;
}

int sst_info(const char *dir, int slot, size_t size, struct sst_info *info) {
  char path[760];
  struct hdr h;
  FILE *f;
  memset(info, 0, sizeof *info);
  if (slot < 0 || slot >= SST_SLOTS) return SST_ERR_ARG;
  slot_path(dir, slot, path, sizeof path);
  f = fopen(path, "rb");
  if (!f) return SST_ERR_EMPTY;
  if (read_hdr(f, &h, size) && fread(info->thumb, sizeof(uint32_t), SST_TW * SST_TH, f) == (size_t)SST_TW * SST_TH) { info->used = 1; info->time = h.time; }
  else memset(info, 0, sizeof *info);
  fclose(f);
  return info->used ? SST_OK : SST_ERR_EMPTY;
}

int sst_load(const char *dir, int slot, const struct sst_ops *ops) {
  char path[760];
  struct hdr h;
  FILE *f;
  uint8_t *buf;
  int ok;
  size_t want;
  if (slot < 0 || slot >= SST_SLOTS || !ops || !ops->size || !ops->load) return SST_ERR_ARG;
  want = ops->size(ops->u);
  slot_path(dir, slot, path, sizeof path);
  f = fopen(path, "rb");
  if (!f) return SST_ERR_EMPTY;
  if (!read_hdr(f, &h, want) || h.state_size != want) { fclose(f); return SST_ERR_EMPTY; }
  if (fseek(f, (long)(sizeof h + sizeof(uint32_t) * SST_TW * SST_TH), SEEK_SET) != 0) { fclose(f); return SST_ERR_IO; }
  buf = malloc(want);
  if (!buf) { fclose(f); return SST_ERR_IO; }
  ok = fread(buf, 1, want, f) == want;
  fclose(f);
  if (!ok) { free(buf); return SST_ERR_IO; }
  ok = ops->load(ops->u, buf, want);
  free(buf);
  return ok ? SST_OK : SST_ERR_CORE;
}

int sst_get_current(const char *dir) {
  char path[760];
  FILE *f;
  int v = 0;
  snprintf(path, sizeof path, "%s/current", dir);
  f = fopen(path, "r");
  if (f) { if (fscanf(f, "%d", &v) != 1) v = 0; fclose(f); }
  return v >= 0 && v < SST_SLOTS ? v : 0;
}

void sst_set_current(const char *dir, int slot) {
  char path[760], tmp[780];
  FILE *f;
  if (slot < 0 || slot >= SST_SLOTS) return;
  snprintf(path, sizeof path, "%s/current", dir);
  snprintf(tmp, sizeof tmp, "%s.part", path);
  f = fopen(tmp, "w");
  if (!f) return;
  fprintf(f, "%d\n", slot);
  if (fclose(f) == 0) rename(tmp, path);
}

void sst_format_time(int64_t t, char *out, size_t n) {
  struct tm tm;
  time_t tt = (time_t)t;
  if (t <= 0) { if (n) out[0] = 0; return; }
  localtime_r(&tt, &tm);
  strftime(out, n, "%Y-%m-%d %H:%M", &tm);
}
