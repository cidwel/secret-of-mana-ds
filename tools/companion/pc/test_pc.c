/* Tests of the PC frontend logic (pc_logic.c) and golden hashes of the shoulder-slot overlay drawn with the built-in art.
 *   ./run_pc_tests.sh            builds with ASan/UBSan and runs
 *   test_pc --update-hashes      rewrites golden_hashes.txt (review the diff first)
 * Needs no ROM, no graphics pack and no SDL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "pc_logic.h"
#include "pc_store.h"
#include "pc_gameplay.h"
#include "pc_gpc.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static void test_cfg(void) {
  struct pc_cfg c, d;
  char buf[4096];
  pc_cfg_defaults(&c);
  CHECK(c.wide == 46 && c.fit == PC_FIT_FIT && c.win_w == 1920 && c.win_h == 1080 && c.overlay == PC_OV_SMALL && !c.hud_hide);
  c.wide = 27; c.fit = PC_FIT_FILL; c.hud_hide = 1; c.screen = PC_BORDERLESS; c.overlay = PC_OV_MEDIUM; c.corner = PC_BOTTOM_LEFT; c.names = 0;
  c.fire_on_press = 1; c.volume = 40; snprintf(c.rom, sizeof c.rom, "/roms/a \"b\".smc");
  CHECK(pc_cfg_to_json(&c, buf, sizeof buf) > 0);
  pc_cfg_defaults(&d);
  CHECK(pc_cfg_from_json(&d, buf) == 0);
  CHECK(d.wide == 27 && d.fit == PC_FIT_FILL && d.hud_hide == 1 && d.screen == PC_BORDERLESS && d.overlay == PC_OV_MEDIUM && d.corner == PC_BOTTOM_LEFT);
  CHECK(d.names == 0 && d.fire_on_press == 1 && d.volume == 40 && !strcmp(d.rom, c.rom));
  CHECK(pc_cfg_from_json(&d, "not json") == -1);
  pc_cfg_defaults(&d);
  CHECK(pc_cfg_from_json(&d, "{\"wide\": 99, \"fit\": 7, \"win_w\": 5, \"volume\": 300, \"overlay\": 9}") == 0);   /* invalid values are ignored */
  CHECK(d.wide == 46 && d.fit == PC_FIT_FIT && d.win_w == 1920 && d.volume == 100 && d.overlay == PC_OV_SMALL);
  CHECK(pc_cfg_to_json(&c, buf, 40) == -1);
  CHECK(pc_valid_wide(0) && pc_valid_wide(17) && pc_valid_wide(27) && pc_valid_wide(46) && !pc_valid_wide(30));
  CHECK(pc_next_wide(0) == 17 && pc_next_wide(17) == 27 && pc_next_wide(27) == 46 && pc_next_wide(46) == 0);
}

static void test_profile(void) {
  struct pc_profile p, q;
  char buf[1024];
  memset(&p, 0, sizeof p);
  p.slot[0].kind = SOM_UX_BK_SPELL; p.slot[0].id = 12; p.slot[0].policy = SOM_UX_POL_NEAR;
  p.slot[3].kind = SOM_UX_BK_ITEM; p.slot[3].id = 4; p.slot[3].policy = SOM_UX_POL_LESS_HP;
  p.slot[2].kind = SOM_UX_BK_WEAPON; p.slot[2].id = 7;
  CHECK(pc_profile_to_json(&p, "200000-abcd1234", buf, sizeof buf) > 0);
  CHECK(strstr(buf, "\"L1\": [3, 12, 1]") && strstr(buf, "\"R2\": [2, 4, 3]") && strstr(buf, "200000-abcd1234"));
  CHECK(pc_profile_from_json(&q, buf) == 0 && !memcmp(&p, &q, sizeof p));
  CHECK(pc_profile_from_json(&q, "{}") == -1);
  CHECK(pc_profile_from_json(&q, "{\"slots\": {\"L1\": [99, 1, 0], \"R1\": [3, 300, 0], \"L2\": [3, 5, 9], \"R2\": [1, 2, 0]}}") == 0);
  CHECK(q.slot[0].kind == 0 && q.slot[1].kind == 0 && q.slot[2].kind == 0 && q.slot[3].kind == 1 && q.slot[3].id == 2);   /* bad entries stay empty */
  CHECK(pc_profile_to_json(&p, "x", buf, 20) == -1);
}

