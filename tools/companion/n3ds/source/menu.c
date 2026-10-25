/* Start menu of the 3DS frontend.  The generator is not available on the console yet (libsomrando is pending), so a randomized game is a
 * pre-generated ROM that the PC tool put in sdmc:/som/roms/gen/<key>.sfc (with its <key>.json sidecar); the menu looks the chosen
 * configuration up there and lists the newest four as saved games.  Deleting a saved game moves its files to sdmc:/som/roms/trash. */
#include <3ds.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "menu.h"
#include "fb.h"
#include "launcher/som_launcher.h"
#include "launcher/som_launcher_draw.h"
#include "launcher/somrando_api.h"
#include "launcher/somrando_menu.h"
#include "settings.h"
#include "screens.h"
#include "sui.h"

#define ROM_ORIGINAL MENU_ROOT "/rom.sfc"
#define PACK_PATH MENU_ROOT "/som_ui_pack.bin"

static struct sl_launcher L;
static uint32_t top_px[400 * 240], bot_px[320 * 240];

static int file_exists(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISREG(s.st_mode); }

/* A generated ROM is 4 MiB with the "Secret of MANA" title in the header. */
static int looks_generated(const char *path) {
  struct stat s;
  char t[15] = {0};
  if (stat(path, &s) != 0 || !S_ISREG(s.st_mode) || s.st_size != 4 * 1024 * 1024) return 0;
  FILE *f = fopen(path, "rb");
  if (!f) return 0;
  int ok = fseek(f, 0xFFC0, SEEK_SET) == 0 && fread(t, 1, 14, f) == 14 && strcmp(t, "Secret of MANA") == 0;
  fclose(f);
  return ok;
}

/* ---- sidecar: {"config":{"preset":"..","mode":"..","seed":"..","difficulty":"..","options":{"k":"v",..}}} read with a tiny scanner */
static const char *find_key(const char *j, const char *key) {
  char pat[40];
  snprintf(pat, sizeof pat, "\"%s\"", key);
  const char *p = strstr(j, pat);
  if (!p) return NULL;
  p += strlen(pat);
  while (*p == ' ' || *p == ':') p++;
  return p;
}
static void json_str(const char *j, const char *key, char *out, size_t n) {
  const char *p = find_key(j, key);
  size_t i = 0;
  out[0] = 0;
  if (!p) return;
  if (*p == '"') { for (p++; *p && *p != '"' && i + 1 < n; p++) out[i++] = *p; }
  else { for (; *p && *p != ',' && *p != '}' && *p != ' ' && i + 1 < n; p++) out[i++] = *p; }
  out[i] = 0;
}
static void json_options(const char *j, char *lines, size_t n) {
  const char *p = find_key(j, "options");
  size_t used = 0;
  lines[0] = 0;
  if (!p || *p != '{') return;
  for (p++; *p && *p != '}';) {
    char k[48], v[48];
    size_t i = 0;
    while (*p && *p != '"' && *p != '}') p++;
    if (*p != '"') break;
    for (p++; *p && *p != '"' && i + 1 < sizeof k; p++) k[i++] = *p;
    k[i] = 0;
    if (*p == '"') p++;
    while (*p == ' ' || *p == ':') p++;
    i = 0;
    if (*p == '"') { for (p++; *p && *p != '"' && i + 1 < sizeof v; p++) v[i++] = *p; if (*p == '"') p++; }
    else for (; *p && *p != ',' && *p != '}' && i + 1 < sizeof v; p++) v[i++] = *p;
    v[i] = 0;
    int w = snprintf(lines + used, n - used, "%s=%s\n", k, v);
    if (w < 0 || (size_t)w >= n - used) { lines[used] = 0; break; }
    used += (size_t)w;
  }
}

struct gen_entry { char key[17]; time_t mtime; };
static int by_newest(const void *a, const void *b) {
  const struct gen_entry *x = a, *y = b;
  if (x->mtime != y->mtime) return x->mtime < y->mtime ? 1 : -1;
  return strcmp(x->key, y->key);
}

