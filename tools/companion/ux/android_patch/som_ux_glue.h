/* Proposed Android glue for som_ui_draw (NOT wired in; see docs/companion-ux-20261017.md, section "Android integration").
 * Header-only, to be included once by app/src/main/cpp/som_jni.c.  Every function must be called with the JNI mutex held,
 * like the existing status code.  The ABGR/ARGB conversion stays in som_jni.c (renderStatus does it for the old screen).
 * Build: the UX layer is four files, ux/som_ui_draw.c, ux/som_ui_state.c, ux/som_ui_sel.c plus som_companion.c.
 *
 * Frame loop:   ux_glue_frame(wram) once per emulated frame (after retro_run).
 * Draw:         ux_glue_draw(&macro, px, w, h)   px = w*h uint32 0xAARRGGBB (convert to the bitmap format as renderStatus does)
 * Touch:        ux_glue_touch(x, y, w, h, &dispatch) then act on `dispatch` inside the loop thread:
 *                 ACT_CMD        -> run_direct(&cmd, target)    (same call as handle_command()/touch_target() make today).
 *                                   Tapped while the ring is open: close the ring first (ux_glue_close_ring), then run it.
 *                 ACT_PAD        -> pad_latch |= pad mask       (event NEXT, Flammie START, front end).  The ring and the shop
 *                                   have NO pad buttons any more: the game ring is driven from the game.
 *                 NAME_LETTER    -> name-entry helper, see ux_glue_name_step()
 *                 BIND_BEGIN/CLEAR/CANCEL -> removed in round 5, never returned
 *                 OPTION         -> apply options[opt_id] (toggle: opt_val is the new value; enumeration: opt_val is the value asked
 *                                   for, wrap it into the host range) then answer with ux_glue_option(opt_id, value, text)
 *                 AVATAR         -> make `hero` the ring owner, see ux_glue_avatar()
 *                 OPEN_MENU      -> open a ring or a config screen, see ux_glue_open_menu()
 *                 QUICK_SET      -> persist quick slot `slot` = (bind_kind, bind_id) (kind 0 = cleared); the UI already shows it
 *                 QUICK_EDIT     -> informational (long press opened the slot editor): vibrate if haptics are on
 *                 CLOSE_RING     -> ux_glue_close_ring(): a quick slot that needs a target was tapped in the ring; the UI keeps the
 *                                   pending target, the target panel appears when the field is back
 * Long press (about 500 ms): ux_glue_longpress(x, y, w, h, &dispatch), same dispatch struct (bind screen, slot editor, hero status).
 * The target selection screen of the old code (draw_target / touch_target) is replaced by the UX layer's own target
 * panel, which already produces target_kind/target_slot in the same encoding as struct somd_target.
 *
 * Round 5 (REMOVED: ACT_BIND_BEGIN / BIND_CLEAR / BIND_CANCEL are never returned, ux_glue_bind_store, ux_glue_bind_policy, ux_glue_hotkey,
 * ux_glue_legend; som_ux_get/set_binding, som_ux_bind_begin/end, som_ux_set_legend, som_ux_set/binding_policy no longer exist.  The options page
 * SOM_UX_OPT_BINDINGS and the KEYS list are gone.  NEW: ux_glue_shoulder(which, &d), ux_glue_shoulder_labels(), ux_glue_slot_flash(),
 * ux_glue_flash_entry(), ux_glue_hero_name().  Quick slots 0..3 are L1 R1 L2 R2; persist all 8 slots with QUICK_SET as before and migrate old
 * physical bindings by putting each bound entry in the slot of its button.)
 *
 * Round 4 additions (API stable, nothing removed):
 *   - Pressed look: call ux_glue_touch_down(x,y,w,h) on ACTION_DOWN (returns 1 when an enabled button is under the finger: play the click and
 *     the haptic tick), ux_glue_touch_move() on ACTION_MOVE, ux_glue_touch_up() on UP/CANCEL (before or after ux_glue_touch()).
 *     A bound hotkey that fires can flash its slot with ux_glue_press_slot(slot, 1) / (slot, 0).
 *   - Enemy sprites: ux_glue_set_enemy_sprite(enemy_id, argb, w, h) with an alpha sprite per enemy id (see below); without one the selector draws
 *     a silhouette.  Spell states: ux_glue_spell_avail().  REVIVE chooser: ux_glue_revive_open / ux_glue_revive_choose.
 *   - Target policy: a spell or item that needs a target asks SELECT / NEAR / LESS HP / ALL when it is bound (chooser screens, inside the editor
 *     and before the 'press a button' capture).  QUICK_SET carries `policy`; persist it with the slot and restore it with ux_glue_quick_policy().
 *     A physical hotkey keeps its policy with the binding: restore with ux_glue_bind_policy() and, when the button fires, call
 *     ux_glue_hotkey(kind, id, &dispatch) with the same dispatch handling as a tap (ACT_CMD with the target already chosen for NEAR / LESS_HP /
 *     ALL; return 0 with the target selector now showing for SELECT; nothing to ask when the player is alone).
 *
 * Start-up: restore the persisted state with ux_glue_option() (every option), ux_glue_quick() (every slot) and ux_glue_bind_store()
 * (every physical binding).  The options panel is opened by the gear in the header (the UX layer handles that tap itself); the old
 * config strip (Wide, Fallback, HUD, Core, Binds) must be removed from the Java/Android layer.
 * UI sounds and haptics: the UX layer only reports taps through the dispatch.  Play the confirm sound for any dispatch that
 * returns 1, the cursor sound for taps that return 0 on a tab/cell and the error buzz for locked states; skip it all when
 * options[UI_SOUNDS] is off (ux_glue_option_value(SOM_UX_OPT_UI_SOUNDS)). */
