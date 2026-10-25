/* Headless tests of pc_sdl.c with the SDL dummy video and audio drivers and virtual joysticks (no hardware, no ROM, no display):
 * controller hot-plug, duplicate and bogus events, mapping from a db file and the generic fallback, trigger hysteresis on real axes,
 * audio underrun / overflow / device-loss handling and window mode changes with fallbacks.  Run by run_pc_tests.sh. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../som_companion.h"
#include "pc_logic.h"
#include "pc_sdl.h"
#include "pc_shader.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

/* Pumps SDL and gives every event to the controller manager; returns the sum of its results (+opened, -closed). */
static int pump(struct pc_pads *p) {
  SDL_Event e;
  int r = 0;
  SDL_PumpEvents();
  while (SDL_PollEvent(&e)) r += pc_pads_event(p, &e);
  return r;
}
static SDL_Joystick *attach_pad(int *dev_index) {
  int idx = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
  if (dev_index) *dev_index = idx;
  return idx >= 0 ? SDL_JoystickOpen(idx) : NULL;
}
/* A virtual pad of an unknown model (no db entry): vendor, product, axes, buttons, hats. */
static int attach_custom(int vendor, int product, int naxes, int nbuttons, int nhats, const char *name) {
  SDL_VirtualJoystickDesc d;
  SDL_zero(d);
  d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION; d.type = SDL_JOYSTICK_TYPE_UNKNOWN;
  d.vendor_id = (Uint16)vendor; d.product_id = (Uint16)product; d.naxes = (Uint16)naxes; d.nbuttons = (Uint16)nbuttons; d.nhats = (Uint16)nhats;
  d.name = name;
  return SDL_JoystickAttachVirtualEx(&d);
}
/* Device indices shift when another pad goes away, so the index is looked up again from the instance id. */
static void detach_pad(SDL_Joystick *j, int idx) {
  SDL_JoystickID id = SDL_JoystickInstanceID(j);
  (void)idx;
  SDL_JoystickClose(j);
  for (int k = 0; k < SDL_NumJoysticks(); k++) if (SDL_JoystickGetDeviceInstanceID(k) == id) { SDL_JoystickDetachVirtual(k); break; }
}
static void press(SDL_Joystick *j, int b, int v) { SDL_JoystickSetVirtualButton(j, b, (Uint8)v); SDL_PumpEvents(); }
static void axis(SDL_Joystick *j, int a, int v) { SDL_JoystickSetVirtualAxis(j, a, (Sint16)v); SDL_PumpEvents(); }

