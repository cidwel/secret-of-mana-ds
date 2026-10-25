/* SoM companion, Nintendo 3DS frontend.
 *
 * Top screen: always the whole 256x224 frame, 8:7 pixels, nearest-neighbour scaled to fit 400x240 (constant geometry
 * in every game mode).  The HUD stays visible: snes9x2005 has no layer option to hide BG3.  Bottom screen: the
 * shared status/touch UI.  With -DSOM_WITH_CORE a snes9x libretro core (static, built with platform=ctr) runs the
 * user's ROM from sdmc:/som/rom.sfc; without it a synthetic state is shown so the UI can be checked.
 * Run only in an emulator so far: docs/companion-device-20261017.md.
 */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "som_companion.h"
#include "som_status_draw.h"
#include "som_hotkey.h"
#include "som_bind_ux.h"
#include "direct/som_direct.h"
#include "ux/android_patch/som_ux_glue.h"
#include "fb.h"
#include "menu.h"
#include "settings.h"
#include "savefile.h"
#include "screens.h"
#include "sui.h"
#include "gameplay_c/som_gameplay.h"
#include <sys/stat.h>

/* The default 32 KiB main stack overflows inside the core (retro_unserialize keeps large locals). */
u32 __stacksize__ = 512 * 1024;

#define STATE_PATH "sdmc:/som/dev/state.sav"   /* test fixture only (never created by the app): a libretro save state loaded into the original ROM at start */

static uint8_t demo_wram[SOM_WRAM_SIZE];
static const uint8_t *wram;
static struct som_state st;
static struct som_macro macro;
static uint32_t ui_px[320 * 240];
static struct somd direct;
static int direct_ok;
static struct som_bx bx;
static int plat = SOM_PLAT_3DS_OLD;
static uint8_t *rom_img;
static size_t rom_len;
static uint32_t frame[256 * 240];
static unsigned frame_w, frame_h, pad_in;
static struct n3_settings cfg;
static char rom_key[24] = "original", rom_variant[48] = "none";
static uint32_t rom_crc;
static char start_notice[96];   /* an attention message for the first frames of the game (an option dropped, a save that could not be read) */

#define SAVE_DIR "sdmc:/som/saves"
#define STATE_DIR "sdmc:/som/states"
#define PATCHED_DIR "sdmc:/som/roms/patched"

#ifdef SOM_WITH_CORE
static int pixfmt;
struct retro_game_info { const char *path; const void *data; size_t size; const char *meta; };
struct retro_av { struct { unsigned bw, bh, mw, mh; float ar; } geo; struct { double fps, sr; } timing; };
void retro_init(void);
void retro_run(void);
bool retro_load_game(const struct retro_game_info *);
void retro_get_system_av_info(struct retro_av *);
void *retro_get_memory_data(unsigned);
size_t retro_get_memory_size(unsigned);
bool retro_serialize(void *, size_t);
size_t retro_serialize_size(void);
bool retro_unserialize(const void *, size_t);
void retro_set_environment(bool (*)(unsigned, void *));
void retro_set_video_refresh(void (*)(const void *, unsigned, unsigned, size_t));
void retro_set_audio_sample(void (*)(int16_t, int16_t));
void retro_set_audio_sample_batch(size_t (*)(const int16_t *, size_t));
void retro_set_input_poll(void (*)(void));
void retro_set_input_state(int16_t (*)(unsigned, unsigned, unsigned, unsigned));

