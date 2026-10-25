#include "som_enemy_sprite.h"
#include <string.h>

#define E_BASE 0xE000u
#define E_STRIDE 0x200u

/* OBSEL size select -> {small w, small h, large w, large h} in pixels */
static const uint8_t es_sizes[8][4] = {
  {8, 8, 16, 16}, {8, 8, 32, 32}, {8, 8, 64, 64}, {16, 16, 32, 32},
  {16, 16, 64, 64}, {32, 32, 64, 64}, {16, 32, 32, 64}, {16, 32, 32, 32},
};

struct es_spr { int x, y, w, h; };

static void es_geom(const struct som_es_src *s, int i, struct es_spr *g) {
  const uint8_t *o = s->oam;
  int hi = (o[512 + (i >> 2)] >> ((i & 3) * 2)) & 3;
  int large = (hi >> 1) & 1;
  const uint8_t *sz = es_sizes[(s->obsel >> 5) & 7];
  g->w = sz[large ? 2 : 0]; g->h = sz[large ? 3 : 1];
  int x = o[4 * i] | ((hi & 1) << 8);
  if (x >= 256) x -= 512;
  int y = o[4 * i + 1];
  if (y + g->h > 256) y -= 256;                    /* wraps to the top of the screen */
  g->x = x; g->y = y + 1;                           /* the PPU draws a sprite one line lower */
}

static int es_onscreen(const struct es_spr *g) {
  return g->x + g->w > 0 && g->x < 256 && g->y + g->h > 0 && g->y < 224;
}

static uint32_t es_color(const uint8_t *cg, int idx) {
  unsigned v = cg[2 * idx] | (cg[2 * idx + 1] << 8);
  unsigned r = v & 31, g = (v >> 5) & 31, b = (v >> 10) & 31;
  r = (r << 3) | (r >> 2); g = (g << 3) | (g >> 2); b = (b << 3) | (b >> 2);
  return r | (g << 8) | (b << 16) | 0xFF000000u;
}

/* draws sprite i into the canvas (cw x ch, origin ox, oy in screen coordinates) */
static void es_draw(const struct som_es_src *s, int i, uint8_t *cv, int cw, int ch, int ox, int oy) {
  struct es_spr g; es_geom(s, i, &g);
  const uint8_t *o = s->oam;
  unsigned tile = o[4 * i + 2], attr = o[4 * i + 3];
  int pal = (attr >> 1) & 7, hf = (attr >> 6) & 1, vf = (attr >> 7) & 1, nt = attr & 1;
  unsigned base = (unsigned)(s->obsel & 7) * 0x4000u;
  if (nt) base += (unsigned)(((s->obsel >> 3) & 3) + 1) * 0x2000u;
  uint8_t cols[16][4];
  for (int k = 0; k < 16; k++) { uint32_t c = es_color(s->cgram, 128 + pal * 16 + k); memcpy(cols[k], &c, 4); }
  for (int py = 0; py < g.h; py++) {
    int sy = vf ? g.h - 1 - py : py;
    int ty = sy >> 3, r = sy & 7;
    int dy = g.y + py - oy;
    if (dy < 0 || dy >= ch) continue;
    for (int px = 0; px < g.w; px++) {
      int sx = hf ? g.w - 1 - px : px;
      int tx = sx >> 3, b = 7 - (sx & 7);
      unsigned row = (((tile >> 4) + (unsigned)ty) & 15), col = ((tile & 15) + (unsigned)tx) & 15;
      unsigned a = (base + (row * 16 + col) * 32u + (unsigned)r * 2) & 0xFFFF;
      const uint8_t *v = s->vram;
      unsigned c = ((v[a] >> b) & 1) | (((v[(a + 1) & 0xFFFF] >> b) & 1) << 1) |
                   (((v[(a + 16) & 0xFFFF] >> b) & 1) << 2) | (((v[(a + 17) & 0xFFFF] >> b) & 1) << 3);
      if (!c) continue;
      int dx = g.x + px - ox;
      if (dx < 0 || dx >= cw) continue;
      memcpy(cv + ((size_t)dy * cw + dx) * 4, cols[c], 4);
    }
  }
}

