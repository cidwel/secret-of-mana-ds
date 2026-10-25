/* JNI side of the Android companion: a minimal libretro host (dlopen core, run on a thread, AAudio output) that
 * feeds the core's WRAM to som_companion, runs touch-started macros and renders the status screen.
 * With no core it shows a synthetic demo state so the two-screen layout can be checked without a ROM.
 * Device results: docs/companion-device-20261017.md. */
#include <jni.h>
#include <android/bitmap.h>
#include <android/log.h>
#include <aaudio/AAudio.h>
#include <math.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "som_dedither.h"
#include "som_companion.h"
#include "som_status_draw.h"
#include "direct/som_direct.h"
#include "som_bind.h"
#include "som_hotkey.h"
#include "som_states.h"
#include <unistd.h>
#include <sys/stat.h>
#include "som_bind_ux.h"

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "somcompanion", __VA_ARGS__)
#define MASK_RELEASE_FRAMES 24   /* a fade or flash shorter than this (spell flashes, hit effects) does not bring the HUD back */
#define MASK_ENTER_FRAMES 30   /* free field play for this long before BG3 (the HUD layer) is switched off */

struct retro_system_info { const char *name, *version, *exts; bool need_fullpath, block_extract; };
struct retro_game_info { const char *path; const void *data; size_t size; const char *meta; };
struct retro_av { struct { unsigned bw, bh, mw, mh; float ar; } geo; struct { double fps, sr; } timing; };
struct var { const char *key, *value; };

static void *core;
static void (*r_init)(void), (*r_run)(void), (*r_deinit)(void);
static bool (*r_load)(const struct retro_game_info *);
static void (*r_av)(struct retro_av *);
static void *(*r_mem)(unsigned);
static size_t (*r_memsize)(unsigned);
static bool (*r_unser)(const void *, size_t);
static size_t (*r_sersize)(void);
static bool (*r_ser)(void *, size_t);

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_t thr;
static volatile int running, have_game;
static int pixfmt;                    /* 0 = 0RGB1555, 1 = XRGB8888, 2 = RGB565 */
static uint32_t frame[512 * 240], frame_w, frame_h;
static unsigned pad_in, pad_latch, cur_mask;
/* ux_mu guards the UX decoder state (zone map buffers) between the per-frame update on the core thread and the draw on the
 * view worker.  The draw can take 30 ms; it must not hold `mu`, or the core would miss frames.  The update simply skips a
 * frame when the draw is running. */
static pthread_mutex_t ux_mu = PTHREAD_MUTEX_INITIALIZER;
static int latch_age;   /* pad_latch keeps a tap that is released within one frame alive */
static struct timespec pad_t;         /* time of the last button press, for the input-to-core latency */
static volatile int pad_stamp;
static double st_run_sum, st_run_max, st_lat_sum, st_lat_max;
static unsigned st_frames, st_lat_n, st_under, st_crop_n;
static double st_ux_upd, st_ux_upd_max, st_crop;
static char stats_line[256] = "";
static uint64_t total_frames;
static unsigned last_m;
static int mask_on, mask_dirty, field_run, hide_hud = 1, away_run, core_ws;   /* core_ws: the core masks the HUD from WRAM; the frontend only toggles BG3 for the stock core */
static int touch_x = -1, touch_y, lp_x = -1, lp_y;
static char opt_ws[16] = "46", opt_fb[16] = "normal";   /* widescreen core options (ignored by the plain core) */
static uint8_t demo_wram[SOM_WRAM_SIZE];
static const uint8_t *wram;
static struct som_state st;
static struct som_macro macro;
static struct som_ui ui;
static char sysdir[512];
static uint8_t *rom_copy;

static int hold_addr[4], hold_len[4], hold_n[4];
static uint8_t hold_d[4][8];   /* test hook state, see Native.pokeHold */

/* ---- direct control (no ring): see docs/companion-direct-20261017.md */
static struct somd direct;
static int direct_ok, direct_reported = 1;
static double direct_t0;
/* Notices: the UX layer draws a small chip (queued, cancelled, dropped, refused).  The loop thread leaves the text here and the frame loop
 * hands it to the UX layer under ux_mu. */
#define NQ 6
static char nq_text[NQ][32];
static int nq_kind[NQ], nq_n, nq_hold;   /* FIFO of chip notices: two casters can report in the same frame, each gets its turn */
static int nf_id[NQ], nf_n, nf_hold;     /* FIFO of "queued cast fired" flashes, spell ids */
static void ui_sfx(int id);
static const char *hero_tag(int hero) { return hero == 1 ? "GIRL" : hero == 2 ? "SPRITE" : "BOY"; }
static void set_notice(int kind, const char *t) {
  if (nq_n == NQ) { memmove(nq_text[0], nq_text[1], sizeof nq_text[0] * (NQ - 1)); memmove(nq_kind, nq_kind + 1, sizeof nq_kind[0] * (NQ - 1)); nq_n--; }
  char *dst = nq_text[nq_n];
  snprintf(dst, sizeof nq_text[0], "%s", t);
  for (char *p = dst; *p; p++) *p = (*p == '-' || *p == '_') ? ' ' : (*p >= 'a' && *p <= 'z') ? (char)(*p - 32) : *p;   /* no-mp -> NO MP */
  nq_kind[nq_n++] = kind;
  LOG("notice %d %s", kind, dst);
  if (kind == SOM_UX_NOTICE_ERROR || kind == SOM_UX_NOTICE_DROPPED) ui_sfx(0x0D);   /* every refusal buzzes */
}
static void set_hero_notice(int kind, int hero, const char *t) {   /* "GIRL" + reason; the UX layer composes "GIRL QUEUED" / "DROPPED: GIRL NO MP" */
  char b[32]; snprintf(b, sizeof b, "%s%s%s", hero_tag(hero), t && *t ? " " : "", t ? t : "");
  set_notice(kind, b);
}
static void flash_push(int spell_id) { if (nf_n < NQ) nf_id[nf_n++] = spell_id; }
static void set_toast(const char *t) {
  set_notice(!strcmp(t, "QUEUED") ? SOM_UX_NOTICE_QUEUED : !strcmp(t, "CANCELLED") ? SOM_UX_NOTICE_CANCELLED : SOM_UX_NOTICE_ERROR, t);
}

/* ---- UI sounds: the game's own menu effects (docs/event-menu-ring-audio.md) played through the direct layer's NMI stub */
/* The game's own menu sounds, measured by resetting $1E01 every frame while driving the ring (tools/companion/android/tests/sfx_map.py):
 * 29 cursor, 2A ring open / spin / owner switch (NOT used for accept any more), 0E select / confirm (equip, cast), 0D refusal (no MP).
 * 0C is the unmeasured neighbour kept for cancel so that cancel, accept and error are three different sounds. */
enum { SFX_MOVE = 0x29, SFX_OK = 0x0E, SFX_BACK = 0x0C, SFX_ERROR = 0x0D };
static int sfx_on = 1, sfx_pending, sfx_wait;
static unsigned sfx_sent, sfx_refused;
static int ux_locked_mode(void);
static int ux_sfx_mode(void);
static void ui_sfx(int id) {   /* mu held */
  if (!sfx_on) return;
  if (!ux_sfx_mode()) return;   /* never while the game itself drives the sound chip (title, name entry, fades, cutscenes, flights) */
  sfx_pending = id;
}

static void run_direct(const struct som_cmd *cmd, struct somd_target t) {
  struct timespec ts;
  int err = som_hk_start_cmd(&direct, wram, cmd, t);
  clock_gettime(CLOCK_MONOTONIC, &ts);
  direct_t0 = ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
  if (err == SOMD_OK) { direct_reported = 0; LOG("direct start kind=%d hero=%u arg=%u spell=%u target=%u/%u", cmd->kind, cmd->hero, cmd->arg, cmd->spell, t.kind, t.slot); return; }
  LOG("direct refused kind=%d: %s", cmd->kind, somd_err_name((enum somd_err)err));
  if (err == SOMD_ERR_NEEDS_RING) { som_macro_start(&macro, wram, cmd); return; }   /* Magic Rope, Flammie Drum: old ring macro */
  set_toast(somd_err_name((enum somd_err)err));
}

/* ---- audio: ring buffer of stereo int16 pulled by an AAudio callback */
static AAudioStream *astream;
static int16_t ring[1 << 16];
static volatile unsigned rhead, rtail;
static aaudio_data_callback_result_t acb(AAudioStream *s, void *u, void *data, int32_t n) {
  int16_t *o = data;
  (void)s; (void)u;
  int under = 0;
  for (int32_t i = 0; i < n * 2; i++) {
    if (rtail != rhead) { o[i] = ring[rtail]; rtail = (rtail + 1) & 0xFFFF; } else { o[i] = 0; under = 1; }
  }
  if (under && have_game) st_under++;
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}
static void audio_open(double rate) {
  AAudioStreamBuilder *b;
  if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return;
  AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_I16);
  AAudioStreamBuilder_setChannelCount(b, 2);
  AAudioStreamBuilder_setSampleRate(b, (int32_t)rate);
  AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudioStreamBuilder_setDataCallback(b, acb, NULL);
  if (AAudioStreamBuilder_openStream(b, &astream) == AAUDIO_OK) AAudioStream_requestStart(astream);
  AAudioStreamBuilder_delete(b);
}
/* Audio meter (emulation thread only): what the core submits per stats interval, plus ring drops. */
static double am_sq; static unsigned am_n, am_peak, am_drop, am_zero_run, am_zero_max;
static volatile int audio_mute;
static size_t audio_batch(const int16_t *d, size_t frames) {
  if (audio_mute) return frames;
  for (size_t i = 0; i < frames * 2; i++) {
    int v = d[i]; unsigned a = (unsigned)(v < 0 ? -v : v);
    am_sq += (double)v * v; am_n++; if (a > am_peak) am_peak = a;
    if (a < 2) { if (++am_zero_run > am_zero_max) am_zero_max = am_zero_run; } else am_zero_run = 0;
  }
  for (size_t i = 0; i < frames * 2; i++) {
    unsigned nh = (rhead + 1) & 0xFFFF;
    if (nh == rtail) { am_drop++; break; }
    ring[rhead] = d[i]; rhead = nh;
  }
  return frames;
}