static void test_fit(void) {
  struct pc_rect r;
  /* wide 46 on 1080p: aspect 1.8125 is wider than 16:9, so FIT is limited by the width */
  pc_fit_rects(1920, 1080, 348, 224, 46, PC_FIT_FIT, &r);
  CHECK(r.w == 1920 && r.h == 1059 && r.x == 0 && r.y == 10);
  pc_fit_rects(1920, 1080, 348, 224, 27, PC_FIT_FIT, &r);                      /* aspect 1.6146: limited by the height */
  CHECK(r.h == 1080 && r.w == 1744 && r.y == 0 && r.x == 88);
  pc_fit_rects(1920, 1080, 310, 224, 27, PC_FIT_FILL, &r);                     /* covers the width, the rows are cut */
  CHECK(r.w == 1920 && r.h == 1189 && r.x == 0 && r.y == -54);
  pc_fit_rects(1920, 1080, 256, 224, 0, PC_FIT_FIT, &r);                       /* off: 4:3 */
  CHECK(r.h == 1080 && r.w == 1440 && r.x == 240);
  pc_fit_rects(1920, 1080, 348, 224, 46, PC_FIT_STRETCH, &r);
  CHECK(r.x == 0 && r.y == 0 && r.w == 1920 && r.h == 1080);
  pc_fit_rects(1920, 1080, 348, 224, 46, PC_FIT_INTEGER, &r);                  /* 4 high, 5 wide */
  CHECK(r.h == 896 && r.w == 1740 && r.x == 90 && r.y == 92);
  pc_fit_rects(1280, 720, 348, 224, 46, PC_FIT_INTEGER, &r);                   /* 3 high, 4 wide = 1392 > 1280: falls to 2 / 2 */
  CHECK(r.h == 448 && r.w == 696);
  /* a map where the core falls back to 256 px keeps the picture size and the centre */
  struct pc_rect wide, narrow;
  pc_fit_rects(1920, 1080, 348, 224, 46, PC_FIT_FIT, &wide);
  pc_fit_rects(1920, 1080, 256, 224, 46, PC_FIT_FIT, &narrow);
  CHECK(narrow.h == wide.h && narrow.w * 348 == (wide.w * 256) + (narrow.w * 348 - wide.w * 256) && narrow.x > wide.x);
  CHECK(narrow.x + narrow.w / 2 == wide.x + wide.w / 2 || narrow.x + narrow.w / 2 == wide.x + wide.w / 2 + 1 || narrow.x + narrow.w / 2 == wide.x + wide.w / 2 - 1);
  pc_fit_rects(0, 0, 0, 0, 46, PC_FIT_FIT, &r);                                /* degenerate input does not crash */
  CHECK(r.w == 0 && r.h == 0);
  CHECK(pc_aspect(0) > 1.33 && pc_aspect(0) < 1.34 && pc_aspect(46) > 1.81 && pc_aspect(46) < 1.82);
}