#ifndef SOM_UX_GLUE_H
#define SOM_UX_GLUE_H

#include <stdlib.h>
#include <string.h>
#include "ux/som_ui_draw.h"

static struct som_ux *ux_g;
static uint8_t *ux_pack_blob;

/* pack: contents of assets/som_ui_pack.bin (may be NULL: built-in art).  rom: the user's ROM (names, world map), may be NULL. */
static inline int ux_glue_init(const uint8_t *pack, size_t pack_len, const uint8_t *rom, size_t rom_len) {
  if (!ux_g) ux_g = som_ux_new();
  if (!ux_g) return 0;
  if (pack && pack_len) {
    free(ux_pack_blob);
    ux_pack_blob = malloc(pack_len);
    if (ux_pack_blob) {
      memcpy(ux_pack_blob, pack, pack_len);
      struct som_ux_pack *p = som_ux_pack_load(ux_pack_blob, pack_len);
      if (p) som_ux_set_pack(ux_g, p);
    }
  }
  if (rom && rom_len) som_ux_set_rom(ux_g, rom, rom_len);   /* rom memory must outlive ux_g: the JNI keeps rom_copy */
  return 1;
}

static inline void ux_glue_frame(const uint8_t *wram) { if (ux_g && wram) som_ux_update(ux_g, wram); }

static inline void ux_glue_draw(const struct som_macro *m, uint32_t *px, int w, int h) {
  if (ux_g) som_ux_draw(ux_g, m, px, w, w, h);
}

/* Result of a tap in a form the loop thread can consume. */
struct ux_dispatch {
  int kind;                 /* enum som_ux_act_kind */
  struct som_cmd cmd;
  int target_kind, target_slot;
  unsigned pad;             /* libretro mask, 1 << SOM_PAD_x */
  int letter;
  int bind_kind, bind_id;   /* ACT_BIND_*, OPEN_MENU, QUICK_SET: enum som_ux_bind_kind and id */
  int hero;                 /* AVATAR, OPEN_MENU */
  int slot;                 /* QUICK_SET, QUICK_EDIT */
  int opt_id, opt_val;      /* OPTION */
  int policy;               /* QUICK_SET: enum som_ux_policy chosen for the slot (SELECT, NEAR, ALL, LESS_HP); 0 for entries without a target */
};
static inline void ux_fill(struct ux_dispatch *d, const struct som_ux_action *a) {
  d->kind = a->kind; d->cmd = a->cmd; d->target_kind = a->target_kind; d->target_slot = a->target_slot;
  d->pad = a->pad; d->letter = a->letter; d->bind_kind = a->bind_kind; d->bind_id = a->bind_id;
  d->hero = a->hero; d->slot = a->slot; d->opt_id = a->opt_id; d->opt_val = a->opt_val; d->policy = a->policy;
}

