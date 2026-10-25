/* Thin accessors so the Python test harness (ctypes) can drive som_direct without knowing the struct layout. */
#include "som_direct.h"
size_t shim_size(void) { return sizeof(struct somd); }
int shim_state(const struct somd *d) { return d->state; }
int shim_err(const struct somd *d) { return d->err; }
unsigned shim_frames(const struct somd *d) { return d->frames; }
int shim_rom_ok(const struct somd *d) { return d->rom_ok; }
int shim_equip(struct somd *d, const uint8_t *w, int h, int t) { return somd_equip(d, w, h, t); }
int shim_item(struct somd *d, const uint8_t *w, int rec, int kind, int slot) { struct somd_target t = {(uint8_t)kind, (uint8_t)slot}; return somd_use_item(d, w, rec, t); }
int shim_cast(struct somd *d, const uint8_t *w, int h, int sp, int sl, int kind, int slot) { struct somd_target t = {(uint8_t)kind, (uint8_t)slot}; return somd_cast(d, w, h, sp, sl, t); }
int shim_spell_class(const struct somd *d, int id) { return somd_spell_class(d, id); }
int shim_spell_mp(const struct somd *d, int id) { return somd_spell_mp_cost(d, id); }
int shim_scan(const uint8_t *w, int cls, int id, int is_spell, struct somd_targets *o) { somd_scan_targets(w, (enum somd_class)cls, id, is_spell, o); return 0; }
int shim_pick(const struct somd_targets *t, int x, int y, int mw, int mh, int r) { struct somd_target o; return somd_pick(t, x, y, mw, mh, r, &o) ? o.slot : -1; }
int shim_group_card(const struct somd_targets *t) { return somd_group_card(t); }
int shim_item_class(int id) { return somd_item_class(id); }
#ifdef SOMD_HAS_SOUND
int shim_sound(struct somd *d, const uint8_t *w, int id) { return somd_sound(d, w, id); }
#endif
#ifdef SOMD_HAS_MENU
int shim_open_menu(struct somd *d, const uint8_t *w, int kind, int hero, int arg) { return somd_open_menu(d, w, kind, hero, arg); }
int shim_close_menu(struct somd *d, const uint8_t *w) { return somd_close_menu(d, w); }
int shim_menu_pad(const struct somd *d) { return somd_menu_pad(d); }
int shim_hide(const struct somd *d) { return somd_hide_video(d); }
#endif
#ifdef SOMD_HAS_QUEUE
int shim_can_cast(const struct somd *d, const uint8_t *w, int hero, int spirit, int slot, struct somd_cast_info *info) { return somd_can_cast(d, w, hero, spirit, slot, info); }
int shim_queue_cast(struct somd *d, const uint8_t *w, int hero, int spirit, int slot, int pk, int ps, int timeout, struct somd_cast_info *info) {
  struct somd_policy p; p.kind = (uint8_t)pk; p.slot = (uint8_t)ps;
  return somd_queue_cast(d, w, hero, spirit, slot, p, timeout, info);
}
int shim_queue_state(const struct somd *d, int hero, struct somd_queue_info *out) { return somd_queue_state(d, hero, out); }
void shim_queue_cancel(struct somd *d, int hero) { somd_queue_cancel(d, hero); }
void shim_queue_ack(struct somd *d, int hero) { somd_queue_ack(d, hero); }
int shim_revive(const struct somd *d, const uint8_t *w, int hero, struct somd_revive_option *out) { return somd_revive_options(d, w, hero, out); }
#endif
int shim_switch_hero(struct somd *d, const uint8_t *w, int hero) { return somd_switch_hero(d, w, hero); }
int shim_name_letter(struct somd *d, uint8_t *w, int l) { return somd_name_letter(d, w, l); }
int shim_name_delete(struct somd *d, uint8_t *w) { return somd_name_delete(d, w); }
void shim_fast_menu(struct somd *d, int on) { somd_set_fast_menu(d, on); }
