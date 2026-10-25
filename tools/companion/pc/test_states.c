/* Tests of the save state slots (pc_states.c): round trip, per-ROM isolation, versioning, corruption and backup, thumbnails.  No SDL, no core. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pc_states.h"
#include "pc_store.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint8_t *mk_state(size_t n, unsigned seed) {
  uint8_t *b = malloc(n);
  uint32_t x = seed * 2654435761u + 1;
  for (size_t i = 0; i < n; i++) { x = x * 1664525u + 1013904223u; b[i] = (uint8_t)(x >> 24); }
  return b;
}
static void frame_fill(uint32_t *f, int w, int h, uint32_t c0, uint32_t c1) {
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) f[y * w + x] = (x < w / 2) ? c0 : c1;
}
static void flip_byte(const char *path, long off) {
  FILE *f = fopen(path, "r+b");
  int c;
  if (!f) return;
  fseek(f, off, SEEK_SET); c = fgetc(f); fseek(f, off, SEEK_SET); fputc(c ^ 0x5A, f);
  fclose(f);
}
static void cut_file(const char *path, long keep) {
  size_t n;
  uint8_t *b = pc_slurp(path, &n);
  if (!b) return;
  pc_spit(path, b, (size_t)keep < n ? (size_t)keep : n);
  free(b);
}

int main(int argc, char **argv) {
  char base[200], path[400];
  struct pcs_meta m;
  struct pcs_thumb th, *tp = malloc(sizeof *tp);
  uint8_t *out;
  size_t on;
  static uint32_t frame[640 * 224];
  if (argc < 2) { printf("usage: test_states EMPTY_WORK_DIR\n"); return 2; }
  snprintf(base, sizeof base, "%s", argv[1]);

  /* thumbnail geometry and filtering */
  frame_fill(frame, 640, 224, 0xFFFF0000u, 0xFF0000FFu);
  pcs_make_thumb(&th, frame, 348, 224, 640);
  CHECK(th.h == PCS_THUMB_H && th.w == 99);
  CHECK(th.px[5] == 0xFFFF0000u && th.px[th.w - 5] == 0xFF0000FFu);
  pcs_make_thumb(&th, frame, 256, 224, 640);
  CHECK(th.w == 73);
  pcs_make_thumb(&th, frame, 640, 224, 640);                       /* absurdly wide: clamped */
  CHECK(th.w <= PCS_THUMB_MAX_W);
  pcs_make_thumb(&th, NULL, 0, 0, 0);
  CHECK(th.w == 0 && th.h == 0);
  pcs_make_thumb(&th, frame, 348, 224, 640);

  /* paths */
  CHECK(pcs_path(path, sizeof path, "/b", "abc", 0) == 0 && !strcmp(path, "/b/states/abc/slot01.state"));
  CHECK(pcs_path(path, sizeof path, "/b", "abc", 9) == 0 && !strcmp(path, "/b/states/abc/slot10.state"));
  CHECK(pcs_path(path, sizeof path, "/b", "abc", 10) != 0 && pcs_path(path, sizeof path, "/b", "abc", -1) != 0);
  CHECK(pcs_path(path, 10, "/b", "abc", 0) != 0);

  /* empty slot */
  CHECK(pcs_info(base, "keyA", 0, 1234, &m, NULL) == PCS_EMPTY && m.status == PCS_EMPTY);

  /* round trip on all ten slots, each with its own content */
  for (int s = 0; s < PCS_SLOTS; s++) {
    uint8_t *st = mk_state(5000 + 37 * s, (unsigned)s);
    CHECK(pcs_save(base, "keyA", s, 1234, st, 5000 + 37 * s, &th, 1700000000 + s) == PCS_OK);
    free(st);
  }
  for (int s = 0; s < PCS_SLOTS; s++) {
    uint8_t *want = mk_state(5000 + 37 * s, (unsigned)s);
    CHECK(pcs_load(base, "keyA", s, 1234, &out, &on, &m, tp) == PCS_OK);
    CHECK(on == (size_t)(5000 + 37 * s) && out && !memcmp(out, want, on));
    CHECK(m.time == 1700000000 + s && m.payload == on && m.version == PCS_VERSION && !m.from_backup);
    CHECK(tp->w == th.w && tp->h == th.h && !memcmp(tp->px, th.px, (size_t)th.w * th.h * 4));
    free(out); free(want);
  }
  /* a different ROM sees nothing, and its saves do not touch keyA */
  CHECK(pcs_info(base, "keyB", 3, 1234, &m, NULL) == PCS_EMPTY);
  {
    uint8_t *st = mk_state(777, 99);
    CHECK(pcs_save(base, "keyB", 3, 1234, st, 777, NULL, 1700000100) == PCS_OK);
    CHECK(pcs_load(base, "keyB", 3, 1234, &out, &on, &m, tp) == PCS_OK && on == 777 && tp->w == 0 && !memcmp(out, st, 777));
    free(out);
    CHECK(pcs_load(base, "keyA", 3, 1234, &out, &on, &m, NULL) == PCS_OK && on == 5000 + 37 * 3);
    free(out); free(st);
  }
  /* a file copied from one ROM's folder to another is refused (the key is inside the file) */
  {
    char dst[400], dd[400];
    size_t n2;
    uint8_t *b;
    pcs_path(path, sizeof path, base, "keyA", 0);
    pcs_dir(dd, sizeof dd, base, "keyC"); pc_mkdirs(dd);
    pcs_path(dst, sizeof dst, base, "keyC", 0);
    b = pc_slurp(path, &n2);
    CHECK(b && pc_spit(dst, b, n2) == 0);
    free(b);
    CHECK(pcs_info(base, "keyC", 0, 1234, &m, NULL) == PCS_INCOMPAT);
  }
  /* another core build */
  CHECK(pcs_info(base, "keyA", 0, 9999, &m, NULL) == PCS_INCOMPAT && m.status == PCS_INCOMPAT);
  /* another format version: patch the version field and re-seal the header CRC */
  {
    char vp[400];
    size_t n2;
    uint8_t *b;
    pcs_path(vp, sizeof vp, base, "keyA", 5);
    b = pc_slurp(vp, &n2);
    b[8] = 2;
    { uint32_t c = pcs_crc32(b, 68); b[68] = (uint8_t)c; b[69] = (uint8_t)(c >> 8); b[70] = (uint8_t)(c >> 16); b[71] = (uint8_t)(c >> 24); }
    CHECK(pc_spit(vp, b, n2) == 0);
    free(b);
    CHECK(pcs_info(base, "keyA", 5, 1234, &m, NULL) == PCS_INCOMPAT);
  }

  /* overwrite keeps the previous good file as a backup; a flipped payload byte falls back to it */
  {
    uint8_t *v1 = mk_state(3000, 1), *v2 = mk_state(3100, 2);
    CHECK(pcs_save(base, "keyD", 0, 7, v1, 3000, NULL, 100) == PCS_OK);
    CHECK(pcs_save(base, "keyD", 0, 7, v2, 3100, NULL, 200) == PCS_OK);
    pcs_path(path, sizeof path, base, "keyD", 0);
    CHECK(pcs_load(base, "keyD", 0, 7, &out, &on, &m, NULL) == PCS_OK && on == 3100 && m.time == 200 && !m.from_backup);
    free(out);
    flip_byte(path, 500);
    CHECK(pcs_load(base, "keyD", 0, 7, &out, &on, &m, NULL) == PCS_OK && on == 3000 && m.time == 100 && m.from_backup && !memcmp(out, v1, 3000));
    free(out);
    /* the damaged main file does not replace the good backup on the next save */
    CHECK(pcs_save(base, "keyD", 0, 7, v2, 3100, NULL, 300) == PCS_OK);
    CHECK(pcs_load(base, "keyD", 0, 7, &out, &on, &m, NULL) == PCS_OK && on == 3100 && m.time == 300 && !m.from_backup);
    free(out);
    {
      char bak[420];
      snprintf(bak, sizeof bak, "%s.bak", path);
      CHECK(pcs_load(base, "keyD", 0, 7, &out, &on, &m, NULL) == PCS_OK);
      free(out);
      size_t bn; uint8_t *bb = pc_slurp(bak, &bn);
      CHECK(bb && bn > 3000);                                       /* the backup is still the old good file */
      free(bb);
    }
    /* cut in half (power loss while the old code wrote in place): the backup answers */
    cut_file(path, 1500);
    CHECK(pcs_load(base, "keyD", 0, 7, &out, &on, &m, NULL) == PCS_OK && m.from_backup);
    free(out);
    free(v1); free(v2);
  }
  /* both files damaged: BAD, never a crash and never a partial payload */
  {
    uint8_t *v = mk_state(2000, 3);
    CHECK(pcs_save(base, "keyE", 1, 7, v, 2000, NULL, 1) == PCS_OK);
    CHECK(pcs_save(base, "keyE", 1, 7, v, 2000, NULL, 2) == PCS_OK);
    pcs_path(path, sizeof path, base, "keyE", 1);
    char bak[420]; snprintf(bak, sizeof bak, "%s.bak", path);
    flip_byte(path, 100); cut_file(bak, 40);
    out = (uint8_t *)1;
    CHECK(pcs_load(base, "keyE", 1, 7, &out, &on, &m, NULL) == PCS_BAD && out == NULL && on == 0);
    free(v);
  }
  /* header damage, every single byte of the first 72 flipped, none loads */
  {
    uint8_t *v = mk_state(1200, 4);
    int bad = 0;
    CHECK(pcs_save(base, "keyF", 2, 7, v, 1200, &th, 5) == PCS_OK);
    pcs_path(path, sizeof path, base, "keyF", 2);
    for (long off = 0; off < 72; off++) {
      flip_byte(path, off);
      if (pcs_info(base, "keyF", 2, 7, &m, NULL) == PCS_OK) bad++;
      flip_byte(path, off);
    }
    CHECK(bad == 0);
    CHECK(pcs_info(base, "keyF", 2, 7, &m, NULL) == PCS_OK);
    /* thumbnail damage is caught by its CRC, payload tail damage by the payload CRC */
    flip_byte(path, 90); CHECK(pcs_info(base, "keyF", 2, 7, &m, NULL) != PCS_OK); flip_byte(path, 90);
    flip_byte(path, 72 + th.w * th.h * 4 + 1100); CHECK(pcs_info(base, "keyF", 2, 7, &m, NULL) != PCS_OK);
    free(v);
  }
  /* garbage file and a stale .part from a crash are ignored */
  {
    char p3[420], part[430];
    pcs_path(p3, sizeof p3, base, "keyG", 0);
    { char dd[400]; pcs_dir(dd, sizeof dd, base, "keyG"); pc_mkdirs(dd); }
    pc_spit(p3, "this is not a state", 19);
    CHECK(pcs_info(base, "keyG", 0, 1, &m, NULL) == PCS_BAD);
    snprintf(part, sizeof part, "%s.part", p3);
    pc_spit(part, "half written", 12);
    CHECK(pcs_info(base, "keyG", 0, 1, &m, NULL) == PCS_BAD);
    uint8_t *v = mk_state(900, 5);
    CHECK(pcs_save(base, "keyG", 0, 1, v, 900, NULL, 9) == PCS_OK);   /* a save over a damaged file works and does not keep it as backup */
    CHECK(pcs_info(base, "keyG", 0, 1, &m, NULL) == PCS_OK);
    free(v);
  }
  /* write failures are reported, not hidden */
  {
    uint8_t *v = mk_state(100, 6);
    CHECK(pcs_save(base, "keyH", 10, 1, v, 100, NULL, 1) == PCS_IO);        /* bad slot */
    CHECK(pcs_save(base, "keyH", 0, 1, v, 0, NULL, 1) == PCS_IO);           /* nothing to save */
    CHECK(pcs_save("/proc/nonexistent", "keyH", 0, 1, v, 100, NULL, 1) == PCS_IO);
    free(v);
  }
  /* no stray .part after all this */
  {
    char dd[400], cmd[600];
    pcs_dir(dd, sizeof dd, base, "keyA");
    snprintf(cmd, sizeof cmd, "ls %s | grep -c '\\.part$' > /dev/null && exit 1 || exit 0", dd);
    CHECK(system(cmd) == 0);
  }
  free(tp);
  printf("test_states: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