static void test_pads(void) {
  struct pc_pads p;
  SDL_Joystick *j1, *j2;
  int i1, i2;
  uint8_t sh;
  uint16_t m;
  SDL_Event e;
  pc_pads_init(&p);
  CHECK(pc_pads_count(&p) == 0);
  /* a bogus event for a device that never existed opens nothing and does not crash */
  SDL_zero(e); e.type = SDL_CONTROLLERDEVICEADDED; e.cdevice.which = 999;
  CHECK(pc_pads_event(&p, &e) == 0);
  SDL_zero(e); e.type = SDL_CONTROLLERDEVICEREMOVED; e.cdevice.which = 4242;
  CHECK(pc_pads_event(&p, &e) == 0 && p.removed == 0);
  SDL_zero(e); e.type = SDL_JOYDEVICEADDED; e.jdevice.which = -3;
  CHECK(pc_pads_event(&p, &e) == 0);

  j1 = attach_pad(&i1);
  CHECK(j1 != NULL && SDL_IsGameController(i1));
  CHECK(pump(&p) == 1 && pc_pads_count(&p) == 1 && p.added == 1);
  /* SDL repeats ADDED for pads that were open before: a second event for the same device opens nothing */
  SDL_zero(e); e.type = SDL_CONTROLLERDEVICEADDED; e.cdevice.which = i1;
  CHECK(pc_pads_event(&p, &e) == 0 && pc_pads_count(&p) == 1);

  /* buttons and axes through the real SDL_GameController path */
  m = 0; sh = 0;
  m = pc_pads_read(&p, &sh); CHECK(m == 0 && sh == 0);
  press(j1, SDL_CONTROLLER_BUTTON_A, 1); sh = 0; m = pc_pads_read(&p, &sh);
  CHECK(m == (1u << SOM_PAD_B));                                                 /* south button = SNES B */
  press(j1, SDL_CONTROLLER_BUTTON_A, 0);
  press(j1, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 1); press(j1, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 1); sh = 0; m = pc_pads_read(&p, &sh);
  CHECK(sh == 3 && m == 0);
  press(j1, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 0); press(j1, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 0);
  press(j1, SDL_CONTROLLER_BUTTON_DPAD_UP, 1); press(j1, SDL_CONTROLLER_BUTTON_START, 1); sh = 0; m = pc_pads_read(&p, &sh);
  CHECK(m == ((1u << SOM_PAD_UP) | (1u << SOM_PAD_START)));
  press(j1, SDL_CONTROLLER_BUTTON_DPAD_UP, 0); press(j1, SDL_CONTROLLER_BUTTON_START, 0);
  axis(j1, SDL_CONTROLLER_AXIS_LEFTX, -30000); sh = 0; m = pc_pads_read(&p, &sh); CHECK(m == (1u << SOM_PAD_LEFT));
  axis(j1, SDL_CONTROLLER_AXIS_LEFTX, 9000); sh = 0; m = pc_pads_read(&p, &sh); CHECK(m == 0);      /* inside the dead zone */
  axis(j1, SDL_CONTROLLER_AXIS_LEFTX, 0);
  /* triggers: hysteresis on the real axis */
  /* the virtual axes are raw joystick axes (-32768..32767) that SDL maps onto the trigger range 0..32767 */
  axis(j1, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768); sh = 0; pc_pads_read(&p, &sh); CHECK(sh == 0);
  axis(j1, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -1000); sh = 0; pc_pads_read(&p, &sh); CHECK(sh == 0);   /* middle band, was off */
  axis(j1, SDL_CONTROLLER_AXIS_TRIGGERLEFT, 10000); sh = 0; pc_pads_read(&p, &sh); CHECK(sh == 4);
  axis(j1, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -1000); sh = 0; pc_pads_read(&p, &sh); CHECK(sh == 4);   /* held through the middle band */
  axis(j1, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768); sh = 0; pc_pads_read(&p, &sh); CHECK(sh == 0);
  axis(j1, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 32767); sh = 0; pc_pads_read(&p, &sh); CHECK(sh == 8);
  axis(j1, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768);

  /* second pad, then unplug the first while its trigger is down: the other keeps working and nothing stays pressed */
  j2 = attach_pad(&i2);
  CHECK(pump(&p) == 1 && pc_pads_count(&p) == 2);
  axis(j1, SDL_CONTROLLER_AXIS_TRIGGERLEFT, 32767); sh = 0; pc_pads_read(&p, &sh); CHECK(sh == 4);
  detach_pad(j1, i1);
  CHECK(pump(&p) == -1 && pc_pads_count(&p) == 1 && p.removed == 1);
  sh = 0; m = pc_pads_read(&p, &sh); CHECK(sh == 0 && m == 0);
  press(j2, SDL_CONTROLLER_BUTTON_B, 1); sh = 0; m = pc_pads_read(&p, &sh); CHECK(m == (1u << SOM_PAD_A));
  press(j2, SDL_CONTROLLER_BUTTON_B, 0);
  /* a pad plugged into the freed slot gets a trigger state of zero */
  j1 = attach_pad(&i1);
  CHECK(pump(&p) == 1 && pc_pads_count(&p) == 2);
  sh = 0; pc_pads_read(&p, &sh); CHECK(sh == 0);
  detach_pad(j1, i1); detach_pad(j2, i2);
  CHECK(pump(&p) == -2 && pc_pads_count(&p) == 0);
  sh = 0; CHECK(pc_pads_read(&p, &sh) == 0);

  /* more than four pads: the extra ones are ignored, then work once a slot frees up */
  {
    SDL_Joystick *js[6]; int ix[6];
    for (int i = 0; i < 6; i++) js[i] = attach_pad(&ix[i]);
    pump(&p);
    CHECK(pc_pads_count(&p) == PC_MAX_PADS);
    for (int i = 0; i < 6; i++) detach_pad(js[i], ix[i]);
    pump(&p);
    CHECK(pc_pads_count(&p) == 0);
  }
  /* hot-plug storm: 60 rounds of plug / unplug with duplicate events in between */
  for (int r = 0; r < 60; r++) {
    SDL_Joystick *a = attach_pad(&i1), *b = (r & 1) ? attach_pad(&i2) : NULL;
    pump(&p);
    SDL_zero(e); e.type = SDL_CONTROLLERDEVICEADDED; e.cdevice.which = i1; pc_pads_event(&p, &e);
    CHECK(pc_pads_count(&p) == ((r & 1) ? 2 : 1));
    detach_pad(a, i1);
    if (b) detach_pad(b, i2);
    pump(&p);
    CHECK(pc_pads_count(&p) == 0);
  }
  pc_pads_close(&p);
}