static ndspWaveBuf wbuf[4];
static int16_t *abuf[4];
static int awr, afill, audio_ok;   /* audio_ok: ndsp initialised (needs the DSP firmware dump on real hardware) */
#define ACHUNK 1024
static size_t audio_batch(const int16_t *d, size_t frames) {
  if (!audio_ok) return frames;
  for (size_t i = 0; i < frames; i++) {
    ndspWaveBuf *w = &wbuf[awr];
    if (w->status != NDSP_WBUF_DONE && w->status != NDSP_WBUF_FREE) return frames;   /* queue full: drop */
    abuf[awr][afill * 2] = d[i * 2];
    abuf[awr][afill * 2 + 1] = d[i * 2 + 1];
    if (++afill == ACHUNK) {
      DSP_FlushDataCache(abuf[awr], ACHUNK * 4);
      w->data_vaddr = abuf[awr]; w->nsamples = ACHUNK; w->status = NDSP_WBUF_FREE;
      ndspChnWaveBufAdd(0, w);
      awr = (awr + 1) & 3; afill = 0;
    }
  }
  return frames;
}
static void audio_one(int16_t l, int16_t r) { int16_t d[2] = {l, r}; audio_batch(d, 1); }
static bool env(unsigned cmd, void *d) {
  switch (cmd) {
  case 10: pixfmt = *(int *)d == 1 ? 1 : *(int *)d == 2 ? 2 : 0; return true;
  case 9: case 31: *(const char **)d = "sdmc:/som"; return true;
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
/* Hi-res (512 wide) and interlaced (448 high) frames are box-filtered down to 256x224 so every screen fits the same
 * buffer: the game switches to 512 wide on its menu and title screens. */
static void video(const void *d, unsigned w, unsigned h, size_t pitch) {
  if (!d) return;
  unsigned sx = w > 256 ? 2 : 1, sy = h > 240 ? 2 : 1, ow = w / sx, oh = h / sy;
  if (ow > 256 || oh > 240 || w % sx || h % sy) return;
  for (unsigned y = 0; y < oh; y++)
    for (unsigned x = 0; x < ow; x++) {
      unsigned r = 0, g = 0, b = 0;
      for (unsigned dy = 0; dy < sy; dy++)
        for (unsigned dx = 0; dx < sx; dx++) {
          uint32_t v = src_px(d, pitch, x * sx + dx, y * sy + dy);
          r += v >> 16 & 255; g += v >> 8 & 255; b += v & 255;
        }
      unsigned n = sx * sy;
      frame[y * 256 + x] = (r / n) << 16 | (g / n) << 8 | (b / n);
    }
  frame_w = ow; frame_h = oh;
}
static void in_poll(void) {}
static int16_t in_state(unsigned port, unsigned dev, unsigned idx, unsigned id) {
  (void)idx;
  return (port == 0 && dev == 1 && id < 12) ? (int16_t)((pad_in >> id) & 1) : 0;
}
static int core_start(const char *rom_path, int original, char *msg, size_t n) {
  FILE *f = fopen(rom_path, "rb");
  if (!f) { snprintf(msg, n, "NO ROM %.36s", rom_path); return 0; }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *rom = malloc((size_t)sz);
  if (!rom || fread(rom, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); snprintf(msg, n, "ROM READ FAILED"); return 0; }
  rom_img = rom; rom_len = (size_t)sz;
  fclose(f);
  sf_key_from_path(rom_path, original, rom_key, sizeof rom_key);
  som_bind_rom_variant(rom_img, rom_len, rom_variant, sizeof rom_variant);   /* variant of the unpatched ROM: hotlink slots do not depend on the gameplay options */
  if (!sgp_is_default(&cfg.gp)) {
    struct sgp_report rep;
    if (sgp_apply(rom, (size_t)sz, &cfg.gp, &rep) != 0) snprintf(start_notice, sizeof start_notice, "GAMEPLAY OPTIONS NOT APPLIED");
    else if (rep.skipped) {   /* names the first dropped option and counts the rest (the notice chip holds 31 characters) */
      int first = 0, more = 0;
      for (int i = 0; i < SGP_COUNT; i++) if (rep.skipped & (1u << i)) { if (!more) first = i; more++; }
      snprintf(start_notice, sizeof start_notice, more > 1 ? "SKIPPED %.22s +%d" : "SKIPPED %.24s", sgp_key(first), more - 1);
      for (char *p = start_notice; *p; p++) if (*p == '_') *p = ' ';
      for (char *p = start_notice; *p; p++) if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);
    }
    mkdir(MENU_ROOT "/roms", 0777); mkdir(PATCHED_DIR, 0777);
    { char pp[160]; snprintf(pp, sizeof pp, PATCHED_DIR "/%s.sfc", rom_key); sf_write_file(pp, rom, (size_t)sz); }   /* the patched ROM copy, for inspection and other emulators */
  }
  rom_crc = sf_crc32(0, rom, (size_t)sz);
  retro_set_environment(env);
  retro_set_video_refresh(video);
  retro_set_audio_sample(audio_one);
  retro_set_audio_sample_batch(audio_batch);
  retro_set_input_poll(in_poll);
  retro_set_input_state(in_state);
  retro_init();
  struct retro_game_info gi = {"rom.sfc", rom, (size_t)sz, NULL};
  if (!retro_load_game(&gi)) { snprintf(msg, n, "CORE REJECTED ROM"); return 0; }
  mkdir(SAVE_DIR, 0777); mkdir(STATE_DIR, 0777);
  {   /* battery RAM of this game */
    char sp[160];
    sf_srm_path(SAVE_DIR, rom_key, sp, sizeof sp);
    if (retro_get_memory_data(0) && retro_get_memory_size(0)) sf_srm_load(sp, retro_get_memory_data(0), retro_get_memory_size(0));
  }
  if (original) {   /* optional dev fixture */
    FILE *sf = fopen(STATE_PATH, "rb");
    if (sf) {
      size_t want = retro_serialize_size();
      uint8_t *sb = malloc(want);
      if (sb && fread(sb, 1, want, sf) == want) retro_unserialize(sb, want);
      free(sb); fclose(sf);
    }
  }
  struct retro_av av;
  retro_get_system_av_info(&av);
  if (R_SUCCEEDED(ndspInit())) {
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, (float)av.timing.sr);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
    audio_ok = 1;
    for (int i = 0; i < 4; i++) {
      abuf[i] = linearAlloc(ACHUNK * 4);
      if (!abuf[i]) audio_ok = 0;
      wbuf[i].status = NDSP_WBUF_DONE;
    }
  }
  snprintf(msg, n, "CORE RUNNING");
  return 1;
}

