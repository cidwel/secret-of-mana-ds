/* Headless tests of the start menu model and renderer.  Build and run with run_launcher_tests.sh.
 *   test_launcher                      run the tests
 *   test_launcher --emit               print the canonical outputs of fixed configurations (cross-checked by test_launcher_config.py)
 *   test_launcher --render DIR [pack]  write PPM screenshots of every screen at several sizes (pack: som_ui_pack.bin, optional)
 *   test_launcher --update-golden      rewrite golden_hashes.txt
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "som_launcher.h"
#include "som_launcher_draw.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_S(a, b) do { checks++; if (strcmp((a), (b))) { fails++; printf("FAIL %s:%d  \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); } } while (0)

static int preset_index(const char *id) {
  for (int i = 0; i < sl_npresets; i++) if (!strcmp(sl_presets[i].id, id)) return i;
  return -1;
}
static int act(struct sl_launcher *L) { int a = SL_ACT_NONE; sl_poll(L, &a); return a; }
static void keys(struct sl_launcher *L, const int *k, int n) { for (int i = 0; i < n; i++) sl_key(L, k[i]); }
#define KEYS(L, ...) do { const int k_[] = {__VA_ARGS__}; keys(L, k_, (int)(sizeof k_ / sizeof k_[0])); } while (0)
static int row_of(const struct sl_launcher *L, int kind, int a) {
  for (int i = 0; i < L->nrows; i++) if (L->rows[i].kind == kind && L->rows[i].a == a) return i;
  return -1;
}
static void go_row(struct sl_launcher *L, int row) {
  int guard = 0;
  while (L->cursor < row && guard++ < 400) sl_key(L, SL_DOWN);
  while (L->cursor > row && guard++ < 400) sl_key(L, SL_UP);
}
static void tap_hit(struct sl_launcher *L, int w, int h, int kind, int a) {
  struct sl_metrics m;
  struct sl_hit hits[SL_MAX_HITS];
  sl_metrics_for(w, h, &m);
  int n = sl_layout(L, &m, hits, SL_MAX_HITS);
  for (int i = 0; i < n; i++)
    if (hits[i].kind == kind && (a < 0 || hits[i].a == a)) { sl_tap(L, (hits[i].r.x + hits[i].r.w / 2) * m.s, (hits[i].r.y + hits[i].r.h / 2) * m.s, w, h); return; }
  printf("FAIL no hit kind %d a %d on %s (%dx%d)\n", kind, a, sl_screen_name(L->screen), w, h);
  fails++;
}
static const char *val(const struct sl_launcher *L, const char *key, char *buf, size_t n) {
  int i = sl_find_opt(L, key);
  buf[0] = 0;
  if (i >= 0) sl_value_string(L, i, buf, n);
  return buf;
}

/* -------- catalogue, presets, serialisation */
static void test_catalogue(void) {
  struct sl_launcher L;
  char lines[512], b[64], js[2048];
  sl_init(&L, 1);
  CHECK(sl_nmodes == 5 && sl_npresets >= 15);
  CHECK(sl_presets[0].mode == 0 && !strcmp(sl_modes[0].key, "ancientcave"));    /* Ancient Cave first */
  for (int p = 0; p < sl_npresets; p++) {
    CHECK(sl_apply_preset(&L, p) == 0);
    CHECK(sl_config_json(&L, js, sizeof js) > 0 && strstr(js, "\"schema\":1"));
    const char *dn = NULL;
    CHECK(sl_difficulty(&L, &dn) == (sl_modes[sl_presets[p].mode].ndiff != 0));
    sl_config_lines(&L, lines, sizeof lines);
    if (!sl_presets[p].nset) CHECK(lines[0] == 0);                              /* presets without changes have no delta */
  }
  CHECK(sl_apply_preset(&L, -1) == -1 && sl_apply_preset(&L, sl_npresets) == -1);
  sl_apply_preset(&L, preset_index("ancientcave-short"));
  sl_config_lines(&L, lines, sizeof lines);
  CHECK_S(lines, "acLength=short\n");
  sl_apply_preset(&L, preset_index("ancientcave-gauntlet"));
  sl_config_cli(&L, lines, sizeof lines);
  CHECK(strstr(lines, "--difficulty reallyhard") && strstr(lines, "--option BossFrequency=every"));
  sl_apply_preset(&L, preset_index("open-owl"));
  sl_config_lines(&L, lines, sizeof lines);
  CHECK(strstr(lines, "confusionFix=no\nopDifficulty=hard\n") == lines);          /* sorted by key, byte order */
  CHECK(strstr(lines, "oopsAllThis") == NULL);                                     /* 21 is the generator default: not part of the delta */
  /* a hidden option (requirement not met) is not emitted: opNumSeeds needs opGoal=mtr */
  sl_apply_preset(&L, preset_index("open"));
  CHECK(sl_set_option(&L, "opNumSeeds", "4") == 0);
  sl_config_lines(&L, lines, sizeof lines);
  CHECK(lines[0] == 0);
  CHECK(sl_set_option(&L, "opGoal", "mtr") == 0);
  sl_config_lines(&L, lines, sizeof lines);
  CHECK_S(lines, "opGoal=mtr\nopNumSeeds=4\n");
  CHECK(sl_set_option(&L, "nonsense", "yes") == -1);
  CHECK(sl_set_option(&L, "opGoal", "bogus") == -1);
  CHECK(sl_set_option(&L, "opExp", "1001") == -1);
  CHECK(sl_set_option(&L, "opExp", "2.5") == 0);
  CHECK_S(val(&L, "opExp", b, sizeof b), "2.5");
  CHECK(sl_set_option(&L, "acLength", "short") == -1);                             /* not an option of this mode */
  CHECK(sl_set_seed(&L, "12345678") == 0 && !strcmp(L.cfg.seed, "12345678"));
  CHECK(sl_set_seed(&L, "00042") == 0 && !strcmp(L.cfg.seed, "42"));              /* no leading zeros in the string handed to the generator */
  CHECK(sl_set_seed(&L, "123456789") == -1 && sl_set_seed(&L, "") == -1 && sl_set_seed(&L, "12a") == -1);
  /* SHA-256 vector and key shape */
  uint8_t d[32];
  sl_sha256("abc", 3, d);
  CHECK(d[0] == 0xba && d[1] == 0x78 && d[2] == 0x16 && d[3] == 0xbf && d[31] == 0xad);
  sl_apply_preset(&L, 0);
  sl_set_seed(&L, "777");
  sl_config_keystring(&L, js, sizeof js);
  CHECK_S(js, "somrando/1.49|ancientcave|777|casual|");
  char k1[17], k2[17];
  sl_config_key(&L, k1);
  sl_set_option(&L, "acLength", "long");
  sl_config_key(&L, k2);
  CHECK(strlen(k1) == 16 && strcmp(k1, k2));
}