static void test_mapping(void) {
  struct pc_pads p;
  char guid[40], db[400], path[200];
  SDL_Joystick *j, *bad;
  int idx, bidx;
  uint8_t sh;
  uint16_t m;
  FILE *f;
  pc_pads_init(&p);
  /* an unknown pad with a standard shape plays through the generic layout */
  idx = attach_custom(0x1234, 0x0001, 6, 15, 1, "Test Pad X");
  CHECK(idx >= 0);
  j = SDL_JoystickOpen(idx);
  CHECK(j != NULL);
  CHECK(!SDL_IsGameController(idx) || SDL_GameControllerMappingForDeviceIndex(idx) != NULL);
  CHECK(pump(&p) == 1 && pc_pads_count(&p) == 1 && p.generic == 1);
  CHECK(strstr(p.last_note, "Test Pad X") != NULL || strstr(p.last_note, "opened") != NULL);
  press(j, 0, 1); sh = 0; m = pc_pads_read(&p, &sh); CHECK(m == (1u << SOM_PAD_B));
  press(j, 0, 0);
  SDL_JoystickSetVirtualHat(j, 0, SDL_HAT_LEFT); SDL_PumpEvents(); sh = 0; m = pc_pads_read(&p, &sh); CHECK(m == (1u << SOM_PAD_LEFT));
  SDL_JoystickSetVirtualHat(j, 0, SDL_HAT_CENTERED);
  press(j, 4, 1); sh = 0; pc_pads_read(&p, &sh); CHECK(sh == 1);                  /* b4 = left shoulder = slot L1 */
  press(j, 4, 0);
  SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(j), guid, sizeof guid);
  detach_pad(j, idx);
  pump(&p);
  CHECK(pc_pads_count(&p) == 0);

  /* a pad too small to be a game pad (no hat, 2 buttons) is ignored and says so */
  bidx = attach_custom(0x4321, 0x0002, 2, 2, 0, "Tiny");
  CHECK(bidx >= 0);
  bad = SDL_JoystickOpen(bidx);
  CHECK(pump(&p) == 0 && pc_pads_count(&p) == 0 && strstr(p.last_note, "ignored") != NULL);
  detach_pad(bad, bidx);
  pump(&p);

  /* a db file wins over the generic layout: this one swaps b0 and b3 */
  idx = attach_custom(0x2222, 0x0003, 6, 15, 1, "Db Pad");
  j = SDL_JoystickOpen(idx);
  SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(idx), guid, sizeof guid);
  snprintf(path, sizeof path, "/tmp/som_pc_db_%d.txt", (int)getpid());
  snprintf(db, sizeof db, "# comment line\nnot a mapping\n%s,Db Pad,platform:%s,a:b3,b:b1,x:b2,y:b0,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7,dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,leftx:a0,lefty:a1\n",
           guid, SDL_GetPlatform());
  f = fopen(path, "w"); fputs(db, f); fclose(f);
  CHECK(pc_pads_load_db(path) >= 1);
  CHECK(pc_pads_load_db("/nonexistent/db.txt") == 0);
  CHECK(pump(&p) >= 1 && pc_pads_count(&p) == 1 && p.generic == 1);               /* not generic: the db mapping was used */
  press(j, 3, 1); sh = 0; m = pc_pads_read(&p, &sh); CHECK(m == (1u << SOM_PAD_B));
  press(j, 3, 0);
  press(j, 0, 1); sh = 0; m = pc_pads_read(&p, &sh); CHECK(m == (1u << SOM_PAD_X));   /* b0 is Y in this db: SNES X */
  press(j, 0, 0);
  unlink(path);
  detach_pad(j, idx);
  pump(&p);
  pc_pads_close(&p);
}

