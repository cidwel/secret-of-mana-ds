/* Tests of som_credits.c.  usage: test_credits [path/to/data/credits.json] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "som_credits.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

int main(int argc, char **argv) {
  char out[4096], small[200];
  int n;
  const char *j1 = "{\"schema\":1,\"title\":\"Thanks\",\"entries\":[{\"who\":\"A \\\"quoted\\\" name\",\"what\":\"Thing\",\"license\":\"MIT\",\"url\":\"https://x.y\"},"
                   "{\"author\":\"B\",\"patch\":\"Patch \\u00e9\",\"license\":\"n/a\",\"nested\":{\"who\":\"IGNORED\"}},{\"url\":\"only url\"}],\"note\":\"x\"}";
  n = sc_format(j1, out, sizeof out);
  CHECK(n > 0 && (size_t)n == strlen(out));
  CHECK(!strncmp(out, "Thanks\n\n", 8));
  CHECK(strstr(out, "A \"quoted\" name\nThing\nLicense: MIT\nhttps://x.y\n\n"));
  CHECK(strstr(out, "B\nPatch ?\n\n") && !strstr(out, "License: n/a") && !strstr(out, "IGNORED") && !strstr(out, "only url"));
  CHECK(sc_format("{}", out, sizeof out) == -1 && sc_format("{\"entries\":[]}", out, sizeof out) == -1 && sc_format(NULL, out, sizeof out) == -1);
  CHECK(sc_format("{\"entries\":[{\"who\":\"x\"", out, sizeof out) > 0);              /* truncated file: what parsed is kept */
  n = sc_format(j1, small, 60);                                                      /* too small: whole entries only */
  CHECK(n > 0 && n < 60 && small[n - 1] == '\n' && !strstr(small, "B\n"));
  CHECK(strstr(sc_fallback(), "Moppleton") && strstr(sc_fallback(), "romhacking"));
  if (argc > 1) {
    FILE *f = fopen(argv[1], "rb");
    static char buf[1 << 17];
    size_t r = 0;
    CHECK(f != NULL);
    if (f) { r = fread(buf, 1, sizeof buf - 1, f); buf[r] = 0; fclose(f); }
    n = sc_format(buf, out, sizeof out);
    CHECK(n > 0 && strstr(out, "Moppleton") && strstr(out, "romhacking"));
    printf("credits.json: %d bytes of text\n", n);
  }
  printf("test_credits: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