static void refresh_saved(void) {
  static struct gen_entry e[64];
  int n = 0;
  sl_saved_clear(&L);
  DIR *d = opendir(MENU_GEN_DIR);
  if (!d) { sl_saved_set_hidden(&L, 0); return; }
  for (struct dirent *de; (de = readdir(d)) && n < 64;) {
    size_t l = strlen(de->d_name);
    if (l != 20 || strcmp(de->d_name + 16, ".sfc") != 0) continue;
    char p[160];
    struct stat s;
    snprintf(p, sizeof p, MENU_GEN_DIR "/%s", de->d_name);
    if (!looks_generated(p) || stat(p, &s) != 0) continue;
    memcpy(e[n].key, de->d_name, 16); e[n].key[16] = 0; e[n].mtime = s.st_mtime; n++;
  }
  closedir(d);
  qsort(e, (size_t)n, sizeof *e, by_newest);
  static char json[4096], lines[2048], path[160];
  for (int i = 0; i < n && i < SL_SAVED_MAX; i++) {
    char preset[32], mode[24], seed[12], diff[16], date[40];
    json[0] = 0;
    snprintf(path, sizeof path, MENU_GEN_DIR "/%.16s.json", e[i].key);
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(path, sizeof path, MENU_GEN_DIR "/%.16s.sfc.json", e[i].key); f = fopen(path, "rb"); }   /* som-dualscreen writes <key>.sfc.json */
    if (f) { size_t r = fread(json, 1, sizeof json - 1, f); json[r] = 0; fclose(f); }
    const char *cfgp = find_key(json, "config");
    if (!cfgp) cfgp = json;
    json_str(cfgp, "preset", preset, sizeof preset); json_str(cfgp, "mode", mode, sizeof mode);
    json_str(cfgp, "seed", seed, sizeof seed); json_str(cfgp, "difficulty", diff, sizeof diff);
    json_options(cfgp, lines, sizeof lines);
    if (!mode[0]) { snprintf(preset, sizeof preset, "Game %.8s", e[i].key); snprintf(seed, sizeof seed, "?"); }
    struct tm *tm = gmtime(&e[i].mtime);
    date[0] = 0;   /* the console file system may report no time at all: show no date then */
    if (e[i].mtime > 86400 * 365 && tm) snprintf(date, sizeof date, "%04d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    snprintf(path, sizeof path, MENU_GEN_DIR "/%.16s.sfc", e[i].key);
    sl_saved_add(&L, e[i].key, preset, mode, seed, diff, date, path, lines);
  }
  sl_saved_set_hidden(&L, n > SL_SAVED_MAX ? n - SL_SAVED_MAX : 0);
}

/* Only things that need attention are shown: no "ROM found" style lines. */
static void refresh_status(void) {
  sl_set_status(&L, file_exists(ROM_ORIGINAL) ? "" : "Put your ROM at sdmc:/som/rom.sfc", "");
}

/* The generator (libsomrando) tells which presets and which current setup it reproduces byte for byte. */
static void refresh_support(void) {
  static int presets_done;
  char why[SL_TEXT] = "";
  if (!somrando_available()) { sl_set_gen_support(&L, 0, "No generator in this build"); return; }
  if (!presets_done) {
    for (int p = 0; p < sl_npresets; p++) sl_set_preset_ok(&L, p, somrando_menu_preset_supported(p, NULL, 0) == 1);
    presets_done = 1;
  }
  int ok = somrando_menu_supports(&L, why, sizeof why);
  sl_set_gen_support(&L, ok ? 1 : 0, ok ? "" : why);
}

static void present(void) {
  sl_draw(&L, bot_px, 320, 320, 240);
  sl_draw_top(&L, top_px, 400, 400, 240);
  if (L.screen == SL_MAIN) sui_text_draw(bot_px, 320, 320, 240, 8, 211, 1, 0x8AA0C0, "Y: settings and credits");
  fb_blit(gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL), bot_px, 320);
  fb_blit(gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), top_px, 400);
  gfxFlushBuffers();
  gfxSwapBuffers();
}

static void pack_unload(void);
static void pack_load(void);

static int gen_progress(void *user, int step, int total, const char *name) {
  (void)user;
  if (!aptMainLoop()) return 1;
  hidScanInput();
  sl_gen_progress(&L, step, total, name);
  present();
  return (hidKeysDown() & KEY_B) ? 1 : 0;
}

