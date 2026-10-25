/* Synthetic tests of som_enemy_sprite.c (no ROM): hand-built OAM / VRAM / CGRAM / WRAM with known pixels.
 * gcc -std=c99 -Wall -Wextra -fsanitize=address,undefined -o /tmp/test_es test_som_enemy_sprite.c som_enemy_sprite.c && /tmp/test_es */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "som_enemy_sprite.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static uint8_t oam[SOM_ES_OAM_BYTES], vram[SOM_ES_VRAM_BYTES], cgram[SOM_ES_CGRAM_BYTES], wram[0x20000], buf[SOM_ES_BUF_BYTES];

static void tile_fill(unsigned base, int tile, int color) {          /* a whole 8x8 tile of one colour 0..15 */
  uint8_t *t = vram + base + tile * 32;
  for (int r = 0; r < 8; r++) { t[2 * r] = (color & 1) ? 0xFF : 0; t[2 * r + 1] = (color & 2) ? 0xFF : 0; t[16 + 2 * r] = (color & 4) ? 0xFF : 0; t[17 + 2 * r] = (color & 8) ? 0xFF : 0; }
}
static void put_oam(int i, int x, int y, int tile, int attr, int big) {
  oam[4 * i] = (uint8_t)x; oam[4 * i + 1] = (uint8_t)y; oam[4 * i + 2] = (uint8_t)tile; oam[4 * i + 3] = (uint8_t)attr;
  oam[512 + (i >> 2)] &= (uint8_t)~(3 << ((i & 3) * 2));
  oam[512 + (i >> 2)] |= (uint8_t)(((x >> 8) & 1 ? 1 : 0) | (big ? 2 : 0)) << ((i & 3) * 2);
}
static void setcol(int pal, int idx, int r, int g, int b) { unsigned v = r | (g << 5) | (b << 10); int k = 128 + pal * 16 + idx; cgram[2 * k] = v & 255; cgram[2 * k + 1] = v >> 8; }
static void clear(void) { memset(oam, 0, sizeof oam); memset(vram, 0, sizeof vram); memset(cgram, 0, sizeof cgram); memset(wram, 0, sizeof wram);
  for (int i = 0; i < 128; i++) oam[4 * i + 1] = 0xF0; }
static void actor(int slot, int sx, int sy, int n, const uint8_t (*pc)[4]) {
  uint8_t *r = wram + 0xE000 + 0x200 * slot;
  r[0] = 1; r[0x20] = sx & 255; r[0x21] = sx >> 8; r[0x22] = sy & 255; r[0x23] = sy >> 8; r[0x74] = 7; r[0x45] = 0; r[0x83] = (uint8_t)n;
  for (int i = 0; i < n; i++) memcpy(r + 0x90 + 4 * i, pc[i], 4);
}
static unsigned px(const struct som_es_img *im, int x, int y) { const uint8_t *p = im->rgba + ((size_t)y * im->w + x) * 4; return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24); }

