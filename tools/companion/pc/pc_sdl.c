/* SDL side of the SoM PC frontend, see pc_sdl.h. */
#include "pc_sdl.h"
#include "pc_shader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_logic.h"
#include "../som_companion.h"

/* ------------------------------------------------------------------ controllers */
int pc_pads_load_db(const char *file) {
  int n = 0, r;
  const char *env = getenv("SOM_PC_GAMECONTROLLERDB");
  const char *cfg = getenv("SOM_PC_CONFIG"), *home = getenv("HOME");
  char path[800];
  if (file && *file && (r = SDL_GameControllerAddMappingsFromFile(file)) > 0) n += r;
  if (env && *env && (r = SDL_GameControllerAddMappingsFromFile(env)) > 0) n += r;
  if (cfg && *cfg) snprintf(path, sizeof path, "%s/gamecontrollerdb.txt", cfg);
  else snprintf(path, sizeof path, "%s/.config/som-decomp-pc/gamecontrollerdb.txt", home ? home : ".");
  if ((r = SDL_GameControllerAddMappingsFromFile(path)) > 0) n += r;
  return n;
}

static int slot_of_id(const struct pc_pads *p, SDL_JoystickID id) {
  for (int i = 0; i < PC_MAX_PADS; i++) if (p->c[i] && p->id[i] == id) return i;
  return -1;
}
static int open_index(struct pc_pads *p, int dev_index) {
  SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(dev_index);
  int slot;
  SDL_GameController *c;
  if (id < 0 || slot_of_id(p, id) >= 0) return 0;                       /* unknown or already open */
  for (slot = 0; slot < PC_MAX_PADS && p->c[slot]; slot++) {}
  if (slot == PC_MAX_PADS) { snprintf(p->last_note, sizeof p->last_note, "controller ignored: four are already open"); return 0; }
  c = SDL_GameControllerOpen(dev_index);
  if (!c) { snprintf(p->last_note, sizeof p->last_note, "controller %d: %s", dev_index, SDL_GetError()); return 0; }
  p->c[slot] = c; p->id[slot] = id; p->trig[slot][0] = p->trig[slot][1] = 0;
  p->added++;
  snprintf(p->last_note, sizeof p->last_note, "controller %d opened: %s", slot, SDL_GameControllerName(c) ? SDL_GameControllerName(c) : "?");
  return 1;
}
/* A joystick with no entry in the db: if it looks like a standard pad (2 axes, 8 buttons, 1 hat) apply the common
 * xinput-like layout so it still plays; its GUID is logged so a proper line can go in gamecontrollerdb.txt. */
static int add_generic_mapping(struct pc_pads *p, int dev_index) {
  SDL_Joystick *j;
  char guid[40], line[600];
  const char *name;
  int ok;
  if (SDL_IsGameController(dev_index)) return 0;
  j = SDL_JoystickOpen(dev_index);
  if (!j) return 0;
  ok = SDL_JoystickNumAxes(j) >= 2 && SDL_JoystickNumButtons(j) >= 8 && SDL_JoystickNumHats(j) >= 1;
  SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(j), guid, sizeof guid);
  name = SDL_JoystickName(j) ? SDL_JoystickName(j) : "Generic pad";
  if (ok) {
    snprintf(line, sizeof line, "%s,%s,platform:%s,a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7,leftstick:b8,rightstick:b9,"
             "dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,leftx:a0,lefty:a1", guid, name, SDL_GetPlatform());
    ok = SDL_GameControllerAddMapping(line) >= 0;
  }
  SDL_JoystickClose(j);
  snprintf(p->last_note, sizeof p->last_note, "no mapping for %s (GUID %s): %s", name, guid,
           ok ? "using the generic layout; add a line to gamecontrollerdb.txt to fix the buttons" : "ignored");
  if (ok) p->generic++;
  return ok;
}

