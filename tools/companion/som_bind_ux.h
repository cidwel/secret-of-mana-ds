/* Shoulder-slot input model shared by the Android JNI and the 3DS app (header-only).
 *
 * Only four physical buttons are bound to actions: L1, R1, L2, R2 (3DS: L, R, ZL, ZR).  They are the four fixed shoulder slots of the bottom
 * screen (quick slots 0..3, see ux/som_ui_draw.h): the entry placed in a slot is what its button triggers, by position.  There is no capture
 * flow, no chord and no per-button binding any more; every other physical button always reaches the game.  The engine of som_bind.h is
 * used with four fixed short-press bindings only, which keeps its debounce, its long-press clock and its swallow rules.
 * Callers serialise access themselves (one lock around the engine, the UX state and these calls). */
#ifndef SOM_BIND_UX_H
#define SOM_BIND_UX_H

#include <string.h>
#include "som_bind.h"
#include "ux/som_ui_draw.h"

struct som_bx {
  struct som_bind_ctx ctx;
  int plat;
};

static const uint8_t SOM_BX_SHOULDER_IN[SOM_UX_SHOULDERS] = {SOM_IN_L1, SOM_IN_R1, SOM_IN_L2, SOM_IN_R2};

static inline void som_bx_init(struct som_bx *b, int plat) {
  memset(b, 0, sizeof *b);
  b->plat = plat;
  som_bind_init(&b->ctx, plat);
  for (int i = 0; i < SOM_UX_SHOULDERS; i++) {   /* the action itself lives in the quick slot; the engine only reports the press */
    struct som_bind s;
    memset(&s, 0, sizeof s);
    s.key = SOM_BX_SHOULDER_IN[i]; s.press = SOM_PRESS_SHORT; s.kind = SOM_BK_CYCLE_WEAPON;
    som_bind_add(&b->ctx, &s);
  }
}
/* The shoulder slot 0..3 a fired binding stands for, -1 when it is none. */
static inline int som_bx_shoulder_of(const struct som_bind *b) {
  for (int i = 0; i < SOM_UX_SHOULDERS; i++) if (b->key == SOM_BX_SHOULDER_IN[i]) return i;
  return -1;
}

/* Which `enum som_bm` the UX classifier's mode is. */
static inline int som_bx_mode(const struct som_ux *ux) {
  if (!ux) return SOM_BM_FIELD;
  switch (ux->mode) {
  case SOM_UX_FIELD: return SOM_BM_FIELD;
  case SOM_UX_RING: return SOM_BM_RING;
  case SOM_UX_SHOP: return SOM_BM_SHOP;
  case SOM_UX_TRANSITION: return SOM_BM_TRANSITION;
  case SOM_UX_BOOT: return SOM_BM_BOOT;
  case SOM_UX_FRONTEND: return SOM_BM_MENU;
  default: return SOM_BM_EVENT;   /* events and Flammie flight: the game's own buttons are in use */
  }
}
/* Cutscenes, fades, flights and the title: UI sounds stay quiet there except the error buzz. */
static inline int som_bx_locked(const struct som_ux *ux) {
  return ux && (ux->mode == SOM_UX_EVENT || ux->mode == SOM_UX_TRANSITION || ux->mode == SOM_UX_FLAMMIE || ux->mode == SOM_UX_BOOT);
}

#endif