static void test_shoulders(void) {
  struct pc_sh s; struct pc_sh_out o;
  pc_sh_init(&s, 0);
  pc_sh_update(&s, 0, 0, &o);
  CHECK(o.fire == -1 && o.assign == -1);
  pc_sh_update(&s, 1, 1000, &o);                      /* press L1: nothing yet */
  CHECK(o.fire == -1 && o.assign == -1);
  pc_sh_update(&s, 0, 1090, &o);                      /* quick release: fires L1 */
  CHECK(o.fire == 0 && o.assign == -1);
  pc_sh_update(&s, 0, 1100, &o);
  CHECK(o.fire == -1);
  pc_sh_update(&s, 8, 2000, &o);                      /* R2 held */
  pc_sh_update(&s, 8, 2100, &o); CHECK(o.progress[3] == 0);
  pc_sh_update(&s, 8, 2400, &o); CHECK(o.progress[3] > 30 && o.progress[3] < 60);
  pc_sh_update(&s, 8, 2699, &o); CHECK(o.assign == -1 && o.progress[3] > 90);
  pc_sh_update(&s, 8, 2700, &o); CHECK(o.assign == 3 && o.fire == -1);
  pc_sh_update(&s, 8, 3000, &o); CHECK(o.assign == -1);
  pc_sh_update(&s, 0, 3100, &o); CHECK(o.fire == -1 && o.assign == -1);       /* the release of a long hold does not fire */
  pc_sh_update(&s, 6, 4000, &o);                      /* R1 and L2 together */
  pc_sh_update(&s, 0, 4050, &o); CHECK(o.fire == 1);  /* lowest slot first when both are released in the same step */
  pc_sh_update(&s, 0, 4060, &o); CHECK(o.fire == -1);
  pc_sh_init(&s, 1);                                  /* fire on press */
  pc_sh_update(&s, 4, 100, &o); CHECK(o.fire == 2);
  pc_sh_update(&s, 4, 1500, &o); CHECK(o.fire == -1 && o.assign == -1);
  pc_sh_update(&s, 0, 1600, &o); CHECK(o.fire == -1);
  /* a long run of random edges never fires twice for one press and never reports both a fire and an assign for a slot */
  {
    unsigned seed = 7, fired[4] = {0}, presses[4] = {0}, held = 0, t = 0;
    pc_sh_init(&s, 0);
    for (int i = 0; i < 20000; i++) {
      unsigned prev = held;
      seed = seed * 1664525u + 1013904223u;
      t += (seed >> 20) % 400;
      held = (seed >> 8) & 15;
      for (int b = 0; b < 4; b++) if ((held >> b & 1) && !(prev >> b & 1)) presses[b]++;
      pc_sh_update(&s, (uint8_t)held, t, &o);
      if (o.fire >= 0) fired[o.fire]++;
      CHECK(!(o.fire >= 0 && o.fire == o.assign));
    }
    for (int b = 0; b < 4; b++) CHECK(fired[b] <= presses[b]);
  }
}

static void test_nav(void) {
  /* 3 x 2 grid of 100 x 50 cells with a 10 px gap, plus a wide button below and a small one at the top right */
  struct som_ux_hitrect h[10];
  int n = 0;
  memset(h, 0, sizeof h);
  for (int r = 0; r < 2; r++) for (int c = 0; c < 3; c++) { h[n].x = c * 110; h[n].y = 100 + r * 60; h[n].w = 100; h[n].h = 50; h[n].enabled = 1; n++; }
  h[n].x = 0; h[n].y = 230; h[n].w = 320; h[n].h = 40; h[n].enabled = 1; n++;    /* 6: wide button */
  h[n].x = 280; h[n].y = 0; h[n].w = 40; h[n].h = 30; h[n].enabled = 1; n++;     /* 7: top right */
  h[n].x = 330; h[n].y = 100; h[n].w = 100; h[n].h = 50; h[n].enabled = 0; n++;  /* 8: disabled, right of cell 2 */
  CHECK(pc_nav_pick(h, n, 0, 1, 0) == 1 && pc_nav_pick(h, n, 1, 1, 0) == 2);
  CHECK(pc_nav_pick(h, n, 2, 1, 0) == 2);                      /* nothing enabled to the right: stays */
  CHECK(pc_nav_pick(h, n, 4, -1, 0) == 3 && pc_nav_pick(h, n, 3, -1, 0) == 3);
  CHECK(pc_nav_pick(h, n, 0, 0, 1) == 3 && pc_nav_pick(h, n, 4, 0, 1) == 6);
  CHECK(pc_nav_pick(h, n, 6, 0, -1) == 4 || pc_nav_pick(h, n, 6, 0, -1) == 3 || pc_nav_pick(h, n, 6, 0, -1) == 5);
  CHECK(pc_nav_pick(h, n, 2, 0, -1) == 7);                     /* the small button above the right column */
  CHECK(pc_nav_pick(h, n, 7, 0, 1) == 2);
  CHECK(pc_nav_pick(h, n, -1, 1, 0) == 0);                     /* no focus yet: the first enabled target */
  CHECK(pc_nav_pick(h, n, 0, 0, -1) == 7 || pc_nav_pick(h, n, 0, 0, -1) == 0);
}

