/* Host test of som_states.c with a fake core.  Build and run: android/tests/run_thor_tests.sh */
#include "../../som_states.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t core_mem[1000];
static size_t core_n = 1000;
static int fail_save, fail_load;
static size_t f_size(void *u) { (void)u; return core_n; }
static int f_save(void *u, void *b, size_t n) { (void)u; if (fail_save) return 0; memcpy(b, core_mem, n); return 1; }
static int f_load(void *u, const void *b, size_t n) { (void)u; if (fail_load) return 0; memcpy(core_mem, b, n); return 1; }
static const struct sst_ops OPS = {NULL, f_size, f_save, f_load};
static int checks, bad;
#define CHECK(c) do { checks++; if (!(c)) { bad++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

int main(int argc, char **argv) {
  const char *root = argc > 1 ? argv[1] : "/tmp/claude-1000/sst_test";
  char a[600], b[600];
  static uint32_t frame[348 * 224], th[SST_TW * SST_TH];
  static struct sst_info inf;
  for (int i = 0; i < 348 * 224; i++) frame[i] = 0xFF000000u | (uint32_t)(i % 348) << 8 | 0x40;
  sst_make_thumb(frame, 348, 348, 224, th);
  CHECK(th[0] >> 24 == 0xFF);
  CHECK((th[SST_TW - 1] >> 8 & 255) > (th[0] >> 8 & 255));        /* the gradient survives the box filter */
  CHECK(sst_dir(root, "game1", a, sizeof a) == SST_OK && sst_dir(root, "game2", b, sizeof b) == SST_OK && strcmp(a, b));
  CHECK(sst_dir(root, "a/b", b, sizeof b) == SST_ERR_ARG);
  CHECK(sst_info(a, 0, 1000, &inf) == SST_ERR_EMPTY && !inf.used);
  for (int i = 0; i < 1000; i++) core_mem[i] = (uint8_t)i;
  CHECK(sst_save(a, 3, &OPS, th, 1760000000) == SST_OK);
  CHECK(sst_info(a, 3, 1000, &inf) == SST_OK && inf.used && inf.time == 1760000000 && inf.thumb[5] == th[5]);
  CHECK(sst_info(a, 2, 1000, &inf) == SST_ERR_EMPTY);
  CHECK(sst_info(b, 3, 1000, &inf) == SST_ERR_EMPTY);             /* per-game isolation */
  memset(core_mem, 0, sizeof core_mem);
  CHECK(sst_load(a, 3, &OPS) == SST_OK && core_mem[7] == 7 && core_mem[999] == (uint8_t)999);
  CHECK(sst_load(b, 3, &OPS) == SST_ERR_EMPTY);
  CHECK(sst_load(a, 4, &OPS) == SST_ERR_EMPTY);
  core_n = 1200;                                                   /* another core build: refused, not loaded */
  CHECK(sst_load(a, 3, &OPS) == SST_ERR_EMPTY);
  CHECK(sst_info(a, 3, 1200, &inf) == SST_ERR_EMPTY);
  core_n = 1000;
  fail_load = 1;
  CHECK(sst_load(a, 3, &OPS) == SST_ERR_CORE);
  fail_load = 0;
  fail_save = 1;
  CHECK(sst_save(a, 5, &OPS, th, 5) == SST_ERR_CORE);
  CHECK(sst_info(a, 5, 1000, &inf) == SST_ERR_EMPTY);             /* a failed save leaves no file */
  fail_save = 0;
  core_mem[0] = 99;
  CHECK(sst_save(a, 3, &OPS, th, 1760000100) == SST_OK && sst_info(a, 3, 1000, &inf) == SST_OK && inf.time == 1760000100);
  CHECK(sst_save(a, 8, &OPS, th, 1) == SST_ERR_ARG && sst_load(a, -1, &OPS) == SST_ERR_ARG);
  CHECK(sst_get_current(a) == 0);
  sst_set_current(a, 6);
  CHECK(sst_get_current(a) == 6 && sst_get_current(b) == 0);
  sst_set_current(a, 9);
  CHECK(sst_get_current(a) == 6);
  {   /* a truncated file is never loaded */
    char p[700];
    FILE *f;
    snprintf(p, sizeof p, "%s/slot3.sst", a);
    f = fopen(p, "r+b"); fseek(f, 0, SEEK_END);
    long sz = ftell(f); fclose(f);
    CHECK(sz > 1000);
    f = fopen(p, "wb"); fwrite("SMSS", 1, 4, f); fclose(f);
    CHECK(sst_info(a, 3, 1000, &inf) == SST_ERR_EMPTY && sst_load(a, 3, &OPS) == SST_ERR_EMPTY);
  }
  sst_format_time(1760000000, a, sizeof a);
  CHECK(strlen(a) == 16);
  printf("states: %d checks, %d failures\n", checks, bad);
  return bad ? 1 : 0;
}
