/* SDL side of the SoM PC frontend (board item T10): game controller hot-plug and mapping, the audio device with underrun / device-loss
 * recovery, and window mode changes that fall back instead of failing.  Split from som_pc.c so test_sdl.c can drive it with the SDL
 * dummy video / audio drivers and synthetic events. */
#ifndef PC_SDL_H
#define PC_SDL_H

#include <SDL.h>
#include <stdint.h>

#define PC_MAX_PADS 4

/* ---- game controllers ---- */
struct pc_pads {
  SDL_GameController *c[PC_MAX_PADS];
  SDL_JoystickID id[PC_MAX_PADS];
  int trig[PC_MAX_PADS][2];
  int added, removed, generic;          /* counters (tests, log) */
  char last_note[200];                  /* last human readable event, for the log and tests */
};
/* Loads mapping files (the SDL game controller db format: GUID,name,a:b0,...): `file` when given, the SOM_PC_GAMECONTROLLERDB variable
 * and gamecontrollerdb.txt next to config.json.  Returns the number of mappings added. */
int pc_pads_load_db(const char *file);
void pc_pads_init(struct pc_pads *p);                       /* opens the controllers that are already connected */
/* Feeds one SDL event.  Handles CONTROLLERDEVICEADDED / REMOVED and JOYDEVICEADDED of a joystick that has no mapping.  Returns +1 when a
 * controller was opened, -1 when one was closed, 0 otherwise.  A duplicate ADDED (SDL repeats them at start-up) opens nothing. */
int pc_pads_event(struct pc_pads *p, const SDL_Event *e);
int pc_pads_count(const struct pc_pads *p);
/* Reads every open controller: returns the SNES pad mask (SOM_PAD_*) and ORs the shoulder slot bits (L1 R1 L2 R2) into *sh. */
uint16_t pc_pads_read(struct pc_pads *p, uint8_t *sh);
void pc_pads_close(struct pc_pads *p);

/* ---- audio ---- */
struct pc_audio {
  SDL_AudioDeviceID dev;
  int rate;
  int underruns, overflows, reopens, lost;
  int gave_up;                          /* the device could not be recovered: the caller paces with a timer */
  double last_try_ms;
  int tries;
};
int pc_audio_open(struct pc_audio *a, int rate);            /* 0 ok, -1 silent */
void pc_audio_close(struct pc_audio *a);
void pc_audio_queue(struct pc_audio *a, const int16_t *d, size_t frames, int volume);   /* volume 0..100 */
/* Core frames to run now (0..PC_AUDIO_BURST); handles underruns and a queue that grew too deep.  `now_ms` is a monotonic clock. */
int pc_audio_frames(struct pc_audio *a, double bytes_per_frame, int running, double now_ms);
/* Call about once a second: when the device is no longer playing (unplugged, suspended by the server) closes and reopens it, a few times
 * at growing intervals, then gives up (gave_up = 1) so the game keeps its pace without sound. */
void pc_audio_health(struct pc_audio *a, double now_ms);

/* ---- window ---- */
/* Sets the mode; when the platform refuses, falls back to a safer one.  Returns the mode in effect (enum pc_screen). */
int pc_window_set_mode(SDL_Window *w, int mode, int w1080, int h1080);
int pc_window_resize(SDL_Window *w, int width, int height);  /* windowed size; 0 ok, -1 when the window is fullscreen or the size is not usable */
int pc_set_vsync(SDL_Renderer *r, int on);                  /* returns the vsync state in effect (the old one when the renderer cannot change it) */
int pc_window_mode(SDL_Window *w);

/* ---- display filters (pc_shader.h) ---- */
struct pc_fx { SDL_Texture *pre, *fx; int pre_w, pre_h, mode, dw, dh, sw, sh, can_target; };
/* Draws the gw x gh part of `game` (an ARGB texture) into dst with the filter `shader`.  `nearest_plain`: the PLAIN filter scales with nearest
 * neighbour instead of bilinear (integer scaling).  Any filter but PLAIN pre-scales to an integer multiple first; scanlines, CRT and LCD add
 * a multiply layer.  A renderer without render targets falls back to plain scaling. */
void pc_fx_draw(struct pc_fx *f, SDL_Renderer *rd, SDL_Texture *game, int gw, int gh, const SDL_Rect *dst, int shader, int nearest_plain);
void pc_fx_free(struct pc_fx *f);                          /* enum pc_screen of the current flags */

#endif