/* renders the sprites flagged in use[] (descending index so that the lowest index ends in front), trims, pads */
static int es_compose(const struct som_es_src *s, const uint8_t *use, int foot_x, int foot_y, struct som_es_img *out) {
  int x0 = 1 << 20, y0 = 1 << 20, x1 = -(1 << 20), y1 = -(1 << 20), n = 0;
  for (int i = 0; i < 128; i++) {
    if (!use[i]) continue;
    struct es_spr g; es_geom(s, i, &g);
    if (g.x < x0) x0 = g.x;
    if (g.y < y0) y0 = g.y;
    if (g.x + g.w > x1) x1 = g.x + g.w;
    if (g.y + g.h > y1) y1 = g.y + g.h;
    n++;
  }
  if (!n) return 0;
  int cw = x1 - x0, ch = y1 - y0;
  if (cw > SOM_ES_MAX_SIDE || ch > SOM_ES_MAX_SIDE) return 0;
  uint8_t *cv = out->rgba;
  memset(cv, 0, (size_t)cw * ch * 4);
  for (int i = 127; i >= 0; i--) if (use[i]) es_draw(s, i, cv, cw, ch, x0, y0);
  int bx0 = cw, by0 = ch, bx1 = -1, by1 = -1;
  for (int y = 0; y < ch; y++)
    for (int x = 0; x < cw; x++)
      if (cv[((size_t)y * cw + x) * 4 + 3]) {
        if (x < bx0) bx0 = x;
        if (x > bx1) bx1 = x;
        if (y < by0) by0 = y;
        if (y > by1) by1 = y;
      }
  if (bx1 < 0) return 0;
  int w = bx1 - bx0 + 1, h = by1 - by0 + 1;
  /* move the trimmed rows to the front of the buffer, then spread them into the padded layout (back to front) */
  for (int y = 0; y < h; y++) memmove(cv + (size_t)y * w * 4, cv + ((size_t)(by0 + y) * cw + bx0) * 4, (size_t)w * 4);
  int pw = w + 2, ph = h + 2;
  for (int y = h - 1; y >= 0; y--) memmove(cv + ((size_t)(y + 1) * pw + 1) * 4, cv + (size_t)y * w * 4, (size_t)w * 4);
  memset(cv, 0, (size_t)pw * 4);
  for (int y = 1; y <= h; y++) { memset(cv + (size_t)y * pw * 4, 0, 4); memset(cv + ((size_t)y * pw + pw - 1) * 4, 0, 4); }
  memset(cv + (size_t)(ph - 1) * pw * 4, 0, (size_t)pw * 4);
  out->w = pw; out->h = ph;
  out->foot_x = foot_x - (x0 + bx0) + 1;
  out->foot_y = foot_y - (y0 + by0) + 1;
  out->n_sprites = n;
  return 1;
}

static int sb(uint8_t v) { return v >= 128 ? (int)v - 256 : (int)v; }

int som_es_render_actor(const struct som_es_src *src, const uint8_t *wram, int slot, struct som_es_img *out) {
  if (!src || !src->oam || !src->vram || !src->cgram || !wram || !out || !out->rgba || slot < 3 || slot > 15) return 0;
  const uint8_t *r = wram + E_BASE + E_STRIDE * (unsigned)slot;
  out->w = out->h = out->n_sprites = out->n_pieces = out->n_matched = 0;
  if (r[0] != 1) return 0;
  int sx = r[0x20] | (r[0x21] << 8), sy = r[0x22] | (r[0x23] << 8);
  if (sx >= 0x8000) sx -= 0x10000;
  if (sy >= 0x8000) sy -= 0x10000;
  /* ordinary actors: up to 15 pieces {x, y, tile, attr} at +0x90; boss-engine actors: count + pieces in the workspace
   * $F800 + 0x80 * (slot - 3), whose flips and priority are rewritten by the emitter (so only tile and palette compare) */
  int n = r[0x83] & 15, boss = 0;
  const uint8_t *pcs = r + 0x90;
  if (n == 0) {
    const uint8_t *ws = wram + 0xF800 + 0x80u * (unsigned)(slot - 3);
    n = ws[0] > 31 ? 31 : ws[0]; pcs = ws + 1; boss = 1;
  }
  int ybase = sy - (r[0x74] & 0x7F) - r[0x45];
  uint8_t use[128];
  memset(use, 0, sizeof use);
  int matched = 0;
  uint8_t done[32];
  const uint8_t *oam = src->oam;
  memset(done, 0, sizeof done);
  /* pass 0: tile and attribute equal (ordinary actors) or tile and palette equal (boss engine); pass 1: tile and palette
   * for the pieces still missing (a flash or priority change rewrote the attribute) */
  for (int pass = boss ? 1 : 0; pass < 2; pass++)
    for (int p = 0; p < n; p++) {
      if (done[p]) continue;
      const uint8_t *pc = pcs + 4 * p;
      int ex = sx + sb(pc[0]), ey = ybase + sb(pc[1]);
      int best = -1, bd = 1 << 30;
      for (int i = 0; i < 128; i++) {
        if (use[i] || oam[4 * i + 2] != pc[2]) continue;
        if (pass == 0 ? oam[4 * i + 3] != pc[3] : (oam[4 * i + 3] & 0x0F) != (pc[3] & 0x0F)) continue;
        struct es_spr g; es_geom(src, i, &g);
        int d = (g.x - ex) * (g.x - ex) + (g.y - 1 - ey) * (g.y - 1 - ey);
        if (d < bd) { bd = d; best = i; }
      }
      int lim = boss ? 160 * 160 : (pass == 0 ? 32 * 32 : 64 * 64);
      if (best >= 0 && bd <= lim) { use[best] = 1; done[p] = 1; matched++; }
    }
  out->n_pieces = n; out->n_matched = matched;
  if (!matched) return 0;
  int ok = es_compose(src, use, sx, sy, out);
  out->n_pieces = n; out->n_matched = matched;
  return ok;
}

