/* The list of generated Ancient Cave / randomizer runs of the SoM PC frontend (board item T12).  Every ROM the generator makes is a "run":
 * it keeps the configuration it was made from (so the start menu can show it), when it was last played, and it has its own battery save and
 * its own save states because both are keyed by the hash of that ROM.  The list lives in runs.json (written through pc_store: temp file, fsync,
 * rename, last good copy kept as .bak).  Runs are never removed from disk by this module; deleting one moves its files elsewhere (the host). No SDL. */
#ifndef PC_RUNS_H
#define PC_RUNS_H

#include <stddef.h>
#include <stdint.h>

#define PR_MAX 64

struct pr_run {
  char key[17];            /* configuration key (first 16 hex digits), also the ROM file name without extension */
  char preset[32], mode[24], seed[12], diff[16];
  char path[512];          /* the generated ROM */
  char lines[1024];        /* "key=value\n" generator options that differ from the mode defaults */
  int64_t created, last;   /* unix seconds; last = 0 when never played */
  int plays;
};
struct pr_list { struct pr_run run[PR_MAX]; int n; };

void pr_init(struct pr_list *l);
int pr_load(const char *path, struct pr_list *l);        /* 0 ok (possibly empty), -1 when neither the file nor its backup parse */
int pr_save(const char *path, const struct pr_list *l);  /* 0 ok */
int pr_find(const struct pr_list *l, const char *key);   /* index or -1 */
/* Adds a run, or refreshes the one with the same key (its play history stays).  Returns the index, or -1 when the list is full of other runs. */
int pr_add(struct pr_list *l, const struct pr_run *r);
void pr_played(struct pr_list *l, const char *key, int64_t now);
int pr_remove(struct pr_list *l, const char *key);       /* 0 ok, -1 not found */
void pr_sort(struct pr_list *l);                         /* last played first, never played by creation date */
/* Reads a som-dualscreen sidecar (<rom>.sfc.json) into a run; 0 ok, -1 when it is not one.  `rom_path` is stored as the path. */
int pr_from_sidecar(const char *json, const char *rom_path, struct pr_run *r);

#endif