/* ---- SoM-style bottom screen (tools/companion/ux): the UX layer owns the layout, greying and target panel */
#include "ux/android_patch/som_ux_glue.h"
static uint8_t *pack_blob; static size_t pack_len;
#include "enemysprites/som_enemy_sprite.h"
static uint8_t *es_pack; static size_t es_pack_len;
/* Fills the sprites of the enemies the UX still lacks (atlas first, live PPU render for the rest). */
static int fill_enemy_sprites(const uint8_t *vram) {
  static uint8_t scratch[SOM_ES_BUF_BYTES];
  static uint32_t argb[SOM_ES_SPRITE_MAX * SOM_ES_SPRITE_MAX];
  uint8_t ids[16];
  struct som_es_src live;
  int n = ux_glue_sprites_wanted(ids, 16), set = 0;
  if (vram) som_es_src_wram(&live, wram, vram);
  for (int i = 0; i < n; i++) {
    int w, h, conf;
    if (som_es_sprite_for_id(es_pack, es_pack_len, vram ? &live : NULL, wram, ids[i], scratch, argb, &w, &h, &conf) != SOM_ES_FROM_NONE)
      if (ux_glue_set_enemy_sprite(ids[i], argb, w, h) == 0) set++;
  }
  return set;
}
static int touch_w, touch_h, back_wait;
static int panel_w, panel_h;      /* size of the bottom panel as last drawn: the pad panel mode needs it */
static volatile int nav_q[16], nav_qh, nav_qt;   /* pad panel mode requests: 1..4 move up/down/left/right, 5 activate, 6 back, 7 toggle */
static double ux_draw_ms;

static volatile int quick_dirty;
static volatile int ux_force = 1;   /* the next render must draw (touch, press, new surface) */
static struct ux_dispatch dbg_cmd;
static volatile int dbg_cmd_valid;
static struct ux_dispatch after_close;
static int after_close_valid, after_close_frames;
/* Events for the Java side (options to persist, haptic ticks); popped by Native.pollEvent(). */
enum { EV_OPT = 1 << 16, EV_TICK = 2 << 16 };
static int evq[16], evq_n;
static void ev_push(int e) { if (evq_n < 16) evq[evq_n++] = e; }
/* Menu quick actions and avatar taps jump straight to the target with the direct layer (no intermediate rings, video
 * hidden while it runs); the macro engine stays as the fallback for ROMs the direct layer does not know. */
static void run_menu(int kind, int hero, int arg) {
#ifdef SOMD_HAS_MENU
  if (st.pad_hero != 0xFF && st.pad_hero < 3 && (st.hero[st.pad_hero].status & 0x4173) && st.mode == SOM_MODE_FIELD) {   /* the game refuses the ring while the hero is disabled */
    LOG("direct menu refused: hero status %04x blocks the ring", st.hero[st.pad_hero].status); set_toast("HERO DISABLED"); ui_sfx(SFX_ERROR); return;
  }
  int err = somd_open_menu(&direct, wram, kind, hero, arg);
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  direct_t0 = ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
  if (err == SOMD_OK) { direct_reported = 0; LOG("direct menu kind=%d hero=%d arg=%d", kind, hero, arg); return; }
  LOG("direct menu refused kind=%d hero=%d arg=%d: %s", kind, hero, arg, somd_err_name((enum somd_err)err));
  set_toast(somd_err_name((enum somd_err)err));
#else
  (void)kind; (void)hero; (void)arg;
#endif
}
static void close_menu(void) {
#ifdef SOMD_HAS_MENU
  if (direct_ok) { int err = somd_close_menu(&direct, wram); if (err != SOMD_OK) set_toast(somd_err_name((enum somd_err)err)); else direct_reported = 0; return; }
#endif
  if (!som_macro_busy(&macro)) som_macro_close_ring(&macro);
}
static void nav_menu(int hero, int cat, int entry) {
  struct som_cmd c = {SOM_CMD_OPEN_MENU, (uint8_t)hero, (uint8_t)cat, (uint8_t)entry};
  som_read_state(wram, &st);
  if (cat == SOM_CAT_MAGIC && hero == 0) c.hero = (uint8_t)(ux_g && (ux_g->hero == 1 || ux_g->hero == 2) ? ux_g->hero : 1);
#ifdef SOMD_HAS_MENU
  if (direct_ok) {
    if (somd_busy(&direct)) { set_toast("BUSY"); return; }
    if (cat == SOM_CAT_CONFIG && entry != 0xFF) run_menu(SOMD_MENU_PAGE, c.hero, entry);
    else run_menu(SOMD_MENU_RING, c.hero, cat == 0xFF ? (st.ring.open ? st.ring.category : SOM_CAT_WEAPON) : cat);
    return;
  }
#endif
  if (som_macro_busy(&macro)) { set_toast("BUSY"); return; }
  LOG("menu macro hero=%d cat=%d entry=%d (ring %d owner %d cat %d)", c.hero, c.arg, c.spell, st.ring.open, st.ring.owner, st.ring.category);
  if (!som_macro_start(&macro, wram, &c)) { LOG("menu macro refused: %s %s", som_result_name(macro.result), macro.note); set_toast(macro.result == SOM_FAIL_UNSAFE ? "UNSAFE_STATE" : "NOT_OWNED"); }
}
static struct ux_dispatch deferred; static int deferred_valid, deferred_frames;