int som_es_render_box(const struct som_es_src *src, int x0, int y0, int x1, int y1, int foot_x, int foot_y, struct som_es_img *out) {
  if (!src || !src->oam || !src->vram || !src->cgram || !out || !out->rgba) return 0;
  uint8_t use[128];
  memset(use, 0, sizeof use);
  out->w = out->h = out->n_sprites = out->n_pieces = out->n_matched = 0;
  for (int i = 0; i < 128; i++) {
    struct es_spr g; es_geom(src, i, &g);
    if (!es_onscreen(&g)) continue;
    if (g.x >= x1 || g.x + g.w <= x0 || g.y >= y1 || g.y + g.h <= y0) continue;
    unsigned tile = src->oam[4 * i + 2], attr = src->oam[4 * i + 3];
    if (tile == 0 && (attr & 0x0F) == 2) continue;      /* ground shadow */
    use[i] = 1;
  }
  return es_compose(src, use, foot_x, foot_y, out);
}

void som_es_fit_argb(const struct som_es_img *img, int side, uint32_t *argb) {
  memset(argb, 0, (size_t)side * side * 4);
  if (!img || img->w <= 0 || img->h <= 0 || side <= 0) return;
  int w = img->w, h = img->h;
  int m = w > h ? w : h;
  const uint8_t *p = img->rgba;
  if (m <= side) {                                  /* integer nearest-neighbour enlargement */
    int k = side / m;
    int ow = w * k, oh = h * k, ox = (side - ow) / 2, oy = (side - oh) / 2;
    for (int y = 0; y < oh; y++)
      for (int x = 0; x < ow; x++) {
        const uint8_t *q = p + ((size_t)(y / k) * w + (x / k)) * 4;
        argb[(size_t)(oy + y) * side + ox + x] = ((uint32_t)q[3] << 24) | ((uint32_t)q[0] << 16) | ((uint32_t)q[1] << 8) | q[2];
      }
    return;
  }
  int ow = (w * side + m - 1) / m, oh = (h * side + m - 1) / m;
  if (ow > side) ow = side;
  if (oh > side) oh = side;
  int ox = (side - ow) / 2, oy = (side - oh) / 2;
  for (int y = 0; y < oh; y++) {
    int sy0 = y * h / oh, sy1 = (y + 1) * h / oh; if (sy1 <= sy0) sy1 = sy0 + 1;
    for (int x = 0; x < ow; x++) {
      int sx0 = x * w / ow, sx1 = (x + 1) * w / ow; if (sx1 <= sx0) sx1 = sx0 + 1;
      unsigned long ar = 0, ag = 0, ab = 0, aa = 0, cnt = 0;
      for (int yy = sy0; yy < sy1; yy++)
        for (int xx = sx0; xx < sx1; xx++) {
          const uint8_t *q = p + ((size_t)yy * w + xx) * 4;
          ar += (unsigned long)q[0] * q[3]; ag += (unsigned long)q[1] * q[3]; ab += (unsigned long)q[2] * q[3]; aa += q[3]; cnt++;
        }
      if (!aa) continue;
      uint32_t a = (uint32_t)(aa / cnt), r = (uint32_t)(ar / aa), g = (uint32_t)(ag / aa), b = (uint32_t)(ab / aa);
      argb[(size_t)(oy + y) * side + ox + x] = (a << 24) | (r << 16) | (g << 8) | b;
    }
  }
}

