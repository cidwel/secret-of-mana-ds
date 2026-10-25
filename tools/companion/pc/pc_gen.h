/* In-process generation of a run for the PC frontend: libsomrando (C port of Moppu/SecretOfManaRandomizer v1.49) called with the start menu's
 * configuration.  No SDL, no child process.  The SHA-256 of the result goes to the sidecar and the log only, never to the screen.
 * Without the library (stub build) every call fails with SOMRANDO_ERR_UNAVAILABLE. */
#ifndef PC_GEN_H
#define PC_GEN_H

#include <stddef.h>
#include "../launcher/somrando_api.h"

struct sl_launcher;

struct pcg_result {
  int rc;                  /* SOMRANDO_OK or one of SOMRANDO_ERR_* */
  char err[200];
  char sha256[65];         /* of the finished 4 MiB ROM, hex */
  double seconds;
};

/* Generates the configuration `L` holds from `src_rom` into `dst_rom` (written as dst_rom.part, checked, renamed).  `progress` may be NULL.
 * Returns r->rc. */
int pcg_generate(const struct sl_launcher *L, const char *src_rom, const char *dst_rom, somrando_progress_fn progress, void *user, struct pcg_result *r);
/* Writes <rom>.json next to the ROM: the configuration, its key, the result hash and the time.  The shape is the one pc_runs imports. 0 ok. */
int pcg_write_sidecar(const struct sl_launcher *L, const char *sidecar_path, const struct pcg_result *r);
/* 1 when the file is a 4 MiB image with the game title in the header. */
int pcg_looks_generated(const char *path);

#endif
