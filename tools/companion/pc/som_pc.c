/* som-pc: PC frontend of the Secret of Mana companion (board item T10).
 *
 * Hosts the patched Snes9x widescreen libretro core (tools/companion/widescreen) with SDL2: 1080p window / borderless / exclusive fullscreen,
 * FILL / FIT / INTEGER / STRETCH scaling, keyboard and SDL game controllers, audio, and the four shoulder slots L1 R1 L2 R2 that run
 * a spell, item, weapon or spirit list through the direct layer (somd_*) with the UX layer's quick-slot model, picker and target policies.
 * The start menu of T9 (SoM original / randomized) runs first when no ROM is given.  Documentation: docs/companion-pc-20261018.md.
 * The ROM is only read; nothing is patched and nothing derived from it is written inside the repository. */
#define _GNU_SOURCE
#include <SDL.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "../direct/som_direct.h"
#include "../launcher/som_launcher.h"
#include "../launcher/som_launcher_draw.h"
#include "../som_bind.h"
#include "../som_companion.h"
#include "../som_hotkey.h"
#include "../ux/som_ui_draw.h"
#include "pc_logic.h"
#include "pc_sdl.h"
#include "pc_store.h"
#include "pc_gameplay.h"
#include "pc_compat.h"
#include "pc_gen.h"
#include "pc_gpc.h"
#include "pc_menu.h"
#include "pc_runs.h"
#include "pc_shader.h"
#include "pc_states.h"
#include "../launcher/som_credits.h"
#include "../launcher/somrando_api.h"
#include "../launcher/somrando_menu.h"

#ifndef SOM_PC_REPO
#define SOM_PC_REPO "."
#endif

/* ------------------------------------------------------------------ libretro */
struct rinfo { const char *path; const void *data; size_t size; const char *meta; };
struct rvar { const char *key; const char *value; };
struct rgeom { unsigned base_w, base_h, max_w, max_h; float aspect; };
struct rav { struct rgeom g; double fps, rate; };
enum { RENV_CAN_DUPE = 3, RENV_PERF = 8, RENV_SYSDIR = 9, RENV_PIXFMT = 10, RENV_INPUTDESC = 11, RENV_GETVAR = 15, RENV_SETVARS = 16, RENV_VARUPD = 17,
       RENV_SAVEDIR = 31, RENV_SETAV = 32, RENV_CTRLINFO = 35, RENV_GEOMETRY = 37 };

static void *core_h;
static void (*r_run)(void), (*r_unload)(void), (*r_deinit)(void);
static size_t (*r_ser_size)(void), (*r_mem_size)(unsigned);
static int (*r_ser)(void *, size_t), (*r_unser)(const void *, size_t);
static void *(*r_mem)(unsigned);
static uint8_t *wram;

struct cfg_var { const char *key; char val[24]; };
static struct cfg_var core_vars[3] = {{"snes9x_som_widescreen", "46"}, {"snes9x_som_hud", "off"}, {"snes9x_som_widescreen_fallback", "normal"}};
static int vars_dirty;
static int pix_fmt = 2;
static char sys_dir[512] = ".";

static int r_env(unsigned cmd, void *d) {
  switch (cmd & 0xFFFF) {
  case RENV_CAN_DUPE: *(uint8_t *)d = 1; return 1;
  case RENV_PERF: case RENV_INPUTDESC: case RENV_CTRLINFO: case RENV_SETAV: case RENV_GEOMETRY: return 1;
  case RENV_SYSDIR: case RENV_SAVEDIR: *(const char **)d = sys_dir; return 1;
  case RENV_PIXFMT: { int f = *(int *)d; if (f == 1 || f == 2) { pix_fmt = f; return 1; } return 0; }
  case RENV_GETVAR: {
    struct rvar *v = d;
    for (size_t i = 0; i < sizeof core_vars / sizeof *core_vars; i++) if (!strcmp(core_vars[i].key, v->key)) { v->value = core_vars[i].val; return 1; }
    return 0;
  }
  case RENV_VARUPD: *(uint8_t *)d = (uint8_t)vars_dirty; vars_dirty = 0; return 1;
  default: return 0;
  }
}

/* ------------------------------------------------------------------ global state */
static struct pc_cfg cfg;
static char cfg_dir[512], state_dir[512], cfg_path[600];
static int headless, script_mode, verbose;
static int out_w, out_h;                     /* render target size in pixels */
static SDL_Window *win;
static SDL_Renderer *rd;
static SDL_Surface *head_surf;
static SDL_Texture *tex_game, *tex_ov, *tex_modal, *tex_menu;
static struct pc_audio aud;
#define adev (aud.dev)
static int audio_rate = 32040;
static double core_fps = 60.0988;

static uint8_t *rom_bytes, *rom_alloc; static size_t rom_len;
static char rom_path[512], rom_key[20], variant[40], srm_src[512];
static struct somd dir;
static struct som_state st;
static struct som_ux *ux;
static uint8_t *pack_blob;
static struct som_ux_pack *pack_ptr;
static struct pc_profile prof;
static char prof_path[700];

static uint32_t game_px[640 * 256]; static int game_w = 256, game_h = 224, game_have;
static int audio_mute;                      /* frames run to skip a menu video: their sound is dropped */
static int user_mute;                       /* F9: the queue keeps getting silence so the pacing does not change */
static unsigned long frame_no;
static uint16_t pad_game;                  /* SNES buttons from the player for the next frame */
static int quit, paused, modal_open;
static int vsync_on;
static struct pcm menu_ui;                  /* the in-game menu (pc_menu.c) */

/* statistics */
static double emu_ms, emu_max, rend_ms, rend_max; static unsigned long emu_n, rend_n, loops_n;

static void logf_(const char *fmt, ...) {
  va_list ap;
  if (!verbose) return;
  va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); fputc('\n', stderr);
}
static double now_ms(void) { return (double)SDL_GetPerformanceCounter() * 1000.0 / (double)SDL_GetPerformanceFrequency(); }

/* ------------------------------------------------------------------ files */
static int mkdirs(const char *path) {
  char t[700];
  snprintf(t, sizeof t, "%s", path);
  for (char *p = t + 1; *p; p++) if (*p == '/') { *p = 0; PC_MKDIR(t); *p = '/'; }
  return PC_MKDIR(t) == 0 || errno == EEXIST ? 0 : -1;
}
static uint8_t *slurp(const char *path, size_t *n) {
  FILE *f = fopen(path, "rb");
  uint8_t *b;
  long sz;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); sz = ftell(f); rewind(f);
  b = malloc(sz > 0 ? (size_t)sz : 1);
  if (!b || fread(b, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(b); return NULL; }
  fclose(f);
  *n = (size_t)sz;
  return b;
}
static int spit(const char *path, const void *d, size_t n) {
  char tmp[720];
  FILE *f;
  snprintf(tmp, sizeof tmp, "%s.part", path);
  f = fopen(tmp, "wb");
  if (!f) return -1;
  if (fwrite(d, 1, n, f) != n) { fclose(f); return -1; }
  fclose(f);
  return pc_rename(tmp, path);
}
static void base_dir(char *out, size_t n);
/* Portable layout: a file `portable.txt` next to the executable (always on Windows) keeps the configuration, saves and states in <exe dir>/userdata
 * instead of the home directory.  SOM_PC_CONFIG and SOM_PC_STATE still win. */
static int portable_mode(char *root, size_t n) {
  char base[600], marker[700];
  base_dir(base, sizeof base);
  snprintf(root, n, "%suserdata", base);
#ifdef _WIN32
  (void)marker;
  return 1;
#else
  snprintf(marker, sizeof marker, "%sportable.txt", base);
  return access(marker, R_OK) == 0;
#endif
}
static void set_dirs(void) {
  const char *e = getenv("SOM_PC_CONFIG"), *h = getenv("HOME"), *s = getenv("SOM_PC_STATE");
  char root[640];
  int portable = portable_mode(root, sizeof root);
  if (!h || !*h) h = getenv("USERPROFILE");
  snprintf(cfg_dir, sizeof cfg_dir, "%s", e && *e ? e : "");
  if (!cfg_dir[0]) { if (portable) snprintf(cfg_dir, sizeof cfg_dir, "%s/config", root); else snprintf(cfg_dir, sizeof cfg_dir, "%s/.config/som-decomp-pc", h ? h : "."); }
  snprintf(state_dir, sizeof state_dir, "%s", s && *s ? s : "");
  if (!state_dir[0]) { if (portable) snprintf(state_dir, sizeof state_dir, "%s/state", root); else snprintf(state_dir, sizeof state_dir, "%s/.local/state/som-decomp/pc", h ? h : "."); }
  SDL_setenv("SOM_PC_CONFIG", cfg_dir, 0);                       /* the controller database lookup of pc_sdl.c reads it */
  snprintf(cfg_path, sizeof cfg_path, "%s/config.json", cfg_dir);
}
static int cfg_text_ok(const char *t) { struct pc_cfg tmp; pc_cfg_defaults(&tmp); return pc_json_complete(t) && pc_cfg_from_json(&tmp, t) == 0; }
static int prof_text_ok(const char *t) { struct pc_profile tmp; return pc_json_complete(t) && pc_profile_from_json(&tmp, t) == 0; }
static void report_source(const char *what, const char *path, int src) {
  if (src == 1) fprintf(stderr, "som-pc: %s %s was unusable, restored from %s.bak\n", what, path, path);
  else if (src == 3) fprintf(stderr, "som-pc: %s %s was unusable (kept as %s.bad), starting empty\n", what, path, path);
}
static void cfg_load(void) {
  int src;
  char *t = pc_store_load(cfg_path, cfg_text_ok, &src);
  report_source("config", cfg_path, src);
  if (!t) return;
  pc_cfg_from_json(&cfg, t);
  free(t);
}
static void cfg_save(void) {
  char buf[4096];
  if (script_mode) return;
  mkdirs(cfg_dir);
  if (pc_cfg_to_json(&cfg, buf, sizeof buf) > 0) pc_store_save(cfg_path, buf);
}
static void profile_save(void) {
  char buf[1024];
  for (int i = 0; i < SOM_UX_SHOULDERS; i++) {
    int k = 0, id = 0;
    som_ux_quick_get(ux, i, &k, &id);
    prof.slot[i].kind = (uint8_t)k; prof.slot[i].id = (uint8_t)id; prof.slot[i].policy = (uint8_t)som_ux_quick_policy(ux, i);
  }
  if (!prof_path[0] || pc_profile_to_json(&prof, variant, buf, sizeof buf) < 0) return;
  { char d[720]; snprintf(d, sizeof d, "%s/profiles", cfg_dir); mkdirs(d); }
  if (pc_store_save(prof_path, buf) != 0) fprintf(stderr, "som-pc: cannot save the slot bindings to %s\n", prof_path);
}
static void profile_load(void) {
  int src = 2;
  char *t = pc_store_load(prof_path, prof_text_ok, &src);
  memset(&prof, 0, sizeof prof);
  report_source("slot bindings", prof_path, src);
  if (t) { pc_profile_from_json(&prof, t); free(t); }
  for (int i = 0; i < SOM_UX_SHOULDERS; i++) {
    som_ux_quick_set(ux, i, prof.slot[i].kind, prof.slot[i].id);
    if (prof.slot[i].kind) som_ux_quick_set_policy(ux, i, prof.slot[i].policy);
  }
}

