/* Tests of the generated runs list (pc_runs.c).  usage: test_runs WORK_DIR [sidecar.json] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_runs.h"
#include "pc_store.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static struct pr_run mk(const char *key, const char *seed, int64_t created) {
  struct pr_run r;
  memset(&r, 0, sizeof r);
  snprintf(r.key, sizeof r.key, "%s", key); snprintf(r.preset, sizeof r.preset, "ancientcave-short"); snprintf(r.mode, sizeof r.mode, "ancientcave");
  snprintf(r.seed, sizeof r.seed, "%s", seed); snprintf(r.diff, sizeof r.diff, "casual"); snprintf(r.path, sizeof r.path, "/roms/gen/%s \"q\".sfc", key);
  snprintf(r.lines, sizeof r.lines, "acLength=short\nname=a\\b\n");
  r.created = created;
  return r;
}

int main(int argc, char **argv) {
  static struct pr_list l, l2;
  char path[400], dir[300];
  if (argc < 2) return 2;
  snprintf(dir, sizeof dir, "%s", argv[1]);
  pc_mkdirs(dir);
  snprintf(path, sizeof path, "%s/runs.json", dir);

  CHECK(pr_load(path, &l) == 0 && l.n == 0);                       /* no file: an empty list */
  { struct pr_run a = mk("aaaa000000000001", "111", 1000), b = mk("bbbb000000000002", "222", 2000), c = mk("cccc000000000003", "333", 1500);
    CHECK(pr_add(&l, &a) == 0 && pr_add(&l, &b) == 1 && pr_add(&l, &c) == 2 && l.n == 3);
    CHECK(pr_add(&l, &b) == 1 && l.n == 3);                         /* same key: refreshed, not duplicated */
    pr_played(&l, "aaaa000000000001", 5000); pr_played(&l, "aaaa000000000001", 6000); pr_played(&l, "zzzz", 7000);
    CHECK(l.run[0].plays == 2 && l.run[0].last == 6000);
    a.plays = 0; a.last = 0; pr_add(&l, &a);
    CHECK(l.run[0].plays == 2 && l.run[0].last == 6000 && l.run[0].created == 1000);   /* a refresh keeps the history */
  }
  pr_sort(&l);
  CHECK(!strcmp(l.run[0].key, "aaaa000000000001") && !strcmp(l.run[1].key, "bbbb000000000002") && !strcmp(l.run[2].key, "cccc000000000003"));
  CHECK(pr_save(path, &l) == 0);
  CHECK(pr_load(path, &l2) == 0 && l2.n == 3);
  CHECK(!strcmp(l2.run[0].key, l.run[0].key) && !strcmp(l2.run[0].path, l.run[0].path) && !strcmp(l2.run[0].lines, l.run[0].lines) && l2.run[0].plays == 2 && l2.run[0].last == 6000);
  CHECK(!strcmp(l2.run[1].seed, "222") && !strcmp(l2.run[1].diff, "casual") && l2.run[1].created == 2000);
  CHECK(pr_find(&l2, "cccc000000000003") == 2 && pr_find(&l2, "nope") == -1);
  /* removal */
  CHECK(pr_remove(&l2, "bbbb000000000002") == 0 && l2.n == 2 && pr_remove(&l2, "bbbb000000000002") == -1 && !strcmp(l2.run[1].key, "cccc000000000003"));
  /* a second save keeps the first as the backup; a truncated file falls back to it */
  CHECK(pr_save(path, &l2) == 0);
  { size_t n; uint8_t *b = pc_slurp(path, &n); CHECK(b && n > 100); pc_spit(path, b, n / 2); free(b); }
  CHECK(pr_load(path, &l) == 0 && l.n == 3);                       /* backup had three runs */
  /* both damaged */
  { char bak[420]; snprintf(bak, sizeof bak, "%s.bak", path); pc_spit(bak, "{\"runs\": [{\"key\":", 17); pc_spit(path, "garbage", 7); }
  CHECK(pr_load(path, &l) == -1 && l.n == 0);
  /* the list is capped */
  pr_init(&l);
  for (int i = 0; i < PR_MAX + 5; i++) { char k[20]; struct pr_run r; snprintf(k, sizeof k, "k%015d", i); r = mk(k, "1", i); if (i < PR_MAX) CHECK(pr_add(&l, &r) == i); else CHECK(pr_add(&l, &r) == -1); }
  CHECK(l.n == PR_MAX);
  /* sidecar of som-dualscreen */
  if (argc > 2) {
    size_t n;
    uint8_t *b = pc_slurp(argv[2], &n);
    struct pr_run r;
    CHECK(b != NULL);
    if (b) {
      CHECK(pr_from_sidecar((char *)b, "/x/y.sfc", &r) == 0);
      CHECK(!strcmp(r.key, "2c845ac55030ff15") && !strcmp(r.mode, "ancientcave") && !strcmp(r.seed, "12345") && !strcmp(r.diff, "casual") && !strcmp(r.preset, "ancientcave-short"));
      CHECK(!strcmp(r.lines, "acLength=short\n") && !strcmp(r.path, "/x/y.sfc"));
      CHECK(pr_from_sidecar("{\"nothing\": 1}", "/x", &r) == -1);
      free(b);
    }
  }
  printf("test_runs: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
