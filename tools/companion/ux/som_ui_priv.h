/* Private declarations shared by som_ui_state.c and som_ui_draw.c.  Not part of the public API. */
#ifndef SOM_UI_PRIV_H
#define SOM_UI_PRIV_H

#include "som_ui_draw.h"

/* ---- pack lookup (som_ui_state.c) ---- */
struct pk_rect { uint16_t id, x, y, w, h; int16_t ox, oy; uint16_t adv; };
const struct pk_rect *pk_find(const struct som_ux_pack *p, int id);
int pk_atlas_w(const struct som_ux_pack *p);
/* straight-alpha RGBA of atlas pixel (x, y) packed 0xAARRGGBB */
uint32_t pk_pixel(const struct som_ux_pack *p, int x, int y);

/* ---- ROM helpers ---- */
const uint8_t *ux_rom(const struct som_ux *ux, size_t *len);   /* headerless view of the ROM, or NULL */
void ux_build_world(struct som_ux *ux);

/* Item, weapon, spell, enemy name block indices (events 0x800 + n). */
#define NAME_SPELL 0
#define NAME_WEAPON 0x32
#define NAME_ITEM 0xBA
#define NAME_ENEMY 0xCF

enum som_ux_tclass som_ux_item_class(int item_id);

/* ---- name tables and ring selection decoding (som_ui_sel.c) ---- */
extern const char *const UX_SPIRIT_NAME[8];
extern const char *const UX_WEAPON_NAME[8];
extern const char *const UX_ITEM_NAME[11];
extern const char *const UX_ELEMENT_NAME[8];
extern const char *const UX_MENU_NAME[4];         /* by ring category: item, magic, config, weapon */
extern const char *const UX_SUBMENU_NAME[7];      /* config ring entries */
extern const char *const UX_SUBMENU_SHORT[7];     /* two letters for a slot */
const char *ux_class_text(int tclass);            /* "ONE ENEMY", "ALL ALLIES" ... */
int ux_weapon_attack(const struct som_ux *ux, int hero, int t);   /* uncharged swing: strength + weapon row power, -1 without ROM */
int ux_spell_cost(const struct som_ux *ux, int id);   /* MP cost from the ROM spell table, -1 without ROM */
void ux_decode_selection(struct som_ux *ux, const uint8_t *wram);
void ux_zone_update(struct som_ux *ux, const uint8_t *wram);   /* som_ui_zone.c */

#endif
