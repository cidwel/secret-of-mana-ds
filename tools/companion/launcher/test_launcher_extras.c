/* Tests of the optional main menu entries (gameplay tweaks, display, credits) and of the credits screen of the start menu.
 * Build: see run_launcher_tests.sh.  No SDL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "som_launcher.h"
#include "som_launcher_draw.h"
#include "som_credits.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static int poll(struct sl_launcher *L) { int a = 0; return sl_poll(L, &a) ? a : 0; }
static int hit_center(struct sl_launcher *L, int w, int h, int kind, int *x, int *y) {
  struct sl_metrics m; struct sl_hit hits[SL_MAX_HITS];
  sl_metrics_for(w, h, &m);
  int n = sl_layout(L, &m, hits, SL_MAX_HITS);
  for (int i = 0; i < n; i++) if (hits[i].kind == kind) { *x = (hits[i].r.x + hits[i].r.w / 2) * m.s; *y = (hits[i].r.y + hits[i].r.h / 2) * m.s; return 1; }
  return 0;
}

int main(int argc, char **argv) {
  static struct sl_launcher L;
  static char text[8192], credits_json[1 << 16];
  static uint32_t px[1920 * 1080];
  int x, y;
  sl_init(&L, 7);
  sl_set_view(&L, 800, 480);

  /* default menu: three entries, nothing else */
  CHECK(sl_main_count(&L) == 3 && sl_main_kind(&L, 0) == SH_MAIN_ORIGINAL && sl_main_kind(&L, 1) == SH_MAIN_RANDOM && sl_main_kind(&L, 2) == SH_MAIN_SAVED);
  sl_key(&L, SL_UP); CHECK(L.cursor == 2); sl_key(&L, SL_DOWN); CHECK(L.cursor == 0);
  CHECK(!hit_center(&L, 800, 480, SH_MAIN_CREDITS, &x, &y));

  /* all extras */
  snprintf(text, sizeof text, "Special thanks\n\nMoppleton and contributors\nA long line that has to be wrapped because it is longer than forty columns of text, really.\nhttps://example.org/a/very/long/url/that/has/no/spaces/at/all/anywhere\n\n");
  for (int i = 0; i < 30; i++) { char b[40]; snprintf(b, sizeof b, "Filler line %d\n", i); strcat(text, b); }
  sl_set_extras(&L, SL_X_TWEAKS | SL_X_DISPLAY | SL_X_CREDITS, text);
  sl_set_tweaks_status(&L, "ACTIVE");
  CHECK(sl_main_count(&L) == 6 && sl_main_kind(&L, 3) == SH_MAIN_TWEAKS && sl_main_kind(&L, 4) == SH_MAIN_DISPLAY && sl_main_kind(&L, 5) == SH_MAIN_CREDITS);
  sl_set_extras(&L, SL_X_CREDITS, NULL);
  CHECK(sl_main_count(&L) == 4 && sl_main_kind(&L, 3) == SH_MAIN_CREDITS);
  sl_set_extras(&L, SL_X_TWEAKS | SL_X_CREDITS, NULL);
  CHECK(sl_main_count(&L) == 5 && sl_main_kind(&L, 3) == SH_MAIN_TWEAKS && sl_main_kind(&L, 4) == SH_MAIN_CREDITS);
  sl_set_extras(&L, SL_X_TWEAKS | SL_X_DISPLAY | SL_X_CREDITS, NULL);

  /* pad: walk the ring, open each entry */
  sl_key(&L, SL_UP); CHECK(L.cursor == 5);
  sl_key(&L, SL_DOWN); CHECK(L.cursor == 0);
  for (int i = 0; i < 3; i++) sl_key(&L, SL_DOWN);
  CHECK(L.cursor == 3); sl_key(&L, SL_A); CHECK(poll(&L) == SL_ACT_TWEAKS && L.screen == SL_MAIN);
  sl_key(&L, SL_DOWN); sl_key(&L, SL_START); CHECK(poll(&L) == SL_ACT_DISPLAY);
  sl_key(&L, SL_DOWN); sl_key(&L, SL_A); CHECK(L.screen == SL_CREDITS && L.cred_scroll == 0 && poll(&L) == 0);
  CHECK(!strcmp(sl_pad_hint(&L), "UP DOWN scroll   L1 R1 page   B back"));
  /* wrapping: nothing longer than the column count, the long URL is cut in pieces, the blank line is kept */
  {
    int total = sl_credits_count(&L);
    char line[SL_TEXT];
    CHECK(total > 35);
    for (int i = 0; i < total; i++) { CHECK(sl_credits_line(&L, i, line, sizeof line)); CHECK(strlen(line) <= SL_CREDITS_COLS); }
    CHECK(!sl_credits_line(&L, total, line, sizeof line) && !sl_credits_line(&L, -1, line, sizeof line));
    sl_credits_line(&L, 0, line, sizeof line); CHECK(!strcmp(line, "Special thanks"));
    sl_credits_line(&L, 1, line, sizeof line); CHECK(line[0] == 0);
    sl_credits_line(&L, 3, line, sizeof line); CHECK(!strcmp(line, "A long line that has to be wrapped"));
  }
  /* scrolling is clamped to the text */
  {
    int total = sl_credits_count(&L), vis = L.cred_vis;
    CHECK(vis > 5 && total > vis);
    sl_key(&L, SL_UP); CHECK(L.cred_scroll == 0);
    sl_key(&L, SL_DOWN); CHECK(L.cred_scroll == 1);
    sl_key(&L, SL_R); CHECK(L.cred_scroll == 1 + vis - 1);
    for (int i = 0; i < 200; i++) sl_key(&L, SL_DOWN);
    CHECK(L.cred_scroll == total - vis);
    sl_key(&L, SL_L); CHECK(L.cred_scroll == total - vis - (vis - 1));
    sl_scroll(&L, -1000); CHECK(L.cred_scroll == 0);
    sl_scroll(&L, 2); CHECK(L.cred_scroll == 2);
  }
  sl_key(&L, SL_B); CHECK(L.screen == SL_MAIN && L.cursor == 5);                 /* back lands on the Credits entry */
  /* touch: tap the credits entry and the back button */
  CHECK(hit_center(&L, 800, 480, SH_MAIN_CREDITS, &x, &y));
  sl_tap(&L, x, y, 800, 480); CHECK(L.screen == SL_CREDITS);
  CHECK(hit_center(&L, 800, 480, SH_SCROLL_DOWN, &x, &y));
  sl_tap(&L, x, y, 800, 480); CHECK(L.cred_scroll > 0);
  CHECK(hit_center(&L, 800, 480, SH_BACK, &x, &y));
  sl_tap(&L, x, y, 800, 480); CHECK(L.screen == SL_MAIN);
  CHECK(hit_center(&L, 800, 480, SH_MAIN_TWEAKS, &x, &y));
  sl_tap(&L, x, y, 800, 480); CHECK(poll(&L) == SL_ACT_TWEAKS && L.cursor == 3);
  CHECK(hit_center(&L, 800, 480, SH_MAIN_DISPLAY, &x, &y));
  sl_tap(&L, x, y, 800, 480); CHECK(poll(&L) == SL_ACT_DISPLAY);
  /* no credits text: the screen opens empty and does not crash */
  {
    static struct sl_launcher E;
    sl_init(&E, 1); sl_set_extras(&E, SL_X_CREDITS, NULL); sl_set_view(&E, 800, 480);
    CHECK(sl_credits_count(&E) == 0);
    sl_key(&E, SL_DOWN); sl_key(&E, SL_DOWN); sl_key(&E, SL_DOWN); sl_key(&E, SL_A);
    CHECK(E.screen == SL_CREDITS);
    sl_key(&E, SL_DOWN); sl_key(&E, SL_R); CHECK(E.cred_scroll == 0);
    sl_draw(&E, px, 800, 800, 480);
  }
  /* drawing at the sizes of the PC window, the Thor bottom screen and the 3DS: nothing cut, nothing too wide */
  {
    static const int sz[4][2] = {{1920, 1080}, {800, 480}, {320, 240}, {1240, 1080}};
    for (int s = 0; s < 4; s++) {
      int w = sz[s][0], h = sz[s][1];
      sl_draw_audit(1);
      sl_set_view(&L, w, h);
      sl_show(&L, SL_MAIN); L.cursor = 0;
      for (int cur = 0; cur < 6; cur++) { L.cursor = cur; sl_draw(&L, px, w, w, h); }
      sl_show(&L, SL_CREDITS);
      for (int sc = 0; sc < 4; sc++) { L.cred_scroll = sc * 7; sl_scroll(&L, 0); sl_draw(&L, px, w, w, h); }
      if (sl_draw_audit(0)) printf("audit %dx%d: %s\n", w, h, sl_draw_audit_note());
      CHECK(sl_draw_audit(0) == 0);
    }
  }
  /* the real data/credits.json as the PC and the Thor load it */
  if (argc > 1) {
    FILE *f = fopen(argv[1], "rb");
    size_t r = 0;
    CHECK(f != NULL);
    if (f) { r = fread(credits_json, 1, sizeof credits_json - 1, f); credits_json[r] = 0; fclose(f); }
    CHECK(sc_format(credits_json, text, sizeof text) > 0);
    sl_init(&L, 3); sl_set_extras(&L, SL_X_CREDITS, text); sl_set_view(&L, 800, 480); sl_show(&L, SL_CREDITS);
    CHECK(sl_credits_count(&L) > 5);
    sl_draw_audit(1);
    for (int s = 0; s < 4; s++) { sl_set_view(&L, 320 + s * 200, 240 + s * 100); sl_draw(&L, px, 320 + s * 200, 320 + s * 200, 240 + s * 100); }
    CHECK(sl_draw_audit(0) == 0);
  }
  printf("test_launcher_extras: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