static unsigned es_u16(const uint8_t *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }
static unsigned long es_u32(const uint8_t *p) { return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24); }

int som_es_pack_find(const uint8_t *blob, size_t len, int id, struct som_es_pack_entry *e) {
  if (!blob || !e || len < 16 || memcmp(blob, "SESP", 4) != 0 || es_u16(blob + 4) != 1) return 0;
  size_t n = es_u16(blob + 6);
  if (16 + 24 * n > len) return 0;
  for (size_t i = 0; i < n; i++) {
    const uint8_t *d = blob + 16 + 24 * i;
    if (d[0] != (unsigned)id) continue;
    unsigned w = es_u16(d + 4), h = es_u16(d + 6);
    unsigned long off = es_u32(d + 12), cnt = es_u32(d + 16);
    if (cnt != (unsigned long)w * h * 4 || off > len || cnt > len - off) return 0;
    e->id = id; e->source = d[1]; e->confidence = d[2]; e->w = (int)w; e->h = (int)h;
    e->foot_x = (int16_t)es_u16(d + 8); e->foot_y = (int16_t)es_u16(d + 10);
    e->rgba = blob + off;
    return 1;
  }
  return 0;
}

static void es_to_argb(const uint8_t *rgba, int w, int h, uint32_t *argb) {
  for (int i = 0; i < w * h; i++) {
    const uint8_t *q = rgba + 4 * i;
    argb[i] = ((uint32_t)q[3] << 24) | ((uint32_t)q[0] << 16) | ((uint32_t)q[1] << 8) | q[2];
  }
}

static int es_emit(const uint8_t *rgba, int w, int h, uint32_t *argb, int *ow, int *oh) {
  if (w <= SOM_ES_SPRITE_MAX && h <= SOM_ES_SPRITE_MAX) { es_to_argb(rgba, w, h, argb); *ow = w; *oh = h; return 1; }
  struct som_es_img im;
  im.rgba = (uint8_t *)rgba; im.w = w; im.h = h;
  som_es_fit_argb(&im, SOM_ES_SPRITE_MAX, argb);
  *ow = *oh = SOM_ES_SPRITE_MAX;
  return 1;
}

int som_es_sprite_for_id(const uint8_t *pack, size_t pack_len, const struct som_es_src *live, const uint8_t *wram, int enemy_id,
                         uint8_t *scratch, uint32_t *argb, int *w, int *h, int *conf) {
  struct som_es_pack_entry e;
  int have = pack && som_es_pack_find(pack, pack_len, enemy_id, &e);
  if (have && e.confidence >= 2) { es_emit(e.rgba, e.w, e.h, argb, w, h); *conf = e.confidence; return SOM_ES_FROM_PACK; }
  if (live && wram && scratch) {
    for (int s = 3; s < 16; s++) {
      const uint8_t *r = wram + E_BASE + E_STRIDE * (unsigned)s;
      if (r[0] != 1 || r[0x180] != (unsigned)enemy_id) continue;
      int sx = r[0x20] | (r[0x21] << 8), sy = r[0x22] | (r[0x23] << 8);
      if (sx < 8 || sx >= 0xF8 || sy >= 0xD8) continue;
      struct som_es_img im;
      im.rgba = scratch;
      if (som_es_render_actor(live, wram, s, &im) && im.n_matched == im.n_pieces) { es_emit(scratch, im.w, im.h, argb, w, h); *conf = 3; return SOM_ES_FROM_LIVE; }
    }
  }
  if (have) { es_emit(e.rgba, e.w, e.h, argb, w, h); *conf = e.confidence; return SOM_ES_FROM_PACK; }
  return SOM_ES_FROM_NONE;
}
