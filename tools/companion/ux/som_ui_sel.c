/* som_ui_sel: what the highlighted ring entry is.  Reads the ring WRAM block and the ROM tables; read-only.
 *
 * Ring WRAM ($1808 owner, $1809 category, $180A cursor, $180B count, $1812 busy) is documented in docs/wram-companion-map.md.
 * Found for this module (docs/companion-ux-20261017.md, section 3f): $1803 bit 7 is set while a spirit's spell sub-ring is
 * open and $1839 holds the id of the highlighted spell (6 * spirit position + 3 * girl + slot; measured: it follows the cursor).
 * The text of item effects and of config entries is authored here (it is not read from the ROM). */
#include <stdio.h>
#include <string.h>
#include "som_ui_priv.h"

const char *const UX_SPIRIT_NAME[8] = {"GNOME", "UNDINE", "SALAMANDO", "SYLPHID", "LUNA", "DRYAD", "SHADE", "LUMINA"};
const char *const UX_WEAPON_NAME[8] = {"GLOVE", "SWORD", "AXE", "SPEAR", "WHIP", "BOW", "BOOMERANG", "JAVELIN"};
const char *const UX_ITEM_NAME[11] = {"CANDY", "CHOCOLATE", "ROYAL JAM", "WALNUT", "HERB", "CUP WISHES", "MAGIC ROPE",
                                      "FL DRUM", "MOOGLE BELT", "MALLET", "BARREL"};
const char *const UX_ELEMENT_NAME[8] = {"EARTH", "WATER", "FIRE", "WIND", "MOON", "WOOD", "LIGHT", "DARK"};
const char *const UX_MENU_NAME[4] = {"ITEM RING", "MAGIC RING", "CONFIG RING", "WEAPON RING"};
const char *const UX_SUBMENU_NAME[7] = {"EQUIP ARMOR", "TARGETS", "STATUS", "WEAPON LEVELS", "ACTION GRID", "CONTROLLER", "WINDOW"};
const char *const UX_SUBMENU_SHORT[7] = {"EQ", "TG", "ST", "LV", "AG", "PD", "WN"};

static const char *const ITEM_EFFECT[11] = {
  "RESTORES A LITTLE HP", "RESTORES SOME HP", "RESTORES MUCH HP", "RESTORES MP", "CURES AILMENTS", "REVIVES AN ALLY",
  "ESCAPE THE DUNGEON", "CALLS FLAMMIE", "MOOGLE FORM", "MIDGE FORM", "BARREL FORM"};
static const char *const CFG_INFO[7] = {"CHANGE ARMOR", "SET TARGETING", "VIEW STATUS", "SEE LEVELS", "EDIT ACTION GRID", "EDIT BUTTONS", "EDIT WINDOWS"};

const char *ux_class_text(int c) {
  switch (c) {
  case SOM_UX_TC_ALLY_ONE: return "ONE ALLY";
  case SOM_UX_TC_ALLY_ONE_OR_ALL: return "ALLY OR ALL";
  case SOM_UX_TC_ENEMY_ONE_OR_ALL: return "ENEMY OR ALL";
  case SOM_UX_TC_ENEMY_ONE: return "ONE ENEMY";
  case SOM_UX_TC_ALLY_DEAD_ONE: return "FALLEN ALLY";
  case SOM_UX_TC_ALLY_STATUS_ONE: return "AILING ALLY";
  case SOM_UX_TC_BOY_ONLY: return "BOY ONLY";
  default: return "NONE";
  }
}

int ux_spell_cost(const struct som_ux *ux, int id) {
  size_t len; const uint8_t *r = ux_rom(ux, &len);
  if (!r || id < 0 || id > 41 || len < 0x102AD0u + 64u * 42u) return -1;
  return r[0x102AD0u + 64u * (unsigned)id + 15];
}

/* Attack of an uncharged swing with weapon type `t` for hero `hero`: strength + the weapon row power (ROM rows at 0x101000, 12 bytes, power at +8, row = type * 9 + grade;
 * docs/rom-combat.md 2.2 and 4.3), -1 without the ROM.  A full charge multiplies it by (2 * level + 4) / 4. */
int ux_weapon_attack(const struct som_ux *ux, int hero, int t) {
  size_t len; const uint8_t *r = ux_rom(ux, &len);
  if (!r || t < 0 || t > 7 || hero < 0 || hero > 2 || ux->st.weapon_slot[t] == 0xFF) return -1;
  unsigned row = (unsigned)t * 9u + (ux->st.weapon_slot[t] & 15u);
  size_t off = 0x101000u + 12u * row + 8u;
  if (off >= len) return -1;
  return (ux->st.hero[hero].str + r[off]) & 255;
}

static void put(char *d, const char *s) { snprintf(d, SOM_UX_INFO_CHARS, "%s", s); }
static void addl(struct som_ux_selinfo *s, const char *fmt, const char *a, int b) {
  if (s->n >= SOM_UX_INFO_LINES) return;
  snprintf(s->line[s->n++], SOM_UX_INFO_CHARS, fmt, a, b);
}

