/* Command line front end of libsomrando with the same options as the C# headless build (tools/companion/randomizer/headless).
 * Used by the differential harness.  Original work of this repository (LGPL-2.1+).
 *
 *   sr_cli --mode rando --seed STR --src ROM --dst OUT [--set key=value]... [--cancel-at N] [--progress] [--supports] [--list-options]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "somrando_api.h"

extern void sr_settings_dump_defaults(const char *mode, FILE *f, int with_legal);
extern void sr_hacks_dump(FILE *f);

struct pstate {
  int verbose, cancel_at, last;
};

static int on_progress(void *user, int step, int total, const char *name) {
  struct pstate *p = (struct pstate *)user;
  if (p->verbose) printf("progress %d/%d %s\n", step, total, name);
  p->last = step;
  return p->cancel_at && step >= p->cancel_at;
}

int main(int argc, char **argv) {
  const char *mode = "rando", *seed = "1", *src = NULL, *dst = NULL, *difficulty = NULL;
  char lines[8192] = "";
  int list = 0, supports = 0, legal = 0, hacks = 0;
  struct pstate ps = {0, 0, 0};
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
#define NEXT() (i + 1 < argc ? argv[++i] : (fprintf(stderr, "missing value for %s\n", a), exit(2), (char *)0))
    if (!strcmp(a, "--mode")) mode = NEXT();
    else if (!strcmp(a, "--seed")) seed = NEXT();
    else if (!strcmp(a, "--src")) src = NEXT();
    else if (!strcmp(a, "--dst")) dst = NEXT();
    else if (!strcmp(a, "--difficulty")) difficulty = NEXT();
    else if (!strcmp(a, "--workdir")) (void)NEXT();
    else if (!strcmp(a, "--set")) { strncat(lines, NEXT(), sizeof lines - strlen(lines) - 2); strcat(lines, "\n"); }
    else if (!strcmp(a, "--cancel-at")) ps.cancel_at = atoi(NEXT());
    else if (!strcmp(a, "--progress")) ps.verbose = 1;
    else if (!strcmp(a, "--supports")) supports = 1;
    else if (!strcmp(a, "--list-options")) list = 1;
    else if (!strcmp(a, "--list-legal")) { list = 1; legal = 1; }
    else if (!strcmp(a, "--list-hacks")) hacks = 1;
    else { fprintf(stderr, "usage: sr_cli --mode M --seed S --src ROM --dst OUT [--set k=v]...\n"); return 2; }
  }
  if (hacks) { sr_hacks_dump(stdout); return 0; }
  if (list) { sr_settings_dump_defaults(mode, stdout, legal); return 0; }
  struct somrando_request req = {mode, seed, difficulty, lines, src, dst, NULL};
  char err[512] = "";
  if (supports) {
    int ok = somrando_supports(&req, err, sizeof err);
    printf("%s\n", ok ? "supported" : err);
    return ok ? 0 : 1;
  }
  if (!src || !dst) { fprintf(stderr, "--src and --dst are required\n"); return 2; }
  int rc = somrando_generate(&req, &on_progress, &ps, err, sizeof err);
  if (rc) { fprintf(stderr, "error %d: %s\n", rc, err); return 10 + rc; }
  printf("done %s\n", dst);
  return 0;
}