/* ------------------------------------------------------------------ video / audio callbacks */
static void r_video(const void *data, unsigned w, unsigned h, size_t pitch) {
  if (!data || w > 640 || h > 256) return;
  game_w = (int)w; game_h = (int)h; game_have = 1;
  for (unsigned y = 0; y < h; y++) {
    const uint8_t *row = (const uint8_t *)data + y * pitch;
    uint32_t *o = game_px + y * 640;
    if (pix_fmt == 2) {
      const uint16_t *s = (const uint16_t *)row;
      for (unsigned x = 0; x < w; x++) { uint16_t p = s[x]; unsigned r = (p >> 11) & 31, g = (p >> 5) & 63, b = p & 31; o[x] = 0xFF000000u | (((r << 3) | (r >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8) | ((b << 3) | (b >> 2)); }
    } else {
      const uint32_t *s = (const uint32_t *)row;
      for (unsigned x = 0; x < w; x++) o[x] = 0xFF000000u | s[x];
    }
  }
}
static void r_audio_s(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t r_audio(const int16_t *d, size_t frames) {
  if (aud.dev && !audio_mute) pc_audio_queue(&aud, d, frames, user_mute ? 0 : cfg.volume);
  return frames;
}
static void r_poll(void) {}
static int16_t r_input(unsigned port, unsigned dev, unsigned idx, unsigned id) {
  (void)idx;
  if (port != 0 || dev != 1 || id > 15) return 0;
  return (int16_t)((pad_game >> id) & 1);
}

/* ------------------------------------------------------------------ core load */
static void core_options_apply(void) {
  snprintf(core_vars[0].val, sizeof core_vars[0].val, "%s", cfg.wide ? (cfg.wide == 17 ? "17" : cfg.wide == 27 ? "27" : "46") : "disabled");
  snprintf(core_vars[1].val, sizeof core_vars[1].val, "%s", cfg.hud_hide ? "hide_except_ring" : "off");
  snprintf(core_vars[2].val, sizeof core_vars[2].val, "%s", cfg.fallback_black ? "black" : "normal");
  vars_dirty = 1;
}
static int core_open(const char *path) {
  core_h = PC_DLOPEN(path);
  if (!core_h) { fprintf(stderr, "som-pc: cannot load core %s: %s\n", path, PC_DLERROR()); return -1; }
#define SYMR(v, n) do { *(void **)&(v) = PC_DLSYM(core_h, n); if (!(v)) { fprintf(stderr, "som-pc: core lacks %s\n", n); return -1; } } while (0)
  SYMR(r_run, "retro_run"); SYMR(r_unload, "retro_unload_game"); SYMR(r_deinit, "retro_deinit");
  SYMR(r_ser_size, "retro_serialize_size"); SYMR(r_ser, "retro_serialize"); SYMR(r_unser, "retro_unserialize");
  SYMR(r_mem, "retro_get_memory_data"); SYMR(r_mem_size, "retro_get_memory_size");
  void (*set_env)(void *), (*set_vid)(void *), (*set_as)(void *), (*set_asb)(void *), (*set_poll)(void *), (*set_inp)(void *), (*init)(void);
  SYMR(set_env, "retro_set_environment"); SYMR(set_vid, "retro_set_video_refresh"); SYMR(set_as, "retro_set_audio_sample");
  SYMR(set_asb, "retro_set_audio_sample_batch"); SYMR(set_poll, "retro_set_input_poll"); SYMR(set_inp, "retro_set_input_state"); SYMR(init, "retro_init");
  set_env(r_env); set_vid(r_video); set_as(r_audio_s); set_asb(r_audio); set_poll(r_poll); set_inp(r_input);
  core_options_apply();
  init();
  return 0;
}
static char save_key[20];                    /* battery save key: the ROM before the gameplay tweaks, so toggling a tweak keeps the game */
static int srm_path(char *out, size_t n) { return snprintf(out, n, "%s/saves/%s.srm", state_dir, save_key[0] ? save_key : rom_key); }
static void srm_load(void) {
  char p[800], bak[820];
  size_t n, cap = r_mem_size(0);
  uint8_t *b, *m = r_mem(0);
  srm_path(p, sizeof p);
  if (!m || !cap || !(b = slurp(p, &n))) return;
  memcpy(m, b, n < cap ? n : cap);
  snprintf(bak, sizeof bak, "%s.bak", p);                          /* the save as the last session left it, kept for the next one */
  if (!script_mode) spit(bak, b, n);
  free(b);
}
static uint8_t *srm_last; static size_t srm_last_n;
static void srm_save(int force) {
  char p[800], d[800];
  size_t cap = r_mem_size(0);
  uint8_t *m = r_mem(0);
  if (!m || !cap || (script_mode && force != 2)) return;
  if (!force && srm_last && srm_last_n == cap && !memcmp(srm_last, m, cap)) return;
  snprintf(d, sizeof d, "%s/saves", state_dir); mkdirs(d);
  srm_path(p, sizeof p);
  if (pc_spit(p, m, cap) == 0) { free(srm_last); srm_last = malloc(cap); memcpy(srm_last, m, cap); srm_last_n = cap; }
  else fprintf(stderr, "som-pc: cannot write the game save %s\n", p);
}

/* ------------------------------------------------------------------ sha256 of the ROM for the save key */
static void make_keys(const char *profile_name) {
  uint8_t h[32];
  sl_sha256(rom_bytes, rom_len, h);
  for (int i = 0; i < 6; i++) snprintf(rom_key + 2 * i, 3, "%02x", h[i]);
  som_bind_rom_variant(rom_bytes, rom_len, variant, sizeof variant);
  snprintf(prof_path, sizeof prof_path, "%s/profiles/%s.json", cfg_dir, profile_name && *profile_name ? profile_name : variant);
  save_key[0] = 0;
  if (srm_src[0] && strcmp(srm_src, rom_path)) {                    /* a patched copy keeps the battery save of its source ROM */
    size_t sn; uint8_t *sb = slurp(srm_src, &sn);
    if (sb) {
      size_t off = sn % 1024 == 512 ? 512 : 0;
      sl_sha256(sb + off, sn - off, h);
      for (int i = 0; i < 6; i++) snprintf(save_key + 2 * i, 3, "%02x", h[i]);
      free(sb);
    }
  }
}

/* ------------------------------------------------------------------ direct layer glue (same rules as the Android JNI) */
static struct som_ux_action defer; static int defer_valid, defer_t;
static uint8_t spell_av[42];

static void notice(int kind, const char *t) { som_ux_notice(ux, kind, t); }
static void refuse(const char *what) { notice(SOM_UX_NOTICE_ERROR, wram && (!strcmp(what, "UNSAFE_STATE") || !strcmp(what, "unsafe-state")) ? som_unsafe_reason(wram) : what); }

static void spell_states_update(void) {
  for (int id = 0; id < 42; id++) {
    int hero = (id % 6) >= 3 ? 1 : 2, spirit = id / 6, slot = id % 3;
    struct somd_cast_info info; struct somd_queue_info q;
    int r = somd_can_cast(&dir, wram, hero, spirit, slot, &info);
    spell_av[id] = (r == SOMD_CAST_OK) ? SOM_UX_AV_ENABLED : info.permanent ? SOM_UX_AV_DISABLED : SOM_UX_AV_ENABLED;
    if (somd_queue_state(&dir, hero, &q) == SOMD_Q_WAITING && q.spell_id == id) spell_av[id] = SOM_UX_AV_QUEUED;
  }
  som_ux_set_spell_avail(ux, spell_av);
}
static void queue_poll(void) {
  for (int h = 1; h <= 2; h++) {
    struct somd_queue_info q;
    int s = somd_queue_state(&dir, h, &q);
    if (s == SOMD_Q_FIRED) { som_ux_flash_entry(ux, SOM_UX_FLASH_QUEUED, SOM_UX_BK_SPELL, q.spell_id); somd_queue_ack(&dir, h); logf_("queued cast fired hero=%d spell=%d after %u frames", h, q.spell_id, q.waited); }
    else if (s == SOMD_Q_DROPPED) { som_ux_notice(ux, SOM_UX_NOTICE_DROPPED, somd_err_name((enum somd_err)q.err)); somd_queue_ack(&dir, h); }
  }
}
static void run_menu(int kind, int hero, int arg) {
  int err = somd_open_menu(&dir, wram, kind, hero, arg);
  if (err != SOMD_OK) refuse(somd_err_name((enum somd_err)err));
}

static void dispatch(const struct som_ux_action *a);
static void apply_option(int id, int val);

static void run_cmd_action(const struct som_ux_action *d) {
  const struct som_cmd *c = &d->cmd;
  struct somd_target t = {SOMD_T_NONE, 0};
  int qpos = (c->hero == 1 && c->arg == 7) ? 6 : c->arg;
  if (c->kind == SOM_CMD_CAST_MAGIC && c->hero >= 1 && c->hero <= 2 && qpos <= 6 && c->spell < 3) {
    enum somd_class cl = somd_spell_class(&dir, somd_spell_id(c->hero, qpos, c->spell));
    if (cl != SOMD_C_NONE && cl != SOMD_C_UNSUPPORTED) {            /* targeted casts: fire now or wait for a transient reason */
      struct somd_policy pol = {SOMD_POL_SLOT, 0};
      struct somd_cast_info info;
      if (d->target_kind == 1) pol.slot = d->target_slot;
      else if (d->target_kind == 2 || d->target_kind == 3) pol.kind = SOMD_POL_ALL;
      int err = somd_queue_cast(&dir, wram, c->hero, qpos, c->spell, pol, 0, &info);
      logf_("cast hero=%d spirit=%d slot=%d reason=%d -> %s", c->hero, qpos, c->spell, info.reason, somd_err_name((enum somd_err)err));
      if (err != SOMD_OK) { refuse(somd_err_name((enum somd_err)err)); som_ux_slot_flash(ux, 0, 0); }
      else {
        struct somd_queue_info q;
        if (somd_queue_state(&dir, c->hero, &q) == SOMD_Q_WAITING) som_ux_notice(ux, q.replaced ? SOM_UX_NOTICE_INFO : SOM_UX_NOTICE_QUEUED, q.replaced ? "REPLACED" : "");
      }
      return;
    }
  }
  if (somd_busy(&dir)) { refuse("BUSY"); return; }
  if (!st.safe_to_inject) { refuse("UNSAFE_STATE"); return; }
  if (d->target_kind == 1) { t.kind = SOMD_T_ONE; t.slot = d->target_slot; }
  else if (d->target_kind == 2) t.kind = SOMD_T_ALL_ALLIES;
  else if (d->target_kind == 3) t.kind = SOMD_T_ALL_ENEMIES;
  else if (c->kind == SOM_CMD_CAST_MAGIC && somd_spell_class(&dir, somd_spell_id(c->hero, qpos, c->spell)) == SOMD_C_BOY_ONLY) { t.kind = SOMD_T_ONE; t.slot = 0; }
  int err = som_hk_start_cmd(&dir, wram, c, t);
  logf_("direct start kind=%d hero=%u arg=%u spell=%u target=%u/%u -> %s", c->kind, c->hero, c->arg, c->spell, t.kind, t.slot, somd_err_name((enum somd_err)err));
  if (err != SOMD_OK) refuse(somd_err_name((enum somd_err)err));
}

static void dispatch(const struct som_ux_action *a) {
  switch (a->kind) {
  case SOM_UX_ACT_CMD:
    if (st.mode == SOM_MODE_RING && !defer_valid) {               /* a slot used inside the ring: close the ring, then run it */
      defer = *a; defer_valid = 1; defer_t = 0;
      if (somd_close_menu(&dir, wram) != SOMD_OK) { /* already closing or busy: the deferred run waits for the field */ }
      return;
    }
    run_cmd_action(a);
    break;
  case SOM_UX_ACT_CANCEL_QUEUED:
    if (a->hero >= 1 && a->hero <= 2) { somd_queue_cancel(&dir, a->hero); somd_queue_ack(&dir, a->hero); som_ux_notice(ux, SOM_UX_NOTICE_CANCELLED, ""); }
    break;
  case SOM_UX_ACT_OPTION: apply_option(a->opt_id, a->opt_val); break;
  case SOM_UX_ACT_OPEN_MENU:
    if (somd_busy(&dir)) { refuse("BUSY"); break; }
    if (a->bind_kind == SOM_UX_BK_MENU) run_menu(SOMD_MENU_RING, a->hero, a->bind_id);
    else if (a->bind_kind == SOM_UX_BK_SUBMENU) run_menu(SOMD_MENU_PAGE, a->hero, a->bind_id);
    else if (a->bind_kind == SOM_UX_BK_SPIRIT) run_menu(SOMD_MENU_SPIRIT, a->hero == 0 ? 1 : a->hero, a->bind_id);
    break;
  case SOM_UX_ACT_QUICK_SET:
    profile_save();
    if (a->slot < SOM_UX_SHOULDERS) {
      char nm[40], line[80];
      const char *lab = som_ux_shoulder_label(ux, a->slot);
      if (a->bind_kind && som_ux_bind_name(ux, a->bind_kind, a->bind_id, nm, sizeof nm)) snprintf(line, sizeof line, "%s = %s", lab, nm);
      else snprintf(line, sizeof line, "%s CLEARED", lab);
      som_ux_notice(ux, SOM_UX_NOTICE_INFO, line);
    }
    som_ux_open_options(ux, 0);                                    /* straight back to the game */
    break;
  case SOM_UX_ACT_CLOSE_RING: somd_close_menu(&dir, wram); break;
  default: break;
  }
}

static const char *const WIDE_TXT[4] = {"OFF", "17", "27", "46"}, *const FIT_TXT[PC_FIT_COUNT] = {"FILL", "FIT", "INTEGER", "STRETCH"};
static int wide_index(void) { return cfg.wide == 17 ? 1 : cfg.wide == 27 ? 2 : cfg.wide == 46 ? 3 : 0; }
static void sync_options(void) {
  som_ux_set_option(ux, SOM_UX_OPT_WIDE, wide_index(), WIDE_TXT[wide_index()]);
  som_ux_set_option(ux, SOM_UX_OPT_FALLBACK, cfg.fallback_black, NULL);
  som_ux_set_option(ux, SOM_UX_OPT_HUD_HIDE, cfg.hud_hide, NULL);
  som_ux_set_option(ux, SOM_UX_OPT_CORE, 0, "SNES9X");
  som_ux_set_option(ux, SOM_UX_OPT_LANGUAGE, 0, "EN");
  som_ux_set_option(ux, SOM_UX_OPT_FIT, cfg.fit, FIT_TXT[cfg.fit]);
}
static void apply_option(int id, int val) {
  switch (id) {
  case SOM_UX_OPT_WIDE: cfg.wide = (int[]){0, 17, 27, 46}[val % 4]; core_options_apply(); break;
  case SOM_UX_OPT_FALLBACK: cfg.fallback_black = val != 0; core_options_apply(); break;
  case SOM_UX_OPT_HUD_HIDE: cfg.hud_hide = val != 0; core_options_apply(); break;
  case SOM_UX_OPT_FIT: cfg.fit = val % PC_FIT_COUNT; break;
  default: break;
  }
  sync_options();
}

/* ------------------------------------------------------------------ one emulated frame */
static void emu_frame(void) {
  double t0 = now_ms(), dt;
  pad_game |= somd_menu_pad(&dir);
  somd_pre_frame(&dir, wram);
  r_run();
  somd_post_frame(&dir, wram);
  frame_no++;
  som_read_state(wram, &st);
  som_ux_update(ux, wram);
  if ((frame_no & 3) == 0) spell_states_update();
  queue_poll();
  if (defer_valid) {
    if (++defer_t > 300) defer_valid = 0;
    else if (st.mode == SOM_MODE_FIELD && !somd_busy(&dir)) { struct som_ux_action a = defer; defer_valid = 0; run_cmd_action(&a); }
  }
  dt = now_ms() - t0;
  emu_ms += dt; emu_n++; if (dt > emu_max) emu_max = dt;
}
/* A frame the player sees: while a menu command runs, frames are run unpresented (no ring animation). */
static void step(void) {
  emu_frame();
  audio_mute = 0;
  for (int ff = 0; somd_hide_video(&dir) && ff < 200; ff++) { audio_mute = 1; pad_game = 0; emu_frame(); }
  audio_mute = 0;
}

/* ------------------------------------------------------------------ input */
static uint8_t sh_held;                     /* shoulder buttons held right now (bit per slot) */
static uint8_t sh_script;                   /* script-driven shoulder state */
static struct pc_sh shm;
static int progress[SOM_UX_SHOULDERS];
static struct pc_pads gp;
static int focus_kind = -1, focus_a;        /* gamepad focus in the picker */

/* A controller came or went: log it and, when one vanished, forget the slot buttons that were down so nothing fires or sticks. */
static void pads_event(const SDL_Event *e) {
  int r = pc_pads_event(&gp, e);
  if (r || gp.last_note[0]) { if (r) fprintf(stderr, "som-pc: %s\n", gp.last_note); gp.last_note[0] = 0; }
  if (r < 0) { pc_sh_cancel(&shm); sh_held = 0; }
}

static void open_picker(int slot) {
  ux->qe_slot = (uint8_t)(slot >= 0 && slot < SOM_UX_SHOULDERS ? slot : 0);
  som_ux_set_four_slots(ux, 1);
  som_ux_open_options(ux, SOM_UX_OPT_QUICK);
  focus_kind = SOM_UX_HIT_QUICK; focus_a = ux->qe_slot;
}
static void open_options(void) { som_ux_set_four_slots(ux, 1); som_ux_open_options(ux, SOM_UX_OPT_MAIN); focus_kind = -1; }

static void shoulder_fire(int slot) {
  struct som_ux_action a;
  if (st.mode == SOM_MODE_NO_GAME || st.mode == SOM_MODE_EVENT) { som_ux_slot_flash(ux, slot, SOM_UX_FLASH_REFUSED); return; }
  if (som_ux_shoulder_press(ux, slot, &a)) dispatch(&a);
}
static void shoulders_update(uint8_t held, uint32_t t_ms) {
  struct pc_sh_out o;
  if (modal_open || menu_ui.open) { memset(progress, 0, sizeof progress); shm.held = held; for (int i = 0; i < 4; i++) shm.consumed[i] = 1; return; }
  pc_sh_update(&shm, held, t_ms, &o);
  memcpy(progress, o.progress, sizeof progress);
  if (o.assign >= 0) open_picker(o.assign);
  else if (o.fire >= 0) shoulder_fire(o.fire);
}

/* ------------------------------------------------------------------ geometry of the picker panel */
static void panel_rect(int w, int h, SDL_Rect *r) {
  int pw = w * 7 / 10, ph = h * 9 / 10;
  if (pw > 1240) pw = 1240;
  if (ph > 970) ph = 970;
  if (pw < 320) pw = w;
  if (ph < 240) ph = h;
  r->w = pw; r->h = ph; r->x = (w - pw) / 2; r->y = (h - ph) / 2;
}
static int modal_wanted(void) { return ux->opt_page || ux->pol_wait || ux->rev_wait || (ux->pend_kind && ux->pend_class); }

static void picker_tap(int x, int y, SDL_Rect *pr) {
  struct som_ux_action a;
  som_ux_touch_up(ux);
  if (som_ux_touch(ux, x, y, pr->w, pr->h, &a)) dispatch(&a);
}
static int picker_hits(struct som_ux_hitrect *h, int max, SDL_Rect *pr) {
  panel_rect(out_w, out_h, pr);
  return som_ux_hit_list(ux, pr->w, pr->h, max, h);
}
static void picker_back(void) {
  struct som_ux_hitrect h[96]; SDL_Rect pr;
  int n = picker_hits(h, 96, &pr);
  int want[] = {SOM_UX_HIT_POL_CANCEL, SOM_UX_HIT_REV_CANCEL, SOM_UX_HIT_CANCEL, SOM_UX_HIT_QE_BACK};
  for (size_t k = 0; k < sizeof want / sizeof *want; k++)
    for (int i = 0; i < n; i++) if (h[i].kind == want[k]) { picker_tap(h[i].x + h[i].w / 2, h[i].y + h[i].h / 2, &pr); return; }
  for (int i = 0; i < n; i++) if (h[i].kind == SOM_UX_HIT_OPT_NAV && (h[i].a == 3 || h[i].a == 0)) { picker_tap(h[i].x + h[i].w / 2, h[i].y + h[i].h / 2, &pr); return; }
  som_ux_open_options(ux, 0);
  ux->pend_kind = 0; ux->pend_class = 0; ux->pol_wait = 0; ux->rev_wait = 0;
}
static void picker_move(int dx, int dy) {
  struct som_ux_hitrect h[96]; SDL_Rect pr;
  int n = picker_hits(h, 96, &pr), from = -1;
  for (int i = 0; i < n; i++) if (h[i].kind == focus_kind && h[i].a == focus_a) from = i;
  if (from < 0) { for (int i = 0; i < n; i++) if (h[i].kind == SOM_UX_HIT_QUICK && h[i].a == ux->qe_slot) from = i; }
  int to = pc_nav_pick(h, n, from, dx, dy);
  if (to >= 0 && to < n) { focus_kind = h[to].kind; focus_a = h[to].a; }
}
/* After a step the focus goes to the first useful target of the new page: the target policies when they are asked, else the first cell. */
static void picker_refocus(int prev_kind) {
  struct som_ux_hitrect h[96]; SDL_Rect pr;
  int n;
  if (!modal_wanted()) { focus_kind = -1; return; }
  n = picker_hits(h, 96, &pr);
  for (int i = 0; i < n; i++) if (h[i].kind == SOM_UX_HIT_POLICY && h[i].enabled) { focus_kind = h[i].kind; focus_a = h[i].a; return; }
  if (prev_kind == SOM_UX_HIT_QE_TAB || prev_kind == SOM_UX_HIT_QE_CELL || prev_kind == SOM_UX_HIT_QE_BACK || prev_kind == SOM_UX_HIT_QE_PAGE)
    for (int i = 0; i < n; i++) if (h[i].kind == SOM_UX_HIT_QE_CELL && h[i].enabled) { focus_kind = h[i].kind; focus_a = h[i].a; return; }
  for (int i = 0; i < n; i++) if (h[i].kind == focus_kind && h[i].a == focus_a) return;
  for (int i = 0; i < n; i++) if (h[i].enabled) { focus_kind = h[i].kind; focus_a = h[i].a; return; }
}
static void picker_confirm(void) {
  struct som_ux_hitrect h[96]; SDL_Rect pr;
  int n = picker_hits(h, 96, &pr);
  for (int i = 0; i < n; i++)
    if (h[i].kind == focus_kind && h[i].a == focus_a && h[i].enabled) { int pk = h[i].kind; picker_tap(h[i].x + h[i].w / 2, h[i].y + h[i].h / 2, &pr); modal_open = modal_wanted(); picker_refocus(pk); return; }
  for (int i = 0; i < n; i++) if (h[i].enabled) { focus_kind = h[i].kind; focus_a = h[i].a; return; }
}
static void picker_tab(int d) {
  struct som_ux_hitrect h[96]; SDL_Rect pr;
  int n = picker_hits(h, 96, &pr), cur = ux->qe_cat, tgt = cur + d;
  if (ux->opt_page != SOM_UX_OPT_QUICK || ux->pol_wait) return;
  for (int i = 0; i < n; i++) if (h[i].kind == SOM_UX_HIT_QE_TAB && h[i].a == tgt) { picker_tap(h[i].x + h[i].w / 2, h[i].y + h[i].h / 2, &pr); return; }
}

/* ------------------------------------------------------------------ save states, in-game menu, credits, runs (board item T12) */
static struct pcs_thumb slot_thumb[PCS_SLOTS];
static char *credits_text;
static struct pr_list runs;
static char runs_path[700];
static int back_to_launcher, combo_active;
static void apply_screen(int mode);

static int res_find(const char *name, const char *repo_rel, char *out, size_t n);
static void credits_init(void) {
  char cand[2][760];
  const char *env = getenv("SOM_PC_CREDITS");
  char out[16384];
  snprintf(cand[0], sizeof cand[0], "%s", env && *env ? env : "");
  cand[1][0] = 0;
  res_find("credits.json", "data/credits.json", cand[1], sizeof cand[1]);
  for (int i = 0; i < 2 && !credits_text; i++) {
    size_t n;
    uint8_t *b = cand[i][0] ? pc_slurp(cand[i], &n) : NULL;
    if (b) { b = realloc(b, n + 1); b[n] = 0; }
    if (b && sc_format((const char *)b, out, sizeof out) > 0) { credits_text = strdup(out); logf_("credits: %s", cand[i]); }
    free(b);
  }
  if (!credits_text) credits_text = strdup(sc_fallback());
}

static uint32_t core_sig(void) { return (uint32_t)r_ser_size(); }
static int state_save(int slot) {
  size_t n = r_ser_size();
  void *b = malloc(n ? n : 1);
  struct pcs_thumb *th = malloc(sizeof *th);
  int r = PCS_IO;
  if (b && th && n && r_ser(b, n)) {
    pcs_make_thumb(th, game_px, game_w, game_h, 640);
    r = pcs_save(state_dir, rom_key, slot, core_sig(), b, n, th, (int64_t)time(NULL));
    logf_("state slot %d saved: status %d, %zu bytes", slot + 1, r, n);
  }
  free(b); free(th);
  return r;
}
static int state_load(int slot) {
  uint8_t *b;
  size_t n;
  struct pcs_meta m;
  int r = pcs_load(state_dir, rom_key, slot, core_sig(), &b, &n, &m, NULL);
  logf_("state slot %d load: status %d", slot + 1, r);
  if (r != PCS_OK) return r;
  if (!r_unser(b, n)) { free(b); return PCS_BAD; }
  free(b);
  defer_valid = 0;
  somd_init(&dir, rom_bytes, rom_len);                              /* no command of the old timeline is in flight */
  som_read_state(wram, &st);
  som_ux_update(ux, wram);
  return PCS_OK;
}
static void states_refresh(void) {
  for (int i = 0; i < PCS_SLOTS; i++) {
    struct pcs_meta m;
    struct pcm_slot *c = &menu_ui.slot[i];
    int s = pcs_info(state_dir, rom_key, i, core_sig(), &m, &slot_thumb[i]);
    memset(c, 0, sizeof *c);
    c->status = s;
    if (s == PCS_OK) {
      time_t t = (time_t)m.time;
      struct tm tmv;
      if (pc_localtime(&t, &tmv)) strftime(c->when, sizeof c->when, "%Y-%m-%d %H:%M", &tmv);
      c->tw = slot_thumb[i].w; c->th = slot_thumb[i].h; c->thumb = slot_thumb[i].w ? slot_thumb[i].px : NULL;
    }
  }
}
static void quick_save(void) {
  if (!ux || !wram) return;
  if (state_save(cfg.state_slot) != PCS_OK) notice(SOM_UX_NOTICE_ERROR, "STATE NOT SAVED");
}
static void quick_load(void) {
  int r;
  if (!ux || !wram) return;
  r = state_load(cfg.state_slot);
  if (r == PCS_EMPTY) notice(SOM_UX_NOTICE_ERROR, "NO STATE IN THIS SLOT");
  else if (r != PCS_OK) notice(SOM_UX_NOTICE_ERROR, "STATE CANNOT BE LOADED");
}
static void slot_set(int slot) {
  char b[24];
  cfg.state_slot = ((slot % PCS_SLOTS) + PCS_SLOTS) % PCS_SLOTS;
  snprintf(b, sizeof b, "SLOT %d", cfg.state_slot + 1);
  if (ux) notice(SOM_UX_NOTICE_INFO, b);
}

static void apply_video(int row) {
  logf_("video row %d: filter %s, window %dx%d, screen %d, vsync %d, wide %d, fit %s", row, pc_shader_key(cfg.shader), cfg.win_w, cfg.win_h, cfg.screen, cfg.vsync, cfg.wide, pc_fit_name(cfg.fit));
  if (row == PCM_V_SCREEN) apply_screen(cfg.screen);
  else if (row == PCM_V_SIZE) { if (win && cfg.screen == PC_WINDOWED) pc_window_resize(win, cfg.win_w, cfg.win_h); }
  else if (row == PCM_V_VSYNC) { if (rd && !headless) vsync_on = pc_set_vsync(rd, cfg.vsync); }
  else if (row == PCM_V_ASPECT) { core_options_apply(); sync_options(); }
  cfg_save();
}
static void menu_pump(void) {
  int act, arg = 0;
  while (pcm_poll(&menu_ui, &act, &arg)) {
    switch (act) {
    case PCM_A_CLOSE: pcm_close(&menu_ui); break;
    case PCM_A_SAVE:
      if (state_save(arg) == PCS_OK) { cfg.state_slot = arg; states_refresh(); pcm_close(&menu_ui); }
      else pcm_note(&menu_ui, "COULD NOT WRITE THE STATE");
      break;
    case PCM_A_LOAD: {
      int r = state_load(arg);
      if (r == PCS_OK) { cfg.state_slot = arg; pcm_close(&menu_ui); }
      else pcm_note(&menu_ui, r == PCS_INCOMPAT ? "THIS SLOT BELONGS TO ANOTHER VERSION" : "COULD NOT READ THE STATE");
      break;
    }
    case PCM_A_VIDEO: apply_video(arg); break;
    case PCM_A_SLOTS: pcm_close(&menu_ui); open_picker(0); break;
    case PCM_A_LAUNCHER: back_to_launcher = 1; quit = 1; break;
    case PCM_A_QUIT: quit = 1; break;
    default: break;
    }
  }
}
static void menu_key(int k) {
  int before = menu_ui.page;
  pcm_key(&menu_ui, k);
  if (menu_ui.page != before && (menu_ui.page == PCM_SAVE || menu_ui.page == PCM_LOAD) && menu_ui.in_game) states_refresh();
  menu_pump();
}
static void menu_open_page(int page) {
  menu_ui.in_game = ux != NULL && wram != NULL;
  menu_ui.can_launcher = menu_ui.in_game && !script_mode;
  menu_ui.single_page = 0;
  menu_ui.credits = credits_text ? credits_text : "";
  if (menu_ui.in_game) { states_refresh(); srm_save(0); }
  pcm_open(&menu_ui, page);
}
static int pcm_key_from_sdl(SDL_Keycode k) {
  switch (k) {
  case SDLK_UP: return PCM_UP; case SDLK_DOWN: return PCM_DOWN; case SDLK_LEFT: return PCM_LEFT; case SDLK_RIGHT: return PCM_RIGHT;
  case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_z: case SDLK_SPACE: return PCM_ACCEPT;
  case SDLK_ESCAPE: case SDLK_x: case SDLK_BACKSPACE: return PCM_BACK;
  case SDLK_PAGEUP: case SDLK_q: return PCM_TAB_PREV; case SDLK_PAGEDOWN: case SDLK_w: return PCM_TAB_NEXT;
  default: return -1;
  }
}
static int pcm_key_from_pad(int b) {
  switch (b) {
  case SDL_CONTROLLER_BUTTON_A: return PCM_ACCEPT; case SDL_CONTROLLER_BUTTON_B: return PCM_BACK;
  case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return PCM_TAB_PREV; case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return PCM_TAB_NEXT;
  default: return -1;
  }
}
/* D-pad and left stick of the pads move the menu focus, with a repeat while held. */
static void menu_pad_nav(uint16_t pm, uint32_t now) {
  static const int bit[4] = {SOM_PAD_UP, SOM_PAD_DOWN, SOM_PAD_LEFT, SOM_PAD_RIGHT}, key[4] = {PCM_UP, PCM_DOWN, PCM_LEFT, PCM_RIGHT};
  static uint16_t prev;
  static uint32_t next_t[4];
  for (int i = 0; i < 4; i++) {
    int down = (pm >> bit[i]) & 1, was = (prev >> bit[i]) & 1;
    if (!menu_ui.open) continue;
    if (down && !was) { menu_key(key[i]); next_t[i] = now + 350; }
    else if (down && now >= next_t[i]) { menu_key(key[i]); next_t[i] = now + 90; }
  }
  prev = pm;
}

/* ---- runs: generated games, each with its own battery save and save states (they are keyed by the hash of the ROM) ---- */
static void gen_dir_path(char *out, size_t n) {
  const char *g = getenv("SOM_PC_GEN_DIR"), *h = getenv("HOME");
  (void)h;
  if (g && *g) snprintf(out, n, "%s", g);
  else snprintf(out, n, "%s/gen", state_dir);
}
static void runs_init(void) {
  char gd[700];
  DIR *d;
  struct dirent *de;
  snprintf(runs_path, sizeof runs_path, "%s/runs.json", state_dir);
  pr_load(runs_path, &runs);
  gen_dir_path(gd, sizeof gd);
  d = opendir(gd);
  if (!d) return;
  while ((de = readdir(d))) {                                       /* ROMs that som-dualscreen made before the list existed */
    size_t l = strlen(de->d_name);
    char sc[1000], rom[1000];
    struct pr_run r;
    struct stat sb;
    uint8_t *j;
    size_t jn;
    if (l < 10 || strcmp(de->d_name + l - 9, ".sfc.json")) continue;
    snprintf(sc, sizeof sc, "%s/%s", gd, de->d_name);
    snprintf(rom, sizeof rom, "%s/%.*s", gd, (int)(l - 5), de->d_name);
    if (stat(rom, &sb) != 0 || !(j = pc_slurp(sc, &jn))) continue;
    if (pr_from_sidecar((const char *)j, rom, &r) == 0 && pr_find(&runs, r.key) < 0) { r.created = (int64_t)sb.st_mtime; pr_add(&runs, &r); }
    free(j);
  }
  closedir(d);
  pr_sort(&runs);
}
static void runs_to_launcher(struct sl_launcher *L) {
  int shown = 0, total = 0;
  pr_sort(&runs);
  sl_saved_clear(L);
  for (int i = 0; i < runs.n; i++) {
    const struct pr_run *r = &runs.run[i];
    char date[16] = "";
    time_t t = (time_t)(r->last ? r->last : r->created);
    struct tm tmv;
    if (access(r->path, R_OK) != 0) continue;
    total++;
    if (shown >= SL_SAVED_MAX) continue;
    if (pc_localtime(&t, &tmv)) strftime(date, sizeof date, "%Y-%m-%d", &tmv);
    sl_saved_add(L, r->key, r->preset, r->mode, r->seed, r->diff, date, r->path, r->lines);
    shown++;
  }
  sl_saved_set_hidden(L, total - shown);
}
static void run_register(struct sl_launcher *L, const char *rom) {
  struct pr_run r;
  const char *dn = NULL;
  memset(&r, 0, sizeof r);
  sl_config_key(L, r.key);
  snprintf(r.preset, sizeof r.preset, "%s", L->cfg.preset >= 0 ? sl_presets[L->cfg.preset].id : "");
  snprintf(r.mode, sizeof r.mode, "%s", sl_modes[L->cfg.mode].key);
  snprintf(r.seed, sizeof r.seed, "%s", L->cfg.seed);
  if (sl_difficulty(L, &dn)) snprintf(r.diff, sizeof r.diff, "%s", dn);
  snprintf(r.path, sizeof r.path, "%s", rom);
  sl_config_lines(L, r.lines, sizeof r.lines);
  r.created = (int64_t)time(NULL);
  pr_add(&runs, &r);
  pr_sort(&runs);
  pr_save(runs_path, &runs);
}
static void run_played(const char *rom) {
  for (int i = 0; i < runs.n; i++) if (!strcmp(runs.run[i].path, rom)) { pr_played(&runs, runs.run[i].key, (int64_t)time(NULL)); pr_save(runs_path, &runs); return; }
}
static void move_to(const char *src, const char *dst_dir) {
  const char *b = strrchr(src, '/');
  char dst[1100];
  if (access(src, F_OK) != 0) return;
  mkdirs(dst_dir);
  snprintf(dst, sizeof dst, "%s/%s", dst_dir, b ? b + 1 : src);
  if (pc_rename(src, dst) != 0) fprintf(stderr, "som-pc: cannot move %s to %s\n", src, dst_dir);
}
/* "Delete" a run: its ROM, sidecar, battery save and save states are moved to <state>/trash/<time>-<key>/, never erased. */
static void run_delete(const char *key) {
  int i = pr_find(&runs, key);
  char tdir[900], p[1000], hk[20] = "";
  size_t n;
  uint8_t *b;
  if (i < 0) return;
  snprintf(tdir, sizeof tdir, "%s/trash/%ld-%s", state_dir, (long)time(NULL), key);
  if ((b = slurp(runs.run[i].path, &n))) {
    uint8_t h[32];
    size_t off = n % 1024 == 512 ? 512 : 0;
    sl_sha256(b + off, n - off, h);
    for (int k = 0; k < 6; k++) snprintf(hk + 2 * k, 3, "%02x", h[k]);
    free(b);
  }
  if (hk[0]) {
    snprintf(p, sizeof p, "%s/saves/%s.srm", state_dir, hk); move_to(p, tdir);
    snprintf(p, sizeof p, "%s/saves/%s.srm.bak", state_dir, hk); move_to(p, tdir);
    snprintf(p, sizeof p, "%s/states/%s", state_dir, hk); move_to(p, tdir);
  }
  snprintf(p, sizeof p, "%s.json", runs.run[i].path); move_to(p, tdir);
  move_to(runs.run[i].path, tdir);
  pr_remove(&runs, key);
  pr_save(runs_path, &runs);
}

/* ------------------------------------------------------------------ rendering */
static SDL_Texture *mk_tex(int w, int h, int streaming) {
  SDL_Texture *t = SDL_CreateTexture(rd, SDL_PIXELFORMAT_ARGB8888, streaming ? SDL_TEXTUREACCESS_STREAMING : SDL_TEXTUREACCESS_STATIC, w, h);
  if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  return t;
}
static void upload(SDL_Texture *t, const uint32_t *px, int stride, int w, int h) {
  SDL_Rect r = {0, 0, w, h};
  SDL_UpdateTexture(t, &r, px, stride * 4);
}
static int overlay_visible(void) {
  if (cfg.overlay == PC_OV_HIDDEN || modal_open) return 0;
  return ux->mode == SOM_UX_FIELD || ux->mode == SOM_UX_RING || ux->mode == SOM_UX_FLAMMIE || ux->mode == SOM_UX_SHOP;
}
static int ov_scale(void) {
  int s = (out_h + 270) / 540;
  if (s < 1) s = 1;
  return s + (cfg.overlay == PC_OV_MEDIUM ? 1 : 0);
}
static SDL_Rect ov_rect(void) {
  int s = ov_scale(), w, h, m = 12 * (out_h >= 900 ? 1 : 0) + 6;
  SDL_Rect r;
  som_ux_shoulders_size(s, cfg.names, &w, &h);
  r.w = w; r.h = h;
  r.y = (cfg.corner == PC_TOP_RIGHT) ? m : out_h - h - m;
  r.x = (cfg.corner == PC_BOTTOM_LEFT) ? m : cfg.corner == PC_BOTTOM_CENTER ? (out_w - w) / 2 : out_w - w - m;
  return r;
}

static SDL_Texture *tex_pcm;
static struct pc_fx fx;
static void draw_game(const SDL_Rect *dst) { pc_fx_draw(&fx, rd, tex_game, game_w, game_h, dst, cfg.shader, cfg.fit == PC_FIT_INTEGER); }

static void draw_menu_overlay(void) {
  static uint32_t *mbuf; static size_t mcap; static uint32_t last_rev; static int lw, lh;
  size_t need = (size_t)out_w * out_h;
  if (!menu_ui.open) return;
  if (need > mcap) { free(mbuf); mbuf = malloc(need * 4); mcap = need; last_rev = 0; }
  if (!tex_pcm || lw != out_w || lh != out_h) { if (tex_pcm) SDL_DestroyTexture(tex_pcm); tex_pcm = mk_tex(out_w, out_h, 1); lw = out_w; lh = out_h; last_rev = 0; }
  SDL_SetRenderDrawBlendMode(rd, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(rd, 0, 0, 0, 170);
  { SDL_Rect all = {0, 0, out_w, out_h}; SDL_RenderFillRect(rd, &all); }
  if (mbuf && tex_pcm) {
    if (last_rev != menu_ui.rev) { pcm_draw(&menu_ui, mbuf, out_w, out_w, out_h); upload(tex_pcm, mbuf, out_w, out_w, out_h); last_rev = menu_ui.rev; }
    SDL_SetTextureScaleMode(tex_pcm, SDL_ScaleModeNearest);
    SDL_RenderCopy(rd, tex_pcm, NULL, NULL);
  }
}

static void render_scene(void) {
  static uint32_t *ovbuf, *panelbuf; static size_t ovcap, pcap;
  double t0 = now_ms();
  SDL_Rect dst;
  struct pc_rect pr;
  int rw, rh;
  SDL_GetRendererOutputSize(rd, &rw, &rh);
  out_w = rw; out_h = rh;
  SDL_SetRenderDrawColor(rd, 0, 0, 0, 255);
  SDL_RenderClear(rd);
  if (game_have) {
    if (!tex_game) tex_game = mk_tex(640, 256, 1);
    upload(tex_game, game_px, 640, game_w, game_h);
    pc_fit_rects(out_w, out_h, game_w, game_h, cfg.wide, cfg.fit, &pr);
    dst.x = pr.x; dst.y = pr.y; dst.w = pr.w; dst.h = pr.h;
    draw_game(&dst);
  }
  if (overlay_visible()) {
    SDL_Rect r = ov_rect();
    size_t need = (size_t)r.w * r.h;
    if (need > ovcap) { free(ovbuf); ovbuf = malloc(need * 4); ovcap = need; }
    if (!tex_ov || r.w != 0) {
      static int tw, th;
      if (!tex_ov || tw != r.w || th != r.h) { if (tex_ov) SDL_DestroyTexture(tex_ov); tex_ov = mk_tex(r.w, r.h, 1); tw = r.w; th = r.h; }
    }
    som_ux_draw_shoulders(ux, ovbuf, r.w, r.w, r.h, ov_scale(), cfg.names);
    upload(tex_ov, ovbuf, r.w, r.w, r.h);
    SDL_SetTextureScaleMode(tex_ov, SDL_ScaleModeNearest);
    SDL_SetTextureAlphaMod(tex_ov, 235);
    SDL_RenderCopy(rd, tex_ov, NULL, &r);
    for (int i = 0; i < SOM_UX_SHOULDERS; i++) {                 /* hold-to-assign progress under the slot */
      if (progress[i] <= 0) continue;
      int s = ov_scale(), cx = r.x + i * (50 + 2) * s, cy = r.y + 24 * s;
      SDL_Rect bar = {cx + s, cy + 34 * s - 4 * s, (50 * s - 2 * s) * progress[i] / 100, 3 * s};
      SDL_SetRenderDrawColor(rd, 248, 224, 96, 255);
      SDL_RenderFillRect(rd, &bar);
    }
  }
  if (modal_open) {
    SDL_Rect p;
    panel_rect(out_w, out_h, &p);
    size_t need = (size_t)p.w * p.h;
    static int tw, th;
    if (need > pcap) { free(panelbuf); panelbuf = malloc(need * 4); pcap = need; }
    if (!tex_modal || tw != p.w || th != p.h) { if (tex_modal) SDL_DestroyTexture(tex_modal); tex_modal = mk_tex(p.w, p.h, 1); tw = p.w; th = p.h; }
    SDL_SetRenderDrawBlendMode(rd, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rd, 0, 0, 0, 170);
    SDL_Rect all = {0, 0, out_w, out_h};
    SDL_RenderFillRect(rd, &all);
    som_ux_draw(ux, NULL, panelbuf, p.w, p.w, p.h);
    upload(tex_modal, panelbuf, p.w, p.w, p.h);
    SDL_SetTextureScaleMode(tex_modal, SDL_ScaleModeNearest);
    SDL_RenderCopy(rd, tex_modal, NULL, &p);
    {                                                              /* gamepad / keyboard focus */
      struct som_ux_hitrect h[96]; SDL_Rect pr2;
      int n = picker_hits(h, 96, &pr2);
      for (int i = 0; i < n; i++)
        if (h[i].kind == focus_kind && h[i].a == focus_a) {
          SDL_Rect f = {p.x + h[i].x - 2, p.y + h[i].y - 2, h[i].w + 4, h[i].h + 4};
          SDL_SetRenderDrawColor(rd, 248, 224, 96, 255);
          SDL_RenderDrawRect(rd, &f);
          f.x++; f.y++; f.w -= 2; f.h -= 2;
          SDL_RenderDrawRect(rd, &f);
        }
    }
  }
  draw_menu_overlay();
  rend_ms += now_ms() - t0; rend_n++;
  if (now_ms() - t0 > rend_max) rend_max = now_ms() - t0;
}

/* ------------------------------------------------------------------ window */
static void apply_screen(int mode) {
  cfg.screen = mode;
  if (!win) return;
  cfg.screen = pc_window_set_mode(win, mode, cfg.excl_w, cfg.excl_h);
}
/* The windowed size to restore next time: only read while the window really is windowed. */
static void remember_window(void) {
  int w, h;
  if (!win || (SDL_GetWindowFlags(win) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) || (SDL_GetWindowFlags(win) & SDL_WINDOW_MINIMIZED)) return;
  SDL_GetWindowSize(win, &w, &h);
  if (w >= 640 && h >= 360 && w <= 7680 && h <= 4320) { cfg.win_w = w; cfg.win_h = h; }
}
static int video_init(void) {
  if (headless) {
    head_surf = SDL_CreateRGBSurfaceWithFormat(0, out_w, out_h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!head_surf) return -1;
    rd = SDL_CreateSoftwareRenderer(head_surf);
  } else {
    Uint32 f = SDL_WINDOW_RESIZABLE | (cfg.screen == PC_BORDERLESS ? SDL_WINDOW_FULLSCREEN_DESKTOP : cfg.screen == PC_EXCLUSIVE ? SDL_WINDOW_FULLSCREEN : 0);
    win = SDL_CreateWindow("Secret of Mana", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, cfg.win_w, cfg.win_h, f);
    if (!win) { fprintf(stderr, "som-pc: window: %s\n", SDL_GetError()); return -1; }
    rd = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | (cfg.vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
    if (!rd) rd = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE | (cfg.vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
    if (rd) { SDL_RendererInfo ri; SDL_GetRendererInfo(rd, &ri); vsync_on = (ri.flags & SDL_RENDERER_PRESENTVSYNC) != 0; fprintf(stderr, "som-pc: renderer %s%s, driver %s\n", ri.name, vsync_on ? " (vsync)" : "", SDL_GetCurrentVideoDriver()); }
    if (cfg.screen == PC_EXCLUSIVE) apply_screen(PC_EXCLUSIVE);
  }
  if (!rd) { fprintf(stderr, "som-pc: renderer: %s\n", SDL_GetError()); return -1; }
  SDL_RenderSetLogicalSize(rd, 0, 0);
  return 0;
}
static int save_bmp(const char *path) {
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, out_w, out_h, 32, SDL_PIXELFORMAT_ARGB8888);
  int r;
  if (!s) return -1;
  SDL_RenderReadPixels(rd, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
  r = SDL_SaveBMP(s, path);
  SDL_FreeSurface(s);
  return r;
}

/* ------------------------------------------------------------------ audio */
static int audio_open(void) { return pc_audio_open(&aud, audio_rate); }

/* ------------------------------------------------------------------ game start */
static int game_start(const char *path, const char *profile_name) {
  size_t n;
  uint8_t *b = slurp(path, &n);
  struct rinfo gi;
  int (*load)(const struct rinfo *);
  struct rav av;
  void (*getav)(struct rav *);
  size_t off = 0;
  if (!b) { fprintf(stderr, "som-pc: cannot read ROM %s\n", path); return -1; }
  if (n % 1024 == 512) off = 512;                                  /* copier header */
  rom_alloc = b; rom_bytes = b + off; rom_len = n - off;
  if (rom_len != 0x200000 && rom_len != 0x400000) { fprintf(stderr, "som-pc: %s is not a 2 or 4 MiB SoM image (%zu bytes)\n", path, rom_len); return -1; }
  snprintf(rom_path, sizeof rom_path, "%s", path);
  make_keys(profile_name);
  if (!core_h && core_open(cfg.core) != 0) return -1;
  *(void **)&load = PC_DLSYM(core_h, "retro_load_game");
  *(void **)&getav = PC_DLSYM(core_h, "retro_get_system_av_info");
  gi.path = path; gi.data = rom_bytes; gi.size = rom_len; gi.meta = NULL;
  if (!load(&gi)) { fprintf(stderr, "som-pc: the core rejected the ROM\n"); return -1; }
  wram = r_mem(2);
  if (!wram) { fprintf(stderr, "som-pc: the core exposes no WRAM\n"); return -1; }
  getav(&av);
  if (av.fps > 30 && av.fps < 130) core_fps = av.fps;
  if (av.rate > 8000) audio_rate = (int)(av.rate + 0.5);
  somd_init(&dir, rom_bytes, rom_len);
  ux = som_ux_new();
  if (cfg.pack[0] && !pack_blob) {
    size_t pn;
    pack_blob = slurp(cfg.pack, &pn);
    if (pack_blob) { pack_ptr = som_ux_pack_load(pack_blob, pn); if (!pack_ptr) fprintf(stderr, "som-pc: %s is not a UI pack: built-in art\n", cfg.pack); }
    else fprintf(stderr, "som-pc: no UI pack at %s: built-in art\n", cfg.pack);
  }
  if (pack_ptr) som_ux_set_pack(ux, pack_ptr);
  som_ux_set_rom(ux, rom_bytes, rom_len);
  som_ux_set_four_slots(ux, 1);
  pc_sh_init(&shm, cfg.fire_on_press);
  profile_load();
  sync_options();
  srm_load();
  som_ux_update(ux, wram);
  som_read_state(wram, &st);
  return 0;
}
static void game_end(void) {
  srm_save(1);
  if (ux) profile_save();
}
/* Leaves the game (back to the start menu): the core keeps running, only the ROM goes. */
static void game_unload(void) {
  if (r_unload) r_unload();
  wram = NULL;
  if (ux) { som_ux_delete(ux); ux = NULL; }
  free(rom_alloc); rom_alloc = NULL; rom_bytes = NULL; rom_len = 0;
  free(srm_last); srm_last = NULL; srm_last_n = 0;
  game_have = 0; defer_valid = 0; paused = 0; modal_open = 0; focus_kind = -1;
  memset(&st, 0, sizeof st);
  menu_ui.open = 0; menu_ui.act_pending = 0;
  srm_src[0] = 0; save_key[0] = 0;
}

/* ---- bundled data: next to the executable (release layout), then the source tree this binary was built from ---- */
static void base_dir(char *out, size_t n) {
  char *b = SDL_GetBasePath();
  snprintf(out, n, "%s", b ? b : "./");
  if (b) SDL_free(b);
}
/* First existing file among $SOM_PC_DATA/name, <exe dir>/data/name, <exe dir>/name and repo_rel in the source tree; 0 when none. */
static int res_find(const char *name, const char *repo_rel, char *out, size_t n) {
  char base[600], cand[4][760];
  const char *env = getenv("SOM_PC_DATA");
  base_dir(base, sizeof base);
  snprintf(cand[0], sizeof cand[0], "%s/%s", env && *env ? env : "/nonexistent", name);
  snprintf(cand[1], sizeof cand[1], "%sdata/%s", base, name);
  snprintf(cand[2], sizeof cand[2], "%s%s", base, name);
  snprintf(cand[3], sizeof cand[3], "%s/%s", SOM_PC_REPO, repo_rel);
  for (int i = 0; i < 4; i++) if (access(cand[i], R_OK) == 0) { snprintf(out, n, "%s", cand[i]); return 1; }
  return 0;
}

/* ------------------------------------------------------------------ gameplay tweaks (T8 / T11 options, applied to the ROM before it starts) */
static struct pg gpg;
static int gpg_ok;
static char gpg_path[700];
static int json_ok(const char *t) { return pc_json_complete(t); }
static void gameplay_init(void) {
  size_t n;
  char menu[800], *t;
  int src;
  snprintf(gpg_path, sizeof gpg_path, "%s/gameplay.json", cfg_dir);
  if (!res_find("gameplay_menu.json", "tools/gameplay/gameplay_menu.json", menu, sizeof menu)) return;
  logf_("gameplay menu: %s", menu);
  uint8_t *b = pc_slurp(menu, &n);
  if (!b) return;
  b = realloc(b, n + 1); b[n] = 0;
  gpg_ok = pg_load_menu(&gpg, (char *)b) == 0;
  free(b);
  if (!gpg_ok) return;
  t = pc_store_load(gpg_path, json_ok, &src);
  report_source("gameplay tweaks", gpg_path, src);
  if (t) { pg_load_values(&gpg, t); free(t); }
}
static void gameplay_save(void) {
  char buf[4096];
  if (!gpg_ok || script_mode) return;
  mkdirs(cfg_dir);
  if (pg_to_json(&gpg, buf, sizeof buf) > 0) pc_store_save(gpg_path, buf);
}
static uint32_t fnv(const char *s) { uint32_t h = 2166136261u; for (; *s; s++) { h ^= (uint8_t)*s; h *= 16777619u; } return h; }
/* Patches a copy of `in` with the tweaks in `o` (cached per ROM and option set).  0 ok, -1 failed (the caller plays the original). */
static int gameplay_apply(const char *in, const struct sgp_opts *o, char *out, size_t n) {
  char dir[760], base[200], key[160], msg[160], skipped[160];
  const char *b = strrchr(in, '/');
  snprintf(base, sizeof base, "%s", b ? b + 1 : in);
  if (strrchr(base, '.')) *strrchr(base, '.') = 0;
  snprintf(dir, sizeof dir, "%s/gameplay", state_dir);
  mkdirs(dir);
  key[0] = 0;
  for (int k = 0; k < SGP_COUNT; k++) snprintf(key + strlen(key), sizeof key - strlen(key), "%d,", (int)o->v[k]);
  snprintf(out, n, "%s/%.80s-%08x.sfc", dir, base, fnv(key) ^ fnv(in));
  if (access(out, R_OK) == 0) return 0;
  if (pgc_apply_file(in, out, o, msg, sizeof msg, skipped, sizeof skipped) != 0) { fprintf(stderr, "som-pc: gameplay tweaks: %s\n", msg); return -1; }
  if (skipped[0]) fprintf(stderr, "som-pc: gameplay tweaks skipped (their bytes conflict with this ROM): %s\n", skipped);
  return 0;
}

/* ------------------------------------------------------------------ start menu (T9) */
/* Generation runs in this process (libsomrando).  The progress callback keeps the window alive: it repaints the launcher and watches for
 * a cancel key, which ends the generator at its next step. */
struct genctx { struct sl_launcher *L; uint32_t *buf; int rw, rh; double last; int quit; };

static int gen_progress(void *user, int step, int total, const char *name) {
  struct genctx *c = user;
  SDL_Event e;
  int cancel = 0;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) { SDL_PushEvent(&e); c->quit = 1; cancel = 1; }
    else if (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_x || e.key.keysym.sym == SDLK_BACKSPACE)) cancel = 1;
    else if (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == SDL_CONTROLLER_BUTTON_B) cancel = 1;
  }
  sl_gen_progress(c->L, step, total, name);
  if (now_ms() - c->last >= 40 || step >= total) {
    c->last = now_ms();
    sl_tick(c->L);
    sl_draw(c->L, c->buf, c->rw, c->rw, c->rh);
    upload(tex_menu, c->buf, c->rw, c->rw, c->rh);
    SDL_RenderCopy(rd, tex_menu, NULL, NULL);
    draw_menu_overlay();
    SDL_RenderPresent(rd);
  }
  return cancel;
}
static void gen_run(struct genctx *c) {
  struct sl_launcher *L = c->L;
  struct pcg_result res;
  char key[17], gd[700], rom[820], side[840], why[160] = "";
  if (!cfg.rom[0] || access(cfg.rom, R_OK) != 0) { sl_gen_fail(L, "Pass the US v1.0 ROM with --rom first"); return; }
  if (!somrando_menu_supports(L, why, sizeof why)) { sl_gen_unavailable(L, why[0] ? why : "Not available in this build"); return; }
  sl_config_key(L, key);
  gen_dir_path(gd, sizeof gd);
  mkdirs(gd);
  snprintf(rom, sizeof rom, "%s/%s.sfc", gd, key);
  snprintf(side, sizeof side, "%s.json", rom);
  sl_gen_begin(L);
  pcg_generate(L, cfg.rom, rom, gen_progress, c, &res);
  if (res.rc == SOMRANDO_ERR_CANCELLED) { logf_("generation cancelled"); sl_show(L, SL_TWEAK); return; }
  if (res.rc == SOMRANDO_ERR_UNSUPPORTED || res.rc == SOMRANDO_ERR_UNAVAILABLE) { logf_("generation unavailable (%d): %s", res.rc, res.err); sl_gen_unavailable(L, res.err); return; }
  if (res.rc != SOMRANDO_OK) { logf_("generation failed (%d): %s", res.rc, res.err); sl_gen_fail(L, res.err[0] ? res.err : "Generation failed"); return; }
  logf_("generated %s sha256 %s in %.2f s", rom, res.sha256, res.seconds);
  if (pcg_write_sidecar(L, side, &res) != 0) fprintf(stderr, "som-pc: could not write %s\n", side);
  run_register(L, rom);
  sl_gen_done(L, rom);
}

/* The generator says which presets and which current setup it reproduces byte for byte. */
static void gen_support(struct sl_launcher *L, int *presets_done) {
  char why[SL_TEXT] = "";
  int ok;
  if (!somrando_available()) { sl_set_gen_support(L, 0, "No generator in this build"); return; }
  if (!*presets_done) {
    for (int p = 0; p < sl_npresets; p++) sl_set_preset_ok(L, p, somrando_menu_preset_supported(p, NULL, 0) == 1);
    *presets_done = 1;
  }
  ok = somrando_menu_supports(L, why, sizeof why);
  sl_set_gen_support(L, ok ? 1 : 0, ok ? "" : why);
}
static int menu_key_from_sdl(SDL_Keycode k) {
  switch (k) {
  case SDLK_UP: return SL_UP; case SDLK_DOWN: return SL_DOWN; case SDLK_LEFT: return SL_LEFT; case SDLK_RIGHT: return SL_RIGHT;
  case SDLK_RETURN: case SDLK_z: return SL_A; case SDLK_ESCAPE: case SDLK_x: case SDLK_BACKSPACE: return SL_B;
  case SDLK_s: return SL_X; case SDLK_a: return SL_Y; case SDLK_PAGEUP: return SL_L; case SDLK_PAGEDOWN: return SL_R; case SDLK_SPACE: return SL_START;
  default: return -1;
  }
}
static int gp_key_from_sdl(SDL_Keycode k) {
  switch (k) {
  case SDLK_UP: return PG_UP; case SDLK_DOWN: return PG_DOWN; case SDLK_LEFT: return PG_LEFT; case SDLK_RIGHT: return PG_RIGHT;
  case SDLK_PAGEUP: return PG_FAST_RIGHT; case SDLK_PAGEDOWN: return PG_FAST_LEFT; case SDLK_RETURN: case SDLK_z: return PG_ACCEPT;
  case SDLK_ESCAPE: case SDLK_x: case SDLK_BACKSPACE: return PG_BACK; case SDLK_DELETE: case SDLK_a: return PG_RESET; case SDLK_p: return PG_PRESET;
  default: return -1;
  }
}
static int gp_key_from_pad(int b) {
  switch (b) {
  case SDL_CONTROLLER_BUTTON_DPAD_UP: return PG_UP; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return PG_DOWN;
  case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return PG_LEFT; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return PG_RIGHT;
  case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return PG_FAST_LEFT; case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return PG_FAST_RIGHT;
  case SDL_CONTROLLER_BUTTON_A: return PG_ACCEPT; case SDL_CONTROLLER_BUTTON_B: case SDL_CONTROLLER_BUTTON_BACK: return PG_BACK;
  case SDL_CONTROLLER_BUTTON_X: return PG_RESET; case SDL_CONTROLLER_BUTTON_Y: return PG_PRESET;
  default: return -1;
  }
}
static int menu_key_from_pad(int b) {
  switch (b) {
  case SDL_CONTROLLER_BUTTON_DPAD_UP: return SL_UP; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return SL_DOWN;
  case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return SL_LEFT; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return SL_RIGHT;
  case SDL_CONTROLLER_BUTTON_A: return SL_A; case SDL_CONTROLLER_BUTTON_B: return SL_B; case SDL_CONTROLLER_BUTTON_X: return SL_X;
  case SDL_CONTROLLER_BUTTON_Y: return SL_Y; case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return SL_L; case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return SL_R;
  case SDL_CONTROLLER_BUTTON_START: return SL_START;
  default: return -1;
  }
}
/* Returns 1 with rom_to_play set when the player picked a ROM, 0 when the menu was left. */
static int run_menu_screen(char *rom_to_play, size_t n) {
  struct sl_launcher L;
  uint32_t *buf = NULL;
  int bw = 0, bh = 0;
  char st_orig[96], st_gen[96];
  int menu_shot = 0, in_gp = 0, presets_done = 0;
  uint32_t sup_rev = 0;
  struct genctx gc;
  sl_init(&L, (uint64_t)SDL_GetTicks() * 2654435761u + 1);
  snprintf(st_orig, sizeof st_orig, "%s", cfg.rom[0] && access(cfg.rom, R_OK) == 0 ? "" : "Pass your ROM with --rom");      /* only what needs attention */
  snprintf(st_gen, sizeof st_gen, "%s", cfg.rom[0] && access(cfg.rom, R_OK) == 0 ? "" : "Needs the US ROM (--rom)");
  logf_("menu: rom '%s' accessible=%d", cfg.rom, cfg.rom[0] && access(cfg.rom, R_OK) == 0);
  sl_set_status(&L, st_orig, st_gen);
  memset(&gc, 0, sizeof gc);
  gc.L = &L;
  L.rev++;
  sl_set_extras(&L, (gpg_ok ? SL_X_TWEAKS : 0) | SL_X_DISPLAY | SL_X_CREDITS, credits_text);
  sl_set_tweaks_status(&L, gpg_ok && !pg_is_default(&gpg) ? "ACTIVE" : "");
  runs_to_launcher(&L);
  if (cfg.pack[0]) { size_t pn; uint8_t *pb = slurp(cfg.pack, &pn); if (pb) sl_draw_load_pack(pb, pn); }
  for (;;) {
    SDL_Event e;
    int rw, rh, act;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) return 0;
      if (menu_ui.open) {                                           /* the display settings over the start menu */
        if (e.type == SDL_KEYDOWN) {
          int mk = pcm_key_from_sdl(e.key.keysym.sym);
          if (e.key.keysym.sym == SDLK_F11) apply_screen(pc_screen_toggle(cfg.screen, 0));
          else if (e.key.keysym.sym == SDLK_F12) menu_shot = 1;
          else if (e.key.keysym.sym == SDLK_F10) pcm_close(&menu_ui);
          else if (mk >= 0) menu_key(mk);
        } else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
          int mk = pcm_key_from_pad(e.cbutton.button);
          if (e.cbutton.button == SDL_CONTROLLER_BUTTON_START) pcm_close(&menu_ui);
          else if (mk >= 0) menu_key(mk);
        } else if (e.type == SDL_MOUSEWHEEL) pcm_scroll(&menu_ui, -e.wheel.y);
        else pads_event(&e);
        continue;
      }
      if (e.type == SDL_KEYDOWN && !e.key.repeat) {
        int k = menu_key_from_sdl(e.key.keysym.sym);
        if (e.key.keysym.sym == SDLK_F11) apply_screen(pc_screen_toggle(cfg.screen, 0));
        if (e.key.keysym.sym == SDLK_F12) menu_shot = 1;
        if (in_gp) {
          int pk = gp_key_from_sdl(e.key.keysym.sym);
          if (pk >= 0) pg_key(&gpg, pk);
        } else if (e.key.keysym.sym == SDLK_g && gpg_ok && L.gen != SL_GEN_RUNNING) in_gp = 1;
        else if (k >= 0) sl_key(&L, k);
      }
      if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        if (in_gp) { int pk = gp_key_from_pad(e.cbutton.button); if (pk >= 0) pg_key(&gpg, pk); }
        else if (e.cbutton.button == SDL_CONTROLLER_BUTTON_BACK && gpg_ok && L.gen != SL_GEN_RUNNING) in_gp = 1;
        else { int k = menu_key_from_pad(e.cbutton.button); if (k >= 0) sl_key(&L, k); }
      }
      if (in_gp && e.type == SDL_MOUSEWHEEL) pg_key(&gpg, e.wheel.y > 0 ? PG_UP : PG_DOWN);
      if (in_gp && gpg.done) { gpg.done = 0; in_gp = 0; gameplay_save(); sl_set_tweaks_status(&L, !pg_is_default(&gpg) ? "ACTIVE" : ""); }
      if (in_gp) continue;
      pads_event(&e);
      if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        int sx, sy, ww, wh;
        SDL_GetRendererOutputSize(rd, &rw, &rh);
        SDL_GetWindowSize(win, &ww, &wh);
        sx = e.button.x * rw / (ww ? ww : 1); sy = e.button.y * rh / (wh ? wh : 1);
        sl_tap(&L, sx, sy, rw, rh);
      }
      if (e.type == SDL_MOUSEWHEEL) sl_scroll(&L, -e.wheel.y);
    }
    SDL_GetRendererOutputSize(rd, &rw, &rh);
    out_w = rw; out_h = rh;
    { uint8_t shx = 0; uint16_t pm = pc_pads_read(&gp, &shx); menu_pad_nav(menu_ui.open ? pm : 0, SDL_GetTicks()); }
    if (rw != bw || rh != bh) { free(buf); buf = malloc((size_t)rw * rh * 4); bw = rw; bh = rh; if (tex_menu) { SDL_DestroyTexture(tex_menu); tex_menu = NULL; } sl_set_view(&L, rw, rh); }
    if (!tex_menu) tex_menu = mk_tex(rw, rh, 1);
    gc.buf = buf; gc.rw = rw; gc.rh = rh;
    sl_tick(&L);
    if (L.rev != sup_rev && L.gen != SL_GEN_RUNNING) { gen_support(&L, &presets_done); sup_rev = L.rev; }
    while (sl_poll(&L, &act)) {
      logf_("menu action %d", act);
      if (act == SL_ACT_QUIT) return 0;
      if (act == SL_ACT_TWEAKS) { if (gpg_ok && L.gen != SL_GEN_RUNNING) in_gp = 1; }
      else if (act == SL_ACT_DISPLAY) { menu_ui.in_game = 0; menu_ui.can_launcher = 0; menu_ui.single_page = 1; menu_ui.credits = credits_text; pcm_open(&menu_ui, PCM_VIDEO); }
      else if (act == SL_ACT_DELETE_SAVED) { int sl = sl_act_arg(&L); if (sl >= 0 && sl < SL_SAVED_MAX && L.saved[sl].used) { char key[17]; snprintf(key, sizeof key, "%s", L.saved[sl].key); run_delete(key); } runs_to_launcher(&L); }
      else if (act == SL_ACT_PLAY_ORIGINAL) {
        if (cfg.rom[0] && access(cfg.rom, R_OK) == 0) { snprintf(rom_to_play, n, "%s", cfg.rom); return 1; }
        snprintf(L.notice, sizeof L.notice, "Pass your ROM with --rom"); L.notice_t = 180;
      } else if (act == SL_ACT_GENERATE) {
        char key[17], p[900], gd[700];
        sl_config_key(&L, key);
        gen_dir_path(gd, sizeof gd);
        snprintf(p, sizeof p, "%s/%s.sfc", gd, key);
        if (access(p, R_OK) == 0 && pcg_looks_generated(p)) { run_register(&L, p); sl_gen_cached(&L, p); }
        else { gen_run(&gc); if (gc.quit) return 0; }
      } else if (act == SL_ACT_PLAY) { snprintf(rom_to_play, n, "%s", sl_rom_path(&L)); run_played(rom_to_play); return 1; }
      else if (act == SL_ACT_CANCEL) { /* generation is synchronous: a cancel arrives through gen_progress */ }
    }
    sl_draw(&L, buf, rw, rw, rh);
    if (in_gp) pg_draw(&gpg, buf, rw, rw, rh, NULL);
    upload(tex_menu, buf, rw, rw, rh);
    SDL_RenderCopy(rd, tex_menu, NULL, NULL);
    draw_menu_overlay();
    if (menu_shot) {
      char p[800]; static int n;
      snprintf(p, sizeof p, "%s/shots", state_dir); mkdirs(p);
      snprintf(p, sizeof p, "%s/shots/menu-%d.bmp", state_dir, n++);
      save_bmp(p); fprintf(stderr, "som-pc: screenshot %s (%s)\n", p, sl_screen_name(L.screen)); menu_shot = 0;
    }
    SDL_RenderPresent(rd);
    if (!vsync_on) SDL_Delay(16);
  }
}

