/* Tests of the retro display filters (pc_shader.c): names, pre-scale, the multiply layers (shape, range, symmetry, grid alignment) and golden hashes.
 *   test_shader                  run
 *   test_shader --update-hashes  rewrite golden_shader.txt (review the diff first)
 * No SDL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_shader.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint32_t fnv(const uint32_t *p, size_t n) { uint32_t h = 2166136261u; for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; } return h; }
static int R(uint32_t p) { return (p >> 16) & 255; }
static int G(uint32_t p) { return (p >> 8) & 255; }
static int B(uint32_t p) { return p & 255; }

struct gold { char name[64]; uint32_t h; };
static struct gold gold[64]; static int ngold;
static void gold_load(const char *path) {
  FILE *f = fopen(path, "r");
  char n[64]; unsigned h;
  if (!f) return;
  while (ngold < 64 && fscanf(f, "%63s %x", n, &h) == 2) { snprintf(gold[ngold].name, 64, "%s", n); gold[ngold++].h = h; }
  fclose(f);
}
static int update;
static FILE *out_gold;
static void golden(const char *name, const uint32_t *px, size_t n) {
  uint32_t h = fnv(px, n);
  if (update) { fprintf(out_gold, "%s %08x\n", name, h); return; }
  for (int i = 0; i < ngold; i++) if (!strcmp(gold[i].name, name)) { checks++; if (gold[i].h != h) { fails++; printf("FAIL golden %s: %08x expected %08x\n", name, h, gold[i].h); } return; }
  checks++; fails++; printf("FAIL golden %s missing (%08x)\n", name, h);
}

int main(int argc, char **argv) {
  update = argc > 1 && !strcmp(argv[1], "--update-hashes");
  if (update) out_gold = fopen("golden_shader.txt", "w"); else gold_load("golden_shader.txt");

  for (int i = 0; i < PC_SH_COUNT; i++) {
    CHECK(pc_shader_valid(i) && pc_shader_name(i)[0] && strcmp(pc_shader_name(i), "?"));
    CHECK(pc_shader_from_key(pc_shader_key(i)) == i);
    CHECK(pc_shader_next(i, 1) == (i + 1) % PC_SH_COUNT && pc_shader_next(i, -1) == (i + PC_SH_COUNT - 1) % PC_SH_COUNT);
  }
  CHECK(!pc_shader_valid(-1) && !pc_shader_valid(PC_SH_COUNT) && pc_shader_from_key("nope") == -1 && pc_shader_from_key(NULL) == -1);
  CHECK(!pc_shader_has_overlay(PC_SH_PLAIN) && !pc_shader_has_overlay(PC_SH_SHARP) && pc_shader_has_overlay(PC_SH_SCANLINES) && pc_shader_has_overlay(PC_SH_CRT) && pc_shader_has_overlay(PC_SH_LCD));
  CHECK(!pc_shader_is_sharp(PC_SH_PLAIN) && pc_shader_is_sharp(PC_SH_SHARP) && pc_shader_is_sharp(PC_SH_LCD));
  CHECK(pc_shader_prescale(348, 1920) == 6 && pc_shader_prescale(256, 1920) == 8 && pc_shader_prescale(348, 348) == 1 && pc_shader_prescale(640, 100) == 1);
  CHECK(pc_shader_prescale(0, 100) == 1 && pc_shader_prescale(100, 0) == 1 && pc_shader_prescale(16, 4000) == 8);

  /* no layer for PLAIN / SHARP, no crash on bad sizes */
  {
    uint32_t px[16];
    CHECK(pc_shader_overlay(PC_SH_PLAIN, 4, 4, 2, 2, px) == -1 && pc_shader_overlay(PC_SH_SHARP, 4, 4, 2, 2, px) == -1);
    CHECK(pc_shader_overlay(PC_SH_CRT, 0, 4, 2, 2, px) == -1 && pc_shader_overlay(PC_SH_CRT, 4, 4, 0, 2, px) == -1 && pc_shader_overlay(PC_SH_CRT, 4, 4, 2, 2, NULL) == -1);
    CHECK(pc_shader_overlay(PC_SH_LCD, 1, 1, 1, 1, px) == 0 && R(px[0]) > 0);
    CHECK(pc_shader_overlay(PC_SH_CRT, 1, 1, 348, 224, px) == 0);
  }

  /* shape: 348x224 shown 4x (1392x896) */
  {
    int dw = 1392, dh = 896;
    uint32_t *px = malloc((size_t)dw * dh * 4);
    CHECK(pc_shader_overlay(PC_SH_SCANLINES, dw, dh, 348, 224, px) == 0);
    int lo = 255, hi = 0, same = 1;
    for (int y = 0; y < dh; y++) {
      uint32_t row = px[(size_t)y * dw];
      for (int x = 0; x < dw; x += 97) if (px[(size_t)y * dw + x] != row) same = 0;
      if (R(row) < lo) lo = R(row);
      if (R(row) > hi) hi = R(row);
      CHECK(R(row) == G(row) && G(row) == B(row) && (row >> 24) == 255);
    }
    CHECK(same);                                              /* a scanline layer is constant along a row */
    CHECK(hi >= 225 && lo >= 70 && lo <= 140);                /* bright centres, visible but not black gaps */
    for (int y = 0; y < dh; y += 4) {                         /* period of 4 rows, symmetric inside a source line */
      uint32_t a = px[(size_t)y * dw], b = px[(size_t)(y + 1) * dw], c = px[(size_t)(y + 2) * dw], d = px[(size_t)(y + 3) * dw];
      CHECK(a == d && b == c && R(b) > R(a));
      if (y + 4 < dh) CHECK(px[(size_t)(y + 4) * dw] == a);
    }
    golden("scanlines_1392x896_348x224", px, (size_t)dw * dh);
    /* LCD: the gap is at the end of every source pixel, in both directions */
    CHECK(pc_shader_overlay(PC_SH_LCD, dw, dh, 348, 224, px) == 0);
    CHECK(R(px[(size_t)1 * dw + 1]) == 255);                  /* inside a cell */
    CHECK(R(px[(size_t)1 * dw + 3]) < 200);                   /* vertical gap column of cell 0 */
    CHECK(R(px[(size_t)3 * dw + 1]) < 200);                   /* horizontal gap row */
    CHECK(R(px[(size_t)3 * dw + 3]) <= R(px[(size_t)3 * dw + 1]) && R(px[(size_t)3 * dw + 3]) <= R(px[(size_t)1 * dw + 3]));
    for (int x = 0; x < dw; x += 4) CHECK(px[(size_t)1 * dw + x + 1] == px[(size_t)1 * dw + 1]);
    golden("lcd_1392x896_348x224", px, (size_t)dw * dh);
    /* CRT: aperture grille phases, vignette */
    CHECK(pc_shader_overlay(PC_SH_CRT, dw, dh, 348, 224, px) == 0);
    {
      uint32_t c0 = px[(size_t)(dh / 2) * dw + 600], c1 = px[(size_t)(dh / 2) * dw + 601], c2 = px[(size_t)(dh / 2) * dw + 602];   /* 600 % 3 == 0 */
      CHECK(R(c0) > G(c0) && G(c0) == B(c0));
      CHECK(G(c1) > R(c1) && R(c1) == B(c1));
      CHECK(B(c2) > R(c2) && R(c2) == G(c2));
      CHECK(px[0] != px[(size_t)(dh / 2) * dw + dw / 2]);
      int cen = 0, cor = 0;
      for (int k = 0; k < 3; k++) { cen += R(px[(size_t)(dh / 2 + 1) * dw + dw / 2 + k]) + G(px[(size_t)(dh / 2 + 1) * dw + dw / 2 + k]); cor += R(px[(size_t)1 * dw + k]) + G(px[(size_t)1 * dw + k]); }
      CHECK(cen > cor);
    }
    golden("crt_1392x896_348x224", px, (size_t)dw * dh);
    free(px);
  }
  /* picture sizes of the 1080p presets and the narrow 27/17 px extensions */
  {
    static const struct { int dw, dh, sw, sh; } c[] = {{1920, 1059, 348, 224}, {1744, 1080, 310, 224}, {1440, 1080, 256, 224}, {1280, 720, 348, 224}};
    for (int m = PC_SH_SCANLINES; m <= PC_SH_LCD; m++)
      for (int i = 0; i < 4; i++) {
        uint32_t *px = malloc((size_t)c[i].dw * c[i].dh * 4);
        char name[64];
        int lo = 255;
        CHECK(pc_shader_overlay(m, c[i].dw, c[i].dh, c[i].sw, c[i].sh, px) == 0);
        for (size_t k = 0; k < (size_t)c[i].dw * c[i].dh; k++) { if (R(px[k]) < lo) lo = R(px[k]); }
        CHECK(lo >= 60);                                      /* never a black screen */
        snprintf(name, sizeof name, "%s_%dx%d_%dx%d", pc_shader_key(m), c[i].dw, c[i].dh, c[i].sw, c[i].sh);
        golden(name, px, (size_t)c[i].dw * c[i].dh);
        free(px);
      }
  }
  /* a picture smaller than the frame: the layer flattens instead of aliasing */
  {
    uint32_t px[100 * 56];
    int lo = 255, hi = 0;
    CHECK(pc_shader_overlay(PC_SH_SCANLINES, 100, 56, 348, 224, px) == 0);
    for (int y = 0; y < 56; y++) { int v = R(px[y * 100]); if (v < lo) lo = v; if (v > hi) hi = v; }
    CHECK(hi - lo <= 12);
  }
  if (update) fclose(out_gold);
  printf("test_shader: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