/* Spell button states from the direct layer: permanent reasons disable, a waiting queue entry shows the clock. */
static void spell_states_update(void) {   /* ux_mu held */
  uint8_t av[42];
  for (int id = 0; id < 42; id++) {
    int hero = (id % 6) >= 3 ? 1 : 2, spirit = id / 6, slot = id % 3;
    struct somd_cast_info info; struct somd_queue_info q;
    int r = somd_can_cast(&direct, wram, hero, spirit, slot, &info);
    av[id] = (r == SOMD_CAST_OK) ? SOM_UX_AV_ENABLED : info.permanent ? SOM_UX_AV_DISABLED : SOM_UX_AV_ENABLED;
    if (somd_queue_state(&direct, hero, &q) == SOMD_Q_WAITING && q.spell_id == id) av[id] = SOM_UX_AV_QUEUED;
  }
  ux_glue_spell_avail(av);
}
/* Every frame: report a fired or dropped queue entry once. */
static void queue_poll(void) {
  if (!direct_ok) return;
  for (int h = 1; h <= 2; h++) {
    struct somd_queue_info q;
    int st = somd_queue_state(&direct, h, &q);
    if (st == SOMD_Q_FIRED) { LOG("queued cast fired hero=%d spell=%d after %u frames", h, q.spell_id, q.waited); ui_sfx(SFX_OK); flash_push(q.spell_id); somd_queue_ack(&direct, h); { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); direct_t0 = ts.tv_sec * 1e3 + ts.tv_nsec / 1e6; } direct_reported = 0; }
    else if (st == SOMD_Q_DROPPED) { LOG("queued cast dropped hero=%d spell=%d: %s", h, q.spell_id, somd_err_name((enum somd_err)q.err)); set_hero_notice(SOM_UX_NOTICE_DROPPED, h, somd_err_name((enum somd_err)q.err)); somd_queue_ack(&direct, h); }
  }
}
/* ---- save states (som_states.c): <sysdir>/states/<game>/slotN.sst, eight slots with thumbnail and time ---- */
static char save_base[48], save_sig[24], rom_id[20], state_dir[700];
static int cur_slot;
static struct sst_info sinfo;
static size_t sst_size_cb(void *u) { (void)u; return r_sersize ? r_sersize() : 0; }
static int sst_save_cb(void *u, void *b, size_t n) { (void)u; return r_ser && r_ser(b, n); }
static int sst_load_cb(void *u, const void *b, size_t n) { (void)u; return r_unser && r_unser(b, n); }
static const struct sst_ops sst_ops = {NULL, sst_size_cb, sst_save_cb, sst_load_cb};
static void state_show_slot(int i) {   /* mu held */
  char t[16] = "";
  if (!state_dir[0] || !ux_g) return;
  sst_info(state_dir, i, r_sersize ? r_sersize() : 0, &sinfo);
  if (sinfo.used) {
    struct tm tm; time_t tt = (time_t)sinfo.time;
    localtime_r(&tt, &tm);
    strftime(t, sizeof t, "%m-%d %H:%M", &tm);
  }
  pthread_mutex_lock(&ux_mu);
  som_ux_set_state_slot(ux_g, i, sinfo.used, t, sinfo.used ? sinfo.thumb : NULL);
  pthread_mutex_unlock(&ux_mu);
}
static void state_show_all(void) {   /* mu held */
  if (!state_dir[0] || !ux_g) return;
  cur_slot = sst_get_current(state_dir);
  for (int i = 0; i < SST_SLOTS; i++) state_show_slot(i);
  pthread_mutex_lock(&ux_mu); som_ux_set_state_sel(ux_g, cur_slot); pthread_mutex_unlock(&ux_mu);
}
static void after_state_load(void) {   /* mu held: the running game was replaced under the helpers */
  som_macro_init(&macro);
  if (direct_ok) { const uint8_t *rr = direct.rom; size_t sz = direct.rom_size; somd_init(&direct, rr, sz); }
  direct_reported = 1; mask_on = 0; mask_dirty = 1; field_run = 0;
}
static int do_state_save(int slot) {   /* mu held; slot < 0 = the remembered slot */
  static uint32_t th[SST_TW * SST_TH];
  int rc;
  if (!have_game || !state_dir[0]) return SST_ERR_ARG;
  if (slot < 0) slot = cur_slot;
  sst_make_thumb(frame, 512, (int)frame_w, (int)frame_h, th);
  rc = sst_save(state_dir, slot, &sst_ops, th, (int64_t)time(NULL));
  if (rc == SST_OK) { cur_slot = slot; sst_set_current(state_dir, slot); state_show_slot(slot); pthread_mutex_lock(&ux_mu); som_ux_set_state_sel(ux_g, slot); pthread_mutex_unlock(&ux_mu); }
  else set_toast("SAVE FAILED");
  LOG("state save slot %d: %d", slot, rc);
  return rc;
}
static int do_state_load(int slot) {   /* mu held */
  int rc;
  if (!have_game || !state_dir[0]) return SST_ERR_ARG;
  if (slot < 0) slot = cur_slot;
  rc = sst_load(state_dir, slot, &sst_ops);
  if (rc == SST_OK) { cur_slot = slot; sst_set_current(state_dir, slot); after_state_load(); }
  else set_toast(rc == SST_ERR_EMPTY ? "EMPTY SLOT" : "LOAD FAILED");
  LOG("state load slot %d: %d", slot, rc);
  return rc;
}
static void ux_dispatch(const struct ux_dispatch *d) {
  /* A UI click still in flight through the direct layer (touch-down sound) must not make the tap look busy: wait for it. */
  if (direct_ok && somd_busy(&direct) && direct.kind == 4 && d->kind != SOM_UX_ACT_PAD) { deferred = *d; deferred_valid = 1; deferred_frames = 0; return; }
  struct somd_target none = {SOMD_T_NONE, 0}, t = none;
  if (ux_glue_option_value(SOM_UX_OPT_HAPTICS)) ev_push(EV_TICK);
  switch (d->kind) {
  case SOM_UX_ACT_CMD: {
    const struct som_cmd *c = &d->cmd;
    ui_sfx(SFX_OK);   /* replaced by the buzz when the command is refused */
    if (st.mode == SOM_MODE_RING && !after_close_valid) {   /* a hotlink tapped inside the ring: close the ring, then run it */
      after_close = *d; after_close_valid = 1; after_close_frames = 0;
      close_menu();
      return;
    }
    int qpos = (c->hero == 1 && c->arg == 7) ? 6 : c->arg;
    if (direct_ok && c->kind == SOM_CMD_CAST_MAGIC && c->hero >= 1 && c->hero <= 2 && qpos <= 6 && c->spell < 3 &&
        somd_spell_class(&direct, somd_spell_id(c->hero, qpos, c->spell)) != SOMD_C_NONE &&
        somd_spell_class(&direct, somd_spell_id(c->hero, qpos, c->spell)) != SOMD_C_UNSUPPORTED) {   /* targeted casts: fire now or wait for a transient reason */
      int pos = qpos;
      struct somd_policy pol = {SOMD_POL_SLOT, 0};
      struct somd_cast_info info;
      if (d->target_kind == 1) pol.slot = (uint8_t)d->target_slot;
      else if (d->target_kind == 2 || d->target_kind == 3) pol.kind = SOMD_POL_ALL;
      int err = somd_queue_cast(&direct, wram, c->hero, pos, c->spell, pol, 0, &info);
      LOG("cast hero=%d spirit=%d slot=%d reason=%d -> %s", c->hero, pos, c->spell, info.reason, somd_err_name((enum somd_err)err));
      if (err != SOMD_OK) set_hero_notice(SOM_UX_NOTICE_ERROR, c->hero, somd_err_name((enum somd_err)err));
      else { struct somd_queue_info q; if (somd_queue_state(&direct, c->hero, &q) == SOMD_Q_WAITING) { if (q.replaced) set_hero_notice(SOM_UX_NOTICE_INFO, c->hero, "REPLACED"); else set_hero_notice(SOM_UX_NOTICE_QUEUED, c->hero, ""); } else { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); direct_t0 = ts.tv_sec * 1e3 + ts.tv_nsec / 1e6; direct_reported = 0; } }
      break;
    }
    if (somd_busy(&direct)) { set_toast("BUSY"); return; }
    if (!st.safe_to_inject) { set_toast("UNSAFE_STATE"); return; }
    if (d->target_kind == 1) { t.kind = SOMD_T_ONE; t.slot = (uint8_t)d->target_slot; }
    else if (d->target_kind == 2) t.kind = SOMD_T_ALL_ALLIES;
    else if (d->target_kind == 3) t.kind = SOMD_T_ALL_ENEMIES;
    else if (c->kind == SOM_CMD_CAST_MAGIC) {
      int pos = (c->hero == 1 && c->arg == 7) ? 6 : c->arg;
      if (somd_spell_class(&direct, somd_spell_id(c->hero, pos, c->spell)) == SOMD_C_BOY_ONLY) { t.kind = SOMD_T_ONE; t.slot = 0; }
    }
    if (!direct_ok) { som_macro_start(&macro, wram, c); return; }
    run_direct(c, t);
    break;
  }
  case SOM_UX_ACT_CANCEL_QUEUED:
    if (direct_ok && d->hero >= 1 && d->hero <= 2) { somd_queue_cancel(&direct, d->hero); somd_queue_ack(&direct, d->hero); set_hero_notice(SOM_UX_NOTICE_CANCELLED, d->hero, ""); ui_sfx(SFX_BACK); }
    break;
  case SOM_UX_ACT_PAD:
    pad_latch |= d->pad; latch_age = 0;
    ui_sfx((d->pad & (1u << SOM_PAD_A)) ? SFX_BACK : SFX_MOVE);
    if ((d->pad & (1u << SOM_PAD_A)) && st.mode == SOM_MODE_RING) back_wait = 24;   /* BACK: A, then Y if the ring is still open */
    break;
  case SOM_UX_ACT_NAME_LETTER:   /* direct text: the letter goes into the game's name buffer, the cursor does not move (somd_name_letter) */
    if (direct_ok && wram) { int e = somd_name_letter(&direct, (uint8_t *)wram, d->letter); LOG("name entry: %c %s", 'A' + d->letter, somd_err_name((enum somd_err)e)); }
    break;
  case SOM_UX_ACT_NAME_DELETE:
    if (direct_ok && wram) { int e = somd_name_delete(&direct, (uint8_t *)wram); LOG("name entry: delete %s", somd_err_name((enum somd_err)e)); }
    break;
  case SOM_UX_ACT_SWITCH_HERO:   /* a hero card was tapped: the game's own hero switch; refused (and ignored) outside free field play */
    if (direct_ok && wram) {
      int e = somd_switch_hero(&direct, wram, d->hero);
      LOG("switch hero %d: %s", d->hero, somd_err_name((enum somd_err)e));
      if (e == SOMD_OK) ui_sfx(SFX_MOVE);
    }
    break;
  case SOM_UX_ACT_STATE:
    if (d->opt_id == 2) { cur_slot = d->opt_val; if (state_dir[0]) sst_set_current(state_dir, cur_slot); ui_sfx(SFX_MOVE); }
    else if (d->opt_id == 0) { if (do_state_save(d->opt_val) == SST_OK) ui_sfx(SFX_OK); else ui_sfx(SFX_ERROR); }
    else if (d->opt_id == 1) { if (do_state_load(d->opt_val) == SST_OK) ui_sfx(SFX_OK); else ui_sfx(SFX_ERROR); }
    break;
  case SOM_UX_ACT_OPTION: ev_push(EV_OPT | (d->opt_id & 0xFF) << 8 | (d->opt_val & 0xFF)); ui_sfx(SFX_OK); break;
  case SOM_UX_ACT_AVATAR: nav_menu(d->hero, 0xFF, 0xFF); ui_sfx(SFX_OK); break;
  case SOM_UX_ACT_OPEN_MENU:
    ui_sfx(SFX_OK);
    if (d->bind_kind == SOM_UX_BK_MENU) nav_menu(d->hero, d->bind_id, 0xFF);
    else if (d->bind_kind == SOM_UX_BK_SUBMENU) nav_menu(d->hero, SOM_CAT_CONFIG, d->bind_id);
#ifdef SOMD_HAS_MENU
    else if (d->bind_kind == SOM_UX_BK_SPIRIT && direct_ok) { som_read_state(wram, &st); if (somd_busy(&direct)) set_toast("BUSY"); else run_menu(SOMD_MENU_SPIRIT, d->hero == 0 ? 1 : d->hero, d->bind_id); }
#endif
    break;
  case SOM_UX_ACT_QUICK_SET: quick_dirty = 1; ui_sfx(SFX_OK); break;
  case SOM_UX_ACT_QUICK_EDIT: break;
  case SOM_UX_ACT_CLOSE_RING: close_menu(); break;
  default: break;
  }
}

/* ---- shoulder slots: L1 R1 L2 R2 run the entry of quick slot 0..3 by position (engine and glue shared with the 3DS app). */
static struct som_bx bx;
static volatile int bind_mode = SOM_BM_FIELD;
static int hkq[8], hkq_n;

static void do_hotkeys(void) {   /* loop thread, mu held */
  int q[8], n = hkq_n;
  memcpy(q, hkq, sizeof q);
  hkq_n = 0;
  for (int i = 0; i < n; i++) {
    struct ux_dispatch d;
    som_read_state(wram, &st);
    if (ux_g && ux_glue_shoulder(q[i], &d)) ux_dispatch(&d);
  }
}
static int ux_locked_mode(void) { return som_bx_locked(ux_g); }
/* A UI sound is a call into the game's own sound routine.  Sent while the game talks to the sound driver itself (the START press on the name
 * entry, a map fade) it races the game's command and the driver stops, after which the game waits for it forever: black screen on map 8. */
static int ux_sfx_mode(void) { return ux_g && (ux_g->mode == SOM_UX_FIELD || ux_g->mode == SOM_UX_RING || ux_g->mode == SOM_UX_SHOP); }
static double mono_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }

static void audio_one(int16_t l, int16_t r) { int16_t d[2] = {l, r}; audio_batch(d, 1); }

/* ---- libretro callbacks */
static bool env(unsigned cmd, void *d) {
  switch (cmd) {
  case 10: pixfmt = *(int *)d == 1 ? 1 : *(int *)d == 2 ? 2 : 0; return true;   /* SET_PIXEL_FORMAT */
  case 9: case 31: *(const char **)d = sysdir; return true;                      /* SYSTEM / SAVE dir */
  case 15: {                                                                      /* GET_VARIABLE */
    struct var *v = d;
    if (!strcmp(v->key, "snes9x_layer_3")) { v->value = (!core_ws && mask_on) ? "disabled" : "enabled"; return true; }
    if (!strcmp(v->key, "snes9x_som_widescreen")) { v->value = opt_ws; return true; }
    if (!strcmp(v->key, "snes9x_som_widescreen_fallback")) { v->value = opt_fb; return true; }
    if (!strcmp(v->key, "snes9x_som_hires")) { v->value = "native"; return true; }
    if (!strcmp(v->key, "snes9x_som_hud")) { v->value = hide_hud ? "hide_except_ring" : "off"; return true; }
    return false;
  }
  case 37: return true;                                                           /* SET_GEOMETRY: video() reads every frame */
  case 17: *(uint8_t *)d = (uint8_t)mask_dirty; mask_dirty = 0; return true;       /* GET_VARIABLE_UPDATE */
  case 3: *(uint8_t *)d = 0; return true;
  default: return false;
  }
}
/* Pixel of the source frame as 0xRRGGBB, whatever the core's pixel format is. */
static inline uint32_t src_px(const void *d, size_t pitch, unsigned x, unsigned y) {
  if (pixfmt == 1) return ((const uint32_t *)((const uint8_t *)d + y * pitch))[x] & 0xFFFFFF;
  unsigned p = ((const uint16_t *)((const uint8_t *)d + y * pitch))[x];
  if (pixfmt == 2) return ((p >> 11) * 255 / 31) << 16 | (((p >> 5) & 63) * 255 / 63) << 8 | ((p & 31) * 255 / 31);
  return (((p >> 10) & 31) * 255 / 31) << 16 | (((p >> 5) & 31) * 255 / 31) << 8 | ((p & 31) * 255 / 31);
}
/* Frames are kept at the core's width (256, or 512 on the hi-res menu and title screens); interlaced 448-high frames are
 * averaged down to 224.  The frontends draw the width as 256 pixels of 8:7. */
static unsigned v_w, v_h;
static void video(const void *d, unsigned w, unsigned h, size_t pitch) {
  v_w = w; v_h = h;
  if (!d) return;
  unsigned sy = h > 240 ? 2 : 1, oh = h / sy;
  if (w > 512 || oh > 240 || h % sy) return;
  for (unsigned y = 0; y < oh; y++)
    for (unsigned x = 0; x < w; x++) {
      uint32_t v = src_px(d, pitch, x, y * sy);
      if (sy == 2) {
        uint32_t u = src_px(d, pitch, x, y * sy + 1);
        v = ((v >> 16 & 255) + (u >> 16 & 255)) / 2 << 16 | ((v >> 8 & 255) + (u >> 8 & 255)) / 2 << 8 | ((v & 255) + (u & 255)) / 2;
      }
      frame[y * 512 + x] = v;
    }
  frame_w = w; frame_h = oh;
}
static void in_poll(void) {}
static int16_t in_state(unsigned port, unsigned dev, unsigned idx, unsigned id) {
  (void)idx;
  if (port != 0 || dev != 1 || id > 15) return 0;
  return (int16_t)((cur_mask >> id) & 1);
}