/* ------------------------------------------------------------------ interactive loop */
static uint16_t keyboard_pad(const Uint8 *k) {
  uint16_t m = 0;
  if (k[SDL_SCANCODE_Z]) m |= 1u << SOM_PAD_B;
  if (k[SDL_SCANCODE_X]) m |= 1u << SOM_PAD_A;
  if (k[SDL_SCANCODE_A]) m |= 1u << SOM_PAD_Y;
  if (k[SDL_SCANCODE_S]) m |= 1u << SOM_PAD_X;
  if (k[SDL_SCANCODE_RETURN]) m |= 1u << SOM_PAD_START;
  if (k[SDL_SCANCODE_BACKSPACE] || k[SDL_SCANCODE_RSHIFT]) m |= 1u << SOM_PAD_SELECT;
  if (k[SDL_SCANCODE_UP]) m |= 1u << SOM_PAD_UP;
  if (k[SDL_SCANCODE_DOWN]) m |= 1u << SOM_PAD_DOWN;
  if (k[SDL_SCANCODE_LEFT]) m |= 1u << SOM_PAD_LEFT;
  if (k[SDL_SCANCODE_RIGHT]) m |= 1u << SOM_PAD_RIGHT;
  return m;
}
static void report_stats(const char *tag, double wall_s, unsigned long frames) {
  fprintf(stderr, "{\"stats\":\"%s\",\"frames\":%lu,\"loops\":%lu,\"wall_s\":%.3f,\"fps\":%.1f,\"loop_hz\":%.1f,\"emu_ms_avg\":%.3f,\"emu_ms_max\":%.3f,\"render_ms_avg\":%.3f,\"render_ms_max\":%.3f,"
          "\"out\":\"%dx%d\",\"wide\":%d,\"fit\":\"%s\",\"frame\":\"%dx%d\"}\n",
          tag, frames, loops_n, wall_s, wall_s > 0 ? frames / wall_s : 0.0, wall_s > 0 ? loops_n / wall_s : 0.0, emu_n ? emu_ms / emu_n : 0.0, emu_max, rend_n ? rend_ms / rend_n : 0.0, rend_max, out_w, out_h,
          cfg.wide, pc_fit_name(cfg.fit), game_w, game_h);
}