/* Generates the current setup into roms/gen/<key>.sfc (4 MiB) and its sidecar.  Blocks until done; B cancels. */
static void generate_now(const char *key, const char *final_path) {
  char lines[4096], tmp[180], err[160] = "", why[SL_TEXT];
  const char *diff = NULL;
  if (!file_exists(ROM_ORIGINAL)) { sl_gen_fail(&L, "No ROM at sdmc:/som/rom.sfc"); return; }
  if (!somrando_menu_supports(&L, why, sizeof why)) { sl_gen_unavailable(&L, why); return; }
  if (sl_config_lines(&L, lines, sizeof lines) < 0) { sl_gen_fail(&L, "Configuration too long"); return; }
  mkdir(MENU_ROOT "/roms", 0777); mkdir(MENU_GEN_DIR, 0777);
  snprintf(tmp, sizeof tmp, MENU_GEN_DIR "/%.16s.tmp", key);
  struct somrando_request req = {0};
  req.mode = sl_modes[L.cfg.mode].key;
  req.seed = L.cfg.seed;
  req.difficulty = sl_difficulty(&L, &diff) ? diff : NULL;
  req.settings = lines;
  req.src_rom = ROM_ORIGINAL;
  req.dst_rom = tmp;
  sl_gen_begin(&L);
  pack_unload();
  u64 t0 = osGetTime();
  int rc = somrando_generate(&req, gen_progress, NULL, err, sizeof err);
  pack_load();
  if (rc == SOMRANDO_ERR_CANCELLED) { sl_show(&L, SL_TWEAK); return; }
  if (rc != SOMRANDO_OK) { sl_gen_fail(&L, err[0] ? err : "Generation failed"); return; }
  if (!looks_generated(tmp) || rename(tmp, final_path) != 0) { sl_gen_fail(&L, "The generated ROM could not be stored on the SD card"); return; }
  {
    static char cj[4096], side[4300], sp[180];
    if (sl_config_json(&L, cj, sizeof cj) > 0) {
      snprintf(side, sizeof side, "{\"tool\":\"n3ds\",\"seconds\":%d,\"config\":%s}\n", (int)((osGetTime() - t0) / 1000), cj);
      snprintf(sp, sizeof sp, MENU_GEN_DIR "/%.16s.json", key);
      FILE *f = fopen(sp, "wb");
      if (f) { fwrite(side, 1, strlen(side), f); fclose(f); }
    }
  }
  refresh_saved();
  sl_gen_done(&L, final_path);
}

static void trash_saved(const char *key) {
  static const char *ext[] = {".sfc", ".json", ".sfc.json"};
  char from[160], to[200];
  mkdir(MENU_ROOT "/roms", 0777); mkdir(MENU_TRASH_DIR, 0777);
  for (int i = 0; i < 3; i++) {
    snprintf(from, sizeof from, MENU_GEN_DIR "/%.16s%s", key, ext[i]);
    snprintf(to, sizeof to, MENU_TRASH_DIR "/%llu-%.16s%s", (unsigned long long)osGetTime(), key, ext[i]);
    if (file_exists(from)) rename(from, to);
  }
}

static const struct { u32 key; int sl; } MKEYS[] = {
  {KEY_DUP, SL_UP}, {KEY_DDOWN, SL_DOWN}, {KEY_DLEFT, SL_LEFT}, {KEY_DRIGHT, SL_RIGHT}, {KEY_A, SL_A}, {KEY_B, SL_B},
  {KEY_X, SL_X}, {KEY_Y, SL_Y}, {KEY_L, SL_L}, {KEY_R, SL_R}, {KEY_START, SL_START}, {KEY_SELECT, SL_SELECT},
};

/* The optional art pack (9.4 MB) is dropped while the generator runs (its peak heap is 9 to 22 MB, measured in Azahar) and read again afterwards. */
static uint8_t *g_pack;
static void pack_unload(void) { sl_draw_free_pack(); free(g_pack); g_pack = NULL; }
static void pack_load(void) {
  FILE *pf = fopen(PACK_PATH, "rb");
  size_t pl = 0;
  if (!pf) return;   /* optional art pack, never in the repo */
  fseek(pf, 0, SEEK_END); pl = (size_t)ftell(pf); fseek(pf, 0, SEEK_SET);
  g_pack = malloc(pl);
  if (g_pack && fread(g_pack, 1, pl, pf) == pl) { if (sl_draw_load_pack(g_pack, pl) != 0) { free(g_pack); g_pack = NULL; } }
  else { free(g_pack); g_pack = NULL; }
  fclose(pf);
}

