/* libsomrando: the generator seam of the start menu (board item T9).
 *
 * The start menu (som_launcher.h) never calls a generator; the host does, through this interface.  Today the only implementation is the stub in
 * somrando_stub.c (somrando_available() == 0), so the Android app offers pre-generated ROMs.  The C port of Moppu/SecretOfManaRandomizer v1.49
 * (LGPL-2.1, plan in docs/randomizer-c-port-plan-20261018.md) will provide the same functions in libsomrando; replacing the stub is a build change,
 * the menu, the JNI layer and the Java code stay as they are.
 *
 * The request mirrors the C ABI of tools/companion/randomizer/headless/NativeExports.cs (somrando_generate) and the configuration of the menu:
 * mode and seed strings as the original app takes them, `difficulty` NULL for modes without one, `settings` the "key=value\n" lines
 * of sl_config_lines().  Output is a 4 MiB ROM at dst_rom, written to a temporary name and renamed by the caller after its own checks.
 */
#ifndef SOMRANDO_API_H
#define SOMRANDO_API_H

#include <stddef.h>

#define SOMRANDO_OK 0
#define SOMRANDO_ERR_UNAVAILABLE 1   /* no generator in this build */
#define SOMRANDO_ERR_FAILED 2        /* generation error, message in err */
#define SOMRANDO_ERR_CANCELLED 3     /* the progress callback asked to stop */
#define SOMRANDO_ERR_BAD_ROM 4       /* source is not the US v1.0 2 MiB ROM */
#define SOMRANDO_ERR_UNSUPPORTED 5   /* the request needs an option or mode the C port does not reproduce byte for byte yet; err names it */

struct somrando_request {
  const char *mode;        /* ancientcave, open, rando, bossrush, chaos */
  const char *seed;
  const char *difficulty;  /* casual, hard, reallyhard or NULL */
  const char *settings;    /* "key=value\n" lines, may be NULL */
  const char *src_rom;     /* the user's US v1.0 ROM, 2 MiB (a 512 byte copier header is accepted) */
  const char *dst_rom;
  const char *workdir;     /* scratch directory for logs, may be NULL */
};

/* Called before each generator step (the upstream pipeline has about 80 named steps: one per hack, plus the post-processing passes);
 * step counts from 1, total is fixed for the request; return nonzero to cancel (somrando_generate then returns SOMRANDO_ERR_CANCELLED
 * and leaves no output file). */
typedef int (*somrando_progress_fn)(void *user, int step, int total, const char *name);

/* 1 when the build offers generation: libsomrando built with SOMRANDO_ADVERTISE.  It does NOT mean every preset works: ask somrando_supports()
 * (or somrando_menu_preset_supported()) per request; only requests it accepts are byte-identical to the C# generator (see
 * docs/randomizer-c-port-progress-20261018.md).  0 for the stub and for a build without SOMRANDO_ADVERTISE. */
int somrando_available(void);
const char *somrando_version(void);     /* upstream version the port is byte-compatible with, "1.49" */
/* Checks a request without generating: 1 when somrando_generate() would produce the same ROM as the C# generator, otherwise 0 and `why`
 * says which option or mode is missing.  The stub returns 0 for everything.  Only mode, seed, difficulty and settings are read. */
int somrando_supports(const struct somrando_request *req, char *why, size_t why_len);
int somrando_generate(const struct somrando_request *req, somrando_progress_fn progress, void *user, char *err, size_t err_len);

#endif
