/* Host test of source/savefile.c (stdio only).  Usage: test_savefile <scratch dir> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/savefile.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(int argc, char **argv) {
  const char *d = argc > 1 ? argv[1] : ".";
  char p[300], key[32];
  CHECK(sf_crc32(0, "123456789", 9) == 0xCBF43926u);
  sf_key_from_path("sdmc:/som/rom.sfc", 1, key, sizeof key); CHECK(!strcmp(key, "original"));
  sf_key_from_path("sdmc:/som/roms/gen/2c845ac55030ff15.sfc", 0, key, sizeof key); CHECK(!strcmp(key, "2c845ac55030ff15"));
  sf_state_path("sdmc:/som/states", "original", 3, p, sizeof p); CHECK(!strcmp(p, "sdmc:/som/states/original.3.sta"));

  /* battery RAM: round trip, partial file, overwrite */
  uint8_t ram[8192], back[8192];
  for (int i = 0; i < 8192; i++) ram[i] = (uint8_t)(i * 7);
  snprintf(p, sizeof p, "%s/t.srm", d);
  CHECK(sf_srm_save(p, ram, sizeof ram) == 0);
  memset(back, 0xAA, sizeof back);
  CHECK(sf_srm_load(p, back, sizeof back) == sizeof back && !memcmp(ram, back, sizeof ram));
  ram[5] ^= 0xFF;
  CHECK(sf_srm_save(p, ram, sizeof ram) == 0);          /* overwrite an existing file */
  memset(back, 0xAA, sizeof back);
  CHECK(sf_srm_load(p, back, 100) == 100 && back[5] == ram[5] && back[100] == 0xAA);
  snprintf(p, sizeof p, "%s/missing.srm", d);
  CHECK(sf_srm_load(p, back, sizeof back) == 0);

  /* save state */
  static uint8_t st[529140], out[600000];
  static uint16_t th[SF_THUMB_W * SF_THUMB_H], th2[SF_THUMB_W * SF_THUMB_H];
  for (size_t i = 0; i < sizeof st; i++) st[i] = (uint8_t)(i * 31 + i / 251);
  for (int i = 0; i < SF_THUMB_W * SF_THUMB_H; i++) th[i] = (uint16_t)(i * 5);
  snprintf(p, sizeof p, "%s/t.1.sta", d);
  CHECK(sf_state_save(p, 0x12345678, 1792000000, th, st, sizeof st) == 0);
  CHECK(sf_state_save(p, 0x12345678, 1792000001, th, st, sizeof st) == 0);   /* over an existing slot */
  struct sf_info in;
  CHECK(sf_state_info(p, &in) == 0 && in.exists && in.rom_crc == 0x12345678 && in.state_size == sizeof st && in.stamp == 1792000001);
  CHECK(sf_state_thumb(p, th2) == 0 && !memcmp(th, th2, sizeof th));
  CHECK(sf_state_load(p, 0x12345678, out, sizeof out) == (long)sizeof st && !memcmp(out, st, sizeof st));
  CHECK(sf_state_load(p, 0x87654321, out, sizeof out) == -2);                 /* other ROM */
  CHECK(sf_state_load(p, 0x12345678, out, 1000) == -3);                       /* buffer too small */
  /* corrupt one byte of the state: the CRC catches it */
  FILE *f = fopen(p, "r+b");
  fseek(f, -10, SEEK_END); int c = fgetc(f); fseek(f, -10, SEEK_END); fputc(c ^ 1, f); fclose(f);
  CHECK(sf_state_load(p, 0x12345678, out, sizeof out) == -3);
  /* truncated file */
  f = fopen(p, "r+b"); fseek(f, 0, SEEK_SET); fwrite("garbage", 1, 7, f); fclose(f);
  CHECK(sf_state_info(p, &in) == -1 && !in.exists);
  CHECK(sf_state_load(p, 0x12345678, out, sizeof out) == -1);
  snprintf(p, sizeof p, "%s/none.9.sta", d);
  CHECK(sf_state_info(p, &in) == -1);
  /* without a thumbnail */
  snprintf(p, sizeof p, "%s/t.2.sta", d);
  CHECK(sf_state_save(p, 1, 0, NULL, st, 1000) == 0 && sf_state_thumb(p, th2) == -1 && sf_state_load(p, 1, out, sizeof out) == 1000);

  /* thumbnail: a flat colour frame stays flat; a 256x224 frame with a white left half has a white left half */
  static uint32_t frame[256 * 240];
  for (int y = 0; y < 224; y++) for (int x = 0; x < 256; x++) frame[y * 256 + x] = x < 128 ? 0xFFFFFF : 0x0000FF;
  sf_thumb_from_frame(frame, 256, 224, 256, th);
  uint32_t rgb[SF_THUMB_W * SF_THUMB_H];
  sf_thumb_to_rgb(th, rgb);
  CHECK(rgb[10 * SF_THUMB_W + 10] == 0xFFFFFF && rgb[10 * SF_THUMB_W + 120] == 0x0000FF);
  printf(fails ? "test_savefile: %d FAILED\n" : "test_savefile: all passed\n", fails);
  return fails != 0;
}
