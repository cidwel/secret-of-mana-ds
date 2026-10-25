/* Battery RAM and save state files of the 3DS frontend (pure C stdio, tested on the PC by test/test_savefile.c).
 *
 *   sdmc:/som/saves/<rom key>.srm          battery RAM, one file per game (original, or the 16 hex digit key of a generated ROM)
 *   sdmc:/som/states/<rom key>.<n>.sta     save state slot n (1..SF_SLOTS): header, 128x112 RGB565 thumbnail, core state
 *
 * A state is only loaded into the same ROM bytes it was taken from (CRC-32 of the ROM image the core runs, patches included). */
#ifndef SOM_N3DS_SAVEFILE_H
#define SOM_N3DS_SAVEFILE_H
#include <stddef.h>
#include <stdint.h>

#define SF_SLOTS 8
#define SF_THUMB_W 128
#define SF_THUMB_H 112

struct sf_info {
  int exists;              /* a state file with a valid header is there */
  uint32_t rom_crc, state_size, stamp;   /* stamp: Unix seconds, 0 when the console clock was not available */
};

uint32_t sf_crc32(uint32_t crc, const void *data, size_t n);
/* "original" or the file name without extension; key must fit 24 bytes. */
void sf_key_from_path(const char *rom_path, int original, char *key, size_t n);
void sf_srm_path(const char *dir, const char *key, char *out, size_t n);
void sf_state_path(const char *dir, const char *key, int slot, char *out, size_t n);

/* Whole-file write: temporary file then rename, falling back to a direct write where rename onto an existing file is refused.  0 ok. */
int sf_write_file(const char *path, const void *data, size_t n);
/* Reads up to n bytes of the battery RAM file into dst (rest untouched); returns the bytes read, 0 when absent. */
size_t sf_srm_load(const char *path, uint8_t *dst, size_t n);
int sf_srm_save(const char *path, const uint8_t *src, size_t n);

/* thumb: SF_THUMB_W x SF_THUMB_H RGB565 or NULL.  Returns 0 on success. */
int sf_state_save(const char *path, uint32_t rom_crc, uint32_t stamp, const uint16_t *thumb, const void *state, size_t n);
int sf_state_info(const char *path, struct sf_info *out);
int sf_state_thumb(const char *path, uint16_t *thumb);                       /* 0 ok */
/* Reads the core state into buf (capacity cap) after checking the ROM CRC.  Returns the size, or -1 (file), -2 (other ROM), -3 (too big or corrupt). */
long sf_state_load(const char *path, uint32_t rom_crc, void *buf, size_t cap);

void sf_thumb_from_frame(const uint32_t *frame, unsigned w, unsigned h, size_t stride, uint16_t *out);   /* box filter to 128x112 */
void sf_thumb_to_rgb(const uint16_t *in, uint32_t *out);
#endif