/* ---- battery RAM, save states, pause menu */
static uint32_t srm_last_crc;
static void flush_sram(int force) {
  uint8_t *d = retro_get_memory_data(0);
  size_t n = retro_get_memory_size(0);
  if (!d || !n) return;
  uint32_t c = sf_crc32(0, d, n);
  if (!force && c == srm_last_crc) return;
  char sp[160];
  sf_srm_path(SAVE_DIR, rom_key, sp, sizeof sp);
  if (sf_srm_save(sp, d, n) == 0) srm_last_crc = c;
}
static int state_save_slot(const char *path) {
  size_t n = retro_serialize_size();
  uint8_t *buf = malloc(n);
  static uint16_t th[SF_THUMB_W * SF_THUMB_H];
  if (!buf) return -1;
  int rc = -1;
  if (retro_serialize(buf, n)) {
    uint32_t stamp = (uint32_t)(osGetTime() / 1000 - 2208988800ULL);   /* osGetTime counts from 1900 */
    if (frame_w) sf_thumb_from_frame(frame, frame_w, frame_h, 256, th);
    rc = sf_state_save(path, rom_crc, stamp, frame_w ? th : NULL, buf, n);
  }
  free(buf);
  return rc;
}
static int state_load_slot(const char *path) {
  size_t n = retro_serialize_size();
  uint8_t *buf = malloc(n);
  if (!buf) return -1;
  long got = sf_state_load(path, rom_crc, buf, n);
  int rc = (got == (long)n && retro_unserialize(buf, n)) ? 0 : -1;
  free(buf);
  return rc;
}
/* Returns 1 when the player chose Exit game. */
static int pause_game(void) {
  struct pause_ctx pc;
  memset(&pc, 0, sizeof pc);
  pc.settings = &cfg; pc.state_dir = STATE_DIR; pc.key = rom_key; pc.rom_crc = rom_crc;
  flush_sram(0);
  for (;;) {
    int a = screens_pause(&pc);
    char path[200];
    sf_state_path(STATE_DIR, rom_key, cfg.slot, path, sizeof path);
    if (a == PAUSE_RESUME) return 0;
    if (a == PAUSE_EXIT) return 1;
    if (a == PAUSE_SAVE) { if (state_save_slot(path) == 0) return 0; snprintf(pc.notice, sizeof pc.notice, "SAVE FAILED, CHECK THE SD CARD"); }
    else if (a == PAUSE_LOAD) { if (state_load_slot(path) == 0) return 0; snprintf(pc.notice, sizeof pc.notice, "THIS STATE CANNOT BE LOADED"); }
  }
}
#endif

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
}

