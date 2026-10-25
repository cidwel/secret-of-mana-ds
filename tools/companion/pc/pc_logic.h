/* Pure logic of the SoM PC frontend (board item T10): configuration and binding files, picture fitting, the shoulder-button
 * press / hold state machine and gamepad navigation of the picker.  No SDL, no libretro, no file access: som_pc.c supplies those,
 * test_pc.c checks this file on its own.  Pure C99. */
#ifndef PC_LOGIC_H
#define PC_LOGIC_H

#include <stddef.h>
#include <stdint.h>
#include "../ux/som_ui_draw.h"

enum pc_fit { PC_FIT_FILL = 0, PC_FIT_FIT, PC_FIT_INTEGER, PC_FIT_STRETCH, PC_FIT_COUNT };
enum pc_screen { PC_WINDOWED = 0, PC_BORDERLESS, PC_EXCLUSIVE, PC_SCREEN_COUNT };
enum pc_overlay { PC_OV_HIDDEN = 0, PC_OV_SMALL, PC_OV_MEDIUM, PC_OV_COUNT };
enum pc_corner { PC_BOTTOM_RIGHT = 0, PC_BOTTOM_LEFT, PC_BOTTOM_CENTER, PC_TOP_RIGHT, PC_CORNER_COUNT };

struct pc_cfg {
  int wide;           /* 0 = off, else 17 / 27 / 46 pixels of extra world per side */
  int fit;            /* enum pc_fit */
  int hud_hide;       /* 1: snes9x_som_hud=hide_except_ring */
  int screen;         /* enum pc_screen at start */
  int win_w, win_h;   /* windowed size */
  int vsync;
  int overlay;        /* enum pc_overlay */
  int corner;         /* enum pc_corner */
  int names;          /* entry name under each slot */
  int fire_on_press;  /* 0: a slot fires on release and a long hold opens the picker; 1: fires on press */
  int fallback_black; /* snes9x_som_widescreen_fallback=black */
  int volume;         /* 0..100 */
  int shader;         /* enum pc_shader (pc_shader.h) */
  int state_slot;     /* current save state slot 0..9 (quick save / quick load use it) */
  int excl_w, excl_h; /* exclusive fullscreen mode asked for */
  char rom[512], core[512], pack[512];
};
void pc_cfg_defaults(struct pc_cfg *c);
int pc_cfg_to_json(const struct pc_cfg *c, char *out, size_t n);          /* returns the length, -1 when it does not fit */
int pc_cfg_from_json(struct pc_cfg *c, const char *text);                 /* keys that are missing keep their value; 0 ok, -1 not a JSON object */
int pc_json_complete(const char *text);                                   /* 1 when text is one balanced {...} object (a truncated write is not) */
int pc_valid_wide(int n);                                                 /* 0, 17, 27 or 46 */
int pc_next_wide(int n);                                                   /* OFF -> 17 -> 27 -> 46 -> OFF */
const char *pc_fit_name(int fit);

/* Picture settings as the video menu cycles them. */
const char *pc_aspect_name(int wide);                                      /* "WIDE 46", "WIDE 27", "WIDE 17", "4:3" */
int pc_aspect_step(int wide, int dir);                                     /* 46 -> 27 -> 17 -> 4:3 (0) -> 46, dir -1 goes back */
int pc_fit_toggle_integer(int fit);                                        /* INTEGER <-> FIT */
int pc_fit_step(int fit, int dir);                                         /* FIT -> FILL -> STRETCH -> FIT (INTEGER is its own toggle) */
/* Windowed size presets 720p, 900p, 1080p, 1440p, 2160p.  pc_win_step moves to the next/previous preset (a custom size goes to the nearest
 * larger one); pc_win_name writes "1080P" or "CUSTOM 1366X768". */
int pc_win_step(int w, int h, int dir, int *nw, int *nh);
void pc_win_name(int w, int h, char *out, size_t n);
int pc_win_valid(int w, int h);                                            /* 320x240 .. 16384x16384 */

