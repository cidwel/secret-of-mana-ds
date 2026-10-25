/* Save state slots of the SoM PC frontend (board item T12).  Ten slots per ROM, each one a single file holding a versioned header (time, ROM key,
 * core signature, payload CRC), a thumbnail of the frame at the moment of saving and the core's serialized state.  A slot is replaced
 * through a temporary file (fsync, then rename) and the previous good file is kept as <slot>.bak, so a crash or a full disk never loses
 * a slot and a damaged file falls back to its backup.  The folder of a ROM is states/<rom key>/ so an original ROM, a patched copy and each
 * generated Ancient Cave run never see each other's states.  No SDL, no core: the caller passes the bytes.  Pure C99 + POSIX. */
#ifndef PC_STATES_H
#define PC_STATES_H

#include <stddef.h>
#include <stdint.h>

#define PCS_SLOTS 10
#define PCS_VERSION 1
#define PCS_THUMB_MAX_W 160
#define PCS_THUMB_H 64

enum pcs_status {
  PCS_OK = 0,
  PCS_EMPTY,          /* no file */
  PCS_BAD,            /* damaged (cut, wrong CRC, not a state file) and no usable backup */
  PCS_INCOMPAT,       /* written by another version of this format, another core build or for another ROM */
  PCS_IO              /* the file system refused (save only) */
};

struct pcs_thumb { int w, h; uint32_t px[PCS_THUMB_MAX_W * PCS_THUMB_H]; };   /* 0xAARRGGBB */

struct pcs_meta {
  int status;          /* enum pcs_status */
  int from_backup;     /* 1 when the main file was unusable and the backup is what the other fields describe */
  int64_t time;        /* unix seconds */
  uint32_t payload;    /* bytes of core state */
  int version;
};

/* base = the frontend state folder; writes "<base>/states/<rom_key>" into out.  Returns 0, -1 when it does not fit. */
int pcs_dir(char *out, size_t n, const char *base, const char *rom_key);
int pcs_path(char *out, size_t n, const char *base, const char *rom_key, int slot);   /* slot 0..PCS_SLOTS-1 */

/* Box-filters a frame (0xAARRGGBB, stride in pixels) into a thumbnail PCS_THUMB_H high and at most PCS_THUMB_MAX_W wide. */
void pcs_make_thumb(struct pcs_thumb *t, const uint32_t *frame, int fw, int fh, int stride);

/* Writes slot `slot`.  core_sig identifies the core build (its serialize size).  thumb may be NULL.  Returns PCS_OK or PCS_IO. */
int pcs_save(const char *base, const char *rom_key, int slot, uint32_t core_sig, const void *state, size_t n, const struct pcs_thumb *thumb, int64_t time);

/* Reads slot `slot`: the main file, then the backup.  On PCS_OK *state is malloc'd (*n bytes) and meta/thumb (both may be NULL) are filled. */
int pcs_load(const char *base, const char *rom_key, int slot, uint32_t core_sig, uint8_t **state, size_t *n, struct pcs_meta *meta, struct pcs_thumb *thumb);

/* Describes a slot without returning the payload (still verifies its CRC).  Returns meta->status. */
int pcs_info(const char *base, const char *rom_key, int slot, uint32_t core_sig, struct pcs_meta *meta, struct pcs_thumb *thumb);

uint32_t pcs_crc32(const void *p, size_t n);

#endif