static void help_overlay_text(void) {
  fprintf(stderr,
    "controls: arrows Z=B X=A A=Y S=X Enter=Start Backspace=Select | slots Q=L1 W=R1 E=L2 R=R2 (hold 0.7 s: assign) | F1 options  F2 assign  F3 fit\n"
    "          F4 wide  F5 overlay size  F6 HUD  F7 save state  F8 load state  F9 mute  F11 fullscreen  F12 screenshot  P pause  Esc quit/back\n");
}

/* ---- autopilot (tests): a timeline of virtual game controller input and screenshots driven by the real clock ---- */
struct ap_ev { double t; char kind[12]; char a[512]; int v; int done; };
static struct ap_ev ap[256]; static int ap_n;
static SDL_Joystick *vjoy;
static int ap_load(const char *path) {
  FILE *f = fopen(path, "r");
  char line[640];
  if (!f) return -1;
  while (fgets(line, sizeof line, f) && ap_n < 256) {
    double t; char kind[12], a[512] = "";
    int v = 0;
    if (line[0] != '@' || sscanf(line + 1, "%lf %11s %511s %d", &t, kind, a, &v) < 2) continue;
    ap[ap_n].t = t; snprintf(ap[ap_n].kind, sizeof ap[ap_n].kind, "%s", kind); snprintf(ap[ap_n].a, sizeof ap[ap_n].a, "%s", a); ap[ap_n].v = v; ap[ap_n].done = 0; ap_n++;
  }
  fclose(f);
  vjoy = NULL;
  {
    int idx = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
    if (idx >= 0) { vjoy = SDL_JoystickOpen(idx); if (SDL_IsGameController(idx)) { SDL_Event ev; SDL_zero(ev); ev.type = SDL_CONTROLLERDEVICEADDED; ev.cdevice.which = idx; pads_event(&ev); } }
    fprintf(stderr, "som-pc: autopilot with %d events, virtual pad %s (index %d, game controller %d, mapping %s)\n", ap_n, vjoy ? "attached" : "unavailable", idx,
            idx >= 0 ? SDL_IsGameController(idx) : 0, idx >= 0 && SDL_GameControllerMappingForDeviceIndex(idx) ? "yes" : "none");
  }
  return 0;
}
static int ap_button(const char *n) {
  static const char *const N[] = {"A", "B", "X", "Y", "BACK", "GUIDE", "START", "LEFTSTICK", "RIGHTSTICK", "LEFTSHOULDER", "RIGHTSHOULDER", "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT"};
  for (int i = 0; i < 15; i++) if (!strcmp(N[i], n)) return i;
  return -1;
}
static int ap_axis(const char *n) {
  static const char *const N[] = {"LEFTX", "LEFTY", "RIGHTX", "RIGHTY", "TRIGGERLEFT", "TRIGGERRIGHT"};
  for (int i = 0; i < 6; i++) if (!strcmp(N[i], n)) return i;
  return -1;
}
static void jprint_state(const char *tag);
static void ap_run(double t_ms) {
  for (int i = 0; i < ap_n; i++) {
    struct ap_ev *e = &ap[i];
    if (e->done || t_ms < e->t) continue;
    e->done = 1;
    if (!strcmp(e->kind, "pad") && vjoy) { int b = ap_button(e->a); if (b >= 0) SDL_JoystickSetVirtualButton(vjoy, b, e->v); }
    else if (!strcmp(e->kind, "axis") && vjoy) { int x = ap_axis(e->a); if (x >= 0) SDL_JoystickSetVirtualAxis(vjoy, x, (Sint16)e->v); }
    else if (!strcmp(e->kind, "shot")) { render_scene(); SDL_RenderPresent(rd); render_scene(); if (save_bmp(e->a) == 0) printf("{\"event\":\"shot\",\"path\":\"%s\",\"out\":\"%dx%d\",\"frame\":\"%dx%d\"}\n", e->a, out_w, out_h, game_w, game_h); }
    else if (!strcmp(e->kind, "state")) jprint_state(e->a);
    else if (!strcmp(e->kind, "set")) { if (!strcmp(e->a, "fit")) cfg.fit = e->v; else if (!strcmp(e->a, "wide")) { cfg.wide = e->v; core_options_apply(); sync_options(); } else if (!strcmp(e->a, "screen")) apply_screen(e->v); }
    else if (!strcmp(e->kind, "sram")) { size_t cap = r_mem_size(0); uint8_t *m = r_mem(0); for (size_t i = 0; m && e->a[2 * i] && e->a[2 * i + 1] && (size_t)e->v + i < cap; i++) { unsigned bv; sscanf(e->a + 2 * i, "%2x", &bv); m[(size_t)e->v + i] = (uint8_t)bv; } }
    else if (!strcmp(e->kind, "quit")) quit = 1;
    fflush(stdout);
  }
}

