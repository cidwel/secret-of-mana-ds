/* Zone map for the MAP tab: the current area drawn from the map data the game keeps in WRAM plus the graphics and palettes decoded from the ROM
 * (the same tables the game's loader uses, see docs/maps.md).  The host may instead hand over a finished image with som_ux_set_zone_image().
 *
 * WRAM: layer 1 tile ids at $7F:0000 and layer 2 at $7F:4000 (128 ids per row), the four tilemap words of every 16x16 tile at $7F:C000 + 8 * id
 * (layer 1) and $7F:C800 + 8 * id (layer 2), map size in pixels at $C0/$C2 (layer 2: $C4/$C6), camera at $A8/$AA, map id at $DC.
 * ROM: map header at $C8:7000 (graphics set byte 0, palette set byte 1, backdrop byte 5), graphics list $CC:E27E, palettes $CC:8000. */
#include <stdlib.h>
#include <string.h>
#include "som_ui_priv.h"

#define ZONE_PPT 8                         /* image pixels per 16x16 tile */
#define W16(w, a) ((unsigned)((w)[a] | ((w)[(a) + 1] << 8)))

static size_t rom_at(unsigned bank, unsigned off) { return ((size_t)(bank & 0x3F) << 16) + off; }
static unsigned r8(const uint8_t *r, size_t len, size_t o) { return o < len ? r[o] : 0; }
static unsigned r16(const uint8_t *r, size_t len, size_t o) { return r8(r, len, o) | (r8(r, len, o + 1) << 8); }

/* the 8 header bytes of a map, or 0 when it has none of its own (same rule as tools/map_tables.py) */
static int zone_header(const uint8_t *r, size_t len, int m, uint8_t *h) {
  if (m < 0 || m >= 0x1FF) return 0;
  size_t base = rom_at(0xC8, 0);
  unsigned p = r16(r, len, base + 0x7000 + 2u * (unsigned)m), q = r16(r, len, base + 0x7000 + 2u * (unsigned)(m + 1));
  if (q < p + 8) return 0;
  int ff = 1;
  for (int i = 0; i < 8; i++) { h[i] = (uint8_t)r8(r, len, base + p + (unsigned)i); if (h[i] != 0xFF) ff = 0; }
  return !ff;
}

/* characters of graphics set `gset` as the 4bpp bytes the game uploads from VRAM address 0; returns the byte count */
static size_t zone_gfx(const uint8_t *r, size_t len, int gset, uint8_t *out, size_t cap) {
  size_t n = 0;
  unsigned start = r16(r, len, rom_at(0xCC, 0xE27E + 2u * (unsigned)(gset & 0x3F)));
  unsigned end = r16(r, len, rom_at(0xCC, 0xE280 + 2u * (unsigned)(gset & 0x3F)));
  size_t pool[4];
  static const unsigned pidx[4] = {0, 3, 6, 9};
  for (int i = 0; i < 4; i++) pool[i] = rom_at(r8(r, len, rom_at(0xCC, 0xE272 + pidx[i])) | 0xC0, r16(r, len, rom_at(0xCC, 0xE270 + pidx[i])));
  for (unsigned y = start; y < end && y < 0x10000; ) {
    unsigned word = r16(r, len, rom_at(0xCC, y)), count = r8(r, len, rom_at(0xCC, y + 2)) + 1, mode = word >> 13, nn = word & 0x1FFF;
    y += 3;
    for (unsigned t = 0; t < count; t++) {
      if (mode < 4) {
        size_t src = pool[0] + nn * 16u + 16u * t;
        if (n + 32 > cap) return n;
        for (int i = 0; i < 16; i++) out[n++] = (uint8_t)r8(r, len, (src + (size_t)i) & 0x1FFFFF);
        for (int i = 0; i < 8; i++) { out[n++] = (mode & 2) ? 0xFF : 0; out[n++] = (mode & 1) ? 0xFF : 0; }
      } else if (mode < 6) {
        size_t src = pool[1] + nn * 24u + 24u * t;
        if (n + 32 > cap) return n;
        for (int i = 0; i < 16; i++) out[n++] = (uint8_t)r8(r, len, (src + (size_t)i) & 0x1FFFFF);
        for (int i = 0; i < 8; i++) { out[n++] = (uint8_t)r8(r, len, (src + 16u + (size_t)i) & 0x1FFFFF); out[n++] = (mode & 1) ? 0xFF : 0; }
      } else if (mode == 6) {
        size_t src = pool[2] + nn * 32u + 32u * t;
        if (n + 32 > cap) return n;
        for (int i = 0; i < 32; i++) out[n++] = (uint8_t)r8(r, len, (src + (size_t)i) & 0x1FFFFF);
      } else {
        size_t src = pool[3] + nn * 64u + 64u * t;
        if (n + 64 > cap) return n;
        for (int i = 0; i < 64; i++) out[n++] = (uint8_t)r8(r, len, (src + (size_t)i) & 0x1FFFFF);
      }
    }
  }
  return n;
}