/* The four shoulder slots of one ROM profile.  The entries are the quick-slot kinds of ux/som_ui_draw.h. */
struct pc_profile { struct { uint8_t kind, id, policy; } slot[SOM_UX_SHOULDERS]; };
int pc_profile_to_json(const struct pc_profile *p, const char *variant, char *out, size_t n);
int pc_profile_from_json(struct pc_profile *p, const char *text);        /* 0 ok, -1 when the text is not a profile */

/* Where the game picture goes inside an out_w x out_h target.  frame_w x frame_h is the frame the core delivered, `wide` the configured
 * extension: the scale is fixed by the configured width (256 + 2 * wide) so a map where the core falls back to 256 px keeps the picture
 * size and gets bars at the sides.  dst is in target pixels (it may stick out of the target for FILL). */
struct pc_rect { int x, y, w, h; };
void pc_fit_rects(int out_w, int out_h, int frame_w, int frame_h, int wide, int fit, struct pc_rect *dst);
double pc_aspect(int wide);

/* Shoulder button handling.  `held` is a mask of the four buttons (bit i = slot i).  The machine reports a slot to fire and a slot whose
 * hold reached the assign time. */
#define PC_HOLD_MS 700
#define PC_HOLD_SHOW_MS 180        /* the progress bar starts here */
struct pc_sh { uint8_t held; uint32_t t0[SOM_UX_SHOULDERS]; uint8_t consumed[SOM_UX_SHOULDERS]; uint8_t fire_on_press; };
struct pc_sh_out { int fire; int assign; int progress[SOM_UX_SHOULDERS]; /* 0..100 per slot, 0 when not held long enough to show */ };
void pc_sh_init(struct pc_sh *s, int fire_on_press);
void pc_sh_update(struct pc_sh *s, uint8_t held, uint32_t now_ms, struct pc_sh_out *out);   /* fire / assign: slot or -1 */

/* A controller vanished while its buttons were down: forget the held slots without firing anything. */
void pc_sh_cancel(struct pc_sh *s);

/* Analog trigger with hysteresis (0.6 on / 0.4 off of 32767): returns the new state from the previous one and the axis value. */
int pc_trigger_step(int prev, int axis);
/* Left stick to a direction bit: -1, 0 or 1 for an axis value, dead zone 16000. */
int pc_stick_dir(int axis);

/* Window mode: F11 toggles window <-> borderless fullscreen (desktop resolution, never an exclusive mode change); Shift+F11 toggles the
 * exclusive mode.  pc_screen_fallback gives the next safer mode when one cannot be set (exclusive -> borderless -> windowed -> windowed). */
int pc_screen_toggle(int cur, int shift);
int pc_screen_fallback(int mode);

/* Audio pacing.  The audio queue is the clock: keep PC_AUDIO_TARGET frames queued, run 0..3 core frames per loop.  An empty queue while the
 * game runs is an underrun (the device played silence): refill with a burst of PC_AUDIO_BURST frames.  A queue deeper than PC_AUDIO_MAX frames
 * (after a stall, a pause or a suspended device) is dropped so the sound does not lag behind the picture. */
#define PC_AUDIO_TARGET 3
#define PC_AUDIO_MAX 9
#define PC_AUDIO_BURST 4
struct pc_audio_plan { int frames; int underrun; int overflow; };
void pc_audio_plan(double queued_bytes, double bytes_per_frame, struct pc_audio_plan *out);

/* Directional focus among tap targets: the enabled rectangle nearest in direction (dx, dy) from the centre of `from` (index, -1 = none yet).
 * Returns the new index or `from` when nothing lies that way. */
int pc_nav_pick(const struct som_ux_hitrect *h, int n, int from, int dx, int dy);

/* FNV-1a over pixels, for the golden hashes of the overlay tests. */
uint32_t pc_hash_pixels(const uint32_t *px, int stride, int w, int h);

#endif