/* Packed 0xRRGGBB blend of a and b, weight w (0..256) of b: the red/blue and green lanes are multiplied together. */
static inline uint32_t mix(uint32_t a, uint32_t b, unsigned w) {
  uint32_t rb = ((a & 0xFF00FF) * (256 - w) + (b & 0xFF00FF) * w) >> 8 & 0xFF00FF;
  uint32_t g = ((a & 0x00FF00) * (256 - w) + (b & 0x00FF00) * w) >> 8 & 0x00FF00;
  return rb | g;
}
static inline void put_px(uint8_t *col, uint32_t rgb) { col[0] = (uint8_t)rgb; col[1] = (uint8_t)(rgb >> 8); col[2] = (uint8_t)(rgb >> 16); }

/* The frame at 8:7 pixels, scaled into 400x240 and filtered (cfg.filter): none = nearest neighbour, smooth = bilinear, scanlines = nearest with every
 * other output line at 70 percent. */
static void draw_top(uint8_t *fb) {
  static uint16_t xmap[400], ymap[240], x1map[400], y1map[240];
  static uint8_t xw[400], yw[240];
  static unsigned key_w, key_h;
  memset(fb, 0, 400 * 240 * 3);
  if (!frame_w) return;
  unsigned h = frame_h;
  unsigned dh = 240, dw = frame_w * 8 * 240 / (7 * h);   /* 8:7 pixels: 256x224 -> 313x240 */
  if (dw > 400) { dw = 400; dh = h * 7 * 400 / (8 * frame_w); }
  int ox = (int)(400 - dw) / 2, oy = (int)(240 - dh) / 2;
  if (key_w != frame_w || key_h != h) {   /* index and weight tables, rebuilt only when the frame size changes */
    for (unsigned x = 0; x < dw; x++) {
      unsigned p = x * frame_w * 256 / dw;   /* 8.8 source position */
      xmap[x] = (uint16_t)(p >> 8); x1map[x] = (uint16_t)((p >> 8) + 1 < frame_w ? (p >> 8) + 1 : frame_w - 1); xw[x] = (uint8_t)(p & 255);
    }
    for (unsigned y = 0; y < dh; y++) {
      unsigned p = y * h * 256 / dh;
      ymap[y] = (uint16_t)(p >> 8); y1map[y] = (uint16_t)((p >> 8) + 1 < h ? (p >> 8) + 1 : h - 1); yw[y] = (uint8_t)(p & 255);
    }
    key_w = frame_w; key_h = h;
  }
  /* column-major BGR8, origin bottom-left: a destination column is contiguous in y */
  for (unsigned x = 0; x < dw; x++) {
    uint8_t *col = fb + (size_t)(ox + (int)x) * 240 * 3 + (size_t)(239 - oy) * 3;
    if (cfg.filter == N3_FILTER_SMOOTH) {
      const uint32_t *s0 = frame + xmap[x], *s1 = frame + x1map[x];
      unsigned wx = xw[x];
      for (unsigned y = 0; y < dh; y++, col -= 3) {
        size_t r0 = (size_t)ymap[y] * 256, r1 = (size_t)y1map[y] * 256;
        put_px(col, mix(mix(s0[r0], s1[r0], wx), mix(s0[r1], s1[r1], wx), yw[y]));
      }
    } else if (cfg.filter == N3_FILTER_SCANLINES) {
      const uint32_t *src = frame + xmap[x];
      for (unsigned y = 0; y < dh; y++, col -= 3) {
        uint32_t rgb = src[(size_t)ymap[y] * 256];
        put_px(col, (y & 1) ? mix(rgb, 0, 77) : rgb);
      }
    } else {
      const uint32_t *src = frame + xmap[x];
      for (unsigned y = 0; y < dh; y++, col -= 3) put_px(col, src[(size_t)ymap[y] * 256]);
    }
  }
}