/* -------- randomize all */
static struct sl_launcher PRE;
static int32_t L_before_val(int p, int i) { (void)p; return PRE.cfg.val[i]; }   /* the options that are not randomized keep the preset value */

static void test_randomize(void) {
  struct sl_launcher L, M;
  char js[4096], js2[4096], lines[2048];
  for (int p = 0; p < sl_npresets; p++)
    for (uint64_t e = 1; e <= 60; e++) {
      sl_init(&L, e * 7919u + (uint64_t)p);
      sl_apply_preset(&L, p);
      sl_apply_preset(&PRE, p);
      sl_randomize_all(&L);
      int n = sl_nopts(&L);
      for (int i = 0; i < n; i++) {
        const struct sl_option *o = sl_opt(&L, i);
        int32_t v = L.cfg.val[i];
        if (!o->randomize) CHECK(v == L_before_val(p, i));
        if (o->type == SL_O_BOOL) CHECK(v == 0 || v == 1);
        else if (o->type == SL_O_ENUM) CHECK(v >= 0 && v < o->nvalues);
        else if (o->type == SL_O_NUM) CHECK(v >= o->min && v <= o->max && (v - o->min) % o->step == 0);
        else CHECK(v != 0);
      }
      CHECK(sl_config_json(&L, js, sizeof js) > 0);
      /* every emitted option is accepted back by name and value, and gives the same output */
      sl_init(&M, 5);
      for (int q = 0; q < sl_npresets; q++) if (sl_presets[q].mode == sl_presets[p].mode && !sl_presets[q].nset && sl_presets[q].diff < 0 + 1) { sl_apply_preset(&M, q); break; }
      sl_set_seed(&M, L.cfg.seed);
      M.cfg.diff = L.cfg.diff;
      sl_config_lines(&L, lines, sizeof lines);
      for (char *ln = strtok(lines, "\n"); ln; ln = strtok(NULL, "\n")) {
        char *eq = strchr(ln, '=');
        *eq = 0;
        CHECK(sl_set_option(&M, ln, eq + 1) == 0);
      }
      char ka[1024], kb[1024];
      sl_config_keystring(&L, ka, sizeof ka);
      sl_config_keystring(&M, kb, sizeof kb);
      CHECK_S(ka, kb);
    }
  /* constraint: at least one character stays */
  sl_init(&L, 3);
  sl_apply_preset(&L, preset_index("ancientcave"));
  sl_set_option(&L, "acBoy", "no"); sl_set_option(&L, "acGirl", "no"); sl_set_option(&L, "acSprite", "no");
  sl_randomize_all(&L);
  CHECK(L.cfg.val[sl_find_opt(&L, "acBoy")] || L.cfg.val[sl_find_opt(&L, "acGirl")] || L.cfg.val[sl_find_opt(&L, "acSprite")]);
  /* determinism: same entropy, same result */
  struct sl_launcher A, B;
  sl_init(&A, 99); sl_init(&B, 99);
  sl_apply_preset(&A, preset_index("open")); sl_apply_preset(&B, preset_index("open"));
  sl_randomize_all(&A); sl_randomize_all(&B);
  sl_config_json(&A, js, sizeof js); sl_config_json(&B, js2, sizeof js2);
  CHECK_S(js, js2);
  sl_init(&B, 100);
  sl_apply_preset(&B, preset_index("open")); sl_randomize_all(&B);
  sl_config_json(&B, js2, sizeof js2);
  CHECK(strcmp(js, js2) != 0);
  /* Mana seeds min <= max after a swap */
  sl_apply_preset(&A, preset_index("open-mtr"));
  sl_set_option(&A, "opNumSeeds", "random"); sl_set_option(&A, "opMinSeeds", "7"); sl_set_option(&A, "opMaxSeeds", "2");
  sl_key(&A, SL_A);   /* no screen change expected; constraint applies on edit paths */
  sl_randomize_all(&A);
  int mn = atoi(sl_opt(&A, sl_find_opt(&A, "opMinSeeds"))->values[A.cfg.val[sl_find_opt(&A, "opMinSeeds")]].v);
  int mx = atoi(sl_opt(&A, sl_find_opt(&A, "opMaxSeeds"))->values[A.cfg.val[sl_find_opt(&A, "opMaxSeeds")]].v);
  CHECK(mn <= mx);
}