static void test_audio(void) {
  struct pc_audio a;
  static int16_t buf[3000 * 2];
  double bpf = 32040.0 / 60.0988 * 4.0;
  memset(&a, 0, sizeof a);
  memset(buf, 0, sizeof buf);
  CHECK(pc_audio_open(&a, 32040) == 0 && a.dev != 0);
  if (!a.dev) return;
  /* empty queue while the game runs: an underrun and a refill burst; not counted while paused / just resumed */
  CHECK(pc_audio_frames(&a, bpf, 0, 0) == PC_AUDIO_BURST && a.underruns == 0);
  CHECK(pc_audio_frames(&a, bpf, 1, 0) == PC_AUDIO_BURST && a.underruns == 1);
  /* queue frames: the device consumes them in real time, so the queue shrinks */
  for (int i = 0; i < 6; i++) pc_audio_queue(&a, buf, 534, 100);
  CHECK((double)SDL_GetQueuedAudioSize(a.dev) >= bpf * 5);
  CHECK(pc_audio_frames(&a, bpf, 1, 0) == 0);                                     /* 6 frames queued: nothing to run */
  SDL_Delay(120);
  CHECK((double)SDL_GetQueuedAudioSize(a.dev) < bpf * 6 - 2000);
  /* volume path queues the same amount of data */
  { SDL_ClearQueuedAudio(a.dev); pc_audio_queue(&a, buf, 534, 30); CHECK((int)SDL_GetQueuedAudioSize(a.dev) > 534 * 4 - 1500); }
  pc_audio_queue(&a, buf, 3000, 50);                                               /* bigger than the internal chunk */
  /* a stall that let the queue grow past the limit: dropped */
  SDL_ClearQueuedAudio(a.dev);
  for (int i = 0; i < 14; i++) pc_audio_queue(&a, buf, 534, 100);
  CHECK(pc_audio_frames(&a, bpf, 1, 0) == 0 && a.overflows == 1);
  CHECK(SDL_GetQueuedAudioSize(a.dev) < bpf * 3);
  /* starve: after a while the queue is empty again and the next call asks for a burst */
  SDL_Delay(400);
  { int u = a.underruns; CHECK(pc_audio_frames(&a, bpf, 1, 0) == PC_AUDIO_BURST && a.underruns == u + 1); }
  /* healthy device: health does nothing */
  pc_audio_health(&a, 1000);
  CHECK(a.lost == 0 && a.reopens == 0 && a.gave_up == 0);
  /* a device that stopped playing (suspended by the server) is reopened */
  SDL_PauseAudioDevice(a.dev, 1);
  pc_audio_health(&a, 2000);
  CHECK(a.lost == 1 && a.reopens == 1 && a.dev != 0 && SDL_GetAudioDeviceStatus(a.dev) == SDL_AUDIO_PLAYING);
  pc_audio_health(&a, 3000);
  CHECK(a.lost == 1 && a.tries == 0);
  /* the device disappears and cannot be opened again: a few attempts at growing intervals, then the game paces by timer */
  pc_audio_close(&a);
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
  a.tries = 0; a.last_try_ms = 0;
  pc_audio_health(&a, 10000); CHECK(a.tries == 1 && !a.gave_up);
  pc_audio_health(&a, 10500); CHECK(a.tries == 1);                                 /* too soon */
  pc_audio_health(&a, 11200); CHECK(a.tries == 2);
  pc_audio_health(&a, 14300); CHECK(a.tries == 3);
  pc_audio_health(&a, 22400); CHECK(a.tries == 4 && !a.gave_up);
  pc_audio_health(&a, 22500); CHECK(a.gave_up == 1 && a.dev == 0);
  CHECK(pc_audio_frames(&a, bpf, 1, 0) == 0);                                      /* no device: the caller uses the timer */
  pc_audio_queue(&a, buf, 534, 100);                                               /* harmless without a device */
  SDL_InitSubSystem(SDL_INIT_AUDIO);
}

