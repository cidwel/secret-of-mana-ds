/* Generates every preset of the launcher with seed 12345 and prints "id rc sha256 ms heap" lines.  One source for two builds:
 *   3DS (devkitARM):  writes sdmc:/som/test/rando_batch.txt, reads sdmc:/som/rom.sfc (see emu/build_rando_batch.sh)
 *   host:             rando_batch ROM OUT.sfc > lines.txt   (compile with libsomrando and the launcher model)
 * test/compare_rando_batch.py compares the two outputs: the 3DS build must make the same ROM bytes as the host build. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "launcher/som_launcher.h"
#include "launcher/somrando_api.h"
#include "launcher/somrando_menu.h"
#ifdef __3DS__
#include <3ds.h>
#include <malloc.h>
#include <sys/stat.h>
u32 __stacksize__ = 512 * 1024;
#else
#include <time.h>
#endif

static unsigned long long now_ms(void) {
#ifdef __3DS__
  return osGetTime();
#else
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned long long)t.tv_sec * 1000 + (unsigned long long)t.tv_nsec / 1000000;
#endif
}

static int peak_heap;   /* highest malloc heap in use seen between generator steps (3DS only) */
static int on_step(void *user, int step, int total, const char *name) {
  (void)user; (void)step; (void)total; (void)name;
#ifdef __3DS__
  int u = mallinfo().uordblks;
  if (u > peak_heap) peak_heap = u;
#endif
  return 0;
}

int main(int argc, char **argv) {
  static struct sl_launcher L;
  static uint8_t buf[4 * 1024 * 1024];
  const char *src, *dst;
  FILE *out;
#ifdef __3DS__
  (void)argc; (void)argv;
  mkdir("sdmc:/som/test", 0777);
  src = "sdmc:/som/rom.sfc"; dst = "sdmc:/som/test/out.sfc";
  out = fopen("sdmc:/som/test/rando_batch.txt", "wb");
#else
  if (argc < 3) { fprintf(stderr, "usage: rando_batch ROM OUT.sfc\n"); return 2; }
  src = argv[1]; dst = argv[2]; out = stdout;
#endif
  if (!out) return 1;
  for (int p = 0; p < sl_npresets; p++) {
    char lines[4096], why[160] = "", err[200] = "";
    sl_init(&L, 1);
    sl_apply_preset(&L, p);
    sl_set_seed(&L, "12345");
    if (!somrando_menu_supports(&L, why, sizeof why)) { fprintf(out, "%s unsupported\n", sl_presets[p].id); fflush(out); continue; }
    sl_config_lines(&L, lines, sizeof lines);
    const char *diff = NULL;
    struct somrando_request req = {0};
    req.mode = sl_modes[L.cfg.mode].key; req.seed = L.cfg.seed; req.difficulty = sl_difficulty(&L, &diff) ? diff : NULL;
    req.settings = lines; req.src_rom = src; req.dst_rom = dst;
    unsigned long long t0 = now_ms();
    peak_heap = 0;
    int rc = somrando_generate(&req, on_step, NULL, err, sizeof err);
    unsigned long long ms = now_ms() - t0;
    uint8_t h[32];
    char hex[65] = "";
    if (rc == 0) {
      FILE *f = fopen(dst, "rb");
      size_t n = f ? fread(buf, 1, sizeof buf, f) : 0;
      if (f) fclose(f);
      sl_sha256(buf, n, h);
      for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", h[i]);
    }
    int heap = 0, arena = 0;
#ifdef __3DS__
    { struct mallinfo mi = mallinfo(); heap = peak_heap; arena = mi.arena; }   /* heap: peak malloc heap in use during the run */
#endif
    fprintf(out, "%s rc=%d %s ms=%llu peak_heap=%d arena=%d %s\n", sl_presets[p].id, rc, hex, ms, heap, arena, err);
    fflush(out);
  }
  fprintf(out, "done\n");
  if (out != stdout) fclose(out);
  return 0;
}