/* -------- button navigation, end to end */
static void test_navigation(void) {
  struct sl_launcher L;
  char b[64], js[2048];
  sl_init(&L, 42);
  sl_set_view(&L, 320, 240);
  CHECK(L.screen == SL_MAIN);
  sl_key(&L, SL_B);
  CHECK(act(&L) == SL_ACT_QUIT);
  sl_key(&L, SL_A);                                                                /* the first entry is the original game */
  CHECK(act(&L) == SL_ACT_PLAY_ORIGINAL && L.screen == SL_MAIN);
  KEYS(&L, SL_DOWN, SL_A);
  CHECK(L.screen == SL_PRESETS && L.cursor == 0 && L.rows[0].kind == SR_PRESET && L.rows[0].a == -1);
  sl_key(&L, SL_B);
  CHECK(L.screen == SL_MAIN && L.cursor == 1);                                      /* the cursor stays on the entry we came from */
  KEYS(&L, SL_UP, SL_A);
  CHECK(act(&L) == SL_ACT_PLAY_ORIGINAL);
  KEYS(&L, SL_DOWN, SL_A, SL_DOWN, SL_DOWN);                                       /* surprise me, header, Ancient Cave, short */
  CHECK(L.rows[L.cursor].kind == SR_PRESET && L.rows[L.cursor].a == preset_index("ancientcave-short"));
  sl_key(&L, SL_A);
  CHECK(L.screen == SL_TWEAK && L.cfg.mode == 0);
  { int row = -1; sl_key(&L, SL_B);                                               /* BACK keeps the cursor on the chosen preset */
    CHECK(L.screen == SL_PRESETS && L.rows[L.cursor].kind == SR_PRESET && L.rows[L.cursor].a == preset_index("ancientcave-short"));
    row = L.cursor; sl_key(&L, SL_B); KEYS(&L, SL_A);                              /* MAIN to PRESETS again: still on the last choice */
    CHECK(L.screen == SL_PRESETS && L.cursor == row);
    sl_key(&L, SL_A); }
  CHECK(L.screen == SL_TWEAK && L.cfg.mode == 0);
  CHECK_S(val(&L, "acLength", b, sizeof b), "short");
  /* seed entry with the touch keypad, 320x240 */
  go_row(&L, row_of(&L, SR_ACTION, SA_SEED));
  sl_key(&L, SL_A);
  CHECK(L.screen == SL_SEED);
  for (int i = 0; i < 9; i++) tap_hit(&L, 320, 240, SH_KEY, 10);                   /* DEL until empty */
  CHECK_S(L.cfg.seed, "0");
  tap_hit(&L, 320, 240, SH_KEY, 4); tap_hit(&L, 320, 240, SH_KEY, 2);
  CHECK_S(L.cfg.seed, "42");
  for (int i = 0; i < 10; i++) tap_hit(&L, 320, 240, SH_KEY, 9);                   /* capped at 8 digits */
  CHECK(strlen(L.cfg.seed) == 8);
  tap_hit(&L, 320, 240, SH_KEY, 11);                                               /* random seed */
  CHECK(strlen(L.cfg.seed) >= 1 && strlen(L.cfg.seed) <= 8 && strcmp(L.cfg.seed, "0"));
  sl_set_seed(&L, "42");
  KEYS(&L, SL_UP);                                                                 /* digit editing with buttons: units 2 -> 3 */
  CHECK_S(L.cfg.seed, "43");
  KEYS(&L, SL_LEFT, SL_UP);                                                        /* tens 4 -> 5 */
  CHECK_S(L.cfg.seed, "53");
  KEYS(&L, SL_DOWN, SL_DOWN, SL_DOWN, SL_DOWN, SL_DOWN, SL_DOWN);                  /* tens wraps: 5 -> 9, units untouched */
  CHECK_S(L.cfg.seed, "93");
  sl_set_seed(&L, "42");
  tap_hit(&L, 320, 240, SH_KEY, 12);                                               /* OK */
  CHECK(L.screen == SL_TWEAK);
  /* difficulty and options */
  go_row(&L, row_of(&L, SR_DIFF, 0));
  sl_key(&L, SL_RIGHT);
  const char *dn = NULL;
  sl_difficulty(&L, &dn);
  CHECK_S(dn, "hard");
  KEYS(&L, SL_RIGHT, SL_RIGHT);
  sl_difficulty(&L, &dn);
  CHECK_S(dn, "casual");                                                           /* wraps */
  go_row(&L, row_of(&L, SR_OPT, sl_find_opt(&L, "acLength")));
  KEYS(&L, SL_RIGHT);
  CHECK_S(val(&L, "acLength", b, sizeof b), "medium");
  KEYS(&L, SL_LEFT, SL_LEFT);
  CHECK_S(val(&L, "acLength", b, sizeof b), "long");
  /* constraint: switching off all three characters leaves one on */
  go_row(&L, row_of(&L, SR_OPT, sl_find_opt(&L, "acBoy")));
  sl_key(&L, SL_A);
  go_row(&L, row_of(&L, SR_OPT, sl_find_opt(&L, "acGirl")));
  sl_key(&L, SL_A);
  go_row(&L, row_of(&L, SR_OPT, sl_find_opt(&L, "acSprite")));
  sl_key(&L, SL_A);
  CHECK_S(val(&L, "acBoy", b, sizeof b), "yes");
  /* advanced options: hidden until "More options"; the multiple choice screen keeps one biome and returns to its row */
  CHECK(row_of(&L, SR_OPT, sl_find_opt(&L, "acBiomeTypes")) < 0);
  go_row(&L, row_of(&L, SR_TOGGLE_ADV, 0));
  sl_key(&L, SL_A);
  int br = row_of(&L, SR_OPT, sl_find_opt(&L, "acBiomeTypes"));
  CHECK(br > 0);
  go_row(&L, br);
  sl_key(&L, SL_A);
  CHECK(L.screen == SL_MULTI && L.nrows == 5);
  for (int i = 0; i < 5; i++) { sl_key(&L, SL_A); sl_key(&L, SL_DOWN); }
  sl_key(&L, SL_B);
  CHECK(L.screen == SL_TWEAK && L.cursor == br);
  CHECK(strlen(val(&L, "acBiomeTypes", b, sizeof b)) > 0);
  CHECK(strcmp(b, "forest,island,ruins,cave,manafortinterior") != 0);
  /* go */
  sl_key(&L, SL_START);
  CHECK(L.screen == SL_GENERATE && act(&L) == SL_ACT_GENERATE && L.gen == SL_GEN_ASK);
  sl_config_json(&L, js, sizeof js);
  CHECK(strstr(js, "\"seed\":\"42\"") && strstr(js, "\"acLength\":\"long\""));
  sl_gen_unavailable(&L, "Generator not available on this device yet (libsomrando pending)");
  CHECK(L.gen == SL_GEN_UNAVAILABLE && act(&L) == SL_ACT_NONE);
  sl_key(&L, SL_B);
  CHECK(L.screen == SL_TWEAK && act(&L) == SL_ACT_NONE);
  sl_key(&L, SL_START);
  CHECK(act(&L) == SL_ACT_GENERATE);
  sl_gen_begin(&L);
  sl_gen_progress(&L, 12, 90, "Adding: [Weapon randomizer]");
  CHECK(L.gen == SL_GEN_RUNNING && L.gen_step == 12 && L.gen_total == 90);
  sl_key(&L, SL_B);                                                                /* cancel */
  CHECK(act(&L) == SL_ACT_CANCEL && L.screen == SL_TWEAK);
  sl_key(&L, SL_START); act(&L);
  sl_gen_begin(&L);
  sl_gen_fail(&L, "Generation failed at step 3");
  CHECK(L.gen == SL_GEN_ERROR);
  sl_key(&L, SL_A);                                                                /* retry */
  CHECK(act(&L) == SL_ACT_GENERATE && L.gen == SL_GEN_ASK);
  sl_gen_done(&L, "/data/app/roms/gen/abcd.sfc");
  CHECK(L.screen == SL_PLAY && act(&L) == SL_ACT_PLAY);
  CHECK_S(sl_rom_path(&L), "/data/app/roms/gen/abcd.sfc");
  /* a cache hit goes straight to PLAY */
  sl_init(&L, 1);
  KEYS(&L, SL_DOWN, SL_A, SL_DOWN, SL_A, SL_START);
  CHECK(act(&L) == SL_ACT_GENERATE);
  sl_gen_cached(&L, "/x/y.sfc");
  CHECK(L.screen == SL_PLAY && act(&L) == SL_ACT_PLAY);
  /* surprise me lands on TWEAK with a valid random config */
  sl_init(&L, 77);
  KEYS(&L, SL_DOWN, SL_A, SL_A);
  CHECK(L.screen == SL_TWEAK);
  CHECK(sl_config_json(&L, js, sizeof js) > 0);
  /* reset restores the preset */
  sl_init(&L, 5);
  sl_apply_preset(&L, preset_index("ancientcave-short"));
  sl_set_option(&L, "acLength", "long");
  L.screen = SL_TWEAK; L.nrows = 0;
  sl_key(&L, SL_Y);
  CHECK_S(val(&L, "acLength", b, sizeof b), "short");
}