static void test_window(void) {
  SDL_Window *w = SDL_CreateWindow("t", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_RESIZABLE);
  int mode, ww, hh;
  CHECK(pc_window_set_mode(NULL, PC_BORDERLESS, 1920, 1080) == PC_BORDERLESS);   /* headless run: nothing to do */
  CHECK(w != NULL);
  if (!w) return;
  CHECK(pc_window_mode(w) == PC_WINDOWED);
  mode = pc_window_set_mode(w, PC_BORDERLESS, 1920, 1080);
  CHECK(mode == PC_BORDERLESS && pc_window_mode(w) == PC_BORDERLESS);
  SDL_PumpEvents();
  mode = pc_window_set_mode(w, PC_WINDOWED, 1920, 1080);
  SDL_PumpEvents();
  SDL_GetWindowSize(w, &ww, &hh);
  CHECK(mode == PC_WINDOWED && pc_window_mode(w) == PC_WINDOWED && ww == 1280 && hh == 720);   /* the windowed size comes back */
  /* exclusive: either it works or it falls back to borderless; the result always describes the window */
  mode = pc_window_set_mode(w, PC_EXCLUSIVE, 1920, 1080);
  CHECK((mode == PC_EXCLUSIVE || mode == PC_BORDERLESS) && pc_window_mode(w) == mode);
  mode = pc_window_set_mode(w, PC_WINDOWED, 1920, 1080);
  CHECK(mode == PC_WINDOWED);
  /* F11 / Shift+F11 sequence: always lands in a mode the window reports, 200 toggles */
  {
    int cur = PC_WINDOWED, ok = 1;
    for (int i = 0; i < 200; i++) {
      cur = pc_window_set_mode(w, pc_screen_toggle(cur, i % 3 == 0), 1920, 1080);
      if (cur != pc_window_mode(w)) ok = 0;
      if (i % 7 == 0) SDL_PumpEvents();
    }
    CHECK(ok);
  }
  pc_window_set_mode(w, PC_WINDOWED, 1920, 1080);
  SDL_DestroyWindow(w);
}

/* Window size, vsync and the display filters on a software renderer: the result is read back from the pixels. */
static void test_video(void) {
  SDL_Window *w = SDL_CreateWindow("t", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_RESIZABLE);
  int ww, hh;
  CHECK(w != NULL);
  if (!w) return;
  CHECK(pc_window_resize(w, 1920, 1080) == 0); SDL_PumpEvents(); SDL_GetWindowSize(w, &ww, &hh); CHECK(ww == 1920 && hh == 1080);
  CHECK(pc_window_resize(w, 1366, 768) == 0); SDL_PumpEvents(); SDL_GetWindowSize(w, &ww, &hh); CHECK(ww == 1366 && hh == 768);
  CHECK(pc_window_resize(w, 100, 100) == -1 && pc_window_resize(w, 99999, 720) == -1 && pc_window_resize(NULL, 1280, 720) == -1);
  SDL_GetWindowSize(w, &ww, &hh); CHECK(ww == 1366 && hh == 768);                  /* a refused size changes nothing */
  pc_window_set_mode(w, PC_BORDERLESS, 1920, 1080); SDL_PumpEvents();
  CHECK(pc_window_resize(w, 1280, 720) == -1);                                    /* not while fullscreen */
  pc_window_set_mode(w, PC_WINDOWED, 1920, 1080); SDL_PumpEvents();
  SDL_GetWindowSize(w, &ww, &hh); CHECK(ww == 1366 && hh == 768);
  {
    SDL_Renderer *r = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE);
    int v = r ? pc_set_vsync(r, 1) : -1;
    CHECK(r != NULL && (v == 0 || v == 1));
    v = r ? pc_set_vsync(r, 0) : -1;
    CHECK(v == 0);
    CHECK(pc_set_vsync(NULL, 1) == 0);
    if (r) SDL_DestroyRenderer(r);
  }
  SDL_DestroyWindow(w);
}

