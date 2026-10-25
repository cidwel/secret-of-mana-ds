/* Shared button-binding engine of the SoM companion apps (Android JNI and Nintendo 3DS).  Pure C99, no OS, no game state.
 *
 * Model (docs/companion-bindings-20261017.md):
 *   - the platform feeds the set of physical inputs currently held (a bit mask, one bit per `enum som_in`) and a clock;
 *   - the engine answers which bindings fired and which inputs must be hidden from the game (swallowed);
 *   - a binding is `key` or the chord `mod + key`, as a short or a long press, bound to an action (cast, equip, ...);
 *   - inputs the game never reads (L, R, the triggers, sticks, extra buttons) are bindable freely; the game's own
 *     buttons need a confirmation when bound and are only swallowed in free field play (docs, section "used buttons");
 *   - capture mode learns a trigger from the next press(es) for an entry the host wants to bind.
 * The executor that runs the actions lives in som_hotkey.c. */
#ifndef SOM_BIND_H
#define SOM_BIND_H

#include <stddef.h>
#include <stdint.h>

/* Platform-neutral physical inputs.  Bit of input `i` in a held mask is 1u << (i - 1). */
enum som_in {
  SOM_IN_NONE = 0,
  SOM_IN_A, SOM_IN_B, SOM_IN_X, SOM_IN_Y,          /* Android names; on the 3DS the same four positions */
  SOM_IN_L1, SOM_IN_R1, SOM_IN_L2, SOM_IN_R2,      /* 3DS: L, R, ZL, ZR */
  SOM_IN_C, SOM_IN_Z,                              /* extra face buttons the Thor reports */
  SOM_IN_THUMBL, SOM_IN_THUMBR, SOM_IN_MODE,
  SOM_IN_SELECT, SOM_IN_START,
  SOM_IN_DUP, SOM_IN_DDOWN, SOM_IN_DLEFT, SOM_IN_DRIGHT,
  SOM_IN_LS_UP, SOM_IN_LS_DOWN, SOM_IN_LS_LEFT, SOM_IN_LS_RIGHT,   /* left stick; the 3DS circle pad */
  SOM_IN_RS_UP, SOM_IN_RS_DOWN, SOM_IN_RS_LEFT, SOM_IN_RS_RIGHT,   /* right stick; the 3DS C-stick */
  SOM_IN_COUNT
};
#define SOM_IN_BIT(i) (1u << ((i) - 1))

enum som_plat { SOM_PLAT_THOR = 0, SOM_PLAT_3DS_NEW, SOM_PLAT_3DS_OLD, SOM_PLAT_COUNT };

/* Modes the game can be in, as far as button use is concerned. */
enum som_bm { SOM_BM_FIELD = 0, SOM_BM_RING, SOM_BM_MENU, SOM_BM_EVENT, SOM_BM_SHOP, SOM_BM_TRANSITION, SOM_BM_BOOT, SOM_BM_COUNT };

/* libretro joypad bit numbers (the SNES buttons). */
enum { SOM_SB_B = 0, SOM_SB_Y, SOM_SB_SELECT, SOM_SB_START, SOM_SB_UP, SOM_SB_DOWN, SOM_SB_LEFT, SOM_SB_RIGHT, SOM_SB_A, SOM_SB_X, SOM_SB_L, SOM_SB_R, SOM_SB_COUNT };

/* ---- data tables ---- */
struct som_in_info { uint8_t id; const char *name; };   /* long name; short badge labels come from som_bind_label() */
const struct som_in_info *som_bind_input_info(int in);                     /* NULL for an invalid id */
uint32_t som_bind_platform_inputs(int plat);                               /* held-mask bits the platform has */
const char *som_bind_label(int plat, int in);                              /* up to 4 characters: "R1", "ZL", "L3" */
const char *som_bind_name(int plat, int in);                               /* "R1 (right shoulder)" */
int som_bind_game_bit(int plat, int in);                                   /* SNES button the input drives, -1 when none */
uint16_t som_bind_game_mask(int plat, uint32_t held);                      /* libretro joypad mask for a set of inputs */
uint16_t som_bind_used_mask(int mode);                                     /* SNES buttons the game reads in that mode */
const char *som_bind_mode_name(int mode);
int som_bind_canonical(int plat, int in);                                  /* a game button of free play: bind needs confirmation */
int som_bind_swallow_ok(int plat, int in, int mode);                       /* may a binding on `in` hide it from the game now */

/* ---- bindings ---- */
enum som_bk { SOM_BK_NONE = 0, SOM_BK_CAST, SOM_BK_EQUIP, SOM_BK_CYCLE_WEAPON, SOM_BK_ITEM, SOM_BK_CYCLE_SPIRIT, SOM_BK_SELECT_SPIRIT, SOM_BK_COUNT };
enum som_pol { SOM_POL_ASK = 0, SOM_POL_NEAREST_ENEMY, SOM_POL_ALL_ENEMIES, SOM_POL_LOWEST_ALLY, SOM_POL_WHOLE_PARTY, SOM_POL_SELF, SOM_POL_COUNT };
enum som_press { SOM_PRESS_SHORT = 0, SOM_PRESS_LONG = 1 };