/* -------- touch */
static void test_touch(void) {
  static const int sizes[][2] = {{320, 240}, {1240, 1080}, {1920, 1080}, {1080, 1240}, {800, 480}};
  for (unsigned si = 0; si < sizeof sizes / sizeof sizes[0]; si++) {
    int w = sizes[si][0], h = sizes[si][1];
    struct sl_launcher L;
    char b[64];
    struct sl_metrics m;
    sl_metrics_for(w, h, &m);
    sl_init(&L, 9);
    sl_set_view(&L, w, h);
    CHECK(m.rows >= 6);
    tap_hit(&L, w, h, SH_MAIN_ORIGINAL, -1);
    CHECK(act(&L) == SL_ACT_PLAY_ORIGINAL);
    tap_hit(&L, w, h, SH_MAIN_RANDOM, -1);
    CHECK(L.screen == SL_PRESETS);
    /* tap the "Ancient Cave: short" row, wherever it is */
    int want = row_of(&L, SR_PRESET, preset_index("ancientcave-short"));
    tap_hit(&L, w, h, SH_ROW, want);
    CHECK(L.screen == SL_TWEAK);
    CHECK_S(val(&L, "acLength", b, sizeof b), "short");
    int r = row_of(&L, SR_OPT, sl_find_opt(&L, "acLength"));
    CHECK(r >= 0);
    L.scroll = r;                                                                    /* make it visible */
    tap_hit(&L, w, h, SH_ROW_INC, r);
    CHECK_S(val(&L, "acLength", b, sizeof b), "medium");
    tap_hit(&L, w, h, SH_ROW_DEC, r);
    CHECK_S(val(&L, "acLength", b, sizeof b), "short");
    tap_hit(&L, w, h, SH_ROW, r);                                                    /* center of the row: next value */
    CHECK_S(val(&L, "acLength", b, sizeof b), "medium");
    sl_scroll(&L, 0);
    int before = L.scroll;
    tap_hit(&L, w, h, SH_SCROLL_DOWN, -1);
    CHECK(L.scroll >= before);
    tap_hit(&L, w, h, SH_SCROLL_UP, -1);
    tap_hit(&L, w, h, SH_GO, -1);
    CHECK(L.screen == SL_GENERATE && act(&L) == SL_ACT_GENERATE);
    sl_gen_begin(&L);
    tap_hit(&L, w, h, SH_CANCEL, -1);
    CHECK(act(&L) == SL_ACT_CANCEL && L.screen == SL_TWEAK);
    tap_hit(&L, w, h, SH_BACK, -1);
    CHECK(L.screen == SL_PRESETS);
    tap_hit(&L, w, h, SH_BACK, -1);
    CHECK(L.screen == SL_MAIN);
    /* touch target audit: every tappable thing is at least 16 virtual pixels each way and inside the screen */
    for (int scr = 0; scr < SL_PLAY; scr++) {
      sl_init(&L, 4);
      sl_set_view(&L, w, h);
      sl_apply_preset(&L, preset_index("ancientcave"));
      L.screen = scr;
      if (scr == SL_MULTI) L.multi_opt = sl_find_opt(&L, "acBiomeTypes");
      if (scr == SL_PRESETS || scr == SL_TWEAK) sl_show(&L, (enum sl_screen)scr);
      if (scr == SL_MULTI) sl_show(&L, SL_MULTI);
      L.gen = SL_GEN_ERROR;
      struct sl_hit hits[SL_MAX_HITS];
      int n = sl_layout(&L, &m, hits, SL_MAX_HITS);
      for (int i = 0; i < n; i++) {
        const struct sl_hit *t = &hits[i];
        CHECK(t->r.x >= 0 && t->r.y >= 0 && t->r.x + t->r.w <= m.W && t->r.y + t->r.h <= m.H);
        CHECK(t->r.w >= 16 && t->r.h >= 16);
      }
    }
  }
}