void pc_pads_init(struct pc_pads *p) {
  memset(p, 0, sizeof *p);
  pc_pads_load_db(NULL);
  for (int i = 0; i < SDL_NumJoysticks(); i++) {
    if (!SDL_IsGameController(i)) add_generic_mapping(p, i);
    if (SDL_IsGameController(i)) open_index(p, i);
  }
}

int pc_pads_event(struct pc_pads *p, const SDL_Event *e) {
  if (e->type == SDL_CONTROLLERDEVICEADDED) return open_index(p, e->cdevice.which);
  if (e->type == SDL_JOYDEVICEADDED) {
    int i = e->jdevice.which;
    if (i < 0 || i >= SDL_NumJoysticks()) return 0;
    if (SDL_IsGameController(i) || add_generic_mapping(p, i)) return open_index(p, i);     /* open_index ignores a duplicate */
    return 0;
  }
  if (e->type == SDL_CONTROLLERDEVICEREMOVED) {
    int s = slot_of_id(p, e->cdevice.which);
    if (s < 0) return 0;
    SDL_GameControllerClose(p->c[s]);
    p->c[s] = NULL; p->trig[s][0] = p->trig[s][1] = 0;
    p->removed++;
    snprintf(p->last_note, sizeof p->last_note, "controller %d removed", s);
    return -1;
  }
  return 0;
}
int pc_pads_count(const struct pc_pads *p) {
  int n = 0;
  for (int i = 0; i < PC_MAX_PADS; i++) if (p->c[i]) n++;
  return n;
}
void pc_pads_close(struct pc_pads *p) {
  for (int i = 0; i < PC_MAX_PADS; i++) if (p->c[i]) { SDL_GameControllerClose(p->c[i]); p->c[i] = NULL; }
}

uint16_t pc_pads_read(struct pc_pads *p, uint8_t *sh) {
  uint16_t m = 0;
  for (int i = 0; i < PC_MAX_PADS; i++) {
    SDL_GameController *c = p->c[i];
    int lx, ly;
    if (!c) continue;
    if (!SDL_GameControllerGetAttached(c)) continue;                    /* unplugged, the REMOVED event is on its way */
#define GB(b) SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_##b)
    if (GB(A)) m |= 1u << SOM_PAD_B;
    if (GB(B)) m |= 1u << SOM_PAD_A;
    if (GB(X)) m |= 1u << SOM_PAD_Y;
    if (GB(Y)) m |= 1u << SOM_PAD_X;
    if (GB(BACK)) m |= 1u << SOM_PAD_SELECT;
    if (GB(START)) m |= 1u << SOM_PAD_START;
    if (GB(DPAD_UP)) m |= 1u << SOM_PAD_UP;
    if (GB(DPAD_DOWN)) m |= 1u << SOM_PAD_DOWN;
    if (GB(DPAD_LEFT)) m |= 1u << SOM_PAD_LEFT;
    if (GB(DPAD_RIGHT)) m |= 1u << SOM_PAD_RIGHT;
    if (GB(LEFTSHOULDER)) *sh |= 1;
    if (GB(RIGHTSHOULDER)) *sh |= 2;
#undef GB
    lx = pc_stick_dir(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX));
    ly = pc_stick_dir(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY));
    if (lx < 0) m |= 1u << SOM_PAD_LEFT;
    if (lx > 0) m |= 1u << SOM_PAD_RIGHT;
    if (ly < 0) m |= 1u << SOM_PAD_UP;
    if (ly > 0) m |= 1u << SOM_PAD_DOWN;
    for (int t = 0; t < 2; t++) {
      p->trig[i][t] = pc_trigger_step(p->trig[i][t], SDL_GameControllerGetAxis(c, t ? SDL_CONTROLLER_AXIS_TRIGGERRIGHT : SDL_CONTROLLER_AXIS_TRIGGERLEFT));
      if (p->trig[i][t]) *sh |= (uint8_t)(4 << t);
    }
  }
  return m;
}