static uint32_t bgr555(unsigned c) {
  unsigned r5 = c & 31, g5 = (c >> 5) & 31, b5 = (c >> 10) & 31;
  return 0xFF000000u | (((r5 << 3) | (r5 >> 2)) << 16) | (((g5 << 3) | (g5 >> 2)) << 8) | ((b5 << 3) | (b5 >> 2));
}

/* palette rows 1..7 colours 1..15 from the palette set, colour 0 of row 0 = backdrop (as $C1:F62D / $C2:AA60 build $7E:0600) */
static void zone_palette(const uint8_t *r, size_t len, const uint8_t *hdr, uint32_t *pal /* 128 */) {
  memset(pal, 0, 128 * sizeof pal[0]);
  size_t base = rom_at(0xCC, 0x8000) + 0xD2u * (hdr[1] & 0x7Fu);
  for (int row = 1; row < 8; row++)
    for (int i = 0; i < 15; i++) pal[row * 16 + 1 + i] = bgr555(r16(r, len, base + 30u * (unsigned)(row - 1) + 2u * (unsigned)i));
  pal[0] = bgr555(r16(r, len, rom_at(0xC8, 0x0100) + 9u * (hdr[5] & 0x3Fu) + 7));
}

static uint32_t zone_hash(const uint8_t *w, int tw, int th, int tw2, int th2) {
  uint32_t h = 2166136261u;
  for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) { h ^= w[0x10000 + y * 128 + x]; h *= 16777619u; }
  for (int y = 0; y < th2; y++) for (int x = 0; x < tw2; x++) { h ^= w[0x14000 + y * 128 + x]; h *= 16777619u; }
  return h;
}

static void zone_draw_layer(uint32_t *img, int iw, const uint8_t *w, unsigned ids_base, unsigned words_base, int tw, int th, const uint8_t *vram, size_t vlen,
                            const uint32_t *pal) {
  for (int ty = 0; ty < th; ty++)
    for (int tx = 0; tx < tw; tx++) {
      unsigned id = w[ids_base + (unsigned)ty * 128u + (unsigned)tx];
      for (int k = 0; k < 4; k++) {
        unsigned e = W16(w, words_base + 8u * id + 2u * (unsigned)k);
        unsigned ch = e & 0x3FF, pl = (e >> 10) & 7;
        if ((size_t)(ch + 1) * 32 > vlen) continue;
        const uint8_t *t = vram + ch * 32;
        int ox = tx * ZONE_PPT + (k & 1) * 4, oy = ty * ZONE_PPT + (k >> 1) * 4;
        for (int py = 0; py < 4; py++)
          for (int px = 0; px < 4; px++) {
            unsigned rr = 0, gg = 0, bb = 0, n = 0;
            for (int sy = 0; sy < 2; sy++)
              for (int sx = 0; sx < 2; sx++) {
                int yy = py * 2 + sy, xx = px * 2 + sx;
                if (e & 0x8000) yy = 7 - yy;
                if (e & 0x4000) xx = 7 - xx;
                int bit = 7 - xx;
                unsigned idx = ((t[2 * yy] >> bit) & 1) | (((t[2 * yy + 1] >> bit) & 1) << 1) | (((t[16 + 2 * yy] >> bit) & 1) << 2) | (((t[17 + 2 * yy] >> bit) & 1) << 3);
                if (!idx) continue;
                uint32_t c = pal[pl * 16 + idx];
                rr += (c >> 16) & 255; gg += (c >> 8) & 255; bb += c & 255; n++;
              }
            if (n) img[(oy + py) * iw + ox + px] = 0xFF000000u | ((rr / n) << 16) | ((gg / n) << 8) | (bb / n);
          }
      }
    }
}