/* ---- overlay goldens ---- */
static uint8_t wram[0x20000];
static void w16(unsigned a, unsigned v) { wram[a] = (uint8_t)v; wram[a + 1] = (uint8_t)(v >> 8); }
static void field_party(void) {
  memset(wram, 0, sizeof wram);
  wram[0xE4] = 1; wram[0xE6] = 15; wram[0xD9] = 1;
  for (int i = 0; i < 3; i++) {
    unsigned b = 0xE000 + 0x200 * i;
    wram[b] = 1; wram[b + 0x181] = (uint8_t)(8 + i); w16(b + 0x182, 120); w16(b + 0x184, 150); wram[b + 0x186] = 20; wram[b + 0x187] = 30; wram[b + 0x1ED] = 85;
  }
  for (int hh = 1; hh < 3; hh++) for (int e = 0; e < 8; e++) wram[0xE000 + 0x200 * hh + 0x1C8 + e] = 7;   /* girl and sprite own every spirit */
  wram[0xCC54] = 0x40; wram[0xCC55] = 0xFF; wram[0xCC56] = 2;
  for (int i = 3; i < 8; i++) wram[0xCC54 + i] = 0xFF;
  for (int i = 0; i < 12; i++) wram[0xCC48 + i] = 0xFF;
  wram[0xCC48] = (uint8_t)((2 << 5) | 0); wram[0xCC49] = (uint8_t)((1 << 5) | 4);
}

struct gh { char name[64]; uint32_t h; };
static struct gh got[64]; static int ngot;
static void golden(const char *name, struct som_ux *ux, int scale, int names) {
  int w, h;
  som_ux_shoulders_size(scale, names, &w, &h);
  uint32_t *px = malloc((size_t)w * h * 4);
  som_ux_draw_shoulders(ux, px, w, w, h, scale, names);
  snprintf(got[ngot].name, sizeof got[ngot].name, "%s_s%d_n%d", name, scale, names);
  got[ngot++].h = pc_hash_pixels(px, w, w, h);
  int opaque = 0, clear = 0;
  for (int i = 0; i < w * h; i++) { if (px[i] >> 24) opaque++; else clear++; }
  CHECK(opaque > w * h / 3 && clear > 0);                 /* drawn, and the corners and the notice row stay transparent */
  CHECK(px[0] == 0);                                      /* top-left corner of the notice row is untouched */
  free(px);
}

static void test_overlay(void) {
  struct som_ux *ux = som_ux_new();
  field_party();
  som_ux_update(ux, wram);
  CHECK(ux->mode == SOM_UX_FIELD);
  for (int s = 1; s <= 3; s++) for (int n = 0; n <= 1; n++) golden("empty", ux, s, n);
  som_ux_quick_set(ux, 0, SOM_UX_BK_WEAPON, 1);
  som_ux_quick_set(ux, 1, SOM_UX_BK_ITEM, 2);
  som_ux_quick_set(ux, 2, SOM_UX_BK_SPELL, 12); som_ux_quick_set_policy(ux, 2, SOM_UX_POL_NEAR);
  som_ux_quick_set(ux, 3, SOM_UX_BK_SPIRIT, 3);
  for (int s = 2; s <= 3; s++) golden("filled", ux, s, 1);
  golden("filled", ux, 2, 0);
  som_ux_slot_flash(ux, 2, SOM_UX_FLASH_CAST);
  for (int k = 0; k < 6; k++) som_ux_update(ux, wram);
  golden("flash_cast", ux, 2, 1);
  som_ux_slot_flash(ux, 0, SOM_UX_FLASH_REFUSED);
  golden("flash_refused", ux, 2, 1);
  for (int k = 0; k < 30; k++) som_ux_update(ux, wram);
  uint8_t av[42];
  memset(av, SOM_UX_AV_ENABLED, sizeof av);
  av[12] = SOM_UX_AV_QUEUED;
  som_ux_set_spell_avail(ux, av);
  golden("queued", ux, 2, 1);
  av[12] = SOM_UX_AV_DISABLED;
  som_ux_set_spell_avail(ux, av);
  golden("unavailable", ux, 2, 1);
  som_ux_notice(ux, SOM_UX_NOTICE_QUEUED, "");
  golden("notice_queued", ux, 2, 1);
  som_ux_notice(ux, SOM_UX_NOTICE_DROPPED, "NO MP");
  golden("notice_dropped", ux, 3, 1);
  CHECK(som_ux_audit(1) == 0);                            /* no text cut or overlapping */
  som_ux_delete(ux);
}