static inline int ux_glue_touch(int x, int y, int w, int h, struct ux_dispatch *d) {
  struct som_ux_action a;
  memset(d, 0, sizeof *d);
  if (!ux_g || !som_ux_touch(ux_g, x, y, w, h, &a)) return 0;
  ux_fill(d, &a);
  return 1;
}

/* Long press: on a slot it opens the slot editor (QUICK_EDIT); on a weapon, item, spirit or spell button it ARMS the entry (returns 0, a 'TAP A SLOT'
 * notice shows) and the next slot tap stores it (QUICK_SET); on an avatar it asks for that hero's STATUS screen (OPEN_MENU). */
static inline int ux_glue_longpress(int x, int y, int w, int h, struct ux_dispatch *d) {
  struct som_ux_action a;
  memset(d, 0, sizeof *d);
  if (!ux_g || !som_ux_longpress(ux_g, x, y, w, h, &a)) return 0;
  ux_fill(d, &a);
  return 1;
}

/* Round 5: there is no binding store.  The four shoulder slots (quick slots 0..3) ARE the L1 / R1 / L2 / R2 buttons by position. */
static inline void ux_glue_shoulder_labels(const char *l1, const char *r1, const char *l2, const char *r2) { if (ux_g) som_ux_set_shoulder_labels(ux_g, l1, r1, l2, r2); }   /* 3DS: "L","R","ZL","ZR" */
/* A physical L1 (which = 0), R1 (1), L2 (2) or R2 (3) press: runs the slot's entry with its stored policy and flashes the slot.
 * Returns 1 with the dispatch to execute (same handling as a tap); 0 when the slot is empty or the target selector opened. */
static inline int ux_glue_shoulder(int which, struct ux_dispatch *d) {
  struct som_ux_action a;
  memset(d, 0, sizeof *d);
  if (!ux_g || !som_ux_shoulder_press(ux_g, which, &a)) return 0;
  ux_fill(d, &a);
  return 1;
}
/* ~0.3 s slot feedback.  Touch taps flash by themselves; call this for a queued spell that finally fires (kind SOM_UX_FLASH_CAST) and anything else
 * the host runs on its own.  entry_kind / entry_id flash every slot holding the entry and the spell-list button. */
static inline void ux_glue_slot_flash(int slot, int kind) { if (ux_g) som_ux_slot_flash(ux_g, slot, kind); }
static inline void ux_glue_flash_entry(int kind, int entry_kind, int entry_id) { if (ux_g) som_ux_flash_entry(ux_g, kind, entry_kind, entry_id); }
/* Hero names as the player entered them (decoded in som_ux_update from WRAM $CC00 + 12 * slot); BOY / GIRL / SPRITE while empty. */
static inline const char *ux_glue_hero_name(int slot) { return ux_g ? som_ux_hero_name(ux_g, slot) : ""; }

/* Options: the host owns the values.  `text` is the value label of enumerations (core name, language) and may be NULL for toggles. */
static inline void ux_glue_option(int id, int value, const char *text) { if (ux_g) som_ux_set_option(ux_g, id, value, text); }
static inline int ux_glue_option_value(int id) { return ux_g ? som_ux_get_option(ux_g, id) : 0; }

/* Quick slots (hotlinks).  Restore at start; store again on every QUICK_SET dispatch. */
static inline int ux_glue_quick(int slot, int kind, int id) { return ux_g ? som_ux_quick_set(ux_g, slot, kind, id) : -1; }