void ux_zone_update(struct som_ux *ux, const uint8_t *w) {
  if (ux->mode == SOM_UX_BOOT || ux->mode == SOM_UX_FRONTEND || ux->mode == SOM_UX_FLAMMIE) return;
  ux->cam_x = (uint16_t)W16(w, 0xA8); ux->cam_y = (uint16_t)W16(w, 0xAA);
  int mw = (int)W16(w, 0xC0), mh = (int)W16(w, 0xC2), m2w = (int)W16(w, 0xC4), m2h = (int)W16(w, 0xC6);
  int tw = mw / 16, th = mh / 16, tw2 = m2w / 16, th2 = m2h / 16;
  if (tw < 1 || th < 1 || tw > 128 || th > 128 || mw % 16 || mh % 16) return;
  if (tw2 > 128 || th2 > 128) tw2 = th2 = 0;
  int map = (int)W16(w, 0xDC);
  if (ux->e4 != 1 || ux->e6 != 0x0F) return;                     /* faded: the map buffers may be half loaded */
  ux->zone_tiles_w = (uint8_t)(tw > 255 ? 255 : tw); ux->zone_tiles_h = (uint8_t)(th > 255 ? 255 : th);
  if (ux->zone_ext) {                                             /* host image for this map: keep it, drop it when the map changes */
    if (ux->zone_map == map) return;
    free(ux->zone); ux->zone = NULL; ux->zone_ok = 0; ux->zone_ext = 0;
  }
  size_t len; const uint8_t *r = ux_rom(ux, &len);
  if (!r) return;
  uint32_t sum = zone_hash(w, tw, th, tw2, th2);
  if (ux->zone_ok && ux->zone_map == map && ux->zone_sum == sum) return;
  uint8_t hdr[8];
  if (!zone_header(r, len, map, hdr)) { ux->zone_ok = 0; return; }
  uint8_t *vram = (uint8_t *)malloc(0x10000);
  uint32_t *img = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)tw * ZONE_PPT * (size_t)th * ZONE_PPT);
  if (!vram || !img) { free(vram); free(img); return; }
  size_t vlen = zone_gfx(r, len, hdr[0] & 0x3F, vram, 0x10000);
  uint32_t pal[128];
  zone_palette(r, len, hdr, pal);
  int iw = tw * ZONE_PPT, ih = th * ZONE_PPT;
  for (int i = 0; i < iw * ih; i++) img[i] = pal[0];
  if (tw2 && th2) zone_draw_layer(img, iw, w, 0x14000, 0x1C800, tw2 < tw ? tw2 : tw, th2 < th ? th2 : th, vram, vlen, pal);
  zone_draw_layer(img, iw, w, 0x10000, 0x1C000, tw, th, vram, vlen, pal);
  free(vram);
  free(ux->zone);
  ux->zone = img; ux->zone_w = iw; ux->zone_h = ih; ux->zone_ppt = ZONE_PPT; ux->zone_map = map; ux->zone_sum = sum; ux->zone_ok = 1; ux->zone_ext = 0;
}

void som_ux_set_zone_image(struct som_ux *ux, const uint32_t *argb, int w, int h, int map_id, int px_per_tile) {
  if (!argb || w < 1 || h < 1 || w > 4096 || h > 4096 || px_per_tile < 1) return;
  uint32_t *img = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)w * (size_t)h);
  if (!img) return;
  for (int i = 0; i < w * h; i++) img[i] = 0xFF000000u | argb[i];
  free(ux->zone);
  ux->zone = img; ux->zone_w = w; ux->zone_h = h; ux->zone_ppt = px_per_tile; ux->zone_map = map_id; ux->zone_ok = 1; ux->zone_ext = 1;
}
void som_ux_clear_zone_image(struct som_ux *ux) { free(ux->zone); ux->zone = NULL; ux->zone_ok = 0; ux->zone_ext = 0; ux->zone_sum = 0; }
int som_ux_zone_info(const struct som_ux *ux, int *w, int *h, int *map_id) {
  if (w) *w = ux->zone_w;
  if (h) *h = ux->zone_h;
  if (map_id) *map_id = ux->zone_map;
  return ux->zone_ok;
}
const uint32_t *som_ux_zone_pixels(const struct som_ux *ux) { return ux->zone; }