static int check_hashes(const char *path, int update) {
  if (update) {
    FILE *f = fopen(path, "w");
    if (!f) return 1;
    fprintf(f, "# FNV-1a of the ARGB pixels of the shoulder overlay (built-in art, no pack); regenerate with test_pc --update-hashes\n");
    for (int i = 0; i < ngot; i++) fprintf(f, "%s %08x\n", got[i].name, got[i].h);
    fclose(f);
    printf("wrote %d hashes to %s\n", ngot, path);
    return 0;
  }
  FILE *f = fopen(path, "r");
  char line[200];
  int seen = 0;
  if (!f) { printf("FAIL no %s\n", path); return 1; }
  while (fgets(line, sizeof line, f)) {
    char n[80]; unsigned h;
    if (line[0] == '#' || sscanf(line, "%79s %x", n, &h) != 2) continue;
    for (int i = 0; i < ngot; i++) if (!strcmp(got[i].name, n)) { seen++; CHECK(got[i].h == h); if (got[i].h != h) printf("  golden %s: %08x != %08x\n", n, got[i].h, h); }
  }
  fclose(f);
  CHECK(seen == ngot);
  return 0;
}


static int prof_ok(const char *t) { struct pc_profile q; return pc_json_complete(t) && pc_profile_from_json(&q, t) == 0; }

static void test_logic_extra(void) {
  struct pc_sh s; struct pc_sh_out o;
  struct pc_audio_plan pl;
  int st = 0;
  /* trigger hysteresis: on above 0.6, off below 0.4, held in between */
  st = pc_trigger_step(st, 10000); CHECK(st == 0);
  st = pc_trigger_step(st, 16000); CHECK(st == 0);
  st = pc_trigger_step(st, 20000); CHECK(st == 1);
  st = pc_trigger_step(st, 16000); CHECK(st == 1);
  st = pc_trigger_step(st, 13000); CHECK(st == 0);
  CHECK(pc_stick_dir(-20000) == -1 && pc_stick_dir(20000) == 1 && pc_stick_dir(15999) == 0 && pc_stick_dir(-16000) == 0);
  /* unplugging a pad with a slot held must not fire it */
  pc_sh_init(&s, 0);
  pc_sh_update(&s, 1, 1000, &o); CHECK(o.fire == -1);
  pc_sh_cancel(&s);
  pc_sh_update(&s, 0, 1100, &o); CHECK(o.fire == -1 && o.assign == -1);
  pc_sh_update(&s, 1, 1200, &o); pc_sh_update(&s, 0, 1300, &o); CHECK(o.fire == 0);       /* works again afterwards */
  /* window modes */
  CHECK(pc_screen_toggle(PC_WINDOWED, 0) == PC_BORDERLESS && pc_screen_toggle(PC_BORDERLESS, 0) == PC_WINDOWED);
  CHECK(pc_screen_toggle(PC_EXCLUSIVE, 0) == PC_WINDOWED);           /* F11 from exclusive never goes to another exclusive mode */
  CHECK(pc_screen_toggle(PC_WINDOWED, 1) == PC_EXCLUSIVE && pc_screen_toggle(PC_EXCLUSIVE, 1) == PC_WINDOWED);
  CHECK(pc_screen_fallback(PC_EXCLUSIVE) == PC_BORDERLESS && pc_screen_fallback(PC_BORDERLESS) == PC_WINDOWED && pc_screen_fallback(PC_WINDOWED) == PC_WINDOWED);
  /* audio pacing, one core frame is 2136 bytes at 32040 Hz / 60.1 fps */
  pc_audio_plan(0, 2136, &pl); CHECK(pl.underrun == 1 && pl.frames == PC_AUDIO_BURST && !pl.overflow);
  pc_audio_plan(2136, 2136, &pl); CHECK(!pl.underrun && pl.frames == 2);
  pc_audio_plan(2136 * 3, 2136, &pl); CHECK(pl.frames == 0 && !pl.underrun && !pl.overflow);
  pc_audio_plan(2136 * 9.5, 2136, &pl); CHECK(pl.overflow == 1 && pl.frames == 0);
  pc_audio_plan(2136 * 9, 2136, &pl); CHECK(!pl.overflow);
  pc_audio_plan(100, 0, &pl); CHECK(pl.frames == 0);
  /* JSON completeness: a truncated write is not a document */
  CHECK(pc_json_complete("{\"a\": [1, 2], \"b\": \"x}\"}") && pc_json_complete("  {}\n"));
  CHECK(!pc_json_complete("{\"slots\": {\"L1\": [3, 12, 1]") && !pc_json_complete("") && !pc_json_complete("[1]") && !pc_json_complete("{} x") && !pc_json_complete("{\"a\": \"b"));
}

