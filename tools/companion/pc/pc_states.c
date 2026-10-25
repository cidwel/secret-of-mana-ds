/* Save state slots of the SoM PC frontend, see pc_states.h. */
#define _POSIX_C_SOURCE 200809L
#include "pc_states.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pc_store.h"

/* File layout (little endian):
 *   0 magic "SOMPCST1"   8 version   12 header size (the offset of the thumbnail)   16 time (u64)   24 rom key (24 bytes, NUL padded)
 *  48 core signature     52 payload length   56 payload CRC   60 thumb w (u16)   62 thumb h (u16)   64 thumb CRC   68 header CRC (of 0..67)
 *  72 thumbnail, w * h * 4 bytes (0xAARRGGBB as u32)   then the payload. */
#define HDR 72
static const char MAGIC[8] = {'S', 'O', 'M', 'P', 'C', 'S', 'T', '1'};

uint32_t pcs_crc32(const void *p, size_t n) {
  static uint32_t tab[256];
  static int init;
  const uint8_t *b = p;
  uint32_t c = 0xFFFFFFFFu;
  if (!init) {
    for (uint32_t i = 0; i < 256; i++) { uint32_t v = i; for (int k = 0; k < 8; k++) v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1; tab[i] = v; }
    init = 1;
  }
  for (size_t i = 0; i < n; i++) c = tab[(c ^ b[i]) & 0xFF] ^ (c >> 8);
  return ~c;
}

static void put32(uint8_t *d, uint32_t v) { d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); d[2] = (uint8_t)(v >> 16); d[3] = (uint8_t)(v >> 24); }
static void put64(uint8_t *d, uint64_t v) { put32(d, (uint32_t)v); put32(d + 4, (uint32_t)(v >> 32)); }
static uint32_t get32(const uint8_t *d) { return (uint32_t)d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24); }
static uint64_t get64(const uint8_t *d) { return get32(d) | ((uint64_t)get32(d + 4) << 32); }

int pcs_dir(char *out, size_t n, const char *base, const char *rom_key) {
  int r = snprintf(out, n, "%s/states/%s", base, rom_key);
  return r < 0 || (size_t)r >= n ? -1 : 0;
}
int pcs_path(char *out, size_t n, const char *base, const char *rom_key, int slot) {
  int r = snprintf(out, n, "%s/states/%s/slot%02d.state", base, rom_key, slot + 1);
  return slot < 0 || slot >= PCS_SLOTS || r < 0 || (size_t)r >= n ? -1 : 0;
}

void pcs_make_thumb(struct pcs_thumb *t, const uint32_t *frame, int fw, int fh, int stride) {
  int tw, th = PCS_THUMB_H;
  memset(t, 0, sizeof *t);
  if (!frame || fw < 1 || fh < 1) { t->w = 0; t->h = 0; return; }
  tw = (int)((int64_t)fw * th / fh);
  if (tw < 8) tw = 8;
  if (tw > PCS_THUMB_MAX_W) tw = PCS_THUMB_MAX_W;
  t->w = tw; t->h = th;
  for (int y = 0; y < th; y++) {
    int y0 = (int)((int64_t)y * fh / th), y1 = (int)((int64_t)(y + 1) * fh / th);
    if (y1 <= y0) y1 = y0 + 1;
    for (int x = 0; x < tw; x++) {
      int x0 = (int)((int64_t)x * fw / tw), x1 = (int)((int64_t)(x + 1) * fw / tw);
      uint32_t r = 0, g = 0, b = 0, k = 0;
      if (x1 <= x0) x1 = x0 + 1;
      for (int yy = y0; yy < y1 && yy < fh; yy++)
        for (int xx = x0; xx < x1 && xx < fw; xx++) { uint32_t p = frame[(size_t)yy * stride + xx]; r += (p >> 16) & 255; g += (p >> 8) & 255; b += p & 255; k++; }
      if (!k) k = 1;
      t->px[y * tw + x] = 0xFF000000u | ((r / k) << 16) | ((g / k) << 8) | (b / k);
    }
  }
}