struct som_bind {
  uint8_t mod;      /* chord modifier (enum som_in) or 0 */
  uint8_t key;      /* the input that fires the binding */
  uint8_t press;    /* enum som_press */
  uint8_t kind;     /* enum som_bk */
  int16_t a, b, c;  /* action arguments, see som_bind_action_label() */
  uint8_t policy;   /* enum som_pol, for CAST and ITEM */
};

#define SOM_BIND_MAX 64
#define SOM_BIND_LONG_MS 450       /* a press held at least this long is "long" */
#define SOM_BIND_CAPTURE_MS 15000  /* capture gives up after this long */
#define SOM_BIND_FIRES 8

enum som_be { SOM_BE_NONE = 0, SOM_BE_CAPTURE_CONFIRM /* a game button: press the same trigger again */, SOM_BE_CAPTURE_DONE, SOM_BE_CAPTURE_CANCEL };

struct som_bind_out {
  int nfire;
  int fire[SOM_BIND_FIRES];   /* indexes into ctx->b */
  uint32_t swallow;           /* held inputs the game must not see */
  int event;                  /* enum som_be */
  struct som_bind trigger;    /* CAPTURE_CONFIRM / CAPTURE_DONE: mod, key, press of the learned trigger */
};

struct som_bind_ctx {
  int plat, n;
  struct som_bind b[SOM_BIND_MAX];
  /* runtime */
  uint32_t prev, swallowed, chord_used, long_fired;
  uint32_t down_ms[SOM_IN_COUNT];
  int8_t short_idx[SOM_IN_COUNT], long_idx[SOM_IN_COUNT];   /* armed binding of a held key, -1 none */
  int cap_on, cap_phase, cap_first, cap_second;
  uint32_t cap_start, cap_t1, cap_t2, cap_unfreeze;
  struct som_bind cap_conf;
  int cap_conf_valid;
};

void som_bind_init(struct som_bind_ctx *c, int plat);
void som_bind_defaults(struct som_bind_ctx *c);                 /* L/R weapon, ZL/ZR spirit (what the platform has) */
int som_bind_add(struct som_bind_ctx *c, const struct som_bind *b);   /* replaces the same trigger; returns its index or -1 */
void som_bind_remove(struct som_bind_ctx *c, int idx);
void som_bind_clear(struct som_bind_ctx *c);
int som_bind_find(const struct som_bind_ctx *c, int mod, int key, int press);
int som_bind_find_entry(const struct som_bind_ctx *c, int kind, int a, int b, int cc);   /* first binding of an entry or -1 */
size_t som_bind_serialize(const struct som_bind_ctx *c, char *out, size_t n);   /* text, one binding per line */
int som_bind_parse(struct som_bind_ctx *c, const char *text);                    /* returns the count, -1 on a bad line */

/* Description strings (for the editor and the logs). */
void som_bind_action_label(const struct som_bind *b, char *out, size_t n);       /* "CAST SPRITE GNOME 1 [NEAREST ENEMY]" */
void som_bind_trigger_label(int plat, const struct som_bind *b, char *out, size_t n);   /* "R1+X (long)" */
void som_bind_badge(int plat, const struct som_bind *b, char *out, size_t n);    /* at most 4 characters + NUL */
const char *som_bind_policy_name(int policy);
const char *som_bind_weapon_name(int type);
const char *som_bind_item_name(int id);
const char *som_bind_spirit_name(int idx);
const char *som_bind_hero_name(int hero);

/* Feeds the inputs held right now.  Call it on every input change and at least every ~30 ms while anything is held or a
 * capture is active.  `now_ms` must be monotonic.  `mode` is an `enum som_bm`. */
void som_bind_update(struct som_bind_ctx *c, uint32_t held, uint32_t now_ms, int mode, struct som_bind_out *out);

void som_bind_capture_begin(struct som_bind_ctx *c, uint32_t now_ms);
void som_bind_capture_cancel(struct som_bind_ctx *c);
int som_bind_capturing(const struct som_bind_ctx *c);

/* Per-ROM key for the stored bindings: image size and the header title/checksum bytes ("200000-xxxxxxxx"). */
void som_bind_rom_variant(const uint8_t *rom, size_t len, char *out, size_t n);

/* Analog helpers: 4 direction bits (bit0 up, 1 down, 2 left, 3 right) from a stick position in -1..1 (y grows downwards),
 * with hysteresis against the previous bits; `trigger` returns 1/0 with hysteresis against `prev` (0.6 on, 0.4 off). */
unsigned som_bind_stick_dirs(unsigned prev, float x, float y);
int som_bind_trigger(int prev, float v);

#endif