/* A red strip on the top edge of the top screen: the shared UX chip is only drawn on field maps, and an option dropped at start must be seen on the title too. */
static void draw_banner(uint8_t *fb, const char *text) {
  static uint32_t px[400 * 18];
  sui_rect(px, 400, 400, 18, 0, 0, 400, 18, 0x8A2A2A);
  sui_text_draw(px, 400, 400, 18, 12, 1, 2, 0xE8EEF8, text);
  for (int y = 0; y < 18; y++) for (int x = 0; x < 400; x++) fb_put(fb, x, y, px[y * 400 + x]);
}

static void draw_bottom(uint8_t *fb) { fb_blit(fb, ui_px, 320); }

/* ---- inputs: libctru keys to the shared input ids */
static const struct { u32 key; int in; } KEYMAP[] = {
  {KEY_A, SOM_IN_A}, {KEY_B, SOM_IN_B}, {KEY_X, SOM_IN_X}, {KEY_Y, SOM_IN_Y}, {KEY_L, SOM_IN_L1}, {KEY_R, SOM_IN_R1},
  {KEY_ZL, SOM_IN_L2}, {KEY_ZR, SOM_IN_R2}, {KEY_SELECT, SOM_IN_SELECT}, {KEY_START, SOM_IN_START},
  {KEY_DUP, SOM_IN_DUP}, {KEY_DDOWN, SOM_IN_DDOWN}, {KEY_DLEFT, SOM_IN_DLEFT}, {KEY_DRIGHT, SOM_IN_DRIGHT},
  {KEY_CPAD_UP, SOM_IN_LS_UP}, {KEY_CPAD_DOWN, SOM_IN_LS_DOWN}, {KEY_CPAD_LEFT, SOM_IN_LS_LEFT}, {KEY_CPAD_RIGHT, SOM_IN_LS_RIGHT},
  {KEY_CSTICK_UP, SOM_IN_RS_UP}, {KEY_CSTICK_DOWN, SOM_IN_RS_DOWN}, {KEY_CSTICK_LEFT, SOM_IN_RS_LEFT}, {KEY_CSTICK_RIGHT, SOM_IN_RS_RIGHT},
};
static uint32_t inputs_of(u32 held) {
  uint32_t m = 0;
  for (size_t i = 0; i < sizeof KEYMAP / sizeof *KEYMAP; i++) if (held & KEYMAP[i].key) m |= SOM_IN_BIT(KEYMAP[i].in);
  return m & som_bind_platform_inputs(plat);
}

/* ---- bottom-screen actions, bindings and toast */
static int name_letter = -1, name_wait, back_wait, latch_age;
static unsigned pad_latch;
static int hkq[8], hkq_n;   /* fired shoulder slots 0..3 */
static char quick_path[96];
static int quick_dirty;