/* -------- drawing */
static uint64_t fnv(const uint32_t *px, size_t n) {
  uint64_t h = 1469598103934665603ULL;
  for (size_t i = 0; i < n; i++) { h ^= px[i]; h *= 1099511628211ULL; }
  return h;
}
static void write_ppm(const char *dir, const char *name, const uint32_t *px, int w, int h) {
  char path[600];
  snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int i = 0; i < w * h; i++) { unsigned char c[3] = {(unsigned char)(px[i] >> 16), (unsigned char)(px[i] >> 8), (unsigned char)px[i]}; fwrite(c, 1, 3, f); }
  fclose(f);
}

static void scene(struct sl_launcher *L, int n, int w, int h) {
  sl_init(L, 12345);
  sl_set_status(L, "", "Pre-generated ROMs only");
  sl_set_view(L, w, h);
  sl_set_seed(L, "20261018");
  sl_apply_preset(L, 0);
  switch (n) {
    case 0: break;                                                                   /* MAIN */
    case 1: sl_show(L, SL_PRESETS); sl_key(L, SL_DOWN); sl_key(L, SL_DOWN); break;
    case 2: sl_apply_preset(L, preset_index("open")); KEYS(L, SL_DOWN, SL_A, SL_DOWN, SL_DOWN); while (L->screen != SL_TWEAK) { sl_key(L, SL_A); } break;
    case 3: sl_apply_preset(L, preset_index("ancientcave-marathon")); sl_show(L, SL_TWEAK); sl_key(L, SL_R); sl_key(L, SL_R); break;
    case 4: L->screen = SL_SEED; L->seed_pos = 5; break;
    case 5: L->screen = SL_GENERATE; L->gen = SL_GEN_RUNNING; L->gen_step = 34; L->gen_total = 90; snprintf(L->gen_name, sizeof L->gen_name, "Adding: [Weapon randomizer]"); break;
    case 6: sl_gen_unavailable(L, "Generator not available on this device yet (libsomrando pending). Push a generated ROM with: som-dualscreen push-random"); break;
    case 7: sl_gen_fail(L, "Generation failed at step 12 (Adding: [Event maker]). Seed 20261018 was not saved."); break;
    case 8: sl_gen_done(L, "/storage/emulated/0/Android/data/org.somdecomp.companion/files/roms/gen/0123456789abcdef.sfc"); break;
    case 9: sl_apply_preset(L, preset_index("ancientcave")); L->show_adv = 1; sl_show(L, SL_TWEAK); sl_key(L, SL_R); sl_key(L, SL_R); sl_key(L, SL_R); break;
    default: break;
  }
}
#define NSCENES 10

static void test_draw(const char *render_dir, const char *packfile, int update_golden) {
  static const int sizes[][2] = {{320, 240}, {1240, 1080}, {1920, 1080}, {800, 480}};
  unsigned char *pk = NULL;
  if (packfile) {
    FILE *f = fopen(packfile, "rb");
    int ok = 0;
    if (f) {
      fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
      pk = malloc((size_t)n);
      if (pk && fread(pk, 1, (size_t)n, f) == (size_t)n) ok = sl_draw_load_pack(pk, (size_t)n) == 0;
      fclose(f);
    }
    CHECK(ok);
  }
  FILE *gf = update_golden ? fopen("golden_hashes.txt", "w") : NULL;
  FILE *rd = (!update_golden && !packfile && !render_dir) ? fopen("golden_hashes.txt", "r") : NULL;
  for (unsigned si = 0; si < sizeof sizes / sizeof sizes[0]; si++) {
    int w = sizes[si][0], h = sizes[si][1];
    uint32_t *px = malloc((size_t)w * h * 4);
    for (int sc = 0; sc < NSCENES; sc++) {
      struct sl_launcher L;
      scene(&L, sc, w, h);
      sl_draw_audit(1);
      sl_draw(&L, px, w, w, h);
      int cut = sl_draw_audit(1);
      if (cut) printf("note: scene %d at %dx%d: %s\n", sc, w, h, sl_draw_audit_note());
      CHECK(cut == 0);
      /* not blank: several distinct colours */
      uint32_t seen[16]; int ns = 0;
      for (int i = 0; i < w * h && ns < 16; i += 7) { int f = 0; for (int k = 0; k < ns; k++) if (seen[k] == px[i]) f = 1; if (!f) seen[ns++] = px[i]; }
      CHECK(ns >= 8);
      char name[64];
      snprintf(name, sizeof name, "%s_%dx%d_%d", sl_screen_name(L.screen), w, h, sc);
      uint64_t hv = fnv(px, (size_t)w * h);
      if (render_dir) write_ppm(render_dir, name, px, w, h);
      if (gf) fprintf(gf, "%s %016llx\n", name, (unsigned long long)hv);
      if (rd) {
        char gname[96]; unsigned long long gh; int found = 0;
        rewind(rd);
        while (fscanf(rd, "%95s %llx", gname, &gh) == 2) if (!strcmp(gname, name)) { found = 1; CHECK(gh == hv); if (gh != hv) printf("golden mismatch %s\n", name); }
        CHECK(found);
      }
    }
    free(px);
  }
  if (gf) fclose(gf);
  if (rd) fclose(rd);
  sl_draw_free_pack();
  free(pk);
}

/* -------- every row of every preset, every value of every option, at a few sizes: no crash, no hard cut */
static unsigned char *load_pack(const char *packfile) {
  FILE *f = fopen(packfile, "rb");
  unsigned char *pk = NULL;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  pk = malloc((size_t)n);
  if (!pk || fread(pk, 1, (size_t)n, f) != (size_t)n || sl_draw_load_pack(pk, (size_t)n)) { free(pk); pk = NULL; }
  fclose(f);
  return pk;
}

