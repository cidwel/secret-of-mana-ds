/* Device-path test: renders through the exact entry points of the proposed Android glue (android_patch/som_ux_glue.h:
 * ux_glue_init with the real pack and ROM, ux_glue_frame with a WRAM image, ux_glue_draw into a w*h uint32 bitmap) at the sizes the
 * Android view really reports.  Every render must pass the layout audit (no cut text, no glued label, no text over a portrait, no
 * overlapping text boxes).  Raw ARGB goes to OUTDIR/<case>.argb and the FNV-1a hash of every render is printed, so
 * test_device_path.py can compare it with the ctypes renderer, the golden hashes and the real device capture.
 *
 *   cc -std=c99 -Wall -Wextra -O1 -I.. -o /tmp/test_glue test_glue_path.c som_ui_draw.c som_ui_state.c som_ui_sel.c ../som_companion.c -lm
 *   /tmp/test_glue PACK ROM WRAM OUTDIR */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "android_patch/som_ux_glue.h"

static uint8_t *slurp(const char *path, size_t *n) {
  FILE *f = fopen(path, "rb");
  if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(2); }
  fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
  uint8_t *b = malloc((size_t)len);
  if (fread(b, 1, (size_t)len, f) != (size_t)len) exit(2);
  fclose(f); *n = (size_t)len;
  return b;
}
static uint64_t fnv(const uint32_t *p, size_t n) {
  uint64_t h = 1469598103934665603ull;
  for (size_t i = 0; i < n; i++)
    for (int b = 0; b < 4; b++) { h ^= (p[i] >> (8 * b)) & 255; h *= 1099511628211ull; }
  return h;
}
static const struct { const char *name; unsigned hp[3], hpm[3], mp[3], mpm[3]; } STATE[] = {
  {"map215", {523, 416, 411}, {524, 417, 412}, {0, 47, 47}, {0, 47, 47}},       /* the state of the device capture */
  {"map215_999", {999, 999, 999}, {999, 999, 999}, {99, 99, 99}, {99, 99, 99}},
  {"map215_9999", {9999, 9999, 9999}, {9999, 9999, 9999}, {255, 255, 255}, {255, 255, 255}},
  {"map215_low", {52, 41, 9}, {524, 417, 412}, {5, 4, 0}, {47, 47, 47}},
};
static const int SIZE[][2] = {{1240, 1080}, {1080, 1240}, {1240, 970}, {1080, 1160}, {320, 240}};