static void main_loop(const char *state_file) {
  double t_start = now_ms(), carry = 0, last = now_ms(), srm_t = now_ms(), health_t = now_ms();
  int resumed = 0;
  unsigned long f0 = frame_no;
  if (state_file && *state_file) {
    size_t sn; uint8_t *b = slurp(state_file, &sn);
    if (b && r_unser(b, sn)) fprintf(stderr, "som-pc: state %s loaded\n", state_file);
    else fprintf(stderr, "som-pc: cannot load state %s\n", state_file);
    free(b);
  }
  help_overlay_text();
  while (!quit) {
    SDL_Event e;
    uint32_t tms = SDL_GetTicks();
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
      case SDL_QUIT: quit = 1; break;
      case SDL_CONTROLLERDEVICEADDED: case SDL_CONTROLLERDEVICEREMOVED: case SDL_JOYDEVICEADDED: pads_event(&e); break;
      case SDL_AUDIODEVICEREMOVED: if (!e.adevice.iscapture) pc_audio_health(&aud, now_ms()); break;
      case SDL_MOUSEWHEEL:
        if (menu_ui.open) pcm_scroll(&menu_ui, -e.wheel.y);
        break;
      case SDL_KEYDOWN:
        if (menu_ui.open && e.key.keysym.sym != SDLK_F11 && e.key.keysym.sym != SDLK_F12) {
          int mk = e.key.keysym.sym == SDLK_F10 ? PCM_BACK : pcm_key_from_sdl(e.key.keysym.sym);
          if (e.key.keysym.sym == SDLK_F10) { if (menu_ui.page == PCM_ROOT) pcm_close(&menu_ui); else menu_key(mk); }
          else if (mk >= 0) menu_key(mk);
          break;
        }
        if (e.key.repeat) break;
        switch (e.key.keysym.sym) {
        case SDLK_ESCAPE: if (modal_open) picker_back(); else menu_open_page(PCM_ROOT); break;
        case SDLK_F10: if (!modal_open) menu_open_page(PCM_ROOT); break;
        case SDLK_F11: apply_screen(pc_screen_toggle(cfg.screen, (e.key.keysym.mod & KMOD_SHIFT) != 0)); break;
        case SDLK_F1: if (modal_open) picker_back(); else open_options(); break;
        case SDLK_F2: if (!modal_open) open_picker(0); break;
        case SDLK_F3:
          if (e.key.keysym.mod & KMOD_SHIFT) { cfg.shader = pc_shader_next(cfg.shader, 1); cfg_save(); }
          else { cfg.fit = (cfg.fit + 1) % PC_FIT_COUNT; sync_options(); }
          break;
        case SDLK_F4: cfg.wide = pc_next_wide(cfg.wide); core_options_apply(); sync_options(); break;
        case SDLK_F5: cfg.overlay = (cfg.overlay + 1) % PC_OV_COUNT; break;
        case SDLK_F6: cfg.hud_hide = !cfg.hud_hide; core_options_apply(); sync_options(); break;
        case SDLK_F9: user_mute = !user_mute; break;
        case SDLK_p: if (!modal_open) paused = !paused; break;
        case SDLK_F7: if (!modal_open) quick_save(); break;
        case SDLK_F8: if (!modal_open) quick_load(); break;
        case SDLK_LEFTBRACKET: if (!modal_open) slot_set(cfg.state_slot - 1); break;
        case SDLK_RIGHTBRACKET: if (!modal_open) slot_set(cfg.state_slot + 1); break;
        case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5: case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
          if (!modal_open) slot_set(e.key.keysym.sym - SDLK_1);
          break;
        case SDLK_0: if (!modal_open) slot_set(9); break;
        case SDLK_F12: {
          char p[800];
          snprintf(p, sizeof p, "%s/shots", state_dir); mkdirs(p);
          snprintf(p, sizeof p, "%s/shots/som-%lu.bmp", state_dir, frame_no);
          render_scene(); save_bmp(p); fprintf(stderr, "som-pc: screenshot %s\n", p);
          break;
        }
        default:
          if (modal_open) {
            switch (e.key.keysym.sym) {
            case SDLK_UP: picker_move(0, -1); break; case SDLK_DOWN: picker_move(0, 1); break;
            case SDLK_LEFT: picker_move(-1, 0); break; case SDLK_RIGHT: picker_move(1, 0); break;
            case SDLK_RETURN: case SDLK_z: picker_confirm(); break;
            case SDLK_x: case SDLK_BACKSPACE: picker_back(); break;
            case SDLK_PAGEUP: picker_tab(-1); break; case SDLK_PAGEDOWN: picker_tab(1); break;
            default: break;
            }
          }
          break;
        }
        break;
      case SDL_CONTROLLERBUTTONDOWN:
        if (menu_ui.open) {
          int mk = pcm_key_from_pad(e.cbutton.button);
          if (e.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE || e.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSTICK || e.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
            if (menu_ui.page == PCM_ROOT || menu_ui.single_page) pcm_close(&menu_ui); else menu_key(PCM_BACK);
          } else if (mk >= 0) menu_key(mk);
          break;
        }
        if (!modal_open && ux) {                                       /* Select held: R1 quick save, L1 quick load, D-pad left / right changes the slot */
          SDL_GameController *gc = SDL_GameControllerFromInstanceID(e.cbutton.which);
          if (gc && SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_BACK)) {
            int b = e.cbutton.button, used = 1;
            if (b == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) quick_save();
            else if (b == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) quick_load();
            else if (b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) slot_set(cfg.state_slot + 1);
            else if (b == SDL_CONTROLLER_BUTTON_DPAD_LEFT) slot_set(cfg.state_slot - 1);
            else used = 0;
            if (used) { combo_active = 1; break; }
          }
        }
        if (e.cbutton.button == SDL_CONTROLLER_BUTTON_RIGHTSTICK && !modal_open) open_picker(0);
        else if (e.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSTICK && !modal_open) menu_open_page(PCM_ROOT);
        else if (e.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) { if (!modal_open) menu_open_page(PCM_ROOT); else picker_back(); }
        else if (modal_open) {
          switch (e.cbutton.button) {
          case SDL_CONTROLLER_BUTTON_DPAD_UP: picker_move(0, -1); break; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: picker_move(0, 1); break;
          case SDL_CONTROLLER_BUTTON_DPAD_LEFT: picker_move(-1, 0); break; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: picker_move(1, 0); break;
          case SDL_CONTROLLER_BUTTON_A: picker_confirm(); break;
          case SDL_CONTROLLER_BUTTON_B: picker_back(); break;
          case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: picker_tab(-1); break;
          case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: picker_tab(1); break;
          default: break;
          }
        }
        break;
      case SDL_MOUSEBUTTONDOWN: {
        int rw, rh, ww, wh, x, y;
        SDL_GetRendererOutputSize(rd, &rw, &rh); SDL_GetWindowSize(win, &ww, &wh);
        x = e.button.x * rw / (ww ? ww : 1); y = e.button.y * rh / (wh ? wh : 1);
        if (modal_open) {
          SDL_Rect pr; panel_rect(rw, rh, &pr);
          if (x >= pr.x && y >= pr.y && x < pr.x + pr.w && y < pr.y + pr.h) {
            if (e.button.button == SDL_BUTTON_LEFT) picker_tap(x - pr.x, y - pr.y, &pr);
          } else if (e.button.button == SDL_BUTTON_RIGHT) picker_back();
        } else if (overlay_visible()) {
          SDL_Rect r = ov_rect();
          int s = ov_scale();
          if (x >= r.x && y >= r.y + 24 * s && x < r.x + r.w && y < r.y + r.h)
            for (int i = 0; i < 4; i++) {
              int cx = r.x + i * 52 * s;
              if (x >= cx && x < cx + 50 * s) { if (e.button.button == SDL_BUTTON_RIGHT) open_picker(i); else shoulder_fire(i); }
            }
        }
        break;
      }
      default: break;
      }
    }
    if (ap_n) ap_run(now_ms() - t_start);
    /* held inputs */
    {
      const Uint8 *k = SDL_GetKeyboardState(NULL);
      uint8_t sh = 0;
      uint16_t pmask = pc_pads_read(&gp, &sh), m = keyboard_pad(k) | pmask;
      if (k[SDL_SCANCODE_Q]) sh |= 1;
      if (k[SDL_SCANCODE_W]) sh |= 2;
      if (k[SDL_SCANCODE_E]) sh |= 4;
      if (k[SDL_SCANCODE_R]) sh |= 8;
      if (!(m & (1u << SOM_PAD_SELECT))) combo_active = 0;
      if (combo_active) { m &= (uint16_t)~(1u << SOM_PAD_SELECT); sh &= (uint8_t)~3u; }   /* the Select chord does not reach the game */
      menu_pad_nav(pmask, tms);
      sh_held = sh;
      pad_game = modal_open || defer_valid || menu_ui.open ? 0 : m;
      shoulders_update(sh_held, tms);
    }
    modal_open = modal_wanted();
    if (modal_open && menu_ui.open) pcm_close(&menu_ui);
    if (!modal_open) { focus_kind = -1; }
    else if (focus_kind < 0) {
      struct som_ux_hitrect h[96]; SDL_Rect pr;
      int n = picker_hits(h, 96, &pr);
      for (int i = 0; i < n && focus_kind < 0; i++) if (h[i].enabled && (h[i].kind == SOM_UX_HIT_QE_CELL || h[i].kind == SOM_UX_HIT_POLICY || h[i].kind == SOM_UX_HIT_T_ENEMY || h[i].kind == SOM_UX_HIT_T_ALLY || h[i].kind == SOM_UX_HIT_OPTION)) { focus_kind = h[i].kind; focus_a = h[i].a; }
    }
    /* how many emulated frames this iteration */
    int n = 0;
    if (!paused && !modal_open && !menu_ui.open) {
      double nowt = now_ms();
      if (adev) {
        n = pc_audio_frames(&aud, (double)audio_rate / core_fps * 4.0, loops_n > 30 && !resumed, nowt);
        resumed = 0;
        if (n == 0) SDL_Delay(1);
      } else {
        carry += nowt - last;
        while (carry >= 1000.0 / core_fps && n < 4) { n++; carry -= 1000.0 / core_fps; }
        if (carry > 100) carry = 0;
        if (n == 0) SDL_Delay(1);
      }
      last = nowt;
    } else { last = now_ms(); resumed = 1; if (modal_open) { som_ux_update(ux, wram); } SDL_Delay(8); }
    for (int i = 0; i < n; i++) step();
    if (modal_open && !(somd_hide_video(&dir))) { /* paused behind the picker: keep the UX flashes and notices ticking */ }
    render_scene();
    SDL_RenderPresent(rd);
    loops_n++;
    if (now_ms() - srm_t > 2000) { srm_save(0); srm_t = now_ms(); }
    if (now_ms() - health_t > 1000) { health_t = now_ms(); pc_audio_health(&aud, health_t); }
  }
  report_stats("interactive", (now_ms() - t_start) / 1000.0, frame_no - f0);
}