static void test_walk(const char *packfile) {
  static const int sizes[][2] = {{320, 240}, {1240, 1080}, {800, 480}};
  unsigned char *pk = packfile ? load_pack(packfile) : NULL;
  if (packfile) { CHECK(pk != NULL); printf("walk with the graphics pack\n"); }
  for (unsigned si = 0; si < sizeof sizes / sizeof sizes[0]; si++) {
    int w = sizes[si][0], h = sizes[si][1], ell = 0, rows_drawn = 0;
    uint32_t *px = malloc((size_t)w * h * 4);
    for (int p = 0; p < sl_npresets; p++) {
      struct sl_launcher L;
      sl_init(&L, 3);
      sl_set_view(&L, w, h);
      sl_apply_preset(&L, p);
      L.show_adv = 1;
      sl_show(&L, SL_TWEAK);
      for (int r = 0; r < L.nrows; r++) {
        if (L.rows[r].kind == SR_HEADER) continue;
        L.cursor = r;
        sl_key(&L, SL_UP); sl_key(&L, SL_DOWN);                                  /* scroll into view */
        if (L.cursor != r) L.cursor = r;
        int n_vals = 1;
        const struct sl_option *o = L.rows[r].kind == SR_OPT ? sl_opt(&L, L.rows[r].a) : NULL;
        if (o && (o->type == SL_O_ENUM)) n_vals = o->nvalues;
        for (int v = 0; v < n_vals; v++) {
          if (o && o->type == SL_O_ENUM) L.cfg.val[L.rows[r].a] = v;
          sl_draw_audit(1); sl_draw_ellipsized(1);
          sl_draw(&L, px, w, w, h);
          { int cut_ = sl_draw_audit(1); if (cut_) printf("note: %s\n", sl_draw_audit_note()); CHECK(cut_ == 0); }
          ell += sl_draw_ellipsized(1);
          rows_drawn++;
        }
      }
    }
    printf("walk %dx%d: %d row states drawn, %d rows shortened with \"..\"\n", w, h, rows_drawn, ell);
    free(px);
  }
  sl_draw_free_pack();
  free(pk);
}


/* -------- pad: every control reachable, wrapping, shortcuts; saved games; top screen */
static int press_until(struct sl_launcher *L, int button, int focus, int max) {
  for (int i = 0; i < max; i++) { if (L->focus == focus) return 1; sl_key(L, button); }
  return L->focus == focus;
}

static void test_pad(void) {
  for (int p = 0; p < sl_npresets; p++) {
    struct sl_launcher L;
    sl_init(&L, 5);
    sl_set_view(&L, 1240, 969);
    sl_apply_preset(&L, p);
    L.show_adv = 1;
    sl_show(&L, SL_TWEAK);
    CHECK(L.focus == SF_LIST);
    /* DOWN walks the rows and then lands on the footer: GO is reachable with the pad alone, however far the list scrolled */
    CHECK(press_until(&L, SL_DOWN, SF_BACK, L.nrows + 4));
    CHECK(L.scroll + L.vis_rows >= L.nrows || L.scroll > 0);
    CHECK(sl_focus_has(&L, SF_GO));
    sl_key(&L, SL_RIGHT); CHECK(L.focus == SF_GO);
    sl_key(&L, SL_RIGHT); CHECK(L.focus == SF_UP);
    sl_key(&L, SL_RIGHT); CHECK(L.focus == SF_DOWN);
    sl_key(&L, SL_RIGHT); CHECK(L.focus == SF_BACK);                              /* wraps */
    sl_key(&L, SL_LEFT); CHECK(L.focus == SF_DOWN);
    sl_key(&L, SL_LEFT); sl_key(&L, SL_LEFT); CHECK(L.focus == SF_GO);
    sl_key(&L, SL_A);
    CHECK(L.screen == SL_GENERATE && act(&L) == SL_ACT_GENERATE);
    sl_key(&L, SL_B);
    CHECK(L.screen == SL_TWEAK);
    /* from the footer, DOWN wraps to the first row and UP at the first row goes to the footer */
    L.focus = SF_BACK;
    sl_key(&L, SL_DOWN);
    CHECK(L.focus == SF_LIST && L.cursor == 0 && L.scroll == 0);
    sl_key(&L, SL_UP);
    CHECK(L.focus == SF_GO);                                                         /* up from the top: the footer, GO first on TWEAK */
    sl_key(&L, SL_UP);
    CHECK(L.focus == SF_LIST);
    /* shortcuts */
    sl_show(&L, SL_TWEAK);
    sl_key(&L, SL_R); CHECK(L.cursor > 3);
    sl_key(&L, SL_L); sl_key(&L, SL_L); CHECK(L.cursor == 0 || L.rows[L.cursor].kind != SR_HEADER);
    int r = row_of(&L, SR_ACTION, SA_RESET);
    CHECK(r >= 0);
    sl_set_seed(&L, "777");
    L.cursor = r > 0 ? r : 0;
    sl_key(&L, SL_START);
    CHECK(L.screen == SL_GENERATE && act(&L) == SL_ACT_GENERATE);                    /* START = GO from any row */
    sl_show(&L, SL_TWEAK);
    sl_apply_preset(&L, p);
    int tested = 0;
    for (int oi = 0; oi < L.nrows && !tested; oi++) {
      if (L.rows[oi].kind != SR_OPT || sl_opt(&L, L.rows[oi].a)->type != SL_O_BOOL) continue;
      int key = L.rows[oi].a, before = L.cfg.val[key];
      L.cursor = oi;
      sl_key(&L, SL_RIGHT);
      if (L.cfg.val[key] == before) continue;                                        /* a constraint put it back: try the next */
      sl_key(&L, SL_SELECT);                                                         /* SELECT = reset to the preset */
      CHECK(L.cfg.val[key] == before);
      tested = 1;
    }
  }
  /* PRESETS: footer reachable, wrap, A on BACK */
  {
    struct sl_launcher L;
    sl_init(&L, 5); sl_set_view(&L, 320, 240);
    sl_show(&L, SL_PRESETS);
    CHECK(press_until(&L, SL_DOWN, SF_BACK, L.nrows + 4));
    CHECK(!sl_focus_has(&L, SF_GO));
    sl_key(&L, SL_RIGHT); CHECK(L.focus == SF_UP);
    sl_key(&L, SL_A); CHECK(L.focus == SF_UP);                                       /* a page up */
    sl_key(&L, SL_LEFT); CHECK(L.focus == SF_BACK);
    sl_key(&L, SL_A); CHECK(L.screen == SL_MAIN);
  }
  /* MAIN: three entries, wrapping both ways */
  {
    struct sl_launcher L;
    sl_init(&L, 5);
    CHECK(L.cursor == 0);
    sl_key(&L, SL_UP); CHECK(L.cursor == 2);
    sl_key(&L, SL_DOWN); CHECK(L.cursor == 0);
    sl_key(&L, SL_DOWN); sl_key(&L, SL_DOWN); CHECK(L.cursor == 2);
    sl_key(&L, SL_A); CHECK(L.screen == SL_SAVED);
    sl_key(&L, SL_B); CHECK(L.screen == SL_MAIN && L.cursor == 2);
    sl_key(&L, SL_UP); sl_key(&L, SL_A); CHECK(L.screen == SL_PRESETS);
    sl_show(&L, SL_MAIN); L.cursor = 0;
    sl_key(&L, SL_START); CHECK(act(&L) == SL_ACT_PLAY_ORIGINAL);
  }
}