int menu_run(char *rom_out, size_t n, int *original, struct n3_settings *settings) {
  int result = 0, tdown = 0, tx = 0, ty = 0, tlast_y = 0, dragged = 0, redraw = 1, idle = 0;
  uint32_t seen_rev = 0;
  pack_load();
  sl_init(&L, (uint64_t)osGetTime() * 2654435761u + 1);
  sl_set_view(&L, 320, 240);
  refresh_saved();
  refresh_status();
  refresh_support();
  hidSetRepeatParameters(20, 5);
  while (aptMainLoop()) {
    hidScanInput();
    u32 down = hidKeysDown(), rep = hidKeysDownRepeat(), held = hidKeysHeld(), up = hidKeysUp();
    if (L.screen == SL_MAIN && (down & (KEY_Y | KEY_SELECT))) {   /* Settings and credits: the launcher model has no entry for them */
      screens_settings(settings);
      hidSetRepeatParameters(20, 5);
      redraw = 1;
      continue;
    }
    for (size_t i = 0; i < sizeof MKEYS / sizeof *MKEYS; i++) {
      u32 k = MKEYS[i].key;
      int dpad = MKEYS[i].sl <= SL_RIGHT;
      if ((down & k) || (dpad && (rep & k))) sl_key(&L, MKEYS[i].sl);
    }
    if (down & KEY_TOUCH) { touchPosition tp; hidTouchRead(&tp); tx = tp.px; ty = tlast_y = tp.py; tdown = 1; dragged = 0; }
    else if ((held & KEY_TOUCH) && tdown) {
      touchPosition tp; hidTouchRead(&tp);
      struct sl_metrics m; sl_metrics_for(320, 240, &m);
      int rh = m.row_h * m.s > 0 ? m.row_h * m.s : 16;
      if (abs((int)tp.py - tlast_y) >= rh && abs((int)tp.py - ty) > 8) { sl_scroll(&L, ((int)tp.py < tlast_y) ? 1 : -1); tlast_y = tp.py; dragged = 1; }
    }
    if ((up & KEY_TOUCH) && tdown) {
      tdown = 0;
      if (!dragged && L.screen == SL_MAIN && ty >= 205 && ty < 222) { screens_settings(settings); hidSetRepeatParameters(20, 5); redraw = 1; continue; }   /* the Settings line */
      if (!dragged) sl_tap(&L, tx, ty, 320, 240);
    }

    sl_tick(&L);
    if (L.rev != seen_rev && (L.screen == SL_TWEAK || L.screen == SL_PRESETS || L.screen == SL_MULTI)) refresh_support();
    int act;
    while (sl_poll(&L, &act)) {
      if (act == SL_ACT_QUIT) goto done;
      if (act == SL_ACT_PLAY_ORIGINAL) {
        if (file_exists(ROM_ORIGINAL)) { snprintf(rom_out, n, "%s", ROM_ORIGINAL); *original = 1; result = 1; goto done; }
        snprintf(L.notice, sizeof L.notice, "No ROM at sdmc:/som/rom.sfc"); L.notice_t = 180;
      } else if (act == SL_ACT_GENERATE) {
        char key[17], p[160];
        sl_config_key(&L, key);
        snprintf(p, sizeof p, MENU_GEN_DIR "/%.16s.sfc", key);
        if (looks_generated(p)) sl_gen_cached(&L, p);
        else if (somrando_available()) generate_now(key, p);
        else {
          char msg[SL_TEXT * 2];
          snprintf(msg, sizeof msg, "No ROM for this setup (key %.16s). Make it on the PC and copy it to sdmc:/som/roms/gen/", key);
          sl_gen_unavailable(&L, msg);
        }
      } else if (act == SL_ACT_PLAY) { snprintf(rom_out, n, "%s", sl_rom_path(&L)); *original = 0; result = 1; goto done; }
      else if (act == SL_ACT_DELETE_SAVED) {
        int slot = sl_act_arg(&L);
        if (slot >= 0 && slot < SL_SAVED_MAX && L.saved[slot].used) { char key[17]; snprintf(key, sizeof key, "%s", L.saved[slot].key); trash_saved(key); refresh_saved(); refresh_status(); }
      }
    }
    if (L.rev != seen_rev || L.notice_t > 0 || ++idle >= 30) { redraw = 1; idle = 0; }
    if (redraw) {
      present();
      seen_rev = L.rev; redraw = 0;
    }
    gspWaitForVBlank();
  }
done:
  pack_unload();
  return result;
}