static void set_toast(const char *t) {   /* refusals show as the UX notice chip */
  char b[32];
  snprintf(b, sizeof b, "%s", t);
  for (char *p = b; *p; p++) *p = (*p == '-' || *p == '_') ? ' ' : (*p >= 'a' && *p <= 'z') ? (char)(*p - 32) : *p;
  ux_glue_notice(SOM_UX_NOTICE_ERROR, b);
}
static void load_binds(void) {   /* shoulder model: L, R, ZL, ZR are the quick slots 0..3; the old per-button binding files are ignored */
  static char buf[SOM_UX_QUICK_N * 24 + 64];
  char var[48];
  som_bx_init(&bx, plat);
  ux_glue_shoulder_labels("L", "R", "ZL", "ZR");
  snprintf(var, sizeof var, "%s", rom_variant);
  FILE *f;
  /* hotlink slots, per ROM variant */
  snprintf(quick_path, sizeof quick_path, "sdmc:/som/quick_%s.txt", var);
  f = fopen(quick_path, "rb");
  if (f) {
    size_t n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f);
    int slot, k, id, pol, used = 0;
    const char *p = buf;
    while (sscanf(p, "%d %d %d%n", &slot, &k, &id, &used) == 3) {   /* "slot kind id [policy]": the policy is optional (older files) */
      p += used;
      pol = 0;
      if (sscanf(p, " %d%n", &pol, &used) == 1 && *p == ' ') p += used; else pol = 0;
      if (slot >= 0 && slot < SOM_UX_QUICK_N) { som_ux_quick_set(ux_g, slot, k, id); ux_glue_quick_policy(slot, pol); }
    }
  }
}
static void save_quick(void) {
  char buf[SOM_UX_QUICK_N * 24 + 1];
  int n = 0;
  buf[0] = 0;
  if (!quick_path[0]) return;
  for (int i = 0; i < SOM_UX_QUICK_N; i++) { int k, id; if (som_ux_quick_get(ux_g, i, &k, &id)) n += snprintf(buf + n, sizeof buf - (size_t)n, "%d %d %d %d\n", i, k, id, ux_g->quick[i].policy); }
  FILE *f = fopen(quick_path, "wb");
  if (f) { fwrite(buf, 1, (size_t)n, f); fclose(f); }
}