static uint32_t read_px(SDL_Surface *s, int x, int y) { return ((uint32_t *)((uint8_t *)s->pixels + y * s->pitch))[x] & 0xFFFFFFu; }
static void test_filters(void) {
  enum { GW = 32, GH = 16, DW = 128, DH = 64 };
  SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(0, 256, 128, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer *r = out ? SDL_CreateSoftwareRenderer(out) : NULL;
  SDL_Texture *game = r ? SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 640, 256) : NULL;
  static uint32_t src[640 * 256];
  struct pc_fx fx;
  SDL_Rect dst = {10, 10, DW, DH};
  static uint32_t ov[DW * DH];
  CHECK(out && r && game);
  if (!game) return;
  for (int y = 0; y < 256; y++) for (int x = 0; x < 640; x++) src[y * 640 + x] = 0xFFFFFFFFu;        /* a white frame */
  for (int y = 0; y < GH; y++) src[y * 640 + 8] = 0xFFFF0000u;                                          /* a red column at x = 8 */
  SDL_SetTextureBlendMode(game, SDL_BLENDMODE_BLEND);
  SDL_UpdateTexture(game, NULL, src, 640 * 4);
  memset(&fx, 0, sizeof fx);
  /* PLAIN with nearest: exact 4x blocks */
  SDL_SetRenderDrawColor(r, 0, 0, 0, 255); SDL_RenderClear(r);
  pc_fx_draw(&fx, r, game, GW, GH, &dst, PC_SH_PLAIN, 1);
  SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ARGB8888, out->pixels, out->pitch);
  CHECK(read_px(out, 10 + 4 * 8 + 1, 10 + 2) == 0xFF0000 && read_px(out, 10 + 4 * 8 + 3, 10 + 3) == 0xFF0000 && read_px(out, 10 + 4 * 7 + 3, 10 + 3) == 0xFFFFFF);
  CHECK(read_px(out, 5, 5) == 0 && read_px(out, 10 + DW + 2, 30) == 0);                                  /* outside the picture stays black */
  /* SHARP at a non-integer scale (3.5x): the inside of a source pixel is exact, only the edges blend */
  {
    SDL_Rect d2 = {10, 10, 112, 56};
    SDL_RenderClear(r);
    pc_fx_draw(&fx, r, game, GW, GH, &d2, PC_SH_SHARP, 0);
    SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ARGB8888, out->pixels, out->pitch);
    CHECK(read_px(out, 10 + 28 + 1, 10 + 10) == 0xFF0000 && read_px(out, 10 + 21 + 1, 10 + 10) == 0xFFFFFF);   /* interior of cell 8 red, cell 6 white */
    int blended = 0;
    for (int x = 10 + 24; x < 10 + 34; x++) { int g = (read_px(out, x, 10 + 10) >> 8) & 255; if (g > 0 && g < 255) blended++; }
    CHECK(blended >= 1 && blended <= 3);                                                                    /* the border between a white and a red cell blends, over a pixel or two */
  }
  /* SCANLINES / LCD / CRT at 4x: white source times the layer */
  for (int m = PC_SH_SCANLINES; m <= PC_SH_LCD; m++) {
    SDL_RenderClear(r);
    pc_fx_draw(&fx, r, game, GW, GH, &dst, m, 0);
    SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ARGB8888, out->pixels, out->pitch);
    CHECK(pc_shader_overlay(m, DW, DH, GW, GH, ov) == 0);
    int bad = 0;
    for (int y = 0; y < DH; y += 3) for (int x = 40; x < DW; x += 5) {            /* away from the red column */
      uint32_t got = read_px(out, dst.x + x, dst.y + y), want = ov[y * DW + x] & 0xFFFFFFu;
      for (int c = 0; c < 24; c += 8) { int g = (got >> c) & 255, wv = (want >> c) & 255; if (g - wv > 2 || wv - g > 2) bad++; }
    }
    CHECK(bad == 0);
  }
  /* switching filters rebuilds the layer; the same filter twice reuses it */
  { SDL_Texture *t1; SDL_RenderClear(r); pc_fx_draw(&fx, r, game, GW, GH, &dst, PC_SH_LCD, 0); t1 = fx.fx; pc_fx_draw(&fx, r, game, GW, GH, &dst, PC_SH_LCD, 0); CHECK(fx.fx == t1 && t1 != NULL);
    pc_fx_draw(&fx, r, game, GW, GH, &dst, PC_SH_PLAIN, 0); CHECK(fx.fx == t1);                                   /* PLAIN keeps the old layer around but draws none */
    pc_fx_draw(&fx, r, game, GW, GH, &dst, PC_SH_CRT, 0); CHECK(fx.fx != NULL && fx.mode == PC_SH_CRT + 1); }
  pc_fx_free(&fx);
  CHECK(fx.pre == NULL && fx.fx == NULL);
  SDL_DestroyTexture(game); SDL_DestroyRenderer(r); SDL_FreeSurface(out);
}

int main(void) {
  setenv("SDL_VIDEODRIVER", "dummy", 1);
  setenv("SDL_AUDIODRIVER", "dummy", 1);
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) { printf("FAIL SDL_Init: %s\n", SDL_GetError()); return 1; }
  test_pads(); test_mapping(); test_window(); test_video(); test_filters(); test_audio();
  SDL_Quit();
  printf("test_sdl: %d checks, %d failures\n", checks, fails);
  return fails != 0;
}