/* ------------------------------------------------------------------ audio */
int pc_audio_open(struct pc_audio *a, int rate) {
  SDL_AudioSpec want, got;
  SDL_zero(want);
  want.freq = rate; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 512;
  a->rate = rate;
  a->dev = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
  if (!a->dev) { fprintf(stderr, "som-pc: no audio (%s): running silent\n", SDL_GetError()); return -1; }
  SDL_PauseAudioDevice(a->dev, 0);
  return 0;
}
void pc_audio_close(struct pc_audio *a) {
  if (a->dev) SDL_CloseAudioDevice(a->dev);
  a->dev = 0;
}
void pc_audio_queue(struct pc_audio *a, const int16_t *d, size_t frames, int volume) {
  if (!a->dev) return;
  if (volume >= 100) { SDL_QueueAudio(a->dev, d, (Uint32)(frames * 4)); return; }
  while (frames) {
    int16_t tmp[1024 * 2];
    size_t n = frames > 1024 ? 1024 : frames;
    for (size_t i = 0; i < n * 2; i++) tmp[i] = (int16_t)(d[i] * (volume > 0 ? volume : 0) / 100);
    SDL_QueueAudio(a->dev, tmp, (Uint32)(n * 4));
    d += n * 2; frames -= n;
  }
}
int pc_audio_frames(struct pc_audio *a, double bpf, int running, double now_ms) {
  struct pc_audio_plan pl;
  (void)now_ms;
  if (!a->dev) return 0;
  pc_audio_plan((double)SDL_GetQueuedAudioSize(a->dev), bpf, &pl);
  if (pl.overflow) { SDL_ClearQueuedAudio(a->dev); a->overflows++; return 0; }
  if (pl.underrun) { if (running) a->underruns++; }
  return pl.frames;
}
void pc_audio_health(struct pc_audio *a, double now_ms) {
  static const double wait_ms[] = {0, 1000, 3000, 8000};
  if (a->gave_up || !a->rate) return;
  if (a->dev && SDL_GetAudioDeviceStatus(a->dev) == SDL_AUDIO_PLAYING) { a->tries = 0; return; }
  if (a->dev) { SDL_CloseAudioDevice(a->dev); a->dev = 0; a->lost++; }
  if (a->tries >= 4) { a->gave_up = 1; fprintf(stderr, "som-pc: audio device lost, continuing without sound\n"); return; }
  if (now_ms - a->last_try_ms < wait_ms[a->tries]) return;
  a->last_try_ms = now_ms; a->tries++;
  if (pc_audio_open(a, a->rate) == 0) { a->reopens++; fprintf(stderr, "som-pc: audio device reopened\n"); }
}