static double now_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

/* ---- cartridge SRAM (inn saves): <filesdir>/save_<romhash>.srm, loaded after retro_load_game, written when it changes */
static char srm_path[640];
static uint8_t *srm_shadow; static size_t srm_len; static unsigned srm_age, srm_writes;
static uint64_t fnv64(const uint8_t *p, size_t n) { uint64_t h = 1469598103934665603ull; for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; } return h; }
static void copy_file(const char *from, const char *to) {
  FILE *a = fopen(from, "rb"), *b;
  char buf[4096];
  size_t k;
  if (!a) return;
  b = fopen(to, "wb");
  if (b) { while ((k = fread(buf, 1, sizeof buf, a)) > 0) fwrite(buf, 1, k, b); fclose(b); }
  fclose(a);
}
/* The battery file belongs to the game, not to the ROM bytes: <base key>.srm, where the base key names the original or the generated run
 * (the start menu passes it), so switching gameplay options on or off keeps one save.  Without a key the whole ROM is hashed. */
static void srm_name(const uint8_t *rom, size_t n) {
  const char *key;
  char sk[96], root[640];
  snprintf(rom_id, sizeof rom_id, "%016llx", (unsigned long long)fnv64(rom, n));
  key = save_base[0] ? save_base : rom_id;
  snprintf(srm_path, sizeof srm_path, "%s/save_%s.srm", sysdir, key);
  if (!save_base[0] && access(srm_path, F_OK) != 0) {   /* saves of the earlier rounds: 32-bit hash of the sampled ROM */
    uint32_t h = 2166136261u ^ (uint32_t)n;
    char old[640];
    for (size_t i = 0; i < n && i < 0x10000; i += 3) h = (h ^ rom[i]) * 16777619u;
    snprintf(old, sizeof old, "%s/save_%08x.srm", sysdir, h);
    if (access(old, F_OK) == 0) { copy_file(old, srm_path); LOG("sram: migrated %s", old); }
  }
  snprintf(sk, sizeof sk, "%s%s%s", key, save_sig[0] ? "-" : "", save_sig);
  snprintf(root, sizeof root, "%s/states", sysdir);
  mkdir(root, 0775);
  if (sst_dir(root, sk, state_dir, sizeof state_dir) != SST_OK) state_dir[0] = 0;
}
static void srm_load(void) {
  free(srm_shadow); srm_shadow = NULL; srm_len = 0;
  void *m = r_mem ? r_mem(0) : NULL; size_t n = r_memsize ? r_memsize(0) : 0;
  if (!m || !n) { LOG("sram: core exposes none"); return; }
  srm_shadow = calloc(1, n); srm_len = n;
  FILE *f = fopen(srm_path, "rb");
  if (f) { size_t got = fread(srm_shadow, 1, n, f); fclose(f); memcpy(m, srm_shadow, n); LOG("sram: loaded %zu of %zu bytes from %s", got, n, srm_path); }
  else { memcpy(srm_shadow, m, n); LOG("sram: no save file yet (%zu bytes), will write %s", n, srm_path); }
}
static void srm_flush(int force) {
  void *m = r_mem ? r_mem(0) : NULL;
  if (!m || !srm_shadow || (!force && memcmp(m, srm_shadow, srm_len) == 0)) return;
  char tmp[700]; snprintf(tmp, sizeof tmp, "%s.tmp", srm_path);
  FILE *f = fopen(tmp, "wb");
  if (!f) { LOG("sram: cannot write %s", tmp); return; }
  size_t w = fwrite(m, 1, srm_len, f); fclose(f);
  if (w == srm_len && rename(tmp, srm_path) == 0) { memcpy(srm_shadow, m, srm_len); srm_writes++; LOG("sram: saved %zu bytes (write #%u)", srm_len, srm_writes); }
  else LOG("sram: write failed");
}

/* test hold (Native.pokeHold): rewrites bytes before and after every frame for a while, e.g. a status the game would normalise away */
static void apply_holds(void) {
  for (int i = 0; i < 4; i++)
    if (hold_n[i] > 0 && wram) { hold_n[i]--; memcpy((uint8_t *)wram + hold_addr[i], hold_d[i], hold_len[i]); }
}
/* One emulated frame with the direct layer around it; the menu pad of a direct menu jump is ORed into the input. */
static void core_frame(void) {
  unsigned saved = cur_mask;
  apply_holds();
  if (direct_ok) somd_pre_frame(&direct, (uint8_t *)wram);
#ifdef SOMD_HAS_MENU
  if (direct_ok) { cur_mask |= somd_menu_pad(&direct); audio_mute = somd_hide_video(&direct); }
#endif
  r_run();
  audio_mute = 0;
  cur_mask = saved;
  if (direct_ok) somd_post_frame(&direct, (uint8_t *)wram);
  apply_holds();
}

