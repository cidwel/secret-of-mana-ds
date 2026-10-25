/* Tests of the in-game menu (pc_menu.c): navigation with pad buttons, the actions it posts, the video settings it edits, the slot cards,
 * the credits scroll and golden hashes of every page at three window sizes.
 *   test_pcmenu                  run
 *   test_pcmenu --update-hashes  rewrite golden_menu.txt (review the diff first)
 * No SDL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_menu.h"
#include "pc_shader.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint32_t fnv(const uint32_t *p, size_t n) { uint32_t h = 2166136261u; for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; } return h; }
struct gold { char name[64]; uint32_t h; };
static struct gold gold[64]; static int ngold, update;
static FILE *out_gold;
static void gold_load(const char *path) {
  FILE *f = fopen(path, "r");
  char n[64]; unsigned h;
  if (!f) return;
  while (ngold < 64 && fscanf(f, "%63s %x", n, &h) == 2) { snprintf(gold[ngold].name, 64, "%s", n); gold[ngold++].h = h; }
  fclose(f);
}
static void golden(const char *name, const uint32_t *px, size_t n) {
  uint32_t h = fnv(px, n);
  if (update) { fprintf(out_gold, "%s %08x\n", name, h); return; }
  for (int i = 0; i < ngold; i++) if (!strcmp(gold[i].name, name)) { checks++; if (gold[i].h != h) { fails++; printf("FAIL golden %s: %08x expected %08x\n", name, h, gold[i].h); } return; }
  checks++; fails++; printf("FAIL golden %s missing (%08x)\n", name, h);
}
static int act(struct pcm *m, int *arg) { int a = 0; if (!pcm_poll(m, &a, arg)) return 0; return a; }

static const char CREDITS[] =
  "SPECIAL THANKS\n\nMOPPLETON AND CONTRIBUTORS\nSECRET OF MANA RANDOMIZER AND ITS ANCIENT CAVE MODE. LIBSOMRANDO IS A C PORT OF THEIR GENERATOR.\nLGPL-2.1  HTTPS://GITHUB.COM/MOPPU/SECRETOFMANARANDOMIZER\n\n"
  "AUTHORS OF THE SECRET OF MANA QOL AND BUG FIX PATCHES ON ROMHACKING.NET\nBEHAVIOURS THAT INSPIRED THE OPTIONAL TOGGLES.\nHTTPS://WWW.ROMHACKING.NET/\n\n"
  "THE REVERSE ENGINEERING AND DOCUMENTATION COMMUNITY\nDISASSEMBLIES, DATA MAPS, TOOLS AND WRITE-UPS.\n\nMORE LINES TO SCROLL 1\nMORE LINES TO SCROLL 2\nMORE LINES TO SCROLL 3\nMORE LINES TO SCROLL 4\nMORE LINES TO SCROLL 5\nMORE LINES TO SCROLL 6\nMORE LINES TO SCROLL 7\nMORE LINES TO SCROLL 8\n";

static void fixture(struct pcm *m, uint32_t *thumb) {
  for (int i = 0; i < 64 * 99; i++) thumb[i] = 0xFF000000u | (uint32_t)((i * 7) & 255) << 16 | (uint32_t)((i / 99) * 4) << 8 | 0x80;
  for (int i = 0; i < PCS_SLOTS; i++) {
    m->slot[i].status = i % 4 == 3 ? PCS_EMPTY : i == 6 ? PCS_BAD : i == 8 ? PCS_INCOMPAT : PCS_OK;
    if (m->slot[i].status == PCS_OK) { snprintf(m->slot[i].when, sizeof m->slot[i].when, "2026-10-%02u 18:%02u", (unsigned)(10 + i) % 28, (unsigned)(5 + i) % 60); m->slot[i].tw = 99; m->slot[i].th = 64; m->slot[i].thumb = thumb; }
  }
}

int main(int argc, char **argv) {
  struct pc_cfg cfg;
  struct pcm m;
  int arg = -1;
  static uint32_t thumb[64 * 99];
  update = argc > 1 && !strcmp(argv[1], "--update-hashes");
  if (update) out_gold = fopen("golden_menu.txt", "w"); else gold_load("golden_menu.txt");
  pc_cfg_defaults(&cfg);
  pcm_init(&m, &cfg);
  m.in_game = 1; m.can_launcher = 1; m.credits = CREDITS;
  fixture(&m, thumb);

  /* root page */
  CHECK(!m.open);
  pcm_key(&m, PCM_DOWN); CHECK(m.cursor[PCM_ROOT] == 0);           /* closed: ignored */
  pcm_open(&m, PCM_ROOT);
  CHECK(m.open && m.page == PCM_ROOT && pcm_root_count(&m) == 8 && !strcmp(pcm_root_label(&m, 0), "RESUME") && !strcmp(pcm_root_label(&m, 7), "QUIT"));
  pcm_key(&m, PCM_UP); CHECK(m.cursor[PCM_ROOT] == 7);              /* wraps */
  pcm_key(&m, PCM_DOWN); CHECK(m.cursor[PCM_ROOT] == 0);
  pcm_key(&m, PCM_ACCEPT); CHECK(act(&m, &arg) == PCM_A_CLOSE);
  pcm_key(&m, PCM_BACK); CHECK(act(&m, &arg) == PCM_A_CLOSE);      /* B resumes in game */
  pcm_key(&m, PCM_DOWN); pcm_key(&m, PCM_DOWN); pcm_key(&m, PCM_DOWN); pcm_key(&m, PCM_ACCEPT);
  CHECK(act(&m, &arg) == PCM_A_SLOTS);
  m.cursor[PCM_ROOT] = 6; pcm_key(&m, PCM_ACCEPT); CHECK(act(&m, &arg) == PCM_A_LAUNCHER);
  m.cursor[PCM_ROOT] = 7; pcm_key(&m, PCM_ACCEPT); CHECK(act(&m, &arg) == PCM_A_QUIT);
  CHECK(act(&m, &arg) == 0);
  /* before a game is chosen: no states, no slots, B does nothing */
  {
    struct pcm n;
    pcm_init(&n, &cfg); n.credits = CREDITS;
    pcm_open(&n, PCM_ROOT);
    CHECK(pcm_root_count(&n) == 3 && !strcmp(pcm_root_label(&n, 0), "VIDEO") && !strcmp(pcm_root_label(&n, 2), "QUIT"));
    pcm_key(&n, PCM_BACK); CHECK(act(&n, &arg) == 0);
  }

  /* save page: cards, overwrite confirmation, empty slot saves at once */
  cfg.state_slot = 2;
  pcm_open(&m, PCM_ROOT); m.cursor[PCM_ROOT] = 1; pcm_key(&m, PCM_ACCEPT);
  CHECK(m.page == PCM_SAVE && m.cursor[PCM_SAVE] == 2);              /* starts on the current slot */
  pcm_key(&m, PCM_ACCEPT); CHECK(act(&m, &arg) == 0 && m.confirm == 3);   /* slot 3 is in use: asks again */
  pcm_key(&m, PCM_RIGHT); CHECK(m.confirm == 0 && m.cursor[PCM_SAVE] == 3);
  pcm_key(&m, PCM_ACCEPT); CHECK(act(&m, &arg) == PCM_A_SAVE && arg == 3);     /* slot 4 is empty */
  pcm_key(&m, PCM_ACCEPT); pcm_key(&m, PCM_ACCEPT); CHECK(act(&m, &arg) == PCM_A_SAVE);   /* empty again: no confirmation (host has not refreshed) */
  m.cursor[PCM_SAVE] = 0; pcm_key(&m, PCM_ACCEPT); pcm_key(&m, PCM_ACCEPT); CHECK(act(&m, &arg) == PCM_A_SAVE && arg == 0);
  pcm_key(&m, PCM_DOWN); CHECK(m.cursor[PCM_SAVE] == 2);
  pcm_key(&m, PCM_UP); pcm_key(&m, PCM_UP); CHECK(m.cursor[PCM_SAVE] == 8);     /* wraps through the grid */
  pcm_key(&m, PCM_LEFT); pcm_key(&m, PCM_LEFT); CHECK(m.cursor[PCM_SAVE] == 8);
  pcm_key(&m, PCM_RIGHT); pcm_key(&m, PCM_RIGHT); CHECK(m.cursor[PCM_SAVE] == 9);
  /* L / R switch to the load page keeping the slot */
  pcm_key(&m, PCM_TAB_NEXT); CHECK(m.page == PCM_LOAD && m.cursor[PCM_LOAD] == 9);
  m.cursor[PCM_LOAD] = 7; pcm_key(&m, PCM_ACCEPT); CHECK(act(&m, &arg) == 0);   /* slot 8 is empty: nothing */
  m.cursor[PCM_LOAD] = 6; pcm_key(&m, PCM_ACCEPT); CHECK(act(&m, &arg) == 0 && strstr(m.note, "DAMAGED"));
  m.cursor[PCM_LOAD] = 8; pcm_key(&m, PCM_ACCEPT); CHECK(act(&m, &arg) == 0 && strstr(m.note, "ANOTHER VERSION"));
  m.cursor[PCM_LOAD] = 4; pcm_key(&m, PCM_ACCEPT); CHECK(act(&m, &arg) == PCM_A_LOAD && arg == 4 && m.note[0] == 0);
  pcm_key(&m, PCM_TAB_PREV); CHECK(m.page == PCM_SAVE && m.cursor[PCM_SAVE] == 4);
  pcm_key(&m, PCM_BACK); CHECK(m.page == PCM_ROOT);
  pcm_close(&m); CHECK(!m.open);

  /* video page: every row changes the cfg and posts VIDEO; values survive the JSON round trip */
  {
    char v[64], js[4096];
    struct pc_cfg back;
    pc_cfg_defaults(&cfg);
    pcm_open(&m, PCM_VIDEO);
    CHECK(m.page == PCM_VIDEO);
    pcm_video_value(&m, PCM_V_FILTER, v, sizeof v); CHECK(!strcmp(v, "SHARP BILINEAR"));
    pcm_video_value(&m, PCM_V_SIZE, v, sizeof v); CHECK(!strcmp(v, "1080P"));
    pcm_video_value(&m, PCM_V_SCREEN, v, sizeof v); CHECK(!strcmp(v, "WINDOWED"));
    pcm_video_value(&m, PCM_V_ASPECT, v, sizeof v); CHECK(!strcmp(v, "WIDE 46"));
    pcm_video_value(&m, PCM_V_SCALING, v, sizeof v); CHECK(!strcmp(v, "FIT"));
    pcm_video_value(&m, PCM_V_INTEGER, v, sizeof v); CHECK(!strcmp(v, "OFF"));
    pcm_video_value(&m, PCM_V_VSYNC, v, sizeof v); CHECK(!strcmp(v, "ON"));
    pcm_key(&m, PCM_RIGHT); CHECK(cfg.shader == PC_SH_SCANLINES && act(&m, &arg) == PCM_A_VIDEO && arg == PCM_V_FILTER);
    pcm_key(&m, PCM_LEFT); pcm_key(&m, PCM_LEFT); CHECK(cfg.shader == PC_SH_PLAIN); act(&m, &arg); act(&m, &arg);
    pcm_key(&m, PCM_LEFT); CHECK(cfg.shader == PC_SH_LCD); act(&m, &arg);       /* wraps backwards */
    pcm_key(&m, PCM_DOWN); pcm_key(&m, PCM_RIGHT); CHECK(cfg.win_w == 2560 && cfg.win_h == 1440 && act(&m, &arg) == PCM_A_VIDEO && arg == PCM_V_SIZE);
    pcm_key(&m, PCM_DOWN); pcm_key(&m, PCM_RIGHT); CHECK(cfg.screen == PC_BORDERLESS); act(&m, &arg);
    pcm_key(&m, PCM_ACCEPT); CHECK(cfg.screen == PC_EXCLUSIVE); act(&m, &arg);
    pcm_key(&m, PCM_ACCEPT); CHECK(cfg.screen == PC_WINDOWED); act(&m, &arg);
    pcm_key(&m, PCM_DOWN); pcm_key(&m, PCM_RIGHT); CHECK(cfg.wide == 27); act(&m, &arg);
    pcm_key(&m, PCM_RIGHT); pcm_key(&m, PCM_RIGHT); CHECK(cfg.wide == 0); act(&m, &arg); act(&m, &arg);
    pcm_video_value(&m, PCM_V_ASPECT, v, sizeof v); CHECK(!strcmp(v, "4:3"));
    pcm_key(&m, PCM_RIGHT); CHECK(cfg.wide == 46); act(&m, &arg);
    pcm_key(&m, PCM_DOWN); pcm_key(&m, PCM_RIGHT); CHECK(cfg.fit == PC_FIT_FILL); act(&m, &arg);
    pcm_key(&m, PCM_DOWN); pcm_key(&m, PCM_ACCEPT); CHECK(cfg.fit == PC_FIT_INTEGER); act(&m, &arg);
    pcm_video_value(&m, PCM_V_SCALING, v, sizeof v); CHECK(!strcmp(v, "-"));
    pcm_key(&m, PCM_UP); pcm_key(&m, PCM_RIGHT); CHECK(cfg.fit == PC_FIT_INTEGER && act(&m, &arg) == 0);   /* scaling is moot while integer is on */
    pcm_key(&m, PCM_DOWN); pcm_key(&m, PCM_ACCEPT); CHECK(cfg.fit == PC_FIT_FIT); act(&m, &arg);
    pcm_key(&m, PCM_DOWN); pcm_key(&m, PCM_ACCEPT); CHECK(cfg.vsync == 0); act(&m, &arg);
    pcm_key(&m, PCM_DOWN); CHECK(m.cursor[PCM_VIDEO] == PCM_V_BACK);
    pcm_key(&m, PCM_ACCEPT); CHECK(m.page == PCM_ROOT);
    CHECK(pc_cfg_to_json(&cfg, js, sizeof js) > 0);
    pc_cfg_defaults(&back);
    CHECK(pc_cfg_from_json(&back, js) == 0 && back.shader == cfg.shader && back.win_w == 2560 && back.win_h == 1440 && back.wide == 46 && back.fit == PC_FIT_FIT && back.vsync == 0 && back.screen == PC_WINDOWED);
    pc_cfg_defaults(&cfg);
  }

  /* credits: scrolling is clamped by what the last draw measured */
  {
    static uint32_t px[1280 * 720];
    pcm_open(&m, PCM_ROOT); m.cursor[PCM_ROOT] = 5; pcm_key(&m, PCM_ACCEPT);
    CHECK(m.page == PCM_CREDITS);
    pcm_draw(&m, px, 1280, 1280, 720);
    CHECK(m.credits_lines > m.credits_vis && m.credits_vis > 4);
    pcm_key(&m, PCM_UP); CHECK(m.scroll == 0);
    for (int i = 0; i < 100; i++) pcm_key(&m, PCM_DOWN);
    CHECK(m.scroll == m.credits_lines - m.credits_vis);
    pcm_key(&m, PCM_TAB_PREV); CHECK(m.scroll == m.credits_lines - m.credits_vis - (m.credits_vis - 1) || m.scroll == 0);
    pcm_scroll(&m, -1000); CHECK(m.scroll == 0);
    pcm_scroll(&m, 3); CHECK(m.scroll == (m.credits_lines - m.credits_vis < 3 ? m.credits_lines - m.credits_vis : 3));
    pcm_key(&m, PCM_BACK); CHECK(m.page == PCM_ROOT);
    m.credits = "";
    pcm_open(&m, PCM_CREDITS); pcm_draw(&m, px, 1280, 1280, 720);
    CHECK(m.scroll == 0);
    m.credits = CREDITS;
  }

  /* drawing: the closed menu is fully transparent; open pages are not; golden hashes at three sizes */
  {
    static const int sz[3][2] = {{1920, 1080}, {1280, 720}, {640, 360}};
    static const char *const pn[PCM_PAGES] = {"root", "save", "load", "video", "credits"};
    for (int s = 0; s < 3; s++) {
      int w = sz[s][0], h = sz[s][1];
      uint32_t *px = malloc((size_t)w * h * 4);
      pc_cfg_defaults(&cfg);
      pcm_close(&m);
      pcm_draw(&m, px, w, w, h);
      { int any = 0; for (size_t i = 0; i < (size_t)w * h; i++) if (px[i]) { any = 1; break; } CHECK(!any); }
      for (int p = 0; p < PCM_PAGES; p++) {
        char name[64];
        int any = 0;
        pcm_open(&m, p);
        if (p == PCM_SAVE) { m.cursor[PCM_SAVE] = 1; m.confirm = 2; }
        if (p == PCM_LOAD) m.cursor[PCM_LOAD] = 6;
        if (p == PCM_VIDEO) m.cursor[PCM_VIDEO] = PCM_V_ASPECT;
        pcm_draw(&m, px, w, w, h);
        for (size_t i = 0; i < (size_t)w * h; i++) if (px[i] >> 24) { any = 1; break; }
        CHECK(any);
        CHECK(px[0] == 0);                                         /* the corners of the window stay transparent */
        snprintf(name, sizeof name, "%s_%dx%d", pn[p], w, h);
        golden(name, px, (size_t)w * h);
      }
      free(px);
    }
  }
  /* a tiny window must not crash */
  {
    static uint32_t px[320 * 240];
    for (int p = 0; p < PCM_PAGES; p++) { pcm_open(&m, p); pcm_draw(&m, px, 320, 320, 240); }
    pcm_open(&m, PCM_CREDITS); m.credits = NULL ? "" : CREDITS; pcm_draw(&m, px, 320, 320, 240);
  }
  if (update) fclose(out_gold);
  printf("test_pcmenu: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