static void test_saved(void) {
  struct sl_launcher L;
  char lines[2048], k1[17], k2[17];
  sl_init(&L, 9);
  sl_set_view(&L, 1240, 969);
  /* a config survives the trip through the saved list: same key */
  for (int p = 0; p < sl_npresets; p++) {
    sl_apply_preset(&L, p); sl_set_seed(&L, "4242"); sl_randomize_all(&L);
    sl_config_lines(&L, lines, sizeof lines); sl_config_key(&L, k1);
    const char *dn = NULL; int hasd = sl_difficulty(&L, &dn);
    struct sl_launcher M;
    sl_init(&M, 2);
    CHECK(sl_load_config(&M, sl_presets[p].id, sl_modes[L.cfg.mode].key, "4242", hasd ? dn : "", lines) == 0);
    sl_config_key(&M, k2);
    CHECK_S(k1, k2);
    if (strcmp(k1, k2)) printf("  preset %s\n", sl_presets[p].id);
  }
  { struct sl_launcher M; sl_init(&M, 2); CHECK(sl_load_config(&M, "", "nomode", "1", "", "") == -1); }
  sl_init(&L, 9);
  sl_set_view(&L, 1240, 969);
  CHECK(sl_saved_count(&L) == 0);
  sl_show(&L, SL_SAVED);
  sl_key(&L, SL_A); CHECK(act(&L) == SL_ACT_NONE && L.notice[0]);                   /* empty slot */
  CHECK(sl_saved_add(&L, "a705b603eb8f2253", "chaos-short", "chaos", "4242", "casual", "2026-10-18", "/x/a.sfc", "chBosses=few\nchFloors=few\n") == 0);
  CHECK(sl_saved_add(&L, "e0343466b771b0c1", "ancientcave", "ancientcave", "12345", "casual", "2026-10-17", "/x/b.sfc", "") == 1);
  CHECK(sl_saved_add(&L, "k3", "", "chaos", "1", "", "2026-10-16", "/x/c.sfc", "") == 2);
  CHECK(sl_saved_add(&L, "k4", "", "bossrush", "2", "", "2026-10-15", "/x/d.sfc", "") == 3);
  CHECK(sl_saved_add(&L, "k5", "", "bossrush", "3", "", "2026-10-14", "/x/e.sfc", "") == -1);   /* four slots at most */
  CHECK(sl_saved_count(&L) == 4);
  /* up/down walk the four slots then BACK, and wrap */
  CHECK(L.cursor == 0 && L.focus == SF_LIST);
  sl_key(&L, SL_UP); CHECK(L.focus == SF_BACK);
  sl_key(&L, SL_UP); CHECK(L.focus == SF_LIST && L.cursor == 3);
  sl_key(&L, SL_DOWN); CHECK(L.focus == SF_BACK);
  sl_key(&L, SL_DOWN); CHECK(L.focus == SF_LIST && L.cursor == 0);
  /* play slot 1 (second): the cfg becomes the saved one, the host plays the stored path */
  sl_key(&L, SL_DOWN);
  sl_key(&L, SL_A);
  CHECK(act(&L) == SL_ACT_PLAY && !strcmp(sl_rom_path(&L), "/x/b.sfc"));
  CHECK(!strcmp(sl_modes[L.cfg.mode].key, "ancientcave") && !strcmp(L.cfg.seed, "12345"));
  /* delete needs a second press; any other key cancels */
  sl_show(&L, SL_SAVED);
  sl_key(&L, SL_X); CHECK(L.del_confirm == 1 && act(&L) == SL_ACT_NONE);
  sl_key(&L, SL_DOWN); CHECK(L.del_confirm == 0);
  sl_key(&L, SL_UP);
  sl_key(&L, SL_X); sl_key(&L, SL_X);
  CHECK(act(&L) == SL_ACT_DELETE_SAVED && sl_act_arg(&L) == 0);
  /* RIGHT focuses the delete button of the slot, A twice deletes, LEFT goes back */
  sl_show(&L, SL_SAVED);
  sl_key(&L, SL_DOWN);
  sl_key(&L, SL_RIGHT); CHECK(L.focus == SF_DEL);
  sl_key(&L, SL_LEFT); CHECK(L.focus == SF_LIST);
  sl_key(&L, SL_RIGHT); sl_key(&L, SL_A); sl_key(&L, SL_A);
  CHECK(act(&L) == SL_ACT_DELETE_SAVED && sl_act_arg(&L) == 1);
  /* touch: tap a slot plays it, the delete button asks twice */
  sl_show(&L, SL_SAVED);
  tap_hit(&L, 1240, 969, SH_SAVED_DEL, 2);
  CHECK(L.del_confirm == 3 && act(&L) == SL_ACT_NONE);
  tap_hit(&L, 1240, 969, SH_SAVED_DEL, 2);
  CHECK(act(&L) == SL_ACT_DELETE_SAVED && sl_act_arg(&L) == 2);
  tap_hit(&L, 1240, 969, SH_SAVED_SLOT, 3);
  CHECK(act(&L) == SL_ACT_PLAY && !strcmp(sl_rom_path(&L), "/x/d.sfc"));
  sl_show(&L, SL_SAVED);
  sl_saved_clear(&L);
  CHECK(sl_saved_count(&L) == 0);
  tap_hit(&L, 1240, 969, SH_BACK, -1);
  CHECK(L.screen == SL_MAIN);
  tap_hit(&L, 1240, 969, SH_MAIN_SAVED, -1);
  CHECK(L.screen == SL_SAVED);
}