int main(int argc, char **argv) {
  if (argc < 5) { fprintf(stderr, "usage: %s PACK ROM WRAM OUTDIR\n", argv[0]); return 2; }
  size_t np, nr, nw;
  uint8_t *pack = slurp(argv[1], &np), *rom = slurp(argv[2], &nr), *wram = slurp(argv[3], &nw);
  if (nw < 0x20000) return 2;
  uint8_t *rom_keep = malloc(nr); memcpy(rom_keep, rom, nr);      /* the JNI keeps rom_copy alive for the UX layer */
  if (!ux_glue_init(pack, np, rom_keep, nr)) return 2;
  int bad = 0;
  for (unsigned s = 0; s < sizeof STATE / sizeof STATE[0]; s++) {
    uint8_t w[0x20000]; memcpy(w, wram, sizeof w);
    w[0xCF00] = 1;                       /* story flag: the bottom screen is dark until the first sword (round 3) */
    for (int i = 0; i < 3; i++) {
      unsigned b = 0xE000 + 0x200 * i;
      w[b + 0x182] = (uint8_t)STATE[s].hp[i]; w[b + 0x183] = (uint8_t)(STATE[s].hp[i] >> 8);
      w[b + 0x184] = (uint8_t)STATE[s].hpm[i]; w[b + 0x185] = (uint8_t)(STATE[s].hpm[i] >> 8);
      if (s) { w[b + 0x186] = (uint8_t)STATE[s].mp[i]; w[b + 0x187] = (uint8_t)STATE[s].mpm[i]; }
    }
    ux_glue_frame(w);
    for (unsigned z = 0; z < sizeof SIZE / sizeof SIZE[0]; z++) {
      int pw = SIZE[z][0], ph = SIZE[z][1];
      uint32_t *px = calloc((size_t)pw * ph, 4);
      struct som_macro m; memset(&m, 0, sizeof m);
      som_ux_audit(1);
      ux_glue_draw(&m, px, pw, ph);
      int n = som_ux_audit(0);
      /* label contract: every hero has an "HP" label (and an "MP" label when it has MP), each a separate text box immediately followed by
       * its numbers box on the same row with a gap of at least 2 vp, and no box of the card log overlaps another or a portrait */
      { int hp_l = 0, mp_l = 0, nb = 0, x[96], y[96], bw[96], bh[96]; char t[96][24];
        for (nb = 0; nb < 96 && som_ux_text_log(nb, t[nb], sizeof t[nb], &x[nb], &y[nb], &bw[nb], &bh[nb]); nb++) {}
        for (int i = 0; i < nb; i++) {
          int is_hp = !strcmp(t[i], "HP"), is_mp = !strcmp(t[i], "MP");
          if (!is_hp && !is_mp) continue;
          hp_l += is_hp; mp_l += is_mp;
          if (i + 1 >= nb || (t[i + 1][0] < '0' || t[i + 1][0] > '9') || x[i + 1] - (x[i] + bw[i]) < 2 || abs(y[i + 1] - y[i]) > 11) {
            n++; printf("LABEL %s %dx%d: %s not followed by its numbers with a gap\n", STATE[s].name, pw, ph, t[i]);
          }
        }
        int mpn = 0; for (int i = 0; i < 3; i++) mpn += (s ? STATE[s].mpm[i] : (i > 0 ? 47u : 0)) != 0;
        if (hp_l != 3 || mp_l != mpn) { n++; printf("LABEL %s %dx%d: %d HP and %d MP labels, expected 3 and %d\n", STATE[s].name, pw, ph, hp_l, mp_l, mpn); }
      }
      som_ux_audit(1);
      if (n) { bad++; printf("AUDIT %s %dx%d: %d (%s)\n", STATE[s].name, pw, ph, n, som_ux_audit_note()); }
      char path[600]; snprintf(path, sizeof path, "%s/%s_%dx%d.argb", argv[4], STATE[s].name, pw, ph);
      FILE *f = fopen(path, "wb"); if (f) { fwrite(px, 4, (size_t)pw * ph, f); fclose(f); }
      printf("%s %dx%d %016llx\n", STATE[s].name, pw, ph, (unsigned long long)fnv(px, (size_t)pw * ph));
      free(px);
    }
  }
  /* round 4 glue API: pressed look, enemy pictures, hotkey with policy */
  {
    uint8_t w[0x20000]; memcpy(w, wram, sizeof w);
    unsigned b = 0xE000 + 0x200 * 3;
    w[b] = 1; w[b + 0x180] = 5; w[b + 0x182] = 100; w[b + 0x184] = 100; w[b + 0x20] = 128; w[b + 0x22] = 120;
    ux_glue_frame(w);
    static uint32_t spr[24 * 24];
    for (int i = 0; i < 24 * 24; i++) spr[i] = (i % 24 > 3 && i % 24 < 20) ? 0xFF40A040u : 0u;
    struct ux_dispatch d;
    uint8_t want[16];
    int nw = ux_glue_sprites_wanted(want, 16);
    if (nw < 1 || ux_glue_set_enemy_sprite(want[0], spr, 24, 24) != 0 || ux_glue_sprites_wanted(want, 16) != nw - 1) { bad++; printf("GLUE sprite API\n"); }
    if (ux_glue_revive_open(0) != 0) { bad++; printf("GLUE revive_open opened for a standing hero\n"); }
    ux_glue_spell_avail(NULL);
    int x, y;
    if (!som_ux_target_center(ux_g, 1240, 970, SOM_UX_HIT_TAB, 1, &x, &y) || !ux_glue_touch_down(x, y, 1240, 970) ||
        !som_ux_pressed(ux_g, NULL, NULL)) { bad++; printf("GLUE touch_down did not press the tab\n"); }
    ux_glue_touch_up();
    if (som_ux_pressed(ux_g, NULL, NULL)) { bad++; printf("GLUE touch_up left the press on\n"); }
    ux_glue_quick(1, SOM_UX_BK_WEAPON, 1);
    if (!ux_glue_shoulder(SOM_UX_SH_R1, &d) || d.kind != SOM_UX_ACT_CMD) { bad++; printf("GLUE shoulder R1 gave no command\n"); }
    ux_glue_shoulder_labels("L", "R", "ZL", "ZR");
    ux_glue_slot_flash(2, SOM_UX_FLASH_OK);
    if (!ux_glue_hero_name(0)[0]) { bad++; printf("GLUE hero_name default\n"); }
  }
  printf("%s\n", bad ? "FAILED" : "OK");
  return bad ? 1 : 0;
}