/* Policy of a quick slot (shoulder slots included), restored at start (policy = enum som_ux_policy). */
static inline int ux_glue_quick_policy(int slot, int policy) { return ux_g ? som_ux_quick_set_policy(ux_g, slot, policy) : -1; }
/* Pressed look (see the round 4 notes above). */
static inline int ux_glue_touch_down(int x, int y, int w, int h) { return ux_g ? som_ux_touch_down(ux_g, x, y, w, h) : 0; }
static inline void ux_glue_touch_move(int x, int y, int w, int h) { if (ux_g) som_ux_touch_move(ux_g, x, y, w, h); }
static inline void ux_glue_touch_up(void) { if (ux_g) som_ux_touch_up(ux_g); }
static inline void ux_glue_press_slot(int slot, int on) { if (ux_g) som_ux_press_hit(ux_g, SOM_UX_HIT_QUICK, slot, on); }

/* Enemy sprites for the target selector (replaces the crop helper of the first round 4 cut: the selector shows ONLY the sprite on a plain card).
 * `argb`: straight-alpha 0xAARRGGBB pixels, w x h <= SOM_UX_SPRITE_MAX, transparent background, from the sprite atlas / live render
 * (tools/companion/enemysprites).  One call per enemy id; ux_glue_sprites_wanted() lists the ids on screen that still lack one. */
static inline int ux_glue_set_enemy_sprite(int enemy_id, const uint32_t *argb, int w, int h) { return ux_g ? som_ux_set_enemy_sprite(ux_g, enemy_id, argb, w, h) : -1; }
static inline void ux_glue_clear_enemy_sprites(void) { if (ux_g) som_ux_clear_enemy_sprites(ux_g); }
static inline int ux_glue_sprites_wanted(uint8_t *ids, int max) { return ux_g ? som_ux_enemy_sprites_wanted(ux_g, ids, max) : 0; }

/* Spell availability: one state per spell id 0..41 (SOM_UX_AV_ENABLED / DISABLED / QUEUED, 0 = let the UX derive it), refreshed whenever the
 * direct layer's somd_can_cast reasons or its queue change.  A tap on a QUEUED spell button, slot, or a hotkey of it, dispatches
 * ACT_CANCEL_QUEUED (bind_id = spell id, hero = caster): drop it from the queue. */
static inline void ux_glue_spell_avail(const uint8_t *avail42) { if (ux_g) som_ux_set_spell_avail(ux_g, avail42); }

/* REVIVE chooser: a tap on a fallen hero's card (field) or avatar (ring) opens it by itself.  The host may also open it (returns the number of
 * options, 0 when the hero is not fallen) and read the options to run a choice at once: ux_glue_revive_choose(idx, &d) fills the same dispatch as a
 * tap (ACT_CMD with USE_ITEM or CAST_MAGIC and target_kind 1 / target_slot = the fallen hero, or ACT_CANCEL_QUEUED). */
static inline int ux_glue_revive_open(int hero) { return ux_g ? som_ux_revive_open(ux_g, hero) : 0; }
static inline int ux_glue_revive_choose(int idx, struct ux_dispatch *d) {
  struct som_ux_action a;
  memset(d, 0, sizeof *d);
  if (!ux_g || !som_ux_revive_choose(ux_g, idx, &a)) return 0;
  ux_fill(d, &a);
  return 1;
}

/* Startup / surface (re)creation: draw at once, without waiting for a touch or a WRAM frame (the UX layer shows the title block until the first
 * ux_glue_frame).  Call it right after ux_glue_init() and after every surface change; then draw only when ux_glue_needs_draw(w, h) says so
 * (new state, a pulse, a press, a notice) or after a touch. */