static uint64_t fnv_px(const uint32_t *p, size_t n) { uint64_t h = 1469598103934665603ull; for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; } return h; }

static void test_top(void) {
  static const int sizes[][2] = {{1920, 1080}, {1240, 969}, {640, 360}, {320, 240}};
  for (unsigned si = 0; si < sizeof sizes / sizeof sizes[0]; si++) {
    int w = sizes[si][0], h = sizes[si][1];
    uint32_t *px = malloc((size_t)w * h * 4);
    uint64_t seen[16]; int ns = 0;
    for (int sc = 0; sc <= SL_SAVED; sc++) {
      for (int p = 0; p < sl_npresets; p++) {
        struct sl_launcher L;
        sl_init(&L, 3); sl_set_view(&L, 1240, 969);
        sl_apply_preset(&L, p);
        sl_saved_add(&L, "k", sl_presets[p].id, sl_modes[L.cfg.mode].key, "55", "", "2026-10-18", "/x/a.sfc", "");
        sl_show(&L, (enum sl_screen)sc);
        if (sc == SL_TWEAK || sc == SL_PRESETS) L.cursor = 1 + p % 4;
        if (sc == SL_PLAY) strcpy(L.rom_path, "/x/a.sfc");
        if (sc == SL_MULTI) L.multi_opt = 0;
        memset(px, 0, (size_t)w * h * 4);
        sl_draw_top(&L, px, w, w, h);
        uint64_t hv = fnv_px(px, (size_t)w * h);
        int blank = 1;
        for (size_t i = 1; i < (size_t)w * h; i++) if (px[i] != px[0]) { blank = 0; break; }
        CHECK(!blank);
        if (p == 0 && ns < 16) seen[ns++] = hv;
      }
    }
    /* different screens look different (the main menu and the play screen share the wallpaper) */
    int distinct = 0;
    for (int i = 0; i < ns; i++) { int dup = 0; for (int j = 0; j < i; j++) if (seen[j] == seen[i]) dup = 1; distinct += !dup; }
    CHECK(distinct >= 5);
    free(px);
  }
  /* the wallpaper does not depend on the cursor of the main menu (the host redraws the top screen only when the summary changes) */
  {
    struct sl_launcher L;
    static uint32_t a[640 * 360], b[640 * 360];
    sl_init(&L, 3);
    sl_draw_top(&L, a, 640, 640, 360);
    sl_key(&L, SL_DOWN);
    sl_draw_top(&L, b, 640, 640, 360);
    CHECK(!memcmp(a, b, sizeof a));
  }
}

/* -------- canonical outputs for the Python cross check */
static void emit(void) {
  static const char *const seeds[] = {"1", "12345", "99999999", "0"};
  struct sl_launcher L;
  char js[4096], ks[2048], cli[2048], key[17];
  int n = 0;
  for (int p = 0; p < sl_npresets; p++)
    for (int s = 0; s < 4; s++) {
      sl_init(&L, 1);
      sl_apply_preset(&L, p);
      sl_set_seed(&L, seeds[s]);
      sl_config_json(&L, js, sizeof js); sl_config_keystring(&L, ks, sizeof ks); sl_config_cli(&L, cli, sizeof cli); sl_config_key(&L, key);
      printf("CASE\t%s\t%s\t%s\t%s\t%s\n", sl_presets[p].id, js, key, ks, cli);
      n++;
    }
  for (int e = 1; e <= 40; e++) {
    int p = (e * 5) % sl_npresets;
    sl_init(&L, (uint64_t)e * 104729u);
    sl_apply_preset(&L, p);
    sl_randomize_all(&L);
    sl_random_seed(&L);
    sl_config_json(&L, js, sizeof js); sl_config_keystring(&L, ks, sizeof ks); sl_config_cli(&L, cli, sizeof cli); sl_config_key(&L, key);
    printf("CASE\trandom%d\t%s\t%s\t%s\t%s\n", e, js, key, ks, cli);
    n++;
  }
  fprintf(stderr, "%d cases\n", n);
}

int main(int argc, char **argv) {
  if (argc > 1 && !strcmp(argv[1], "--emit")) { emit(); return 0; }
  const char *render = NULL, *pack = NULL;
  int update = 0;
  if (argc > 2 && !strcmp(argv[1], "--render")) { render = argv[2]; pack = argc > 3 ? argv[3] : NULL; }
  if (argc > 1 && !strcmp(argv[1], "--update-golden")) update = 1;
  if (!render) {
    test_catalogue();
    test_randomize();
    test_navigation();
    test_touch();
    test_pad();
    test_saved();
    test_top();
    test_walk(NULL);
    if (getenv("SOM_UI_PACK")) test_walk(getenv("SOM_UI_PACK"));
  }
  test_draw(render, pack, update);
  printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "PASS", checks, fails);
  return fails ? 1 : 0;
}
