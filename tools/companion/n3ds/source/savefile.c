#include <stdio.h>
#include <string.h>
#include "savefile.h"

#define MAGIC "SOM3STA1"
struct hdr { char magic[8]; uint32_t rom_crc, state_size, stamp, thumb_bytes, state_crc; };

uint32_t sf_crc32(uint32_t crc, const void *data, size_t n) {
  static uint32_t table[256];
  if (!table[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; table[i] = c; }
  const uint8_t *p = data;
  crc = ~crc;
  for (size_t i = 0; i < n; i++) crc = table[(crc ^ p[i]) & 255] ^ (crc >> 8);
  return ~crc;
}

void sf_key_from_path(const char *rom_path, int original, char *key, size_t n) {
  if (original) { snprintf(key, n, "original"); return; }
  const char *b = strrchr(rom_path, '/');
  b = b ? b + 1 : rom_path;
  size_t l = strlen(b), cut = l;
  const char *dot = strrchr(b, '.');
  if (dot) cut = (size_t)(dot - b);
  if (cut >= n) cut = n - 1;
  memcpy(key, b, cut);
  key[cut] = 0;
}
void sf_srm_path(const char *dir, const char *key, char *out, size_t n) { snprintf(out, n, "%s/%s.srm", dir, key); }
void sf_state_path(const char *dir, const char *key, int slot, char *out, size_t n) { snprintf(out, n, "%s/%s.%d.sta", dir, key, slot); }

static int write_direct(const char *path, const void *data, size_t n) {
  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  size_t w = n ? fwrite(data, 1, n, f) : 0;
  int rc = fclose(f);
  return (w == n && rc == 0) ? 0 : -1;
}
int sf_write_file(const char *path, const void *data, size_t n) {
  char tmp[300];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  if (write_direct(tmp, data, n) == 0 && rename(tmp, path) == 0) return 0;
  return write_direct(path, data, n);
}

size_t sf_srm_load(const char *path, uint8_t *dst, size_t n) {
  FILE *f = fopen(path, "rb");
  if (!f) return 0;
  size_t r = fread(dst, 1, n, f);
  fclose(f);
  return r;
}
int sf_srm_save(const char *path, const uint8_t *src, size_t n) { return sf_write_file(path, src, n); }

static int state_write(const char *path, const struct hdr *h, const uint16_t *thumb, const void *state, size_t n) {
  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  int ok = fwrite(h, sizeof *h, 1, f) == 1 && (!h->thumb_bytes || fwrite(thumb, 1, h->thumb_bytes, f) == h->thumb_bytes) && fwrite(state, 1, n, f) == n;
  if (fclose(f) != 0) ok = 0;
  return ok ? 0 : -1;
}
int sf_state_save(const char *path, uint32_t rom_crc, uint32_t stamp, const uint16_t *thumb, const void *state, size_t n) {
  char tmp[300];
  struct hdr h;
  memcpy(h.magic, MAGIC, 8);
  h.rom_crc = rom_crc; h.state_size = (uint32_t)n; h.stamp = stamp;
  h.thumb_bytes = thumb ? SF_THUMB_W * SF_THUMB_H * 2 : 0;
  h.state_crc = sf_crc32(0, state, n);
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  if (state_write(tmp, &h, thumb, state, n) == 0 && rename(tmp, path) == 0) return 0;
  return state_write(path, &h, thumb, state, n);   /* rename onto an existing file refused: write in place */
}

static int read_hdr(FILE *f, struct hdr *h) {
  return fread(h, sizeof *h, 1, f) == 1 && memcmp(h->magic, MAGIC, 8) == 0 && h->thumb_bytes <= SF_THUMB_W * SF_THUMB_H * 2 && h->state_size > 0 && h->state_size < (16u << 20);
}
int sf_state_info(const char *path, struct sf_info *out) {
  memset(out, 0, sizeof *out);
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  struct hdr h;
  int ok = read_hdr(f, &h);
  fclose(f);
  if (!ok) return -1;
  out->exists = 1; out->rom_crc = h.rom_crc; out->state_size = h.state_size; out->stamp = h.stamp;
  return 0;
}
int sf_state_thumb(const char *path, uint16_t *thumb) {
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  struct hdr h;
  int ok = read_hdr(f, &h) && h.thumb_bytes == SF_THUMB_W * SF_THUMB_H * 2 && fread(thumb, 1, h.thumb_bytes, f) == h.thumb_bytes;
  fclose(f);
  return ok ? 0 : -1;
}
long sf_state_load(const char *path, uint32_t rom_crc, void *buf, size_t cap) {
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  struct hdr h;
  if (!read_hdr(f, &h)) { fclose(f); return -1; }
  if (h.rom_crc != rom_crc) { fclose(f); return -2; }
  if (h.state_size > cap || fseek(f, (long)h.thumb_bytes, SEEK_CUR) != 0 || fread(buf, 1, h.state_size, f) != h.state_size) { fclose(f); return -3; }
  fclose(f);
  if (sf_crc32(0, buf, h.state_size) != h.state_crc) return -3;
  return (long)h.state_size;
}

void sf_thumb_from_frame(const uint32_t *frame, unsigned w, unsigned h, size_t stride, uint16_t *out) {
  for (unsigned y = 0; y < SF_THUMB_H; y++)
    for (unsigned x = 0; x < SF_THUMB_W; x++) {
      unsigned x0 = x * w / SF_THUMB_W, x1 = (x + 1) * w / SF_THUMB_W, y0 = y * h / SF_THUMB_H, y1 = (y + 1) * h / SF_THUMB_H;
      if (x1 == x0) x1 = x0 + 1;
      if (y1 == y0) y1 = y0 + 1;
      unsigned r = 0, g = 0, b = 0, n = 0;
      for (unsigned j = y0; j < y1 && j < h; j++)
        for (unsigned i = x0; i < x1 && i < w; i++) { uint32_t p = frame[j * stride + i]; r += p >> 16 & 255; g += p >> 8 & 255; b += p & 255; n++; }
      if (!n) n = 1;
      out[y * SF_THUMB_W + x] = (uint16_t)(((r / n) >> 3) << 11 | ((g / n) >> 2) << 5 | ((b / n) >> 3));
    }
}
void sf_thumb_to_rgb(const uint16_t *in, uint32_t *out) {
  for (int i = 0; i < SF_THUMB_W * SF_THUMB_H; i++) {
    unsigned p = in[i], r = (p >> 11) * 255 / 31, g = ((p >> 5) & 63) * 255 / 63, b = (p & 31) * 255 / 31;
    out[i] = r << 16 | g << 8 | b;
  }
}