static void run_direct(const struct som_cmd *cmd, struct somd_target t) {
  int err = som_hk_start_cmd(&direct, wram, cmd, t);
  if (err == SOMD_OK) return;
  if (err == SOMD_ERR_NEEDS_RING) { som_macro_start(&macro, wram, cmd); return; }   /* Magic Rope, Flammie Drum: ring macro */
  set_toast(somd_err_name((enum somd_err)err));
}
static void ux_dispatch(const struct ux_dispatch *d) {
  struct somd_target t = {SOMD_T_NONE, 0};
  switch (d->kind) {
  case SOM_UX_ACT_CMD: {
    const struct som_cmd *c = &d->cmd;
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
  case SOM_UX_ACT_PAD:
    pad_latch |= d->pad; latch_age = 0;
    if ((d->pad & (1u << SOM_PAD_A)) && st.mode == SOM_MODE_RING) back_wait = 24;   /* BACK: A, then Y if the ring is still open */
    break;
  case SOM_UX_ACT_NAME_LETTER: name_letter = d->letter; name_wait = 0; break;
  case SOM_UX_ACT_OPTION: ux_glue_option(d->opt_id, d->opt_val ? 1 : 0, NULL); break;   /* the 3DS has no widescreen, core or sound options: the toggles only keep their value */
  case SOM_UX_ACT_AVATAR:
  case SOM_UX_ACT_OPEN_MENU: {   /* ring navigation through the shared macro engine */
    int cat = 0xFF, entry = 0xFF, hero = d->hero;
    if (d->kind == SOM_UX_ACT_OPEN_MENU) {
      if (d->bind_kind == SOM_UX_BK_MENU) cat = d->bind_id;
      else if (d->bind_kind == SOM_UX_BK_SUBMENU) { cat = SOM_CAT_CONFIG; entry = d->bind_id; }
    }
    if (cat == SOM_CAT_MAGIC && hero == 0) hero = (ux_g->hero == 1 || ux_g->hero == 2) ? ux_g->hero : 1;
    struct som_cmd c = {SOM_CMD_OPEN_MENU, (uint8_t)hero, (uint8_t)cat, (uint8_t)entry};
    if (som_macro_busy(&macro)) { set_toast("BUSY"); break; }
    som_read_state(wram, &st);
    if (!som_macro_start(&macro, wram, &c)) set_toast(macro.result == SOM_FAIL_UNSAFE ? "UNSAFE_STATE" : "NOT_OWNED");
    break;
  }
  case SOM_UX_ACT_QUICK_SET: quick_dirty = 1; break;
  case SOM_UX_ACT_CLOSE_RING: som_macro_close_ring(&macro); break;
  default: break;
  }
}

int main(void) {
  char msg[48] = "DEMO MODE (NO CORE)";
  int running_core = 0;
  gfxInitDefault();
  gfxSetDoubleBuffering(GFX_TOP, true);
  gfxSetDoubleBuffering(GFX_BOTTOM, false);   /* the status UI is redrawn only when it changes, so one buffer must keep the picture */
  osSetSpeedupEnable(true);   /* New 3DS: 804 MHz; ignored on the old models */
  { bool nw = false; if (R_SUCCEEDED(aptInit()) && R_SUCCEEDED(APT_CheckNew3DS(&nw)) && nw) plat = SOM_PLAT_3DS_NEW; }
  n3_load(&cfg, SETTINGS_PATH);
  som_macro_init(&macro);
  make_demo();
  wram = demo_wram;
#ifdef SOM_WITH_CORE
  {
    char rom_path[160];
    int original = 1;
    if (!menu_run(rom_path, sizeof rom_path, &original, &cfg)) { gfxExit(); return 0; }
    running_core = core_start(rom_path, original, msg, sizeof msg);
  }
#endif
  {   /* optional art pack (never in the repo): sdmc:/som/som_ui_pack.bin */
    uint8_t *pack = NULL; size_t pl = 0;
    FILE *pf = fopen("sdmc:/som/som_ui_pack.bin", "rb");
    if (pf) { fseek(pf, 0, SEEK_END); pl = (size_t)ftell(pf); fseek(pf, 0, SEEK_SET); pack = malloc(pl); if (pack && fread(pack, 1, pl, pf) != pl) { free(pack); pack = NULL; } fclose(pf); }
    ux_glue_init(pack, pack ? pl : 0, rom_img, rom_len);
    free(pack);
  }
  if (rom_img) { somd_init(&direct, rom_img, rom_len); direct_ok = direct.rom_ok; }
  load_binds();
  som_read_state(wram, &st);
  if (start_notice[0]) set_toast(start_notice);

  u64 t0 = osGetTime();
  int force_ui = 0, banner_frames = start_notice[0] ? 300 : 0;
#ifdef SOM_WITH_CORE
  int sram_age = 0;
#endif
  int frames = 0, tdown = 0, tfired = 0, tx = 0, ty = 0;
  u64 tt_down = 0;
  char fps[40] = "";
  u64 acc_run = 0, acc_ui = 0, acc_top = 0;
  int ui_draws = 0;
  while (aptMainLoop()) {
    hidScanInput();
    u32 held = hidKeysHeld(), down = hidKeysDown(), up = hidKeysUp();
    if (down & KEY_START && (held & KEY_SELECT)) {   /* Select+Start: pause menu (states, filter, credits, exit) */
#ifdef SOM_WITH_CORE
      if (running_core) {
        int quit = pause_game();
        force_ui = 1; pad_latch = 0; latch_age = 0; hkq_n = 0;
        if (quit) break;
        do { hidScanInput(); gspWaitForVBlank(); } while (hidKeysHeld() && aptMainLoop());   /* the button that closed the menu must not reach the game */
        continue;
      }
#endif
      break;
    }
    u64 now = osGetTime();

    /* bindings: the engine says what fired and which inputs the game must not see */
    struct som_bind_out bo;
    uint32_t in = inputs_of(held);
    som_bind_update(&bx.ctx, in, (uint32_t)(now & 0xFFFFFFFFu), som_bx_mode(ux_g), &bo);
    for (int i = 0; i < bo.nfire && i < SOM_BIND_FIRES; i++) { int sh = som_bx_shoulder_of(&bx.ctx.b[bo.fire[i]]); if (sh >= 0 && hkq_n < 8) hkq[hkq_n++] = sh; }
    if (quick_dirty) { save_quick(); quick_dirty = 0; }
    unsigned m = som_bind_game_mask(plat, in & ~bo.swallow);

    /* touch: a tap acts on release, a hold of 0.5 s is a long press (bind screen) */
    if (down & KEY_TOUCH) { touchPosition tp; hidTouchRead(&tp); tx = tp.px; ty = tp.py; tdown = 1; tfired = 0; tt_down = now; }
    else if ((held & KEY_TOUCH) && tdown) {
      touchPosition tp; hidTouchRead(&tp);
      if (abs((int)tp.px - tx) > 12 || abs((int)tp.py - ty) > 12) tfired = 1;   /* a drag is neither */
      else if (!tfired && now - tt_down >= 500) {
        struct ux_dispatch ud;
        tfired = 1;
        som_read_state(wram, &st);
        if (ux_glue_longpress(tx, ty, 320, 240, &ud)) ux_dispatch(&ud);
      }
    }
    if ((up & KEY_TOUCH) && tdown) {
      tdown = 0;
      if (!tfired) {
        struct ux_dispatch ud;
        som_read_state(wram, &st);
        if (ux_glue_touch(tx, ty, 320, 240, &ud)) ux_dispatch(&ud);
      }
    }

    pad_in = m | pad_latch;
    if (++latch_age >= 5) { pad_latch = 0; latch_age = 0; }
#ifdef SOM_WITH_CORE
    if (running_core) {
      wram = retro_get_memory_data(2);
      som_read_state(wram, &st);
      if (hkq_n) {
        int q[8], n = hkq_n;
        memcpy(q, hkq, sizeof q); hkq_n = 0;
        for (int i = 0; i < n; i++) { struct ux_dispatch d; if (ux_glue_shoulder(q[i], &d)) ux_dispatch(&d); }
      }
      if (back_wait > 0 && --back_wait == 0 && st.mode == SOM_MODE_RING) { pad_latch |= 1u << SOM_PAD_Y; latch_age = 0; back_wait = 24; }
      if (name_letter >= 0 && name_wait-- <= 0) {
        unsigned nm = ux_glue_name_step(name_letter);
        pad_latch |= nm; latch_age = 0; name_wait = 28;
        if (nm == (1u << SOM_PAD_B)) name_letter = -1;
      }
      if (som_macro_busy(&macro)) pad_in |= som_macro_step(&macro, wram);
      if (direct_ok) somd_pre_frame(&direct, (uint8_t *)wram);
      u64 tr = osGetTime();
      retro_run();
      acc_run += osGetTime() - tr;
      if (direct_ok) somd_post_frame(&direct, (uint8_t *)wram);
      if (++sram_age >= 300) { sram_age = 0; flush_sram(0); }
    }
#endif
    som_read_state(wram, &st);
    ux_glue_frame(wram);

    u64 tu = osGetTime();
    if (!ux_g || force_ui || som_ux_needs_redraw(ux_g, 320, 240)) {
      force_ui = 0;
      ux_glue_draw(&macro, ui_px, 320, 240);
      ui_draws++;
      draw_bottom(gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL));
    }
    u64 tt = osGetTime();
    acc_ui += tt - tu;
    {
      uint8_t *tfb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
      draw_top(tfb);
      if (banner_frames > 0) { draw_banner(tfb, start_notice); banner_frames--; }
    }
    acc_top += osGetTime() - tt;
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
    if (++frames == 60) {
      u64 t = osGetTime();
      snprintf(fps, sizeof fps, "%d MS/F R%d U%d T%d D%d", (int)((t - t0) / 60), (int)(acc_run / 60), (int)(acc_ui / 60), (int)(acc_top / 60), ui_draws);
      svcOutputDebugString(fps, (int)strlen(fps));
      if (running_core) {   /* latest measurement, for the person testing on a console: ms per frame, core, bottom UI, top scaling */
        FILE *pf = fopen("sdmc:/som/perf.txt", "wb");
        if (pf) { struct mallinfo mi = mallinfo();
        fprintf(pf, "%s model=%s heap_used=%d linear_free=%u\n", fps, plat == SOM_PLAT_3DS_NEW ? "new" : "old", mi.uordblks, (unsigned)linearSpaceFree()); fclose(pf); }
      }
      t0 = t; frames = 0; acc_run = acc_ui = acc_top = 0; ui_draws = 0;
    }
  }
#ifdef SOM_WITH_CORE
  if (running_core) flush_sram(1);
  if (running_core && audio_ok) ndspExit();
#endif
  gfxExit();
  return 0;
}