int pcs_save(const char *base, const char *rom_key, int slot, uint32_t core_sig, const void *state, size_t n, const struct pcs_thumb *thumb, int64_t time) {
  char dir[900], path[1000], bak[1010];
  size_t tn = thumb && thumb->w > 0 && thumb->h > 0 ? (size_t)thumb->w * thumb->h * 4 : 0, total = HDR + tn + n;
  uint8_t *buf;
  int r;
  if (pcs_dir(dir, sizeof dir, base, rom_key) != 0 || pcs_path(path, sizeof path, base, rom_key, slot) != 0 || n == 0 || n > 0x7FFFFFFFu) return PCS_IO;
  if (pc_mkdirs(dir) != 0) return PCS_IO;
  buf = calloc(1, total);
  if (!buf) return PCS_IO;
  memcpy(buf, MAGIC, 8);
  put32(buf + 8, PCS_VERSION); put32(buf + 12, HDR); put64(buf + 16, (uint64_t)time);
  snprintf((char *)buf + 24, 24, "%s", rom_key);
  put32(buf + 48, core_sig); put32(buf + 52, (uint32_t)n); put32(buf + 56, pcs_crc32(state, n));
  buf[60] = (uint8_t)(tn ? thumb->w : 0); buf[61] = 0; buf[62] = (uint8_t)(tn ? thumb->h : 0); buf[63] = 0;
  for (size_t i = 0; i < tn / 4; i++) put32(buf + HDR + 4 * i, thumb->px[i]);
  put32(buf + 64, pcs_crc32(buf + HDR, tn));
  put32(buf + 68, pcs_crc32(buf, 68));
  memcpy(buf + HDR + tn, state, n);
  /* The previous file becomes the backup only when it still verifies, so a damaged file never overwrites a good backup. */
  snprintf(bak, sizeof bak, "%s.bak", path);
  {
    struct pcs_meta m;
    if (access(path, R_OK) == 0 && pcs_info(base, rom_key, slot, core_sig, &m, NULL) == PCS_OK && !m.from_backup) {
      size_t on;
      uint8_t *old = pc_slurp(path, &on);
      if (old) { pc_spit(bak, old, on); free(old); }
    }
  }
  r = pc_spit(path, buf, total);
  free(buf);
  return r == 0 ? PCS_OK : PCS_IO;
}

/* Parses one file.  Returns a status; on PCS_OK fills meta/thumb and points *payload into data. */
static int parse(const uint8_t *d, size_t len, const char *rom_key, uint32_t core_sig, struct pcs_meta *meta, struct pcs_thumb *thumb, const uint8_t **payload) {
  uint32_t hl, pl, tw, th;
  size_t tn;
  if (len < HDR || memcmp(d, MAGIC, 8) != 0) return PCS_BAD;
  if (get32(d + 68) != pcs_crc32(d, 68)) return PCS_BAD;
  if (get32(d + 8) != PCS_VERSION) return PCS_INCOMPAT;
  hl = get32(d + 12); pl = get32(d + 52); tw = d[60] | ((uint32_t)d[61] << 8); th = d[62] | ((uint32_t)d[63] << 8);
  if (hl != HDR || tw > PCS_THUMB_MAX_W || th > PCS_THUMB_H) return PCS_BAD;
  tn = (size_t)tw * th * 4;
  if (len != HDR + tn + pl) return PCS_BAD;
  if (get32(d + 64) != pcs_crc32(d + HDR, tn) || get32(d + 56) != pcs_crc32(d + HDR + tn, pl)) return PCS_BAD;
  if (strncmp((const char *)d + 24, rom_key, 24) != 0 || get32(d + 48) != core_sig) return PCS_INCOMPAT;
  if (meta) { meta->time = (int64_t)get64(d + 16); meta->payload = pl; meta->version = (int)get32(d + 8); }
  if (thumb) {
    memset(thumb, 0, sizeof *thumb);
    thumb->w = (int)tw; thumb->h = (int)th;
    for (size_t i = 0; i < tn / 4; i++) thumb->px[i] = get32(d + HDR + 4 * i);
  }
  *payload = d + HDR + tn;
  return PCS_OK;
}

int pcs_load(const char *base, const char *rom_key, int slot, uint32_t core_sig, uint8_t **state, size_t *n, struct pcs_meta *meta, struct pcs_thumb *thumb) {
  char path[1000], bak[1010];
  struct pcs_meta local;
  int st = PCS_EMPTY, worst = PCS_EMPTY;
  if (!meta) meta = &local;
  memset(meta, 0, sizeof *meta);
  if (state) *state = NULL;
  if (n) *n = 0;
  if (pcs_path(path, sizeof path, base, rom_key, slot) != 0) { meta->status = PCS_EMPTY; return PCS_EMPTY; }
  snprintf(bak, sizeof bak, "%s.bak", path);
  for (int pass = 0; pass < 2; pass++) {
    size_t len;
    uint8_t *d = pc_slurp(pass ? bak : path, &len);
    const uint8_t *pay = NULL;
    if (!d) continue;
    st = parse(d, len, rom_key, core_sig, meta, thumb, &pay);
    if (st == PCS_OK) {
      meta->status = PCS_OK; meta->from_backup = pass;
      if (state) {
        *state = malloc(meta->payload ? meta->payload : 1);
        if (!*state) { free(d); meta->status = PCS_IO; return PCS_IO; }
        memcpy(*state, pay, meta->payload);
        if (n) *n = meta->payload;
      }
      free(d);
      return PCS_OK;
    }
    free(d);
    if (worst == PCS_EMPTY || st == PCS_INCOMPAT) worst = st;   /* an incompatible file is reported as such: it is not damaged */
  }
  memset(meta, 0, sizeof *meta);
  meta->status = worst;
  return worst;
}

int pcs_info(const char *base, const char *rom_key, int slot, uint32_t core_sig, struct pcs_meta *meta, struct pcs_thumb *thumb) {
  return pcs_load(base, rom_key, slot, core_sig, NULL, NULL, meta, thumb);
}