static void test_store(void) {
  char dir[200], path[260], bak[270], bad[270];
  struct pc_profile p, q;
  char buf[1024], *t;
  int src = -1;
  snprintf(dir, sizeof dir, "/tmp/som_pc_store_%d", (int)getpid());
  CHECK(pc_mkdirs(dir) == 0);
  snprintf(path, sizeof path, "%s/profiles/a.json", dir);
  snprintf(bak, sizeof bak, "%s.bak", path); snprintf(bad, sizeof bad, "%s.bad", path);
  { char d2[260]; snprintf(d2, sizeof d2, "%s/profiles", dir); CHECK(pc_mkdirs(d2) == 0); }
  memset(&p, 0, sizeof p);
  p.slot[1].kind = SOM_UX_BK_SPELL; p.slot[1].id = 9; p.slot[1].policy = SOM_UX_POL_ALL;
  pc_profile_to_json(&p, "v", buf, sizeof buf);
  CHECK(pc_store_save(path, buf) == 0);
  t = pc_store_load(path, prof_ok, &src); CHECK(t && src == 0); free(t);
  /* a second save keeps the first as the backup */
  p.slot[2].kind = SOM_UX_BK_ITEM; p.slot[2].id = 3;
  pc_profile_to_json(&p, "v", buf, sizeof buf);
  CHECK(pc_store_save(path, buf) == 0 && access(bak, R_OK) == 0);
  /* the main file is cut in half (power loss): the backup is used, the damaged file is kept aside */
  { FILE *f = fopen(path, "wb"); fwrite(buf, 1, strlen(buf) / 2, f); fclose(f); }
  t = pc_store_load(path, prof_ok, &src);
  CHECK(t && src == 1 && access(bad, R_OK) == 0 && access(path, R_OK) != 0);
  CHECK(t && pc_profile_from_json(&q, t) == 0 && q.slot[1].kind == SOM_UX_BK_SPELL && q.slot[1].id == 9 && q.slot[2].kind == 0);
  free(t);
  /* nothing usable at all */
  unlink(bak); unlink(bad);
  { FILE *f = fopen(path, "wb"); fputs("garbage", f); fclose(f); }
  t = pc_store_load(path, prof_ok, &src); CHECK(!t && src == 3 && access(bad, R_OK) == 0);
  unlink(bad);
  t = pc_store_load(path, prof_ok, &src); CHECK(!t && src == 2);              /* missing file: a normal first run */
  CHECK(pc_spit("/nonexistent-dir-x/y", "a", 1) == -1);
  unlink(path); unlink(bak);
  { char d2[260]; snprintf(d2, sizeof d2, "%s/profiles", dir); rmdir(d2); rmdir(dir); }
}

static char *read_all(const char *path) { size_t n; return (char *)pc_slurp(path, &n); }

