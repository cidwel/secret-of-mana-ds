/* File helpers of the SoM PC frontend (board item T10): atomic writes with a last-good backup, so a crash or a full disk while saving the
 * slot bindings or the options never leaves the player with an empty profile.  No SDL.  Pure C99 + POSIX. */
#ifndef PC_STORE_H
#define PC_STORE_H

#include <stddef.h>
#include <stdint.h>

int pc_mkdirs(const char *path);                                   /* 0 ok */
uint8_t *pc_slurp(const char *path, size_t *n);                    /* malloc'd, NULL when unreadable */
int pc_spit(const char *path, const void *d, size_t n);            /* write path.part, fsync, rename; 0 ok */

/* Saves text at path; the previous file (when it parses, see load) becomes path.bak first.  0 ok. */
int pc_store_save(const char *path, const char *text);

/* Loads the text at path.  `valid` (may be NULL) decides whether a text is usable.  A file that exists but is not valid is moved to
 * path.bad and the backup path.bak is tried.  *source: 0 = path, 1 = backup, 2 = nothing usable (returns NULL), 3 = a bad file was set aside.
 * Returns malloc'd text (NUL terminated) or NULL. */
char *pc_store_load(const char *path, int (*valid)(const char *text), int *source);

#endif