void ux_decode_selection(struct som_ux *ux, const uint8_t *w) {
  struct som_ux_selinfo *s = &ux->sel;
  const struct som_state *st = &ux->st;
  const struct som_ring *rg = &st->ring;
  memset(s, 0, sizeof *s);
  if (ux->mode != SOM_UX_RING && ux->mode != SOM_UX_SHOP) return;
  s->owner = rg->owner < SOM_PARTY ? rg->owner : 0;
  s->menu = rg->category < 4 ? rg->category : 0;
  if (rg->category < 4) put(s->head, UX_MENU_NAME[rg->category]); else put(s->head, "OPENING");
  if (!rg->ready) { put(s->title, "..."); return; }
  const struct som_hero *hero = &st->hero[s->owner];
  char b[40];
  int cur = rg->cursor;
  switch (rg->category) {
  case SOM_CAT_WEAPON: {
    int t = -1;
    for (int i = 0; i < SOM_WEAPON_TYPES; i++) if (som_weapon_ring_index(st, i) == cur) { t = i; break; }
    if (t < 0) { put(s->title, "..."); return; }
    int tier = st->weapon_slot[t] & 15;
    const char *n = som_ux_name(ux, NAME_WEAPON + t * 9 + (tier < 9 ? tier : 0));
    put(s->title, n ? n : UX_WEAPON_NAME[t]);
    s->kind = SOM_UX_BK_WEAPON; s->id = (uint8_t)t; s->valid = 1;
    addl(s, "TYPE %s", UX_WEAPON_NAME[t], 0);
    snprintf(b, sizeof b, "%d:%02d", hero->weapon_level[t], hero->weapon_prog[t] % 100);     /* level:progress (round 3c: the ring card, bottom screen) */
    addl(s, "LEVEL %s", b, 0);
    { int atk = ux_weapon_attack(ux, s->owner, t); if (atk >= 0) { snprintf(b, sizeof b, "%d", atk); addl(s, "ATTACK %s", b, 0); } }
    b[0] = 0;
    for (int h = 0; h < SOM_PARTY; h++)
      if (st->hero[h].present && som_weapon_held_by(st, h, t)) { snprintf(b, sizeof b, "%s", som_ux_hero_name(ux, h)); break; }
    if (b[0]) addl(s, "EQUIPPED %s", b, 0); else addl(s, "%s", "NOT EQUIPPED", 0);
    break;
  }
  case SOM_CAT_ITEM: {
    int rec = -1;
    for (int i = 0; i < SOM_ITEM_RECORDS; i++) if (som_item_ring_index(st, i) == cur) { rec = i; break; }
    if (rec < 0) { put(s->title, "..."); return; }
    int id = st->item[rec].id;
    if (id > 10) { put(s->title, "?"); return; }
    const char *n = som_ux_name(ux, NAME_ITEM + id);
    put(s->title, n ? n : UX_ITEM_NAME[id]);
    s->kind = SOM_UX_BK_ITEM; s->id = (uint8_t)id; s->valid = 1;
    addl(s, "QUANTITY %.0s%d", "", st->item[rec].qty);
    addl(s, "%s", ITEM_EFFECT[id], 0);
    int cls = som_ux_item_class(id);
    if (cls != SOM_UX_TC_UNSUPPORTED) addl(s, "TARGET %s", ux_class_text(cls), 0);
    break;
  }
  case SOM_CAT_MAGIC: {
    if (w[0x1803] & 0x80) {                         /* spell sub-ring of one spirit */
      int id = w[0x1839];                          /* tracks the highlighted spell once the ring is idle */
      if (id < 0 || id > 41) { put(s->title, "..."); return; }
      int sp = id / 6;
      const char *n = som_ux_name(ux, NAME_SPELL + id);
      if (n) put(s->title, n); else { snprintf(b, sizeof b, "SPELL %d", id); put(s->title, b); }
      snprintf(b, sizeof b, "%s SPELLS", UX_SPIRIT_NAME[(sp == 6 && id % 6 >= 3) ? 7 : (sp & 7)]);   /* position 6: sprite Shade, girl Lumina */ put(s->head, b);
      s->sub_ring = 1; s->kind = SOM_UX_BK_SPELL; s->id = (uint8_t)id; s->valid = 1;
      int cost = ux_spell_cost(ux, id);
      if (cost >= 0) addl(s, "MP COST %.0s%d", "", cost);
      addl(s, "TARGET %s", ux_class_text(ux->spell_class[id]), 0);
      addl(s, "ELEMENT %s", UX_ELEMENT_NAME[sp & 7], 0);
      snprintf(b, sizeof b, "%d/%d", hero->mp, hero->mp_max);
      if (cost >= 0 && hero->mp < cost) addl(s, "NOT ENOUGH MP %s", b, 0); else addl(s, "YOUR MP %s", b, 0);
      break;
    }
    int sp = -1;
    for (int i = 0; i < SOM_SPIRITS; i++) if (som_spirit_ring_index(st, s->owner, i) == cur) { sp = i; break; }
    if (sp < 0) { put(s->title, "..."); return; }
    put(s->title, UX_SPIRIT_NAME[sp]);
    s->kind = SOM_UX_BK_SPIRIT; s->id = (uint8_t)sp; s->valid = 1;
    addl(s, "LEVEL %.0s%d", "", hero->spirit_level[sp]);
    addl(s, "ELEMENT %s", UX_ELEMENT_NAME[sp], 0);
    snprintf(b, sizeof b, "%d/%d", hero->mp, hero->mp_max);
    addl(s, "YOUR MP %s", b, 0);
    break;
  }
  case SOM_CAT_CONFIG: {
    if (cur >= SOM_UX_CFG_COUNT) { put(s->title, "..."); return; }
    put(s->title, UX_SUBMENU_NAME[cur]);
    s->kind = SOM_UX_BK_SUBMENU; s->id = (uint8_t)cur; s->valid = 1;
    addl(s, "%s", CFG_INFO[cur], 0);
    break;
  }
  default: break;
  }
}