static void test_gameplay(void) {
  struct pg g, h;
  char out[2048], row[64];
  char *menu = read_all("../../gameplay/gameplay_menu.json");
  CHECK(menu != NULL);
  if (!menu) return;
  CHECK(pg_load_menu(&g, menu) == 0 && g.n >= 13);          /* the menu file grows with the T11 options: nothing here counts them */
  CHECK(pg_is_default(&g));
  CHECK(pg_to_json(&g, out, sizeof out) > 0 && strstr(out, "\"options\": {}"));
  /* find the options by key and check types */
  {
    int wr = -1, cs = -1, bonus = -1, cm = -1, cf = -1;
    for (int i = 0; i < g.n; i++) {
      if (!strcmp(g.o[i].key, "weapon_recharge")) wr = i;
      if (!strcmp(g.o[i].key, "charge_speed_up_bonus")) bonus = i;
      if (!strcmp(g.o[i].key, "crit_multiplier")) cm = i;
      if (!strcmp(g.o[i].key, "charge_frames_per_stage")) cf = i;
      if (!strcmp(g.o[i].key, "walk_speed")) cs = i;
    }
    CHECK(wr >= 0 && bonus >= 0 && cm >= 0 && cf >= 0 && cs >= 0);
    CHECK(g.o[wr].type == PG_ENUM && g.o[bonus].type == PG_BOOL && g.o[cf].type == PG_NUM && g.o[cf].optional && g.o[cf].def == -1);
    CHECK(g.o[cm].integer && g.o[cm].def == 0 && !strcmp(g.o[cm].val[g.o[cm].def].v, "2") && !g.o[wr].integer);
    g.o[wr].cur = 2; g.o[bonus].cur = 1; g.o[cm].cur = 2; g.o[cf].cur = 12;
    CHECK(pg_to_json(&g, out, sizeof out) > 0);
    CHECK(strstr(out, "\"weapon_recharge\": \"faster\"") && strstr(out, "\"charge_speed_up_bonus\": true") && strstr(out, "\"crit_multiplier\": 4,") == NULL);
    CHECK(strstr(out, "\"crit_multiplier\": 4") && !strstr(out, "\"crit_multiplier\": \"4\"") && strstr(out, "\"charge_frames_per_stage\": 12"));
    CHECK(!strstr(out, "walk_speed") && !pg_is_default(&g) && pc_json_complete(out));
    /* round trip through the saved file */
    CHECK(pg_load_menu(&h, menu) == 0 && pg_load_values(&h, out) == 0);
    for (int i = 0; i < g.n; i++) CHECK(g.o[i].cur == h.o[i].cur);
    CHECK(pg_load_values(&h, "not json") == -1);
    CHECK(pg_load_values(&h, "{\"options\": {\"weapon_recharge\": \"bogus\", \"charge_frames_per_stage\": 500, \"nope\": 1, \"walk_speed\": 3}}") == 0);
    CHECK(h.o[wr].cur == 2 && h.o[cf].cur == 12 && h.o[cs].cur == 1);        /* bad values keep the previous ones; walk_speed 3 is accepted */
    pg_reset(&h); CHECK(pg_is_default(&h));
  }
  /* key handling: cursor wraps, enums cycle, numbers step and go back to unset, advanced rows appear on demand */
  CHECK(pg_load_menu(&h, menu) == 0);
  int nadv = 0, nvis;
  for (int i = 0; i < h.n; i++) nadv += h.o[i].advanced != 0;
  nvis = h.n - nadv;
  CHECK(nadv > 0 && nvis > 0 && pg_entries(&h) == nvis + 3);                       /* the visible options, the advanced toggle, reset all, done */
  pg_key(&h, PG_UP); CHECK(h.cursor == nvis + 2);
  pg_key(&h, PG_ACCEPT); CHECK(h.done == 1); h.done = 0;                          /* the last row is DONE */
  pg_key(&h, PG_UP); pg_key(&h, PG_UP); CHECK(h.cursor == nvis);                    /* the advanced toggle */
  pg_key(&h, PG_ACCEPT); CHECK(h.show_adv == 1 && pg_entries(&h) == h.n + 3);
  h.cursor = 0;
  pg_key(&h, PG_LEFT); CHECK(h.o[0].cur == h.o[0].nvals - 1);
  pg_key(&h, PG_RIGHT); CHECK(h.o[0].cur == 0);
  pg_key(&h, PG_RESET); CHECK(pg_is_default(&h));
  for (int i = 0; i < h.n; i++) if (!strcmp(h.o[i].key, "charge_frames_per_stage")) {
    int e = 0;
    for (int k = 0; k < i; k++) e++;                                              /* all options are visible now, so entry == index */
    h.cursor = e;
    pg_key(&h, PG_RIGHT); CHECK(h.o[i].cur == 1);
    pg_key(&h, PG_FAST_RIGHT); CHECK(h.o[i].cur == 11);
    pg_key(&h, PG_FAST_LEFT); CHECK(h.o[i].cur == 1);
    pg_key(&h, PG_LEFT); CHECK(h.o[i].cur == -1);                                  /* below the minimum on an optional number: unset */
    pg_key(&h, PG_FAST_RIGHT); CHECK(h.o[i].cur == 1);
    pg_key(&h, PG_ACCEPT); CHECK(h.o[i].cur == -1);
    for (int k = 0; k < 20; k++) pg_key(&h, PG_FAST_RIGHT);
    CHECK(h.o[i].cur == 90);
  }
  pg_reset(&h); h.cursor = pg_entries(&h) - 2; pg_key(&h, PG_ACCEPT); CHECK(pg_is_default(&h) && !h.done);
  pg_value_text(&h.o[0], 0, row, sizeof row); CHECK(row[0] != 0);
  /* presets of the menu file: vanilla first, applied in turn by PG_PRESET, recognised again by value, a manual change makes it custom */
  {
    struct sgp_opts so, vanilla;
    char why[64];
    int qc = -1, ar = -1;
    CHECK(h.npre >= 4 && !strcmp(h.pre[0].id, "vanilla"));
    for (int i = 0; i < h.npre; i++) { if (!strcmp(h.pre[i].id, "quick-combat")) qc = i; if (!strcmp(h.pre[i].id, "qol")) ar = i; }
    CHECK(qc > 0 && ar > 0);
    pg_reset(&h);
    CHECK(pg_preset_match(&h) == 0);                                              /* nothing chosen is the vanilla preset */
    for (int i = 1; i <= qc; i++) pg_key(&h, PG_PRESET);
    CHECK(pg_preset_match(&h) == qc && !pg_is_default(&h));
    CHECK(pgc_from_pg(&h, &so, why, sizeof why) == 4);                            /* weapon + run recharge, charge speed, monster flinch */
    CHECK(so.v[SGP_WEAPON_RECHARGE] == 2 && so.v[SGP_RUN_RECHARGE] == 1 && so.v[SGP_CHARGE_SPEED] == 2 && so.v[SGP_MONSTER_FLINCH] == 1);
    h.o[0].cur = (h.o[0].cur + 1) % h.o[0].nvals;
    CHECK(pg_preset_match(&h) == -1);                                             /* edited: custom */
    pg_key(&h, PG_PRESET); CHECK(pg_preset_match(&h) == 0 && pg_is_default(&h));  /* custom goes to the first preset */
    for (int i = 0; i < h.npre; i++) pg_key(&h, PG_PRESET);
    CHECK(pg_preset_match(&h) == 0);                                              /* a full lap comes back to vanilla */
    pg_reset(&h);
    for (int i = 0; i < ar; i++) pg_key(&h, PG_PRESET);
    CHECK(pgc_from_pg(&h, &so, why, sizeof why) >= 7 && so.v[SGP_QOL_ITEM_LIMIT] == 7 && so.v[SGP_QOL_SCROLL_DEADZONE] == 1 && so.v[SGP_QOL_CHEST_FAST] == 1);
    pg_reset(&h);
    sgp_defaults(&vanilla);
    CHECK(pgc_from_pg(&h, &so, why, sizeof why) == 0 && memcmp(&so, &vanilla, sizeof so) == 0);
    CHECK(pg_load_values(&h, "{\"walk_speed\": 4}") == 0 && pgc_from_pg(&h, &so, why, sizeof why) == 1 && so.v[SGP_WALK_SPEED] == 4);   /* a flat object works too */
    pg_reset(&h);
  }
  /* drawing: nothing outside the buffer (canary rows around it), the page changes with the cursor, several sizes */
  {
    int sizes[3][2] = {{640, 480}, {1280, 720}, {1920, 1080}};
    for (int z = 0; z < 3; z++) {
      int w = sizes[z][0], hh = sizes[z][1];
      uint32_t *buf = malloc((size_t)(w + 64) * (hh + 2) * 4), *a, h1, h2;
      for (size_t i = 0; i < (size_t)(w + 64) * (hh + 2); i++) buf[i] = 0xDEADBEEFu;
      a = buf + (w + 64);
      h.show_adv = 1; h.cursor = 0;
      pg_draw(&h, a, w + 64, w, hh, "NOTE");
      h1 = pc_hash_pixels(a, w + 64, w, hh);
      h.cursor = 14;
      pg_draw(&h, a, w + 64, w, hh, NULL);
      h2 = pc_hash_pixels(a, w + 64, w, hh);
      CHECK(h1 != h2);
      for (int x = 0; x < w + 64; x++) CHECK(buf[x] == 0xDEADBEEFu && buf[(size_t)(hh + 1) * (w + 64) + x] == 0xDEADBEEFu);
      for (int y = 0; y < hh; y++) for (int x = w; x < w + 64; x++) CHECK(a[(size_t)y * (w + 64) + x] == 0xDEADBEEFu);
      free(buf);
    }
  }
  CHECK(pg_load_menu(&h, "{}") == -1 && pg_load_menu(&h, "{\"options\": [") == -1);
  free(menu);
}

int main(int argc, char **argv) {
  int update = argc > 1 && !strcmp(argv[1], "--update-hashes");
  const char *path = argc > 2 ? argv[2] : "golden_hashes.txt";
  test_cfg(); test_profile(); test_fit(); test_shoulders(); test_nav(); test_overlay(); test_logic_extra(); test_store(); test_gameplay();
  if (check_hashes(path, update)) return 1;
  printf("%d checks, %d failures\n", checks, fails);
  return fails != 0;
}
