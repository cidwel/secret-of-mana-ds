/* Calls somrando_generate from the NativeAOT shared library: test_native <lib.so> <mode> <seed> <src rom> <dst> [workdir] */
#include <dlfcn.h>
#include <stdio.h>
typedef int (*gen_fn)(const char *, const char *, const char *, const char *, const char *, const char *, const char *, char *, int);
int main(int argc, char **argv) {
  if (argc < 6) { fprintf(stderr, "usage: %s lib mode seed src dst [workdir]\n", argv[0]); return 2; }
  void *h = dlopen(argv[1], RTLD_NOW);
  if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 3; }
  gen_fn gen = (gen_fn)dlsym(h, "somrando_generate");
  const char *(*ver)(void) = (const char *(*)(void))dlsym(h, "somrando_version");
  if (!gen || !ver) { fprintf(stderr, "missing export\n"); return 3; }
  char err[512] = "";
  printf("somrando %s\n", ver());
  int rc = gen(argv[2], argv[3], argv[4], argv[5], "casual", "", argc > 6 ? argv[6] : ".", err, sizeof err);
  printf("rc=%d %s\n", rc, err);
  return rc;
}