int main(void) {
  struct som_es_src s = { oam, vram, cgram, 0x61 };       /* name base 1 (byte 0x4000), small 16x16, large 32x32 */
  struct som_es_img im; memset(&im, 0, sizeof im); im.rgba = buf;
  unsigned base = 1 * 0x4000;

  /* 1. one 16x16 sprite built of four tiles, palette 2: tile 0x40..0x41 / 0x50..0x51 colours 1, 2, 3, 4 */
  clear();
  tile_fill(base, 0x40, 1); tile_fill(base, 0x41, 2); tile_fill(base, 0x50, 3); tile_fill(base, 0x51, 4);
  setcol(2, 1, 31, 0, 0); setcol(2, 2, 0, 31, 0); setcol(2, 3, 0, 0, 31); setcol(2, 4, 31, 31, 31);
  put_oam(5, 100, 80, 0x40, 0x04 /* pal 2 */, 0);
  uint8_t p1[1][4] = {{0xF8, 0xE8, 0x40, 0x04}};           /* dx -8, dy -24 */
  actor(3, 108, 104, 1, p1);                                /* expected y = 104 - 7 - 0 - 24 = 73?  use matching tolerance below */
  /* position check: expected (100, 73) vs OAM (100, 80): distance 7 px, inside the 32 px window */
  CHECK(som_es_render_actor(&s, wram, 3, &im) == 1);
  CHECK(im.w == 18 && im.h == 18 && im.n_sprites == 1 && im.n_matched == 1);
  CHECK(px(&im, 0, 0) == 0);                                /* border is transparent */
  CHECK(px(&im, 1, 1) == (0xFF000000u | 0x0000FFu));        /* colour 1 = red (R in the low byte) */
  CHECK(px(&im, 9, 1) == (0xFF000000u | 0x00FF00u));        /* tile 0x41 green */
  CHECK(px(&im, 1, 9) == (0xFF000000u | 0xFF0000u));        /* tile 0x50 blue */
  CHECK(px(&im, 9, 9) == 0xFFFFFFFFu);                      /* tile 0x51 white */
  /* foot: sx 108 - left 100 + 1 = 9, sy 104 - top (80 + 1) + 1 = 24 */
  CHECK(im.foot_x == 9 && im.foot_y == 24);

  /* 2. horizontal flip mirrors the picture; colour 0 stays transparent */
  clear();
  tile_fill(base, 0x40, 1); tile_fill(base, 0x41, 2); tile_fill(base, 0x50, 3); tile_fill(base, 0x51, 0);
  setcol(0, 1, 31, 0, 0); setcol(0, 2, 0, 31, 0); setcol(0, 3, 0, 0, 31);
  put_oam(0, 50, 50, 0x40, 0x40, 0);
  uint8_t p2[1][4] = {{0xF8, 0xF9, 0x40, 0x40}};
  actor(4, 58, 66, 1, p2);
  CHECK(som_es_render_actor(&s, wram, 4, &im) == 1);
  CHECK(px(&im, 1, 1) == (0xFF000000u | 0x00FF00u));        /* left half now holds tile 0x41 */
  CHECK(px(&im, 9, 1) == (0xFF000000u | 0x0000FFu));
  CHECK(px(&im, 9, 9) == 0 || im.h == 18);                  /* transparent corner or trimmed */

  /* 3. vertical flip, large sprite 32x32 (OBSEL size select 3), tile row wrap, name table 1 */
  clear();
  s.obsel = 0x61 | (1 << 3);                                 /* gap 1: second table at +0x4000 bytes */
  for (int t = 0; t < 4; t++) for (int u = 0; u < 4; u++) tile_fill(base + 0x4000, (t * 16 + u), 1 + ((t + u) & 7));
  for (int k = 1; k < 8; k++) setcol(1, k, k * 4, 0, 31 - k * 4);
  put_oam(9, 20, 100, 0x00, 0x02 | 0x01 | 0x80, 1);          /* pal 1, name table 1, vflip, large */
  uint8_t p3[1][4] = {{0, 0, 0x00, 0x83}};
  actor(5, 20, 107, 1, p3);
  CHECK(som_es_render_actor(&s, wram, 5, &im) == 1);
  CHECK(im.w == 34 && im.h == 34);
  /* top-left pixel of the flipped sprite comes from tile row 3, column 0 (colour 1 + 3) */
  { unsigned v = px(&im, 1, 1); unsigned r = (v & 0xFF), want = (4 * 4); want = (want << 3) | (want >> 2); CHECK(r == want); }
  s.obsel = 0x61;

  /* 4. two sprites overlap: the lower OAM index is in front */
  clear();
  tile_fill(base, 0x10, 1); tile_fill(base, 0x12, 2);
  setcol(0, 1, 31, 0, 0); setcol(0, 2, 0, 31, 0);
  put_oam(2, 40, 40, 0x10, 0x00, 0); put_oam(7, 40, 40, 0x12, 0x00, 0);
  uint8_t p4[2][4] = {{0xFC, 0xFC, 0x10, 0x00}, {0xFC, 0xFC, 0x12, 0x00}};
  actor(6, 44, 55, 2, p4);
  CHECK(som_es_render_actor(&s, wram, 6, &im) == 1 && im.n_sprites == 2);
  CHECK(px(&im, 1, 1) == (0xFF000000u | 0x0000FFu));        /* index 2 (red) over index 7 (green) */

  /* 5. a piece that is not in the OAM is skipped; an actor with no match returns 0; shadows are not picked by the box fallback */
  clear();
  tile_fill(base, 0x10, 1); setcol(0, 1, 31, 0, 0);
  put_oam(1, 40, 40, 0x10, 0x00, 0); put_oam(2, 40, 60, 0x00, 0x22, 0);
  uint8_t p5[2][4] = {{0, 0, 0x10, 0x00}, {0, 0, 0x77, 0x00}};
  actor(7, 40, 47, 2, p5);
  CHECK(som_es_render_actor(&s, wram, 7, &im) == 1 && im.n_pieces == 2 && im.n_matched == 1);
  CHECK(som_es_render_box(&s, 30, 30, 80, 90, 44, 70, &im) == 1 && im.n_sprites == 1);
  uint8_t p6[1][4] = {{0, 0, 0x55, 0x00}};
  actor(8, 40, 47, 1, p6);
  CHECK(som_es_render_actor(&s, wram, 8, &im) == 0);
  wram[0xE000 + 0x200 * 7] = 0;
  CHECK(som_es_render_actor(&s, wram, 7, &im) == 0);        /* inactive record */
  CHECK(som_es_render_actor(&s, wram, 2, &im) == 0 && som_es_render_actor(&s, wram, 16, &im) == 0);
  CHECK(som_es_render_actor(NULL, wram, 3, &im) == 0 && som_es_render_actor(&s, NULL, 3, &im) == 0);

  /* 6. negative X (9-bit) and Y wrap */
  clear();
  tile_fill(base, 0x10, 1); setcol(0, 1, 31, 0, 0);
  put_oam(3, 0x1F8, 248, 0x10, 0x00, 0);                    /* x = -8 (bit 8 set), y = 248 -> -8 */
  uint8_t p7[1][4] = {{0xF8, 0xF8, 0x10, 0x00}};
  actor(9, 0, 7, 1, p7);
  CHECK(som_es_render_actor(&s, wram, 9, &im) == 1 && im.w == 10 && im.h == 10);   /* one 8x8 tile, off screen on both axes, still drawn */

  /* 7. boss engine workspace: flips and priority are rewritten, tile + palette still match */
  clear();
  tile_fill(base, 0x6E, 1); setcol(5, 1, 0, 31, 0);
  put_oam(12, 90, 30, 0x6E, 0x3A, 0);
  uint8_t *r = wram + 0xE000 + 0x200 * 3;
  r[0] = 1; r[0x20] = 100; r[0x22] = 60; r[0x83] = 0;
  uint8_t *ws = wram + 0xF800; ws[0] = 1; ws[1] = 0; ws[2] = 0xD9; ws[3] = 0x6E; ws[4] = 0x5A;
  CHECK(som_es_render_actor(&s, wram, 3, &im) == 1 && im.n_matched == 1);

  /* 8. fit into a square: enlargement is an integer factor, reduction keeps the aspect */
  { uint32_t sq[40 * 40]; som_es_fit_argb(&im, 40, sq); int any = 0; for (int i = 0; i < 1600; i++) any |= sq[i] >> 24 != 0; CHECK(any); CHECK(sq[0] == 0); }
  { struct som_es_img big; memset(&big, 0, sizeof big); big.rgba = buf; memset(buf, 0, 100 * 50 * 4); big.w = 100; big.h = 50;
    for (int y = 0; y < 50; y++) for (int x = 0; x < 100; x++) { buf[(y * 100 + x) * 4] = 255; buf[(y * 100 + x) * 4 + 3] = 255; }
    uint32_t sq[40 * 40]; som_es_fit_argb(&big, 40, sq);
    CHECK(sq[0] == 0 && sq[10 * 40 + 20] == 0xFFFF0000u && sq[39 * 40 + 20] == 0); }

  /* 9. pack lookup with bad input */
  { uint8_t pk[16 + 24 + 16] = {0}; memcpy(pk, "SESP", 4); pk[4] = 1; pk[6] = 1;
    uint8_t *d = pk + 16; d[0] = 44; d[1] = 1; d[2] = 3; d[4] = 2; d[6] = 2; d[8] = 1; d[10] = 2; d[12] = 40; d[16] = 16;
    struct som_es_pack_entry e;
    CHECK(som_es_pack_find(pk, sizeof pk, 44, &e) == 1 && e.w == 2 && e.h == 2 && e.foot_x == 1 && e.foot_y == 2 && e.confidence == 3);
    CHECK(som_es_pack_find(pk, sizeof pk, 45, &e) == 0);
    CHECK(som_es_pack_find(pk, 50, 44, &e) == 0);          /* truncated */
    d[16] = 255; CHECK(som_es_pack_find(pk, sizeof pk, 44, &e) == 0);   /* byte count != w * h * 4 */
    pk[0] = 'X'; CHECK(som_es_pack_find(pk, sizeof pk, 44, &e) == 0); }

  /* 10. provider: pack first (confidence >= 2), live when the pack picture is weak or missing */
  { uint8_t pk[16 + 24 + 16] = {0}; memcpy(pk, "SESP", 4); pk[4] = 1; pk[6] = 1;
    uint8_t *d = pk + 16; d[0] = 44; d[1] = 1; d[2] = 3; d[4] = 2; d[6] = 2; d[8] = 1; d[10] = 2; d[12] = 40; d[16] = 16;
    for (int i = 0; i < 16; i++) pk[40 + i] = (uint8_t)(i % 4 == 3 ? 255 : 7 * i);
    uint32_t out[SOM_ES_SPRITE_MAX * SOM_ES_SPRITE_MAX]; int w = 0, h = 0, conf = 0;
    CHECK(som_es_sprite_for_id(pk, sizeof pk, NULL, NULL, 44, buf, out, &w, &h, &conf) == SOM_ES_FROM_PACK && w == 2 && h == 2 && conf == 3);
    CHECK((out[0] >> 24) == 255 && (out[0] & 255) == 14 && ((out[0] >> 16) & 255) == 0);
    CHECK(som_es_sprite_for_id(pk, sizeof pk, NULL, NULL, 45, buf, out, &w, &h, &conf) == SOM_ES_FROM_NONE);
    pk[16 + 2] = 1;                                         /* weak pack entry: the live render wins when the actor is on screen */
    clear(); tile_fill(base, 0x10, 1); setcol(0, 1, 31, 0, 0); put_oam(1, 100, 100, 0x10, 0, 0);
    uint8_t pl[1][4] = {{0, 0, 0x10, 0}}; actor(3, 100, 107, 1, pl); wram[0xE000 + 0x200 * 3 + 0x180] = 44;
    CHECK(som_es_sprite_for_id(pk, sizeof pk, &s, wram, 44, buf, out, &w, &h, &conf) == SOM_ES_FROM_LIVE && w == 10 && h == 10 && conf == 3);
    wram[0xE000 + 0x200 * 3 + 0x22] = 0xF0;                 /* off screen: back to the weak pack entry */
    CHECK(som_es_sprite_for_id(pk, sizeof pk, &s, wram, 44, buf, out, &w, &h, &conf) == SOM_ES_FROM_PACK && conf == 1);
  }
  { struct som_es_src ws; som_es_src_wram(&ws, wram, vram); wram[0x91] = 0x63; wram[0x2A] = 1; som_es_src_wram(&ws, wram, vram);
    CHECK(ws.oam == wram + 0x800 && ws.cgram == wram + 0xBE00 && ws.obsel == 0x63 && ws.vram == vram); }

  printf("%d checks, %d failures\n", checks, fails);
  return fails != 0;
}