static void *loop(void *arg) {
  struct timespec next;
  (void)arg;
  clock_gettime(CLOCK_MONOTONIC, &next);
  while (running) {
    uint16_t m = 0;
    pthread_mutex_lock(&mu);
    if (have_game) {
      wram = r_mem(2);
      if (wram) {
        if (touch_x >= 0) {
          struct ux_dispatch ud;
          som_read_state(wram, &st);
          unsigned before = ux_g ? (ux_g->tab | ux_g->hero << 8 | ux_g->spirit << 16 | ux_g->pend_kind << 24) : 0;
          if (ux_glue_touch(touch_x, touch_y, touch_w, touch_h, &ud)) ux_dispatch(&ud);
          else if (ux_g && before != (unsigned)(ux_g->tab | ux_g->hero << 8 | ux_g->spirit << 16 | ux_g->pend_kind << 24)) ui_sfx(SFX_MOVE);
          touch_x = -1; ux_force = 1;
        }
        while (nav_qh != nav_qt) {
          struct ux_dispatch ud;
          int rq = nav_q[nav_qh & 15], pw = panel_w > 0 ? panel_w : touch_w, ph = panel_h > 0 ? panel_h : touch_h;
          nav_qh++;
          som_read_state(wram, &st);
          if (pw > 0 && ph > 0) {
            if (rq == 7) ux_glue_nav_toggle();
            else if (!ux_glue_nav_on()) { /* not in panel mode: nothing to do */ }
            else if (rq >= 1 && rq <= 4) { if (ux_glue_nav_move(rq - 1, pw, ph)) ui_sfx(SFX_MOVE); }
            else if (rq == 5 && ux_glue_nav_activate(pw, ph, &ud)) ux_dispatch(&ud);
            else if (rq == 6 && ux_glue_nav_back(pw, ph, &ud)) ux_dispatch(&ud);
            ux_force = 1;
          }
        }
        if (lp_x >= 0) {
          struct ux_dispatch ud;
          som_read_state(wram, &st);
          if (ux_glue_longpress(lp_x, lp_y, touch_w, touch_h, &ud)) ux_dispatch(&ud);
          lp_x = -1;
        }
        if (after_close_valid) {   /* run the command that was tapped inside the ring once the field is back */
          som_read_state(wram, &st);
          if (!som_macro_busy(&macro) && !somd_busy(&direct) && st.mode == SOM_MODE_FIELD && st.safe_to_inject) { struct ux_dispatch d = after_close; after_close_valid = 0; ux_dispatch(&d); }
          else if (++after_close_frames > 600) after_close_valid = 0;
        }
        if (deferred_valid) {
          if (!(direct_ok && somd_busy(&direct) && direct.kind == 4)) { struct ux_dispatch d = deferred; deferred_valid = 0; som_read_state(wram, &st); ux_dispatch(&d); }
          else if (++deferred_frames > 60) deferred_valid = 0;
        }
        {   /* sound mailbox watch (logcat 'snd'): every changed byte of $1E00..$1E3F, lists the game's own menu sound requests */
          static uint8_t mb_last[64];
          char sb[300]; int sn = 0;
          for (int i = 0; i < 64; i++) if (mb_last[i] != wram[0x1E00 + i]) { if (sn < (int)sizeof sb - 12) sn += snprintf(sb + sn, sizeof sb - (size_t)sn, " %02x:%02x>%02x", i, mb_last[i], wram[0x1E00 + i]); mb_last[i] = wram[0x1E00 + i]; }
          if (sn) LOG("snd f=%u mode=%s%s", total_frames, som_mode_name(st.mode), sb);
        }
        if (dbg_cmd_valid) { struct ux_dispatch d = dbg_cmd; dbg_cmd_valid = 0; som_read_state(wram, &st); ux_dispatch(&d); }
        if (hkq_n) do_hotkeys();
#ifdef SOMD_HAS_SOUND
        if (sfx_wait > 0) sfx_wait--;
        if (sfx_pending && !ux_sfx_mode()) sfx_pending = 0;
        if (sfx_pending && sfx_wait == 0 && direct_ok && !somd_busy(&direct)) {
          int r = somd_sound(&direct, wram, sfx_pending);
          if (r == SOMD_OK) { sfx_sent++; sfx_wait = 4; LOG("sfx %02x requested (mailbox %02x %02x %02x %02x)", sfx_pending, wram[0x1E00], wram[0x1E01], wram[0x1E02], wram[0x1E03]); }
          else { sfx_refused++; LOG("sfx %02x refused: %s", sfx_pending, somd_err_name((enum somd_err)r)); }
          sfx_pending = 0;
        }
#else
        sfx_pending = 0;
#endif
        if (back_wait > 0 && --back_wait == 0) {
          som_read_state(wram, &st);
          if (st.mode == SOM_MODE_RING) { pad_latch |= 1u << SOM_PAD_Y; latch_age = 0; back_wait = 24; }
        }
        if (som_macro_busy(&macro)) {
          m = som_macro_step(&macro, wram);
          if (m != last_m || !som_macro_busy(&macro)) LOG("macro f=%u phase=%d pad=0x%03x result=%s note=%s mode=%s", macro.frames, macro.phase, m,
                                                           som_result_name(macro.result), macro.note, som_mode_name(st.mode));
          last_m = m;
        }
      }
      cur_mask = pad_in | m | pad_latch;
      { static unsigned logged_mask; if (cur_mask != logged_mask) { LOG("pad mask %03x (in %03x macro %03x latch %03x)", cur_mask, pad_in, m, pad_latch); logged_mask = cur_mask; } }
      if (pad_stamp) {
        double l = now_ms() - (pad_t.tv_sec * 1e3 + pad_t.tv_nsec / 1e6);
        st_lat_sum += l; if (l > st_lat_max) st_lat_max = l; st_lat_n++; pad_stamp = 0;
      }
      double t0 = now_ms();
      core_frame();
#ifdef SOMD_HAS_MENU
      /* Round 3: a direct menu jump plays in real time; no emulated frame is skipped, hidden or muted (somd_hide_video is 0 by default). */
#endif
      if (direct_ok) {
        if (!direct_reported && !somd_busy(&direct)) {
          direct_reported = 1;
          LOG("direct done state=%d err=%s frames=%u latency %.1f ms (tap to result) note=%s", direct.state, somd_err_name(direct.err), direct.frames, now_ms() - direct_t0, direct.note);
          if (direct.state == SOMD_FAILED) set_toast(somd_err_name(direct.err));
        }
      }
      double tu0 = now_ms();
      if (pthread_mutex_trylock(&ux_mu) == 0) { ux_glue_frame(wram); if (nq_hold > 0) nq_hold--; if (nf_hold > 0) nf_hold--;
        if (nq_n && nq_hold == 0) { ux_glue_notice(nq_kind[0], nq_text[0]); memmove(nq_text[0], nq_text[1], sizeof nq_text[0] * (NQ - 1)); memmove(nq_kind, nq_kind + 1, sizeof nq_kind[0] * (NQ - 1)); nq_n--; nq_hold = nq_n ? 40 : 0; }
        if (nf_n && nf_hold == 0) { ux_glue_flash_entry(SOM_UX_FLASH_QUEUED, SOM_UX_BK_SPELL, nf_id[0]); memmove(nf_id, nf_id + 1, sizeof nf_id[0] * (NQ - 1)); nf_n--; nf_hold = 14; }
        pthread_mutex_unlock(&ux_mu); }
      double tu1 = now_ms(); st_ux_upd += tu1 - tu0; if (tu1 - tu0 > st_ux_upd_max) st_ux_upd_max = tu1 - tu0;
      if ((total_frames & 7) == 0 && ux_g && wram) {   /* enemy sprites for the target cards: pack or live PPU render, no camera */
        const uint8_t *vram = (const uint8_t *)r_mem(3);   /* RETRO_MEMORY_VIDEO_RAM; NULL is tolerated */
        if (pthread_mutex_trylock(&ux_mu) == 0) { fill_enemy_sprites(vram); pthread_mutex_unlock(&ux_mu); }
        st_crop += now_ms() - tu1; st_crop_n++;
      }
      if ((total_frames & 3) == 0 && ux_g && wram && direct_ok && pthread_mutex_trylock(&ux_mu) == 0) { spell_states_update(); pthread_mutex_unlock(&ux_mu); }
      queue_poll();
      bind_mode = som_bx_mode(ux_g);
      double dt = now_ms() - t0;
      st_run_sum += dt; if (dt > st_run_max) st_run_max = dt; st_frames++; total_frames++;
      if (++latch_age >= 5) { pad_latch = 0; latch_age = 0; }
            if (wram) som_read_state(wram, &st);
      /* The geometry never changes.  The HUD is hidden only in free field play by switching the core's BG3 layer off
       * (snes9x_layer_3), with hysteresis on entry and immediate release; every other mode shows the whole frame. */
      if (wram && st.valid && st.mode == SOM_MODE_FIELD && st.safe_to_inject) { if (field_run < 1000) field_run++; away_run = 0; }
      else {
        /* The ring and dialogs live on the HUD layer: release at once.  Fades, flashes and a momentarily unsafe hero
         * (not a menu) release only after MASK_RELEASE_FRAMES, so the HUD no longer blinks during fights. */
        int menu = wram && st.valid && (st.mode == SOM_MODE_RING || st.mode == SOM_MODE_EVENT);
        if (menu || ++away_run >= MASK_RELEASE_FRAMES) field_run = 0;
      }
      { int want = !core_ws && hide_hud && field_run >= MASK_ENTER_FRAMES;   /* widescreen core: no frontend toggling */
        if (want != mask_on) {
          LOG("hud-mask %d -> %d: mode %s safe %d fade E6=%02x E4=%02x event D0=%02x ring 7D=%02x away %d", mask_on, want, som_mode_name(st.mode), st.safe_to_inject,
              wram ? wram[0xE6] : 0, wram ? wram[0xE4] : 0, wram ? wram[0xD0] : 0, wram ? wram[0x7D] : 0, away_run);
          mask_on = want; mask_dirty = 1; } }
      if (++srm_age >= 300) { srm_age = 0; srm_flush(0); }
      if (st_frames >= 120) {
        snprintf(stats_line, sizeof stats_line, "core frame %ux%u hud-mask %d run avg %.2f max %.2f ms, input->core avg %.1f max %.1f ms, underruns %u, frames %llu | %s %s map %u HP %u/%u MP %u/%u",
                 v_w, v_h, mask_on, st_run_sum / st_frames, st_run_max, st_lat_n ? st_lat_sum / st_lat_n : 0.0, st_lat_max, st_under,
                 (unsigned long long)total_frames, som_mode_name(st.mode), st.safe_to_inject ? "safe" : "locked", st.map_id,
                 st.hero[0].hp, st.hero[0].hp_max, st.hero[0].mp, st.hero[0].mp_max);
        LOG("%s", stats_line);
        LOG("ux update avg %.2f max %.2f ms, enemy crops avg %.2f ms, bottom render %.2f ms", st_ux_upd / st_frames, st_ux_upd_max, st_crop_n ? st_crop / st_crop_n : 0.0, ux_draw_ms);
        st_ux_upd = st_ux_upd_max = st_crop = 0; st_crop_n = 0;
        LOG("audio rms %.0f peak %u samples %u silent-run-max %u ring-drops %u ring-fill %u sfx sent %u refused %u mailbox %02x %02x",
            am_n ? sqrt(am_sq / am_n) : 0.0, am_peak, am_n, am_zero_max, am_drop, (rhead - rtail) & 0xFFFF, sfx_sent, sfx_refused, wram ? wram[0x1E00] : 0, wram ? wram[0x1E01] : 0);
        am_sq = 0; am_n = am_peak = am_drop = am_zero_max = 0;
        st_run_sum = st_run_max = st_lat_sum = st_lat_max = 0; st_frames = st_lat_n = st_under = 0;
      }
    }
    pthread_mutex_unlock(&mu);
    next.tv_nsec += 16639000;   /* 60.0988 Hz */
    if (next.tv_nsec >= 1000000000L) { next.tv_nsec -= 1000000000L; next.tv_sec++; }
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
  }
  return NULL;
}

static void make_demo(void) {
  uint8_t *w = demo_wram;
  memset(w, 0, SOM_WRAM_SIZE);
  w[0xE000] = 1; w[0xE200] = 1; w[0xE400] = 1;
  w[0xE182] = 50; w[0xE184] = 50; w[0xE181] = 6; w[0xE1E4] = 2;
  w[0xE382] = 30; w[0xE384] = 45; w[0xE386] = 8; w[0xE387] = 9; w[0xE3C8] = 0x3F; w[0xE3CD] = 0x3F;
  w[0xE582] = 20; w[0xE584] = 40; w[0xE586] = 5; w[0xE587] = 9; w[0xE5C8] = 0x3F;
  w[0xE4] = 1; w[0xE6] = 0x0F; w[0xD9] = 1; w[0xDC] = 131;
  memset(w + 0xCC48, 0xFF, 12); w[0xCC48] = 0x60; w[0xCC4A] = 0x45;
  memset(w + 0xCC54, 0xFF, 8); w[0xCC55] = 0x40; w[0xCC56] = 2; w[0xCC5B] = 3;
  wram = w;
  som_read_state(w, &st);
}

JNIEXPORT jstring JNICALL Java_org_somdecomp_companion_Native_init(JNIEnv *e, jclass c, jstring jcore, jstring jsys) {
  const char *sys = (*e)->GetStringUTFChars(e, jsys, NULL);
  (void)c;
  strncpy(sysdir, sys, sizeof sysdir - 1);
  (*e)->ReleaseStringUTFChars(e, jsys, sys);
  som_ui_init(&ui);
  ux_glue_init(NULL, 0, NULL, 0);
  som_ux_set_options_ext(ux_g, 1);
  som_macro_init(&macro);
  make_demo();
  if (!jcore) return (*e)->NewStringUTF(e, "Demo mode: no libretro core in the APK");
  const char *path = (*e)->GetStringUTFChars(e, jcore, NULL);
  core_ws = strstr(path, "ws_core") != NULL || strstr(path, "widescreen") != NULL;   /* the widescreen core hides the HUD itself (snes9x_som_hud) */
  core = dlopen(path, RTLD_NOW);
  (*e)->ReleaseStringUTFChars(e, jcore, path);
  if (!core) return (*e)->NewStringUTF(e, dlerror());
#define SYM(v, n) *(void **)&v = dlsym(core, n)
  void (*sv)(void *);
  SYM(r_init, "retro_init"); SYM(r_run, "retro_run"); SYM(r_deinit, "retro_deinit"); SYM(r_load, "retro_load_game");
  SYM(r_unser, "retro_unserialize"); SYM(r_ser, "retro_serialize"); SYM(r_sersize, "retro_serialize_size");
  SYM(r_av, "retro_get_system_av_info"); SYM(r_mem, "retro_get_memory_data"); SYM(r_memsize, "retro_get_memory_size");
  SYM(sv, "retro_set_environment"); sv((void *)env);
  SYM(sv, "retro_set_video_refresh"); sv((void *)video);
  SYM(sv, "retro_set_audio_sample"); sv((void *)audio_one);
  SYM(sv, "retro_set_audio_sample_batch"); sv((void *)audio_batch);
  SYM(sv, "retro_set_input_poll"); sv((void *)in_poll);
  SYM(sv, "retro_set_input_state"); sv((void *)in_state);
  if (!r_init || !r_run || !r_load || !r_mem) return (*e)->NewStringUTF(e, "core misses libretro symbols");
  r_init();
  return (*e)->NewStringUTF(e, "Core loaded");
}

JNIEXPORT jstring JNICALL Java_org_somdecomp_companion_Native_loadRom(JNIEnv *e, jclass c, jbyteArray rom) {
  (void)c;
  if (!core) return (*e)->NewStringUTF(e, "No core: demo mode only");
  jsize n = (*e)->GetArrayLength(e, rom);
  pthread_mutex_lock(&mu);
  free(rom_copy);
  rom_copy = malloc((size_t)n);
  (*e)->GetByteArrayRegion(e, rom, 0, n, (jbyte *)rom_copy);
  struct retro_game_info gi = {"rom.sfc", rom_copy, (size_t)n, NULL};
  have_game = r_load(&gi);
  if (have_game) { srm_name(rom_copy, (size_t)n); srm_load(); state_show_all(); }
  mask_on = 0; mask_dirty = 1; field_run = 0;
  if (have_game) {
    struct retro_av av;
    r_av(&av);
    audio_open(av.timing.sr);
    som_macro_init(&macro);
    somd_init(&direct, rom_copy, (size_t)n);
    ux_glue_init(NULL, 0, rom_copy, (size_t)n);
    direct_ok = direct.rom_ok; direct_reported = 1; 
    LOG("direct mode %s", direct_ok ? "ready" : "unavailable (unknown ROM), ring macro only");
  }
  pthread_mutex_unlock(&mu);
  return (*e)->NewStringUTF(e, have_game ? "ROM loaded" : "Core rejected the ROM");
}

JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_start(JNIEnv *e, jclass c) {
  (void)e; (void)c;
  if (running) return;
  running = 1;
  pthread_create(&thr, NULL, loop, NULL);
}
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_stop(JNIEnv *e, jclass c) {
  (void)e; (void)c;
  if (!running) return;
  running = 0;
  pthread_join(thr, NULL);
  if (have_game) srm_flush(0);
  if (astream) { AAudioStream_close(astream); astream = NULL; }
}
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_setPad(JNIEnv *e, jclass c, jint m) {
  (void)e; (void)c;
  unsigned n = (unsigned)m & 0xFFF;
  if (n & ~pad_in) { clock_gettime(CLOCK_MONOTONIC, &pad_t); pad_stamp = 1; pad_latch |= n & ~pad_in; latch_age = 0; }
  pad_in = n;
}
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_setHideHud(JNIEnv *e, jclass c, jboolean on) { (void)e; (void)c; hide_hud = on; mask_dirty = 1; }
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_setWidescreen(JNIEnv *e, jclass c, jstring ws, jstring fb) {
  (void)c;
  const char *a = (*e)->GetStringUTFChars(e, ws, NULL), *b = (*e)->GetStringUTFChars(e, fb, NULL);
  strncpy(opt_ws, a, sizeof opt_ws - 1); strncpy(opt_fb, b, sizeof opt_fb - 1);
  (*e)->ReleaseStringUTFChars(e, ws, a); (*e)->ReleaseStringUTFChars(e, fb, b);
  mask_dirty = 1;
}
JNIEXPORT jstring JNICALL Java_org_somdecomp_companion_Native_stats(JNIEnv *e, jclass c) { (void)c; return (*e)->NewStringUTF(e, stats_line); }
static uint32_t *ux_prev; static size_t ux_prev_n;
/* Renders the bottom screen into the bitmap and reports whether the picture differs from the previous call, so the view
 * only redraws (and re-uploads a 5 MB texture) when something changed.  Called from a worker thread. */