static inline int ux_glue_first_frame(uint32_t *px, int w, int h) {
  if (!ux_g || !px) return 0;
  som_ux_draw(ux_g, NULL, px, w, w, h);
  return 1;
}
/* Pad panel mode (see som_ui_draw.h): toggle it with the host's panel button; while it is on the host feeds D-pad / A / B here instead of the game. */
static inline int ux_glue_nav_toggle(void) { return ux_g ? som_ux_nav_toggle(ux_g) : 0; }
static inline int ux_glue_nav_on(void) { return ux_g ? som_ux_nav_on(ux_g) : 0; }
static inline int ux_glue_nav_move(int dir, int w, int h) { return ux_g ? som_ux_nav_move(ux_g, dir, w, h) : 0; }
static inline int ux_glue_nav_activate(int w, int h, struct ux_dispatch *d) {
  struct som_ux_action a;
  memset(d, 0, sizeof *d);
  if (!ux_g || !som_ux_nav_activate(ux_g, w, h, &a)) return 0;
  ux_fill(d, &a);
  return 1;
}
static inline int ux_glue_nav_back(int w, int h, struct ux_dispatch *d) {
  struct som_ux_action a;
  memset(d, 0, sizeof *d);
  if (!ux_g || !som_ux_nav_back(ux_g, w, h, &a)) return 0;
  ux_fill(d, &a);
  return 1;
}

static inline int ux_glue_quiet(void) { return ux_g ? som_ux_quiet(ux_g) : 0; }
static inline int ux_glue_needs_draw(int w, int h) { return ux_g ? som_ux_needs_redraw(ux_g, w, h) : 0; }

/* Queue feedback chip (replaces the red toast for queue events).  kind: SOM_UX_NOTICE_QUEUED / CANCELLED / DROPPED (text = short reason, e.g.
 * "NO MP") / INFO / ERROR (text = the message).  Non-blocking, fades by itself after SOM_UX_NOTICE_FRAMES frames of ux_glue_frame(). */
static inline void ux_glue_notice(int kind, const char *text) { if (ux_g) som_ux_notice(ux_g, kind, text); }

/* What the ring is showing, for a screen reader or the top-screen overlay: NULL-safe, see struct som_ux_selinfo. */
static inline const struct som_ux_selinfo *ux_glue_selection(void) { return ux_g ? som_ux_selection(ux_g) : NULL; }

/* ---- macros the Android side has to provide (state machines on the loop thread, like som_macro) ----
 * They only press real buttons and read the same WRAM bytes the UX layer reads ($7D ring open, $1808 owner, $1809 category,
 * $180A cursor, $1812 busy, $1803 bit 7 spell sub-ring).  Verified facts (tools/companion/ux/capture_ring.py, four ROMs):
 *   - Y opens the ring at the boy ($1808 == 0) and closes it again (A first when a sub-ring or a target prompt is open).
 *   - X moves the owner 0 -> 1 -> 2 -> 1 -> 2 ...: it never returns to the boy.  Girl <-> sprite is one X press.
 *   - UP cycles the category of that owner (boy: weapon, item, config; girl and sprite also magic).
 *   - Closing and reopening restores owner 0.
 * ux_glue_avatar(hero): if the ring owner is already `hero` do nothing; if owner and hero are both in {1, 2} tap X once; otherwise
 *   close the ring (Y until $7D == 0), reopen it (Y, wait for $1812 == 0), then tap X until $1808 == hero (hero 1: one tap,
 *   hero 2: two).  Timeout 3 s, then leave the ring as it is.  The category stays whatever UP selected: press UP until $1809
 *   equals the category that was showing, when it exists for the new owner.
 * ux_glue_open_menu(kind, id, hero): MENU: open the ring (as above) for `hero` and press UP until $1809 == id.  SUBMENU: do that for
 *   category 2 (config), tap RIGHT/LEFT until $180A == id, press B.  In field mode only; when the ring is already open start from
 *   its current state.  Never confirm anything else.
 * ux_glue_close_ring(): tap A (sub-ring, target) then Y until $7D == 0, at most 12 taps (the same recovery as P_RECOVER). */

/* Name entry: one call per ~28 frames; returns the pad mask to tap (LEFT/RIGHT toward the letter, B when there). */
static inline int ux_glue_name_cursor(void) { return ux_g ? ux_g->name_cursor : 0xFF; }
static inline unsigned ux_glue_name_step(int letter) {
  unsigned m = ux_g ? som_ux_name_next_pad(ux_g, letter) : 0;
  return m ? m : (1u << SOM_PAD_B);
}

static inline void ux_glue_free(void) { if (ux_g) som_ux_delete(ux_g); ux_g = NULL; free(ux_pack_blob); ux_pack_blob = NULL; }

#endif
