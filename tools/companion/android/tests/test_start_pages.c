/* Host test and renderer of som_start_pages.c: test_start_pages OUTDIR (writes PPMs of the pages).  Uses the repository's gameplay menu and credits. */
#include "../app/src/main/cpp/som_start_pages.h"
#include "../../launcher/som_launcher.h"
#include "../../launcher/som_launcher_draw.h"
#include "../../som_gameplay_apply.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, bad;
#define CHECK(c) do { checks++; if (!(c)) { bad++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static char *slurp(const char *p) {
  FILE *f = fopen(p, "rb");
  long n;
  char *b;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1);
  if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); return NULL; }
  b[n] = 0; fclose(f);
  return b;
}
static void ppm(const char *dir, const char *name, const uint32_t *px, int w, int h) {
  char p[600];
  FILE *f;
  snprintf(p, sizeof p, "%s/%s_%dx%d.ppm", dir, name, w, h);
  f = fopen(p, "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int i = 0; i < w * h; i++) { fputc(px[i] >> 16 & 255, f); fputc(px[i] >> 8 & 255, f); fputc(px[i] & 255, f); }
  fclose(f);
}

int main(int argc, char **argv) {
  const char *out = argc > 1 ? argv[1] : "/tmp/claude-1000/ssp";
  const char *root = argc > 2 ? argv[2] : ".";
  char path[700], json[8192], text[8192];
  char *menu, *cred;
  static struct sl_launcher L;
  int sizes[3][2] = {{1240, 969}, {1080, 1160}, {800, 480}};
  snprintf(path, sizeof path, "%s/tools/gameplay/gameplay_menu.json", root);
  menu = slurp(path);
  snprintf(path, sizeof path, "%s/data/credits.json", root);
  cred = slurp(path);
  CHECK(menu && cred);
  int n = ssp_gameplay_load(menu, NULL);
  CHECK(n >= 13);
  CHECK(ssp_gameplay_default());
  ssp_gameplay_json(json, sizeof json);
  CHECK(strstr(json, "options") != NULL);
  CHECK(ssp_credits_text(cred, text, sizeof text) > 50 && strstr(text, "# Moppleton") != NULL && strstr(text, "github.com/Moppu") != NULL);
  ssp_set_credits_json(cred);
  for (int si = 0; si < 3; si++) {
    int w = sizes[si][0], h = sizes[si][1];
    uint32_t *px = calloc((size_t)w * h, 4);
    ssp_open(SSP_GAMEPLAY);
    CHECK(ssp_page() == SSP_GAMEPLAY);
    ssp_draw(px, w, w, h);
    ppm(out, "gameplay", px, w, h);
    /* pad: down twice, right changes the second option; the result is non-default and is reported as dirty */
    ssp_key(SSP_DOWN); ssp_key(SSP_DOWN); ssp_key(SSP_RIGHT);
    CHECK(!ssp_gameplay_default() && ssp_dirty(1) && !ssp_dirty(0));
    ssp_draw(px, w, w, h);
    ppm(out, "gameplay_changed", px, w, h);
    ssp_gameplay_json(json, sizeof json);
    {   /* the file feeds the applier */
      struct sga_opts o; char err[160];
      CHECK(sga_parse(&o, json, err, sizeof err) == 0 && !sga_is_default(&o));
    }
    ssp_key(SSP_Y);                                  /* Y sets the option back to its default */
    ssp_key(SSP_B);
    CHECK(ssp_page() == SSP_NONE);
    /* touch: footer opens the page, a tap on a row focuses it, BACK closes */
    memset(px, 0, (size_t)w * h * 4);
    sl_init(&L, 1); sl_set_view(&L, w, h);
    sl_draw(&L, px, w, w, h);
    ssp_draw_footer(px, w, w, h, 0, 0);
    ppm(out, "main_footer", px, w, h);
    CHECK(ssp_footer_hit(w / 4, h - 20, w, h) == SSP_H_GAMEPLAY && ssp_footer_hit(w * 3 / 4, h - 20, w, h) == SSP_H_CREDITS && ssp_footer_hit(w / 2, h / 2, w, h) == SSP_H_NONE);
    ssp_open(SSP_GAMEPLAY); ssp_draw(px, w, w, h);
    ssp_tap(w / 12 + 20, h / 20 + 32 * (h / 300 > 0 ? h / 300 : 1) + 3 * 11 * (h / 300 > 0 ? h / 300 : 1) + 5, w, h);
    ssp_tap(w - w / 12 - 10, h / 20 + 3, w, h);       /* BACK is at the top right */
    CHECK(ssp_page() == SSP_NONE);
    /* credits */
    ssp_open(SSP_CREDITS); ssp_draw(px, w, w, h);
    ppm(out, "credits", px, w, h);
    ssp_key(SSP_R); ssp_draw(px, w, w, h);
    ppm(out, "credits_page2", px, w, h);
    ssp_key(SSP_DOWN); ssp_key(SSP_UP);
    ssp_key(SSP_B);
    CHECK(ssp_page() == SSP_NONE);
    free(px);
  }
  free(menu); free(cred);
  printf("start pages: %d checks, %d failures\n", checks, bad);
  return bad ? 1 : 0;
}