/* ------------------------------------------------------------------ scripted session (tests, no input devices) */
static int hitname(const char *s) {
  static const char *const N[] = {"NONE", "PARTY", "TAB", "CELL", "T_ALLY", "T_ENEMY", "T_ALL_ALLIES", "T_ALL_ENEMIES", "CANCEL", "PAD", "LETTER", "NEXT", "KEYROW",
    "KEYS_SCROLL", "BIND_CANCEL", "BIND_CLEAR", "OPT", "OPTION", "OPT_NAV", "AVATAR", "QUICK", "BIND_ENTRY", "BIND_MENU", "QE_TAB", "QE_CELL", "QE_PAGE", "QE_CLEAR",
    "QE_KEY", "MAPVIEW", "POLICY", "POL_CANCEL", "QE_BACK", "REVIVE", "REV_CANCEL"};
  for (size_t i = 0; i < sizeof N / sizeof *N; i++) if (!strcmp(N[i], s)) return (int)i;
  return -1;
}
static uint16_t parse_buttons(const char *s) {
  static const char *const nm[12] = {"B", "Y", "SELECT", "START", "UP", "DOWN", "LEFT", "RIGHT", "A", "X", "L", "R"};
  char t[200]; uint16_t m = 0;
  snprintf(t, sizeof t, "%s", s);
  for (char *p = strtok(t, ","); p; p = strtok(NULL, ",")) for (int i = 0; i < 12; i++) if (!strcmp(p, nm[i])) m |= 1u << i;
  return m;
}
static int slot_of(const char *s) { return !strcmp(s, "L1") ? 0 : !strcmp(s, "R1") ? 1 : !strcmp(s, "L2") ? 2 : !strcmp(s, "R2") ? 3 : -1; }
static uint32_t virt_ms(void) { return (uint32_t)(frame_no * 1000 / 60); }
static void script_frames(int n, uint16_t pad, int render_each) {
  while (n-- > 0) {
    modal_open = modal_wanted();
    pad_game = (modal_open || defer_valid) ? 0 : pad;
    shoulders_update(sh_script, virt_ms());
    modal_open = modal_wanted();
    if (!modal_open && !menu_ui.open) step(); else { som_ux_update(ux, wram); frame_no++; }
    if (render_each) { render_scene(); SDL_RenderPresent(rd); }
  }
}
static void jprint_state(const char *tag) {
  struct somd_queue_info q1, q2;
  int s1 = somd_queue_state(&dir, 1, &q1), s2 = somd_queue_state(&dir, 2, &q2);
  printf("{\"tag\":\"%s\",\"frame\":%lu,\"mode\":\"%s\",\"safe\":%d,\"map\":%u,\"modal\":%d,\"frame_w\":%d,\"party\":[", tag, frame_no, som_mode_name(st.mode), st.safe_to_inject, st.map_id, modal_open, game_w);
  for (int i = 0; i < SOM_PARTY; i++)
    printf("%s{\"present\":%d,\"hp\":%d,\"hp_max\":%d,\"mp\":%d,\"mp_max\":%d,\"weapon\":%d,\"status\":%d}", i ? "," : "", st.hero[i].present, st.hero[i].hp, st.hero[i].hp_max, st.hero[i].mp, st.hero[i].mp_max, st.hero[i].weapon_type, st.hero[i].status);
  printf("],\"items\":[");
  for (int i = 0; i < SOM_ITEM_RECORDS; i++) printf("%s[%d,%d]", i ? "," : "", st.item[i].id, st.item[i].qty);
  printf("],\"queue\":[%d,%d],\"slots\":[", s1, s2);
  for (int i = 0; i < SOM_UX_SHOULDERS; i++) { int k = 0, id = 0; som_ux_quick_get(ux, i, &k, &id); printf("%s[%d,%d,%d]", i ? "," : "", k, id, som_ux_quick_policy(ux, i)); }
  printf("],\"focus\":[%d,%d],\"menu\":[%d,%d,%d],\"slot\":%d,\"direct\":{\"state\":%d,\"err\":\"%s\"}}\n", focus_kind, focus_a, menu_ui.open, menu_ui.page, menu_ui.cursor[menu_ui.page], cfg.state_slot + 1, dir.state, somd_err_name(dir.err));
  fflush(stdout);
}
static int run_script(const char *path) {
  FILE *f = fopen(path, "r");
  char line[1024];
  double bench_t0 = 0;
  if (!f) { fprintf(stderr, "som-pc: cannot open script %s\n", path); return 2; }
  while (fgets(line, sizeof line, f)) {
    char w[32] = "", a[512] = "", b[64] = "", c[64] = "", d[64] = "";
    unsigned v[6] = {0, 0, 0, 0, 0, 0};
    if (sscanf(line, "%31s", w) != 1 || w[0] == '#') continue;
    const char *p = line + strlen(w);
    if (!strcmp(w, "LOAD")) {
      size_t sn; uint8_t *bb;
      sscanf(p, "%511s", a);
      bb = slurp(a, &sn);
      if (!bb || !r_unser(bb, sn)) { printf("{\"event\":\"load-failed\",\"path\":\"%s\"}\n", a); return 3; }
      free(bb);
      script_frames(2, 0, 0);
      printf("{\"event\":\"loaded\",\"path\":\"%s\",\"frame_w\":%d,\"frame_h\":%d}\n", a, game_w, game_h);
    } else if (!strcmp(w, "RUN")) { sscanf(p, "%u", &v[0]); script_frames((int)v[0], 0, 0); }
    else if (!strcmp(w, "PAD")) { sscanf(p, "%511s %u", a, &v[0]); script_frames((int)v[0], parse_buttons(a), 0); }
    else if (!strcmp(w, "SET")) {
      sscanf(p, "%63s %63s", b, c);
      if (!strcmp(b, "wide")) { cfg.wide = !strcmp(c, "off") ? 0 : atoi(c); core_options_apply(); sync_options(); }
      else if (!strcmp(b, "fit")) { cfg.fit = !strcmp(c, "fill") ? 0 : !strcmp(c, "fit") ? 1 : !strcmp(c, "integer") ? 2 : 3; sync_options(); }
      else if (!strcmp(b, "overlay")) cfg.overlay = !strcmp(c, "off") ? 0 : !strcmp(c, "small") ? 1 : 2;
      else if (!strcmp(b, "hud")) { cfg.hud_hide = !strcmp(c, "hide"); core_options_apply(); sync_options(); }
      else if (!strcmp(b, "names")) cfg.names = atoi(c);
      else if (!strcmp(b, "corner")) cfg.corner = atoi(c);
      else if (!strcmp(b, "shader")) { int sh = pc_shader_from_key(c); if (sh >= 0) cfg.shader = sh; }
      else if (!strcmp(b, "slot")) cfg.state_slot = atoi(c) - 1 < 0 ? 0 : (atoi(c) - 1) % PCS_SLOTS;
      else if (!strcmp(b, "vsync")) cfg.vsync = atoi(c) != 0;
      else if (!strcmp(b, "size")) { int ww, hh; if (sscanf(c, "%dx%d", &ww, &hh) == 2 && pc_win_valid(ww, hh)) { cfg.win_w = ww; cfg.win_h = hh; } }
      else if (!strcmp(b, "fire")) { cfg.fire_on_press = !strcmp(c, "press"); pc_sh_init(&shm, cfg.fire_on_press); }
    } else if (!strcmp(w, "BIND")) {   /* BIND L1 spell 12 near  (writes the quick slot directly, as the picker would) */
      sscanf(p, "%63s %63s %u %63s", b, c, &v[0], d);
      int slot = slot_of(b), kind = !strcmp(c, "spell") ? SOM_UX_BK_SPELL : !strcmp(c, "item") ? SOM_UX_BK_ITEM : !strcmp(c, "weapon") ? SOM_UX_BK_WEAPON : !strcmp(c, "spirit") ? SOM_UX_BK_SPIRIT : 0;
      int pol = !strcmp(d, "near") ? SOM_UX_POL_NEAR : !strcmp(d, "all") ? SOM_UX_POL_ALL : !strcmp(d, "lesshp") ? SOM_UX_POL_LESS_HP : SOM_UX_POL_SELECT;
      if (slot < 0) { printf("{\"event\":\"error\",\"line\":\"%s\"}\n", w); continue; }
      som_ux_quick_set(ux, slot, kind, (int)v[0]);
      if (kind) som_ux_quick_set_policy(ux, slot, pol);
      profile_save();
    } else if (!strcmp(w, "PICKER")) { sscanf(p, "%63s", b); open_picker(slot_of(b) < 0 ? 0 : slot_of(b)); modal_open = modal_wanted(); }
    else if (!strcmp(w, "OPTIONS")) { open_options(); modal_open = modal_wanted(); }
    else if (!strcmp(w, "UITAP")) {      /* UITAP HITNAME a : tap the centre of that target in the picker, like a mouse click */
      sscanf(p, "%63s %u", b, &v[0]);
      SDL_Rect pr; int x, y, hk = hitname(b);
      panel_rect(out_w, out_h, &pr);
      if (hk > 0 && som_ux_target_center(ux, pr.w, pr.h, hk, (int)v[0], &x, &y)) { picker_tap(x, y, &pr); printf("{\"event\":\"uitap\",\"hit\":\"%s\",\"a\":%u}\n", b, v[0]); }
      else printf("{\"event\":\"uitap-missing\",\"hit\":\"%s\",\"a\":%u}\n", b, v[0]);
      modal_open = modal_wanted();
    } else if (!strcmp(w, "UINAV")) { sscanf(p, "%63s", b); picker_move(!strcmp(b, "left") ? -1 : !strcmp(b, "right") ? 1 : 0, !strcmp(b, "up") ? -1 : !strcmp(b, "down") ? 1 : 0); }
    else if (!strcmp(w, "UIOK")) picker_confirm();
    else if (!strcmp(w, "UIBACK")) { picker_back(); modal_open = modal_wanted(); }
    else if (!strcmp(w, "SLOT")) {       /* SLOT L1 [frames held]: a press of a shoulder button */
      sscanf(p, "%63s %u", b, &v[0]);
      int slot = slot_of(b);
      if (v[0] == 0) v[0] = 4;
      if (slot < 0) continue;
      sh_script = (uint8_t)(1u << slot);
      script_frames((int)v[0], 0, 0);
      sh_script = 0;
      script_frames(1, 0, 0);
    } else if (!strcmp(w, "WAIT")) {     /* run until no direct command and no queued cast is pending (max n frames) */
      unsigned n; sscanf(p, "%u", &n);
      unsigned long f1 = frame_no;
      while (n--) {
        struct somd_queue_info q;
        int busy = somd_busy(&dir) || somd_queue_state(&dir, 1, &q) == SOMD_Q_WAITING || somd_queue_state(&dir, 2, &q) == SOMD_Q_WAITING || defer_valid;
        if (!busy) break;
        script_frames(1, 0, 0);
      }
      script_frames(2, 0, 0);
      printf("{\"event\":\"waited\",\"frames\":%lu,\"direct_err\":\"%s\"}\n", frame_no - f1, somd_err_name(dir.err));
    } else if (!strcmp(w, "STATE")) { sscanf(p, "%63s", b); jprint_state(b); }
    else if (!strcmp(w, "SHOT")) {
      unsigned sw = 0, sh = 0;
      sscanf(p, "%511s %u %u", a, &sw, &sh);
      render_scene(); SDL_RenderPresent(rd);
      if (save_bmp(a) == 0) printf("{\"event\":\"shot\",\"path\":\"%s\",\"out\":\"%dx%d\",\"frame\":\"%dx%d\",\"wide\":%d,\"fit\":\"%s\"}\n", a, out_w, out_h, game_w, game_h, cfg.wide, pc_fit_name(cfg.fit));
      else printf("{\"event\":\"shot-failed\",\"path\":\"%s\"}\n", a);
    } else if (!strcmp(w, "BENCH")) {
      unsigned n; sscanf(p, "%u", &n);
      emu_ms = emu_max = rend_ms = rend_max = 0; emu_n = rend_n = 0;
      bench_t0 = now_ms();
      unsigned long f1 = frame_no;
      script_frames((int)n, 0, 1);
      report_stats("bench", (now_ms() - bench_t0) / 1000.0, frame_no - f1);
    } else if (!strcmp(w, "SAVEBIND")) { profile_save(); }
    else if (!strcmp(w, "RELOADBIND")) { profile_load(); }
    else if (!strcmp(w, "PRINT")) { char t[400]; snprintf(t, sizeof t, "%s", p + (*p == ' ')); t[strcspn(t, "\r\n")] = 0; printf("{\"print\":\"%s\"}\n", t); }
    else if (!strcmp(w, "POKE")) {   /* test fixture only: POKE hexaddr hexbytes (WRAM) */
      unsigned addr; sscanf(p, "%x %511s", &addr, a);
      for (size_t i = 0; a[2 * i] && a[2 * i + 1] && addr + i < SOM_WRAM_SIZE; i++) { unsigned bv; sscanf(a + 2 * i, "%2x", &bv); wram[addr + i] = (uint8_t)bv; }
    } else if (!strcmp(w, "TARGETS")) {   /* TARGETS tag: the six actor cards the target panel and the enemy list are built from */
      struct somd_targets t;
      sscanf(p, "%63s", b);
      somd_scan_targets(wram, SOMD_C_ENEMY_ONE_OR_ALL, 0, 1, &t);
      printf("{\"targets\":\"%s\",\"frame\":%lu,\"map\":%u,\"enemies_valid\":%d,\"allies_valid\":%d,\"actors\":[", b, frame_no, st.map_id, t.n_enemies_valid, t.n_allies_valid);
      for (int i = 0; i < 6; i++) {
        const struct somd_actor *x = &t.actor[i];
        printf("%s[%d,%d,%d,%d,%d,%d,%d,%d,%d]", i ? "," : "", x->slot, x->present, x->on_screen, x->alive, x->id, x->hp, x->hp_max, x->sx, x->sy);
      }
      printf("]}\n");
    } else if (!strcmp(w, "PEEK")) {   /* PEEK tag hexaddr len: WRAM bytes as hex (test evidence) */
      unsigned addr, len = 1;
      sscanf(p, "%63s %x %u", b, &addr, &len);
      printf("{\"peek\":\"%s\",\"addr\":\"%X\",\"hex\":\"", b, addr);
      for (unsigned i = 0; i < len && addr + i < SOM_WRAM_SIZE; i++) printf("%02X", wram[addr + i]);
      printf("\"}\n");
    } else if (!strcmp(w, "SAVESTATE")) {
      sscanf(p, "%511s", a);
      size_t sn = r_ser_size(); void *bb = malloc(sn);
      if (r_ser(bb, sn) && spit(a, bb, sn) == 0) printf("{\"event\":\"state-saved\",\"path\":\"%s\",\"bytes\":%zu}\n", a, sn);
      free(bb);
    }
    else if (!strcmp(w, "SSAVE") || !strcmp(w, "SLOAD") || !strcmp(w, "SINFO")) {   /* SSAVE n / SLOAD n / SINFO n: save state slot n (1..10) */
      unsigned sl = 1;
      int r;
      sscanf(p, "%u", &sl);
      if (sl < 1 || sl > PCS_SLOTS) sl = 1;
      if (!strcmp(w, "SSAVE")) { r = state_save((int)sl - 1); printf("{\"event\":\"state-save\",\"slot\":%u,\"status\":%d}\n", sl, r); }
      else if (!strcmp(w, "SLOAD")) { r = state_load((int)sl - 1); printf("{\"event\":\"state-load\",\"slot\":%u,\"status\":%d,\"frame\":%lu}\n", sl, r, frame_no); }
      else {
        struct pcs_meta m;
        r = pcs_info(state_dir, rom_key, (int)sl - 1, core_sig(), &m, NULL);
        printf("{\"event\":\"state-info\",\"slot\":%u,\"status\":%d,\"time\":%lld,\"payload\":%u,\"backup\":%d}\n", sl, r, (long long)m.time, m.payload, m.from_backup);
      }
    } else if (!strcmp(w, "QUICKSAVE")) { quick_save(); printf("{\"event\":\"quick-save\",\"slot\":%d}\n", cfg.state_slot + 1); }
    else if (!strcmp(w, "QUICKLOAD")) { quick_load(); printf("{\"event\":\"quick-load\",\"slot\":%d,\"frame\":%lu}\n", cfg.state_slot + 1, frame_no); }
    else if (!strcmp(w, "SLOTSET")) { int sl = 1; sscanf(p, "%d", &sl); slot_set(sl - 1); printf("{\"event\":\"slot\",\"slot\":%d}\n", cfg.state_slot + 1); }
    else if (!strcmp(w, "FRAMEHASH")) {   /* FRAMEHASH tag: FNV of the last frame the core delivered */
      uint32_t h = 2166136261u;
      sscanf(p, "%63s", b);
      for (int y = 0; y < game_h; y++) for (int x = 0; x < game_w; x++) { h ^= game_px[y * 640 + x]; h *= 16777619u; }
      printf("{\"framehash\":\"%s\",\"hash\":\"%08x\",\"w\":%d,\"h\":%d,\"frame\":%lu}\n", b, h, game_w, game_h, frame_no);
    } else if (!strcmp(w, "SRAMPOKE")) {   /* SRAMPOKE hexaddr hexbytes: battery RAM, test fixture only */
      unsigned addr; size_t cap = r_mem_size(0); uint8_t *m = r_mem(0);
      sscanf(p, "%x %511s", &addr, a);
      for (size_t i = 0; m && a[2 * i] && a[2 * i + 1] && addr + i < cap; i++) { unsigned bv; sscanf(a + 2 * i, "%2x", &bv); m[addr + i] = (uint8_t)bv; }
    } else if (!strcmp(w, "SRAMPEEK")) {
      unsigned addr, len = 1; size_t cap = r_mem_size(0); uint8_t *m = r_mem(0);
      sscanf(p, "%63s %x %u", b, &addr, &len);
      printf("{\"srampeek\":\"%s\",\"cap\":%zu,\"addr\":\"%X\",\"hex\":\"", b, cap, addr);
      for (unsigned i = 0; m && i < len && addr + i < cap; i++) printf("%02X", m[addr + i]);
      printf("\"}\n");
    } else if (!strcmp(w, "SRAMFLUSH")) { srm_save(2); { char sp[800]; srm_path(sp, sizeof sp); printf("{\"event\":\"sram-flush\",\"path\":\"%s\"}\n", sp); } }
    else if (!strcmp(w, "MENU")) {   /* MENU root|save|load|video|credits */
      int pg;
      sscanf(p, "%63s", b);
      pg = !strcmp(b, "save") ? PCM_SAVE : !strcmp(b, "load") ? PCM_LOAD : !strcmp(b, "video") ? PCM_VIDEO : !strcmp(b, "credits") ? PCM_CREDITS : PCM_ROOT;
      menu_open_page(pg);
    } else if (!strcmp(w, "MKEY")) {
      static const char *const N[] = {"up", "down", "left", "right", "accept", "back", "prev", "next"};
      sscanf(p, "%63s", b);
      for (int i = 0; i < 8; i++) if (!strcmp(b, N[i])) menu_key(i);
    } else if (!strcmp(w, "MSTATE")) {
      printf("{\"menu\":{\"open\":%d,\"page\":%d,\"cursor\":%d,\"scroll\":%d,\"note\":\"%s\",\"slot\":%d,\"quit\":%d,\"launcher\":%d,\"shader\":\"%s\",\"fit\":\"%s\",\"wide\":%d,\"win\":\"%dx%d\",\"screen\":%d,\"vsync\":%d}}\n",
             menu_ui.open, menu_ui.page, menu_ui.cursor[menu_ui.page], menu_ui.scroll, menu_ui.note, cfg.state_slot + 1, quit, back_to_launcher,
             pc_shader_key(cfg.shader), pc_fit_name(cfg.fit), cfg.wide, cfg.win_w, cfg.win_h, cfg.screen, cfg.vsync);
    }
    else if (!strcmp(w, "CLEARBIND")) { for (int i = 0; i < SOM_UX_QUICK_N; i++) som_ux_quick_set(ux, i, 0, 0); profile_save(); }
    else { printf("{\"event\":\"unknown-command\",\"cmd\":\"%s\"}\n", w); }
    fflush(stdout);
  }
  fclose(f);
  return 0;
}