/* ------------------------------------------------------------------ window */
int pc_window_mode(SDL_Window *w) {
  Uint32 f = SDL_GetWindowFlags(w);
  if ((f & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP) return PC_BORDERLESS;
  if (f & SDL_WINDOW_FULLSCREEN) return PC_EXCLUSIVE;
  return PC_WINDOWED;
}
static int try_mode(SDL_Window *w, int mode, int fw, int fh) {
  if (mode == PC_WINDOWED) return SDL_SetWindowFullscreen(w, 0) == 0;
  if (mode == PC_BORDERLESS) return SDL_SetWindowFullscreen(w, SDL_WINDOW_FULLSCREEN_DESKTOP) == 0;
  {
    int disp = SDL_GetWindowDisplayIndex(w);
    SDL_DisplayMode m, best;
    if (disp < 0 || SDL_GetDesktopDisplayMode(disp, &m) != 0) return 0;
    m.w = fw; m.h = fh;
    if (!SDL_GetClosestDisplayMode(disp, &m, &best) || best.w < 640 || best.h < 480) return 0;
    if (SDL_SetWindowDisplayMode(w, &best) != 0) return 0;
    return SDL_SetWindowFullscreen(w, SDL_WINDOW_FULLSCREEN) == 0;
  }
}
int pc_window_set_mode(SDL_Window *w, int mode, int fw, int fh) {
  if (!w) return mode;
  while (!try_mode(w, mode, fw, fh)) {
    fprintf(stderr, "som-pc: window mode %d refused (%s)\n", mode, SDL_GetError());
    if (mode == PC_WINDOWED) return pc_window_mode(w);
    mode = pc_screen_fallback(mode);
  }
  return mode;
}

int pc_window_resize(SDL_Window *w, int width, int height) {
  if (!w || width < 320 || height < 240 || width > 16384 || height > 16384) return -1;
  if (SDL_GetWindowFlags(w) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) return -1;
  SDL_SetWindowSize(w, width, height);
  return 0;
}
int pc_set_vsync(SDL_Renderer *r, int on) {
  SDL_RendererInfo ri;
  if (!r) return 0;
  if (SDL_RenderSetVSync(r, on ? 1 : 0) != 0) fprintf(stderr, "som-pc: vsync cannot be changed here (%s)\n", SDL_GetError());
  return SDL_GetRendererInfo(r, &ri) == 0 && (ri.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
}

void pc_fx_free(struct pc_fx *f) {
  if (f->pre) SDL_DestroyTexture(f->pre);
  if (f->fx) SDL_DestroyTexture(f->fx);
  memset(f, 0, sizeof *f);
}
void pc_fx_draw(struct pc_fx *f, SDL_Renderer *rd, SDL_Texture *game, int gw, int gh, const SDL_Rect *dst, int shader, int nearest_plain) {
  SDL_Rect src = {0, 0, gw, gh};
  if (!f->can_target) { SDL_RendererInfo ri; f->can_target = SDL_GetRendererInfo(rd, &ri) == 0 && (ri.flags & SDL_RENDERER_TARGETTEXTURE) ? 1 : -1; }
  if (pc_shader_is_sharp(shader) && f->can_target > 0) {
    int k = pc_shader_prescale(gw, dst->w), tw = gw * k, th = gh * k;
    if (!f->pre || f->pre_w != tw || f->pre_h != th) {
      if (f->pre) SDL_DestroyTexture(f->pre);
      f->pre = SDL_CreateTexture(rd, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, tw, th);
      f->pre_w = tw; f->pre_h = th;
    }
    if (f->pre) {
      SDL_SetTextureScaleMode(game, SDL_ScaleModeNearest);
      SDL_SetRenderTarget(rd, f->pre);
      SDL_RenderCopy(rd, game, &src, NULL);
      SDL_SetRenderTarget(rd, NULL);
      SDL_SetTextureScaleMode(f->pre, SDL_ScaleModeLinear);
      SDL_RenderCopy(rd, f->pre, NULL, dst);
    }
  } else {
    SDL_SetTextureScaleMode(game, nearest_plain ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
    SDL_RenderCopy(rd, game, &src, dst);
  }
  if (pc_shader_has_overlay(shader) && dst->w > 0 && dst->h > 0) {
    if (!f->fx || f->mode != shader + 1 || f->dw != dst->w || f->dh != dst->h || f->sw != gw || f->sh != gh) {
      uint32_t *buf = malloc((size_t)dst->w * dst->h * 4);
      if (f->fx) { SDL_DestroyTexture(f->fx); f->fx = NULL; }
      if (buf && pc_shader_overlay(shader, dst->w, dst->h, gw, gh, buf) == 0) {
        f->fx = SDL_CreateTexture(rd, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, dst->w, dst->h);
        if (f->fx) { SDL_UpdateTexture(f->fx, NULL, buf, dst->w * 4); SDL_SetTextureBlendMode(f->fx, SDL_BLENDMODE_MOD); }
      }
      free(buf);
      f->mode = shader + 1; f->dw = dst->w; f->dh = dst->h; f->sw = gw; f->sh = gh;
    }
    if (f->fx) SDL_RenderCopy(rd, f->fx, NULL, dst);
  }
}