JNIEXPORT jboolean JNICALL Java_org_somdecomp_companion_Native_renderUx(JNIEnv *e, jclass c, jobject bmp) {
  AndroidBitmapInfo bi;
  void *px;
  (void)c;
  if (AndroidBitmap_getInfo(e, bmp, &bi) != 0 || AndroidBitmap_lockPixels(e, bmp, &px) != 0) return JNI_FALSE;
  int w = (int)bi.width, h = (int)bi.height;
  uint32_t *o = px;
  size_t n = (size_t)w * (size_t)h;
  panel_w = w; panel_h = h;
  pthread_mutex_lock(&ux_mu);
  double t0 = now_ms();
  if (!have_game && ux_g) ux_glue_frame(demo_wram);
  /* Draw only when the UX layer says the picture would differ (state, pulse, press, notice) or a touch / a new surface asks for it. */
  int need = ux_force || n != ux_prev_n || !ux_prev || (ux_g && ux_glue_needs_draw(w, h));
  ux_force = 0;
  if (!need) { pthread_mutex_unlock(&ux_mu); AndroidBitmap_unlockPixels(e, bmp); return JNI_FALSE; }
  if (ux_g) { if (n != ux_prev_n) ux_glue_first_frame(o, w, h); else ux_glue_draw(&macro, o, w, h); }
  ux_draw_ms = now_ms() - t0;
  pthread_mutex_unlock(&ux_mu);
  for (int i = 0; i < w * h; i++) {   /* ARGB -> RGBA_8888 bytes;  */
    uint32_t v = o[i];
    o[i] = 0xFF000000u | (v & 0xFF) << 16 | (v & 0xFF00) | (v >> 16 & 0xFF);
  }
  if (n != ux_prev_n) { free(ux_prev); ux_prev = malloc(n * 4); ux_prev_n = ux_prev ? n : 0; }
  int changed = !ux_prev || memcmp(ux_prev, o, n * 4) != 0;
  if (changed && ux_prev) memcpy(ux_prev, o, n * 4);
  AndroidBitmap_unlockPixels(e, bmp);
  return changed ? JNI_TRUE : JNI_FALSE;
}
/* Pressed look: touch-down returns 1 when an enabled control is under the finger (click sound here, haptic pulse in Java). */
JNIEXPORT jboolean JNICALL Java_org_somdecomp_companion_Native_pressUx(JNIEnv *e, jclass c, jint x, jint y, jint w, jint h, jboolean down) {
  int hit = 0;
  (void)e; (void)c;
  pthread_mutex_lock(&mu);
  if (have_game) {
    if (down) { hit = ux_glue_touch_down(x, y, w, h); if (hit) ui_sfx(SFX_MOVE); }
    else ux_glue_touch_up();
    ux_force = 1;
  }
  pthread_mutex_unlock(&mu);
  return hit ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_moveUx(JNIEnv *e, jclass c, jint x, jint y, jint w, jint h) {
  (void)e; (void)c;
  pthread_mutex_lock(&mu);
  if (have_game) { ux_glue_touch_move(x, y, w, h); ux_force = 1; }
  pthread_mutex_unlock(&mu);
}
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_touchUx(JNIEnv *e, jclass c, jint x, jint y, jint w, jint h) {
  (void)e; (void)c;
  pthread_mutex_lock(&mu);
  if (have_game) { touch_x = x; touch_y = y; touch_w = w; touch_h = h; ux_force = 1; }
  pthread_mutex_unlock(&mu);
}
/* Pad panel mode: request 1..4 move the focus ring (up, down, left, right), 5 presses the focused target, 6 is back, 7 toggles the mode. */
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_navRequest(JNIEnv *e, jclass c, jint req) { (void)e; (void)c; if (nav_qt - nav_qh < 16) { nav_q[nav_qt & 15] = (int)req; nav_qt++; } }
JNIEXPORT jboolean JNICALL Java_org_somdecomp_companion_Native_navOn(JNIEnv *e, jclass c) {
  (void)e; (void)c;
  return ux_glue_nav_on() ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_loadSprites(JNIEnv *e, jclass c, jbyteArray b) {
  (void)c;
  jsize n = (*e)->GetArrayLength(e, b);
  pthread_mutex_lock(&mu);
  free(es_pack);
  es_pack = malloc((size_t)n);
  (*e)->GetByteArrayRegion(e, b, 0, n, (jbyte *)es_pack);
  es_pack_len = (size_t)n;
  pthread_mutex_unlock(&mu);
}
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_loadPack(JNIEnv *e, jclass c, jbyteArray b) {
  (void)c;
  jsize n = (*e)->GetArrayLength(e, b);
  pthread_mutex_lock(&mu);
  free(pack_blob);
  pack_blob = malloc((size_t)n);
  (*e)->GetByteArrayRegion(e, b, 0, n, (jbyte *)pack_blob);
  pack_len = (size_t)n;
  ux_glue_init(pack_blob, pack_len, NULL, 0);
  pthread_mutex_unlock(&mu);
}
JNIEXPORT jdouble JNICALL Java_org_somdecomp_companion_Native_uxDrawMs(JNIEnv *e, jclass c) { (void)e; (void)c; return ux_draw_ms; }
JNIEXPORT jint JNICALL Java_org_somdecomp_companion_Native_frameWidth(JNIEnv *e, jclass c) { (void)e; (void)c; return (jint)frame_w; }
JNIEXPORT jint JNICALL Java_org_somdecomp_companion_Native_frameHeight(JNIEnv *e, jclass c) {
  (void)e; (void)c;
  return (jint)frame_h;
}
JNIEXPORT jboolean JNICALL Java_org_somdecomp_companion_Native_copyFrame(JNIEnv *e, jclass c, jobject bmp) {
  void *px;
  AndroidBitmapInfo bi;
  (void)c;
  if (!frame_w || AndroidBitmap_getInfo(e, bmp, &bi) != 0 || AndroidBitmap_lockPixels(e, bmp, &px) != 0) return JNI_FALSE;
  pthread_mutex_lock(&mu);
  for (uint32_t y = 0; y < bi.height && y < frame_h; y++)
    for (uint32_t x = 0; x < bi.width && x < 512; x++) {
      uint32_t v = frame[y * 512 + x];
      ((uint32_t *)((uint8_t *)px + y * bi.stride))[x] = 0xFF000000u | (v & 0xFF) << 16 | (v & 0xFF00) | (v >> 16 & 0xFF);
    }
  pthread_mutex_unlock(&mu);
  /* field frames only: the 512 px text frames keep their sharp font (round 3c, see som_dedither.h) */
  if (frame_w < 512 && bi.width <= 1024 && bi.height <= 256) som_dedither_rows((uint32_t *)px, (int)bi.width, (int)bi.height, (int)(bi.stride / 4));
  AndroidBitmap_unlockPixels(e, bmp);
  return JNI_TRUE;
}

/* Test hook (debuggable builds only, see MainActivity): writes bytes into the core's WRAM.  The companion module
 * itself never writes; this exists to build the same item/weapon fixtures as the PC tests on a real device. */
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_poke(JNIEnv *e, jclass c, jint addr, jbyteArray data) {
  (void)c;
  jsize n = (*e)->GetArrayLength(e, data);
  pthread_mutex_lock(&mu);
  if (have_game && r_mem && addr >= 0x1000000) {   /* 0x1000000 + offset: cartridge SRAM (save-file test) */
    uint8_t *sr = r_mem(0); size_t off = (size_t)addr - 0x1000000;
    if (sr && off + (size_t)n <= r_memsize(0)) (*e)->GetByteArrayRegion(e, data, 0, n, (jbyte *)(sr + off));
    pthread_mutex_unlock(&mu); return;
  }
  uint8_t *w = have_game && r_mem ? r_mem(2) : NULL;
  if (w && addr >= 0 && (size_t)addr + (size_t)n <= SOM_WRAM_SIZE) (*e)->GetByteArrayRegion(e, data, 0, n, (jbyte *)(w + addr));
  pthread_mutex_unlock(&mu);
}

JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_pokeHold(JNIEnv *e, jclass c, jint addr, jbyteArray data, jint frames) {
  (void)c;
  jsize n = (*e)->GetArrayLength(e, data);
  if (n > 8 || addr < 0 || (size_t)addr + (size_t)n > SOM_WRAM_SIZE) return;
  pthread_mutex_lock(&mu);
  int slot = -1;
  for (int i = 0; i < 4; i++) if (hold_n[i] > 0 && hold_addr[i] == addr) slot = i;
  for (int i = 0; slot < 0 && i < 4; i++) if (hold_n[i] <= 0) slot = i;
  if (slot >= 0) { (*e)->GetByteArrayRegion(e, data, 0, n, (jbyte *)hold_d[slot]); hold_addr[slot] = addr; hold_len[slot] = n; hold_n[slot] = frames; }
  pthread_mutex_unlock(&mu);
}

/* Loads a libretro save state (same core build, same ROM already loaded).  Returns a status string. */
JNIEXPORT jstring JNICALL Java_org_somdecomp_companion_Native_loadState(JNIEnv *e, jclass c, jbyteArray st_) {
  (void)c;
  char msg[96];
  jsize n = (*e)->GetArrayLength(e, st_);
  pthread_mutex_lock(&mu);
  if (!have_game || !r_unser) snprintf(msg, sizeof msg, "no game loaded");
  else {
    jbyte *b = (*e)->GetByteArrayElements(e, st_, NULL);
    size_t want = r_sersize ? r_sersize() : 0;
    bool ok = r_unser(b, (size_t)n);
    (*e)->ReleaseByteArrayElements(e, st_, b, JNI_ABORT);
    snprintf(msg, sizeof msg, "state %s (%d bytes, core wants %zu)", ok ? "loaded" : "REJECTED", (int)n, want);
    if (ok) after_state_load();
  }
  pthread_mutex_unlock(&mu);
  LOG("%s", msg);
  return (*e)->NewStringUTF(e, msg);
}


/* Debug probe used to derive the per-mode button table: from the current state, runs `frames` frames with no input and
 * again with each SNES button held for `hold` frames, then compares WRAM (ignoring the joypad words themselves).
 * The emulator is deterministic, so any difference beyond the baseline noise means the game consumed the button. */
static int probe_ignore(int a) {
  return (a >= 0x00 && a <= 0x02) || (a >= 0x42 && a <= 0x47) || (a >= 0xCC && a <= 0xCE) || (a >= 0xE9 && a <= 0xEB) || (a >= 0x100 && a <= 0x1FFF);
}
JNIEXPORT jstring JNICALL Java_org_somdecomp_companion_Native_probe(JNIEnv *e, jclass c, jint frames, jint hold) {
  (void)c;
  static char out[1024];
  pthread_mutex_lock(&mu);
  if (!have_game || !r_ser || !r_unser || !r_sersize) { pthread_mutex_unlock(&mu); return (*e)->NewStringUTF(e, "no game"); }
  size_t n = r_sersize();
  void *st = malloc(n);
  uint8_t *base = malloc(SOM_WRAM_SIZE), *cur = malloc(SOM_WRAM_SIZE), *base2 = malloc(SOM_WRAM_SIZE);
  r_ser(st, n);
  unsigned saved = cur_mask;
  const char *names[12] = {"B", "Y", "SELECT", "START", "UP", "DOWN", "LEFT", "RIGHT", "A", "X", "L", "R"};
  int len = 0;
  for (int pass = -2; pass < 12; pass++) {
    r_unser(st, n);
    for (int f = 0; f < frames; f++) {
      cur_mask = (pass >= 0 && f < hold) ? (1u << pass) : 0;
      r_run();
    }
    const uint8_t *w = r_mem(2);
    uint8_t *dst = pass == -2 ? base : pass == -1 ? base2 : cur;
    memcpy(dst, w, SOM_WRAM_SIZE);
    if (pass >= 0) {
      int nd = 0, nb = 0, first = -1;
      for (int a = 0; a < SOM_WRAM_SIZE; a++) {
        if (probe_ignore(a)) continue;
        if (cur[a] != base[a] && base2[a] == base[a]) { nd++; if (first < 0) first = a; }
      }
      for (int a = 0; a < SOM_WRAM_SIZE; a++) if (!probe_ignore(a) && base2[a] != base[a]) nb++;
      len += snprintf(out + len, sizeof out - len, "%s:%d ", names[pass], nd);
      char al[160]; int an = 0; al[0] = 0;
      for (int a = 0; a < SOM_WRAM_SIZE && nd <= 12 && an < 140; a++)
        if (!probe_ignore(a) && cur[a] != base[a] && base2[a] == base[a]) an += snprintf(al + an, sizeof al - an, "%05X:%02X>%02X ", a, base[a], cur[a]);
      LOG("probe %s diff=%d first=%05X noise=%d %s", names[pass], nd, first < 0 ? 0 : first, nb, al);
    }
  }
  r_unser(st, n);
  cur_mask = saved;
  free(st); free(base); free(cur); free(base2);
  pthread_mutex_unlock(&mu);
  LOG("probe frames=%d hold=%d: %s", frames, hold, out);
  return (*e)->NewStringUTF(e, out);
}

/* ---- shoulder natives: the four fixed L1 R1 L2 R2 buttons (docs/companion-bindings-20261017.md, replaced by the shoulder-slot model) */
static const struct { int key, in; } KEYMAP[] = {   /* Android KEYCODE_* to the shared input ids */
  {96, SOM_IN_A}, {97, SOM_IN_B}, {98, SOM_IN_C}, {99, SOM_IN_X}, {100, SOM_IN_Y}, {101, SOM_IN_Z}, {102, SOM_IN_L1}, {103, SOM_IN_R1},
  {104, SOM_IN_L2}, {105, SOM_IN_R2}, {106, SOM_IN_THUMBL}, {107, SOM_IN_THUMBR}, {108, SOM_IN_START}, {109, SOM_IN_SELECT}, {110, SOM_IN_MODE},
  {19, SOM_IN_DUP}, {20, SOM_IN_DDOWN}, {21, SOM_IN_DLEFT}, {22, SOM_IN_DRIGHT},
};
JNIEXPORT jint JNICALL Java_org_somdecomp_companion_Native_bindKeyInput(JNIEnv *e, jclass c, jint key) {
  (void)e; (void)c;
  for (size_t i = 0; i < sizeof KEYMAP / sizeof *KEYMAP; i++) if (KEYMAP[i].key == key) return KEYMAP[i].in;
  return 0;
}
JNIEXPORT jint JNICALL Java_org_somdecomp_companion_Native_bindGameMask(JNIEnv *e, jclass c, jint inputs) { (void)e; (void)c; return som_bind_game_mask(SOM_PLAT_THOR, (uint32_t)inputs); }
JNIEXPORT jint JNICALL Java_org_somdecomp_companion_Native_bindStick(JNIEnv *e, jclass c, jint prev, jfloat x, jfloat y) { (void)e; (void)c; return (jint)som_bind_stick_dirs((unsigned)prev, x, y); }
JNIEXPORT jboolean JNICALL Java_org_somdecomp_companion_Native_bindTrigger(JNIEnv *e, jclass c, jboolean prev, jfloat v) { (void)e; (void)c; return som_bind_trigger(prev, v) ? JNI_TRUE : JNI_FALSE; }
/* Feeds the held inputs.  Returns the swallow mask in the low 32 bits (the event bits are always 0 now: there is no capture). */
JNIEXPORT jlong JNICALL Java_org_somdecomp_companion_Native_bindUpdate(JNIEnv *e, jclass c, jint held, jlong now_ms) {
  (void)e; (void)c;
  struct som_bind_out o;
  pthread_mutex_lock(&mu);
  som_bind_update(&bx.ctx, (uint32_t)held, (uint32_t)now_ms, bind_mode, &o);
  for (int i = 0; i < o.nfire && i < SOM_BIND_FIRES; i++) { int sh = som_bx_shoulder_of(&bx.ctx.b[o.fire[i]]); if (sh >= 0 && hkq_n < 8) hkq[hkq_n++] = sh; }
  pthread_mutex_unlock(&mu);
  return (jlong)(uint32_t)o.swallow | ((jlong)o.event << 32);
}

JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_bindLoad(JNIEnv *e, jclass c, jstring text) {
  (void)e; (void)c; (void)text;   /* stored bindings of the old model are dropped: the shoulder slots live in the quick slots */
  pthread_mutex_lock(&mu);
  som_bx_init(&bx, SOM_PLAT_THOR);
  ux_glue_shoulder_labels("L1", "R1", "L2", "R2");
  pthread_mutex_unlock(&mu);
}
/* Long press on the bottom screen (about 0.5 s): same dispatch path as a tap. */
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_longPressUx(JNIEnv *e, jclass c, jint x, jint y, jint w, jint h) {
  (void)e; (void)c;
  pthread_mutex_lock(&mu);
  if (have_game) { lp_x = x; lp_y = y; touch_w = w; touch_h = h; ux_force = 1; }
  pthread_mutex_unlock(&mu);
}
/* ROM variant id for per-ROM bindings: size and the header title/checksum bytes. */
JNIEXPORT jstring JNICALL Java_org_somdecomp_companion_Native_romVariant(JNIEnv *e, jclass c) {
  (void)c;
  char b[48];
  pthread_mutex_lock(&mu);
  som_bind_rom_variant(rom_copy, rom_copy ? direct.rom_size : 0, b, sizeof b);
  pthread_mutex_unlock(&mu);
  return (*e)->NewStringUTF(e, b);
}
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_toast(JNIEnv *e, jclass c, jstring t) {
  (void)c;
  const char *u = (*e)->GetStringUTFChars(e, t, NULL);
  pthread_mutex_lock(&mu);
  set_toast(u);
  pthread_mutex_unlock(&mu);
  (*e)->ReleaseStringUTFChars(e, t, u);
}

JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_setUiSounds(JNIEnv *e, jclass c, jboolean on) { (void)e; (void)c; sfx_on = on ? 1 : 0; }

/* Debug: the ring state the loop last read (test scripts navigate by it). */
JNIEXPORT jstring JNICALL Java_org_somdecomp_companion_Native_ringInfo(JNIEnv *e, jclass c) {
  (void)c;
  char b[160];
  pthread_mutex_lock(&mu);
  snprintf(b, sizeof b, "ringq open=%d ready=%d owner=%d cat=%d cursor=%d count=%d pad_hero=%d mode=%s ux=%d", st.ring.open, st.ring.ready, st.ring.owner,
           st.ring.category, st.ring.cursor, st.ring.count, st.pad_hero, som_mode_name(st.mode), ux_g ? (int)ux_g->mode : -1);
  pthread_mutex_unlock(&mu);
  LOG("%s", b);
  return (*e)->NewStringUTF(e, b);
}

/* ---- options, quick slots and events (the Java side persists them; the UX layer draws and edits them) */
JNIEXPORT jint JNICALL Java_org_somdecomp_companion_Native_pollEvent(JNIEnv *e, jclass c) {
  (void)e; (void)c;
  pthread_mutex_lock(&mu);
  int r = -1;
  if (evq_n > 0) { r = evq[0]; memmove(evq, evq + 1, sizeof(int) * (size_t)(--evq_n)); }
  pthread_mutex_unlock(&mu);
  return r;
}
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_setOption(JNIEnv *e, jclass c, jint id, jint val, jstring text) {
  (void)c;
  const char *t = text ? (*e)->GetStringUTFChars(e, text, NULL) : NULL;
  pthread_mutex_lock(&mu);
  ux_glue_option(id, val, t);
  if (id == SOM_UX_OPT_UI_SOUNDS) sfx_on = val ? 1 : 0;
  pthread_mutex_unlock(&mu);
  if (t) (*e)->ReleaseStringUTFChars(e, text, t);
}
/* Quick slots as text "slot kind id" per line. */
JNIEXPORT jstring JNICALL Java_org_somdecomp_companion_Native_quickSave(JNIEnv *e, jclass c) {
  (void)c;
  char b[SOM_UX_QUICK_N * 20 + 1];
  int n = 0;
  b[0] = 0;
  pthread_mutex_lock(&mu);
  for (int i = 0; i < SOM_UX_QUICK_N && ux_g; i++) {
    int k, id;
    if (som_ux_quick_get(ux_g, i, &k, &id)) n += snprintf(b + n, sizeof b - (size_t)n, "%d %d %d %d\n", i, k, id, som_ux_quick_policy(ux_g, i));
  }
  quick_dirty = 0;
  pthread_mutex_unlock(&mu);
  return (*e)->NewStringUTF(e, b);
}
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_quickLoad(JNIEnv *e, jclass c, jstring text) {
  (void)c;
  pthread_mutex_lock(&mu);
  for (int i = 0; i < SOM_UX_QUICK_N && ux_g; i++) som_ux_quick_set(ux_g, i, 0, 0);
  if (text && ux_g) {
    const char *u = (*e)->GetStringUTFChars(e, text, NULL);
    char cp[256];
    snprintf(cp, sizeof cp, "%s", u);
    for (char *q = cp; *q; q++) if (*q == ';') *q = '\n';   /* ';' separates slots when typed on an adb command line */
    char *save = NULL;
    for (char *ln = strtok_r(cp, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
      int slot, k, id, pol = 0, n = sscanf(ln, "%d %d %d %d", &slot, &k, &id, &pol);
      if (n >= 3 && slot >= 0 && slot < SOM_UX_QUICK_N) { som_ux_quick_set(ux_g, slot, k, id); if (n == 4) ux_glue_quick_policy(slot, pol); }
    }
    (*e)->ReleaseStringUTFChars(e, text, u);
  }
  quick_dirty = 0;
  pthread_mutex_unlock(&mu);
}
JNIEXPORT jboolean JNICALL Java_org_somdecomp_companion_Native_quickDirty(JNIEnv *e, jclass c) { (void)e; (void)c; return quick_dirty ? JNI_TRUE : JNI_FALSE; }

/* Game identity (round 3): the hash of the three hero names, the one-shot "new game" / "loaded game" event, and the discovered-entry bits.  The Java side keeps the
 * quick slots and the discovered bits per game key and wipes both on a new game. */
JNIEXPORT jstring JNICALL Java_org_somdecomp_companion_Native_gameKey(JNIEnv *e, jclass c) {
  (void)c;
  char b[16]; b[0] = 0;
  pthread_mutex_lock(&mu);
  if (ux_g && som_ux_game_key(ux_g)) snprintf(b, sizeof b, "%08x", som_ux_game_key(ux_g));
  pthread_mutex_unlock(&mu);
  return (*e)->NewStringUTF(e, b);
}
JNIEXPORT jint JNICALL Java_org_somdecomp_companion_Native_gameEvent(JNIEnv *e, jclass c) {
  (void)e; (void)c;
  pthread_mutex_lock(&mu);
  int ev = ux_g ? som_ux_game_event(ux_g) : 0;
  pthread_mutex_unlock(&mu);
  return ev;
}
JNIEXPORT jint JNICALL Java_org_somdecomp_companion_Native_seenGet(JNIEnv *e, jclass c) {
  (void)e; (void)c;
  pthread_mutex_lock(&mu);
  jint v = ux_g ? (jint)som_ux_seen_get(ux_g) : 0;
  pthread_mutex_unlock(&mu);
  return v;
}
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_seenSet(JNIEnv *e, jclass c, jint v) {
  (void)e; (void)c;
  pthread_mutex_lock(&mu);
  if (ux_g) som_ux_seen_set(ux_g, (uint32_t)v);
  pthread_mutex_unlock(&mu);
}
/* New game: every quick slot is emptied (the hotkeys of the previous game must not carry over). */
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_quickClear(JNIEnv *e, jclass c) {
  (void)e; (void)c;
  pthread_mutex_lock(&mu);
  for (int i = 0; i < SOM_UX_QUICK_N && ux_g; i++) som_ux_quick_set(ux_g, i, 0, 0);
  quick_dirty = 0;
  pthread_mutex_unlock(&mu);
}

/* Test hooks: run the command a bottom-screen tap would send (kind 1 equip, 2 item, 3 cast), and print the decoded state. */
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_debugCmd(JNIEnv *e, jclass c, jint kind, jint hero, jint arg, jint spell, jint tk, jint ts) {
  (void)e; (void)c;
  pthread_mutex_lock(&mu);
  memset(&dbg_cmd, 0, sizeof dbg_cmd);
  dbg_cmd.kind = SOM_UX_ACT_CMD; dbg_cmd.cmd.kind = (enum som_cmd_kind)kind; dbg_cmd.cmd.hero = (uint8_t)hero; dbg_cmd.cmd.arg = (uint8_t)arg; dbg_cmd.cmd.spell = (uint8_t)spell;
  dbg_cmd.target_kind = tk; dbg_cmd.target_slot = ts;
  dbg_cmd_valid = 1;
  pthread_mutex_unlock(&mu);
}
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_stateDump(JNIEnv *e, jclass c) {
  (void)e; (void)c;
  char b[600];
  int n;
  pthread_mutex_lock(&mu);
  if (wram) som_read_state(wram, &st);
  n = snprintf(b, sizeof b, "state mode=%s safe=%d w=%d,%d,%d hp=%u,%u,%u mp=%u,%u,%u st=%04x,%04x,%04x pad_hero=%d ", som_mode_name(st.mode), st.safe_to_inject,
               st.hero[0].weapon_type, st.hero[1].weapon_type, st.hero[2].weapon_type, st.hero[0].hp, st.hero[1].hp, st.hero[2].hp, st.hero[0].mp, st.hero[1].mp, st.hero[2].mp,
               st.hero[0].status, st.hero[1].status, st.hero[2].status, st.pad_hero);
  n += snprintf(b + n, sizeof b - (size_t)n, "wm=");
  for (int i = 0; i < SOM_WEAPON_TYPES; i++) n += snprintf(b + n, sizeof b - (size_t)n, "%02x", st.weapon_slot[i]);
  for (int h = 0; h < 3 && wram; h++) {   /* weapon fields per hero: +1E4 type, +1E3 / +1E8 rows, owner-derived type from $CC54 */
    int held = -1;
    for (int t = 0; t < 8; t++) { unsigned v = st.weapon_slot[t]; if (v != 0xFF && (v & 0xC0) == (unsigned)((h + 1) << 6)) held = t; }
    n += snprintf(b + n, sizeof b - (size_t)n, " h%d[t=%d r3=%d r8=%d held=%d]", h, wram[0xE000 + 0x200 * h + 0x1E4], wram[0xE000 + 0x200 * h + 0x1E3], wram[0xE000 + 0x200 * h + 0x1E8], held);
  }
  n += snprintf(b + n, sizeof b - (size_t)n, " items=");
  for (int i = 0; i < SOM_ITEM_RECORDS && n < (int)sizeof b - 12; i++) n += snprintf(b + n, sizeof b - (size_t)n, "%d:%d ", st.item[i].id, st.item[i].qty);
  pthread_mutex_unlock(&mu);
  LOG("%s", b);
}

/* ---- Thor round 4: battery save, save states, credits ---- */
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_setSaveKeys(JNIEnv *e, jclass c, jstring base, jstring sig) {
  const char *a = base ? (*e)->GetStringUTFChars(e, base, NULL) : NULL, *b = sig ? (*e)->GetStringUTFChars(e, sig, NULL) : NULL;
  (void)c;
  pthread_mutex_lock(&mu);
  snprintf(save_base, sizeof save_base, "%s", a ? a : "");
  snprintf(save_sig, sizeof save_sig, "%s", b ? b : "");
  for (char *p = save_base; *p; p++) if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '-' || *p == '_')) *p = '_';
  for (char *p = save_sig; *p; p++) if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))) *p = '0';
  pthread_mutex_unlock(&mu);
  if (a) (*e)->ReleaseStringUTFChars(e, base, a);
  if (b) (*e)->ReleaseStringUTFChars(e, sig, b);
}
/* Writes the cartridge RAM now when it changed (pause, stop, a save-state chord). */
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_flushSave(JNIEnv *e, jclass c) {
  (void)e; (void)c;
  pthread_mutex_lock(&mu);
  if (have_game) srm_flush(0);
  pthread_mutex_unlock(&mu);
}
JNIEXPORT jint JNICALL Java_org_somdecomp_companion_Native_stateSave(JNIEnv *e, jclass c, jint slot) {
  int rc;
  (void)e; (void)c;
  pthread_mutex_lock(&mu);
  rc = do_state_save(slot);
  if (rc == SST_OK && ux_glue_option_value(SOM_UX_OPT_HAPTICS)) ev_push(EV_TICK);
  pthread_mutex_unlock(&mu);
  return rc;
}
JNIEXPORT jint JNICALL Java_org_somdecomp_companion_Native_stateLoad(JNIEnv *e, jclass c, jint slot) {
  int rc;
  (void)e; (void)c;
  pthread_mutex_lock(&mu);
  rc = do_state_load(slot);
  if (rc == SST_OK && ux_glue_option_value(SOM_UX_OPT_HAPTICS)) ev_push(EV_TICK);
  pthread_mutex_unlock(&mu);
  return rc;
}
/* Moves the remembered slot by delta (wraps) and returns it. */
JNIEXPORT jint JNICALL Java_org_somdecomp_companion_Native_stateStep(JNIEnv *e, jclass c, jint delta) {
  (void)e; (void)c;
  pthread_mutex_lock(&mu);
  cur_slot = ((cur_slot + delta) % SST_SLOTS + SST_SLOTS) % SST_SLOTS;
  if (state_dir[0]) sst_set_current(state_dir, cur_slot);
  if (ux_g) { pthread_mutex_lock(&ux_mu); som_ux_set_state_sel(ux_g, cur_slot); pthread_mutex_unlock(&ux_mu); }
  pthread_mutex_unlock(&mu);
  return cur_slot;
}
JNIEXPORT void JNICALL Java_org_somdecomp_companion_Native_setCredits(JNIEnv *e, jclass c, jstring text) {
  const char *t = text ? (*e)->GetStringUTFChars(e, text, NULL) : NULL;
  (void)c;
  pthread_mutex_lock(&ux_mu);
  if (ux_g) som_ux_set_credits(ux_g, t);
  pthread_mutex_unlock(&ux_mu);
  if (t) (*e)->ReleaseStringUTFChars(e, text, t);
}