/* ------------------------------------------------------------------ main */
static void usage(void) {
  fprintf(stderr,
    "usage: som-pc [--rom ROM] [--core CORE.so] [options]\n"
    "  --wide off|17|27|46     extra world per side (default 46)       --fit fill|fit|integer|stretch (default fit)\n"
    "  --hud show|hide         hide the game HUD outside the ring        --fullscreen | --exclusive | --windowed\n"
    "  --size WxH              window size (default 1920x1080)           --no-vsync        --exclusive-size WxH (default 1920x1080)\n"
    "  --shader plain|sharp|scanlines|crt|lcd   display filter (default sharp)       --integer on|off   integer scaling\n"
    "  --overlay off|small|medium   --corner br|bl|bc|tr   --no-names    --fire press|release\n"
    "  --menu                  start menu (SoM original / randomized)    --profile NAME   binding profile (default: by ROM)\n"
    "  --gameplay FILE         apply a gameplay options file (T8 / T11, {\"options\": {...}}) to the ROM before playing; the menu page G (pad Back) does it for menu starts\n"
    "  --generate-all DIR      no window: generate every start menu preset into DIR (one JSON line each), needs --rom; --gen-seed N (default 12345), --gen-preset ID\n"
    "  --state FILE            load a core save state at start           --pack FILE      UI graphics pack (optional)\n"
    "  --save-config           write the options to ~/.config/som-decomp-pc/config.json\n"
    "  --script FILE --headless WxH   scripted session without a window (tests)       --stats  --verbose\n"
    "  --volume 0..100         master volume (default 100)               --no-audio\n"
    "  --autopilot FILE        timeline of virtual game controller input and screenshots on the real window (tests)\n");
}

