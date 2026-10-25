/* som_states: save-state slots of the companion apps (board item T4/T12).  One directory per game (<root>/<key>/), eight slot files, each with
 * a small thumbnail and the time, written atomically (tmp + rename).  The libretro core is reached through three callbacks, so the module is
 * testable on the desktop with a fake core.  No Android dependency.  Pure C99 + POSIX. */
#ifndef SOM_STATES_H
#define SOM_STATES_H

#include <stddef.h>
#include <stdint.h>

#define SST_SLOTS 8
#define SST_TW 128
#define SST_TH 72

struct sst_ops {
  void *u;
  size_t (*size)(void *u);                          /* retro_serialize_size */
  int (*save)(void *u, void *buf, size_t n);         /* retro_serialize: 1 ok */
  int (*load)(void *u, const void *buf, size_t n);   /* retro_unserialize: 1 ok */
};

struct sst_info {
  int used;                    /* a valid file for this core is there */
  int64_t time;                /* unix seconds of the save */
  uint32_t thumb[SST_TW * SST_TH];   /* 0xAARRGGBB; zero alpha-free picture, valid when used */
};

enum { SST_OK = 0, SST_ERR_IO = -1, SST_ERR_EMPTY = -2, SST_ERR_CORE = -3, SST_ERR_ARG = -4 };

/* Box-filter a w x h frame (0xAARRGGBB, stride in pixels) into the SST_TW x SST_TH thumbnail. */
void sst_make_thumb(const uint32_t *frame, int stride, int w, int h, uint32_t out[SST_TW * SST_TH]);
/* Creates <root>/<key> when missing; writes the full path to dir.  0 ok. */
int sst_dir(const char *root, const char *key, char *dir, size_t n);
int sst_save(const char *dir, int slot, const struct sst_ops *ops, const uint32_t thumb[SST_TW * SST_TH], int64_t now);
int sst_load(const char *dir, int slot, const struct sst_ops *ops);
/* Reads the header and thumbnail of a slot (info->used = 0 for an empty or foreign file).  `size` is the serialize size of the running core: a
 * file made by another core build is reported as not used so it is never loaded by mistake (pass 0 to skip the check). */
int sst_info(const char *dir, int slot, size_t size, struct sst_info *info);
/* Remembered slot of the quick save / load chord. */
int sst_get_current(const char *dir);
void sst_set_current(const char *dir, int slot);
/* "2026-10-20 18:30" for the page; empty time gives "". */
void sst_format_time(int64_t t, char *out, size_t n);

#endif