/* ---- batch generation without a window: every preset of the start menu (or one) into DIR, one JSON line each.  Used by the parity tests. ---- */
static int cli_generate(const char *dir, const char *seed, const char *only) {
  struct sl_launcher *L = calloc(1, sizeof *L);
  int bad = 0, found = 0;
  if (!L) return 1;
  if (!cfg.rom[0] || access(cfg.rom, R_OK) != 0) { fprintf(stderr, "som-pc: --generate-all needs --rom\n"); free(L); return 2; }
  mkdirs(dir);
  for (int p = 0; p < sl_npresets; p++) {
    char rom[900], side[920], why[160] = "";
    struct pcg_result r;
    if (only && strcmp(only, sl_presets[p].id)) continue;
    found++;
    sl_init(L, 1);
    sl_apply_preset(L, p);
    if (sl_set_seed(L, seed) != 0) { fprintf(stderr, "som-pc: bad seed %s (1 to 8 digits)\n", seed); free(L); return 2; }
    if (!somrando_menu_supports(L, why, sizeof why)) {
      printf("{\"preset\":\"%s\",\"supported\":0,\"why\":\"%s\"}\n", sl_presets[p].id, why);
      continue;
    }
    snprintf(rom, sizeof rom, "%s/%s.sfc", dir, sl_presets[p].id);
    snprintf(side, sizeof side, "%s.json", rom);
    pcg_generate(L, cfg.rom, rom, NULL, NULL, &r);
    if (r.rc == SOMRANDO_OK) pcg_write_sidecar(L, side, &r);
    else bad++;
    printf("{\"preset\":\"%s\",\"supported\":1,\"rc\":%d,\"sha256\":\"%s\",\"seconds\":%.3f,\"path\":\"%s\",\"error\":\"%s\"}\n",
           sl_presets[p].id, r.rc, r.sha256, r.seconds, rom, r.err);
    fflush(stdout);
  }
  free(L);
  if (only && !found) { fprintf(stderr, "som-pc: no preset %s\n", only); return 2; }
  return bad ? 1 : 0;
}

int main(int argc, char **argv) {
  const char *state_file = NULL, *script = NULL, *profile_name = NULL;
  int want_menu = 0, save_cfg = 0, no_audio = 0, menu_used = 0, rc = 0;
  const char *gameplay_file = NULL;
  const char *autopilot = NULL;
  const char *gen_dir = NULL, *gen_seed = "12345", *gen_only = NULL;
  char menu_rom[512], play_rom[512];
  set_dirs();
  pc_cfg_defaults(&cfg);
  cfg_load();
  {
    const char *h = getenv("HOME") ? getenv("HOME") : ".";
    char bd[600], cand[760];
    base_dir(bd, sizeof bd);
    snprintf(cand, sizeof cand, "%score/" PC_CORE_NAME, bd);          /* release layout */
    if (!cfg.core[0] && access(cand, R_OK) == 0) snprintf(cfg.core, sizeof cfg.core, "%s", cand);
    snprintf(cand, sizeof cand, "%sdata/som_ui_pack.bin", bd);
    if (!cfg.pack[0] && access(cand, R_OK) == 0) snprintf(cfg.pack, sizeof cfg.pack, "%s", cand);
    if (!cfg.core[0]) snprintf(cfg.core, sizeof cfg.core, "%s/.local/state/som-decomp/companion-20261017/widescreen/pc-x86_64/" PC_CORE_NAME, h);
    if (!cfg.pack[0]) snprintf(cfg.pack, sizeof cfg.pack, "%s/.local/state/som-decomp/companion-20261017/assets/som_ui_pack.bin", h);
  }
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
#define NEXT() (i + 1 < argc ? argv[++i] : (usage(), exit(2), (char *)0))
    if (!strcmp(a, "--rom")) snprintf(cfg.rom, sizeof cfg.rom, "%s", NEXT());
    else if (!strcmp(a, "--core")) snprintf(cfg.core, sizeof cfg.core, "%s", NEXT());
    else if (!strcmp(a, "--pack")) snprintf(cfg.pack, sizeof cfg.pack, "%s", NEXT());
    else if (!strcmp(a, "--wide")) { const char *v = NEXT(); cfg.wide = !strcmp(v, "off") ? 0 : atoi(v); if (!pc_valid_wide(cfg.wide)) { fprintf(stderr, "--wide takes off, 17, 27 or 46\n"); return 2; } }
    else if (!strcmp(a, "--fit")) { const char *v = NEXT(); cfg.fit = !strcmp(v, "fill") ? 0 : !strcmp(v, "fit") ? 1 : !strcmp(v, "integer") ? 2 : !strcmp(v, "stretch") ? 3 : -1; if (cfg.fit < 0) { usage(); return 2; } }
    else if (!strcmp(a, "--hud")) cfg.hud_hide = !strcmp(NEXT(), "hide");
    else if (!strcmp(a, "--shader")) { int sh = pc_shader_from_key(NEXT()); if (sh < 0) { fprintf(stderr, "--shader takes plain, sharp, scanlines, crt or lcd\n"); return 2; } cfg.shader = sh; }
    else if (!strcmp(a, "--integer")) cfg.fit = !strcmp(NEXT(), "on") ? PC_FIT_INTEGER : PC_FIT_FIT;
    else if (!strcmp(a, "--exclusive-size")) { if (sscanf(NEXT(), "%dx%d", &cfg.excl_w, &cfg.excl_h) != 2 || !pc_win_valid(cfg.excl_w, cfg.excl_h)) { usage(); return 2; } }
    else if (!strcmp(a, "--fullscreen")) cfg.screen = PC_BORDERLESS;
    else if (!strcmp(a, "--exclusive")) cfg.screen = PC_EXCLUSIVE;
    else if (!strcmp(a, "--windowed")) cfg.screen = PC_WINDOWED;
    else if (!strcmp(a, "--size")) { if (sscanf(NEXT(), "%dx%d", &cfg.win_w, &cfg.win_h) != 2) { usage(); return 2; } }
    else if (!strcmp(a, "--no-vsync")) cfg.vsync = 0;
    else if (!strcmp(a, "--overlay")) { const char *v = NEXT(); cfg.overlay = !strcmp(v, "off") ? 0 : !strcmp(v, "small") ? 1 : 2; }
    else if (!strcmp(a, "--corner")) { const char *v = NEXT(); cfg.corner = !strcmp(v, "bl") ? 1 : !strcmp(v, "bc") ? 2 : !strcmp(v, "tr") ? 3 : 0; }
    else if (!strcmp(a, "--no-names")) cfg.names = 0;
    else if (!strcmp(a, "--fire")) cfg.fire_on_press = !strcmp(NEXT(), "press");
    else if (!strcmp(a, "--menu")) want_menu = 1;
    else if (!strcmp(a, "--profile")) profile_name = NEXT();
    else if (!strcmp(a, "--gameplay")) gameplay_file = NEXT();
    else if (!strcmp(a, "--generate-all")) gen_dir = NEXT();
    else if (!strcmp(a, "--gen-seed")) gen_seed = NEXT();
    else if (!strcmp(a, "--gen-preset")) gen_only = NEXT();
    else if (!strcmp(a, "--state")) state_file = NEXT();
    else if (!strcmp(a, "--save-config")) save_cfg = 1;
    else if (!strcmp(a, "--script")) { script = NEXT(); script_mode = 1; }
    else if (!strcmp(a, "--headless")) { if (sscanf(NEXT(), "%dx%d", &out_w, &out_h) != 2) { usage(); return 2; } headless = 1; }
    else if (!strcmp(a, "--no-audio")) no_audio = 1;
    else if (!strcmp(a, "--volume")) { cfg.volume = atoi(NEXT()); if (cfg.volume < 0) cfg.volume = 0; if (cfg.volume > 100) cfg.volume = 100; }
    else if (!strcmp(a, "--autopilot")) autopilot = NEXT();
    else if (!strcmp(a, "--verbose") || !strcmp(a, "--stats")) verbose = !strcmp(a, "--verbose");
    else if (!strcmp(a, "--help") || !strcmp(a, "-h")) { usage(); return 0; }
    else { fprintf(stderr, "som-pc: unknown option %s\n", a); usage(); return 2; }
  }
  if (gen_dir) return cli_generate(gen_dir, gen_seed, gen_only);
  if (script_mode && !headless) { out_w = 1920; out_h = 1080; headless = 1; }
  if (headless && (out_w < 320 || out_h < 240)) { fprintf(stderr, "som-pc: bad --headless size\n"); return 2; }
  if (!headless) { out_w = cfg.win_w; out_h = cfg.win_h; }
  if (getenv("SOM_PC_DEBUG")) verbose = 1;
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) { fprintf(stderr, "som-pc: SDL: %s\n", SDL_GetError()); return 1; }
    no_audio = 1;
  }
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
  if (video_init() != 0) return 1;
  gameplay_init();
  credits_init();
  runs_init();
  pcm_init(&menu_ui, &cfg);
  pc_pads_init(&gp);
  if (gp.last_note[0]) fprintf(stderr, "som-pc: %s\n", gp.last_note);
  if (autopilot) ap_load(autopilot);
  for (int round = 0;; round++) {
    char pvar[64];
    int use = 0, in_game = 0;
    if (!script_mode && (round > 0 || want_menu || !cfg.rom[0])) {
      if (!run_menu_screen(menu_rom, sizeof menu_rom)) break;
      snprintf(play_rom, sizeof play_rom, "%s", menu_rom);
      menu_used = 1;
      if (tex_menu) { SDL_DestroyTexture(tex_menu); tex_menu = NULL; }
    } else snprintf(play_rom, sizeof play_rom, "%s", cfg.rom);
    if (!play_rom[0]) { fprintf(stderr, "som-pc: no ROM (use --rom, or --menu)\n"); rc = 2; break; }
    srm_src[0] = 0;
    {
      struct sgp_opts so;
      char patched[800], why[120];
      if (gameplay_file) {
        static struct pg from_file;
        size_t n;
        uint8_t *b = gpg_ok ? pc_slurp(gameplay_file, &n) : NULL;
        if (!gpg_ok) fprintf(stderr, "som-pc: gameplay_menu.json not found (set SOM_PC_DATA): the options file is ignored\n");
        else if (!b) fprintf(stderr, "som-pc: cannot read %s\n", gameplay_file);
        else {
          from_file = gpg; pg_reset(&from_file);
          b = realloc(b, n + 1); b[n] = 0;
          if (pg_load_values(&from_file, (char *)b) != 0) fprintf(stderr, "som-pc: %s is not an options file\n", gameplay_file);
          else use = pgc_from_pg(&from_file, &so, why, sizeof why) > 0;
        }
        free(b);
      } else if (menu_used && gpg_ok && !pg_is_default(&gpg)) use = pgc_from_pg(&gpg, &so, why, sizeof why) > 0;
      if (use) {
        if (gameplay_apply(play_rom, &so, patched, sizeof patched) == 0) {
          size_t on; uint8_t *ob = pc_slurp(play_rom, &on);
          fprintf(stderr, "som-pc: gameplay tweaks applied: %s\n", patched);
          if (ob && !profile_name) { size_t off = on % 1024 == 512 ? 512 : 0; som_bind_rom_variant(ob + off, on - off, pvar, sizeof pvar); profile_name = pvar; }   /* same slot bindings as the original ROM */
          free(ob);
          snprintf(srm_src, sizeof srm_src, "%s", play_rom);                 /* the battery save stays that of the unpatched ROM */
          snprintf(play_rom, sizeof play_rom, "%s", patched);
        }
        else fprintf(stderr, "som-pc: gameplay tweaks could not be applied: playing the ROM unchanged\n");
      }
    }
    if (game_start(play_rom, profile_name) != 0) { rc = 1; break; }
    in_game = 1;
    if (!headless && !no_audio && !aud.dev) audio_open();
    if (save_cfg) cfg_save();
    if (script) rc = run_script(script);
    else { main_loop(round == 0 ? state_file : NULL); remember_window(); cfg_save(); }
    if (script) report_stats("script", 0, frame_no);
    game_end();
    if (script || !back_to_launcher) break;
    game_unload();
    in_game = 0;
    back_to_launcher = 0; quit = 0; profile_name = NULL; gameplay_file = NULL; state_file = NULL;
    (void)in_game;
  }
  pc_audio_close(&aud);
  pc_pads_close(&gp);
  SDL_Quit();
  return rc;
}
