/* som_companion: WRAM reader and button-macro state machine for a Secret of Mana companion screen.
 *
 * Pure C99, no libretro or platform dependency.  The frontend hands in a pointer to the 128 KiB WRAM image
 * ($7E:0000-$7F:FFFF) once per emulated frame and gets back a pad mask to press.  Nothing is ever written to
 * WRAM or ROM.  Field meanings and evidence: docs/wram-companion-map.md, data/wram_companion_map.json.
 */
#ifndef SOM_COMPANION_H
#define SOM_COMPANION_H

#include <stdint.h>

#define SOM_WRAM_SIZE 0x20000u
#define SOM_PARTY 3
#define SOM_ITEM_RECORDS 12
#define SOM_WEAPON_TYPES 8
#define SOM_SPIRITS 8

/* libretro RETRO_DEVICE_ID_JOYPAD bit positions (pad mask bit n = button n) */
enum som_button {
  SOM_PAD_B = 0, SOM_PAD_Y, SOM_PAD_SELECT, SOM_PAD_START, SOM_PAD_UP, SOM_PAD_DOWN, SOM_PAD_LEFT, SOM_PAD_RIGHT,
  SOM_PAD_A, SOM_PAD_X, SOM_PAD_L, SOM_PAD_R
};

enum som_mode {
  SOM_MODE_NO_GAME = 0,   /* hero 0 record inactive: boot, title, load */
  SOM_MODE_TRANSITION,    /* screen fade in progress ($E4/$E6) */
  SOM_MODE_EVENT,         /* event VM running ($D0 != 0): dialogue, cutscene, map change script */
  SOM_MODE_RING,          /* ring menu / status page / spell target selection open ($7D == 1) */
  SOM_MODE_FIELD          /* free movement: the only mode in which a macro may start */
};

enum som_ring_category { SOM_CAT_ITEM = 0, SOM_CAT_MAGIC = 1, SOM_CAT_CONFIG = 2, SOM_CAT_WEAPON = 3 };

struct som_hero {
  uint8_t present;        /* record active byte == 1 */
  uint8_t alive;          /* present and status word bit 15 clear */
  uint8_t id;             /* object id 0x80/0x81/0x82 (+0x1E7) */
  uint8_t level;          /* display level (internal + 1) */
  uint16_t hp, hp_max;
  uint8_t mp, mp_max;
  uint8_t recharge;       /* weapon recharge counter: 0 = ready, 0x55 = just swung */
  uint8_t recharge_pct;   /* 100 - recharge * 100 / 85 */
  uint8_t weapon_type;    /* 0..7, +0x1E4 */
  uint8_t weapon_row;     /* weapon table row = type * 9 + tier, +0x1E8 */
  uint8_t weapon_level[SOM_WEAPON_TYPES];  /* 0..8 per weapon type, nibbles of +0x1C0 */
  uint8_t weapon_prog[SOM_WEAPON_TYPES];   /* progress to the next level 0..99 per weapon type, +0x1D0 + type (docs/rom-combat.md 11.1) */
  uint8_t str;                             /* +0x188 strength: attack = str + weapon row power */
  uint8_t spirit_mask[SOM_SPIRITS];        /* +0x1C8: spell availability bits per spirit */
  uint8_t spirit_level[SOM_SPIRITS];       /* nibbles of +0x1C4 */
  uint16_t status;        /* +0x190 status word */
  uint8_t poison_timer;   /* +0x1B6 */
};

struct som_item { uint8_t id; uint8_t qty; };  /* id 0..10 (Candy..Barrel), qty 1..7; qty 0 = empty record */

struct som_ring {
  uint8_t open;       /* $7D */
  uint8_t ready;      /* open, not animating ($1812 == 0) and list built ($1809 != 0x55) */
  uint8_t owner;      /* hero slot whose ring is shown ($1808) */
  uint8_t category;   /* enum som_ring_category ($1809) */
  uint8_t cursor;     /* index of the selected entry ($180A) */
  uint8_t count;      /* number of entries in the visible ring ($180B) */
};

struct som_state {
  uint8_t valid;                 /* WRAM looks like a running game */
  enum som_mode mode;
  uint16_t map_id;               /* $DC */
  uint8_t pad_hero;              /* hero slot bound to pad 1 ($D9), 0xFF if none */
  uint32_t gold;                 /* $CC6A, 24 bit */
  struct som_hero hero[SOM_PARTY];
  uint8_t weapon_slot[SOM_WEAPON_TYPES];   /* $CC54+type: 0xFF = not owned; low 4 bits tier, high bits = equipped marks */
  struct som_item item[SOM_ITEM_RECORDS];  /* $CC48.. in record order */
  struct som_ring ring;
  uint8_t safe_to_inject;
};

void som_read_state(const uint8_t *wram, struct som_state *out);
const char *som_mode_name(enum som_mode m);

/* ------------------------------------------------------------------ macros */
enum som_cmd_kind { SOM_CMD_NONE = 0, SOM_CMD_EQUIP_WEAPON, SOM_CMD_USE_ITEM, SOM_CMD_CAST_MAGIC,
                    SOM_CMD_OPEN_MENU /* open the ring of `hero` (also from an open ring), leave it open: arg = category or 0xFF, spell = config entry to open (B) or 0xFF */ };

struct som_cmd {
  enum som_cmd_kind kind;
  uint8_t hero;       /* ring owner: hero slot 0..2 */
  uint8_t arg;        /* weapon type 0..7 | item record index 0..11 | spirit slot 0..7 (E3C8 index) */
  uint8_t spell;      /* magic only: entry 0..2 in the spell ring */
};

enum som_result {
  SOM_RUNNING = 0,
  SOM_DONE,                 /* effect observed in WRAM */
  SOM_DONE_UNVERIFIED,      /* ring closed but the expected WRAM change was not seen (for example no target for an attack spell) */
  SOM_FAIL_UNSAFE,          /* not in field mode when starting, or the game left field mode while running */
  SOM_FAIL_BAD_ARGUMENT,    /* weapon not owned, empty item record, spirit without spells, hero absent */
  SOM_FAIL_TIMEOUT,         /* an expected WRAM transition did not happen in time */
  SOM_FAIL_UNAVAILABLE,     /* ring category or owner cannot be reached */
  SOM_ABORTED               /* som_cmd_abort() */
};

struct som_macro {
  struct som_cmd cmd;
  enum som_result result;
  int phase, timer, wait, taps, hold;
  uint16_t pad;
  uint16_t blocked;               /* frames spent waiting for the controlled hero's disabling status to end */
  uint8_t sub;                    /* magic: 0 = spirit ring, 1 = spell ring */
  uint8_t want_cat, want_index;
  struct som_state before;        /* state when the macro started (for effect verification) */
  uint32_t frames;
  char note[64];                  /* short reason on failure */
};

void som_macro_init(struct som_macro *m);
/* Starts a macro.  Returns 1 if accepted.  On refusal m->result holds the reason. */
int som_macro_start(struct som_macro *m, const uint8_t *wram, const struct som_cmd *cmd);
/* Call once per frame BEFORE the core runs.  Returns the pad mask to hold for that frame.  Valid while
 * m->result == SOM_RUNNING; afterwards it returns 0. */
uint16_t som_macro_step(struct som_macro *m, const uint8_t *wram);
/* Starts closing the ring (A for sub-rings, then Y), result DONE.  Valid in any mode. */
void som_macro_close_ring(struct som_macro *m);
/* Releases buttons and closes any open ring on the following steps; result becomes SOM_ABORTED. */
void som_macro_abort(struct som_macro *m);
int som_macro_busy(const struct som_macro *m);
const char *som_result_name(enum som_result r);

/* Ring entry index helpers (pure functions of the state). */
int som_weapon_ring_index(const struct som_state *s, int weapon_type);
/* The game marks the holder of a weapon type in $CC54+type (bits 6-7 = hero + 1); the panel highlights by this mark, which is what the
 * game's own ring shows, so a hero without a weapon (weapon_type 0 by default) never looks like it holds the glove. */
static inline int som_weapon_held_by(const struct som_state *s, int hero, int type) {
  return type >= 0 && type < SOM_WEAPON_TYPES && s->weapon_slot[type] != 0xFF && (s->weapon_slot[type] & 0xC0) == (unsigned)((hero + 1) << 6);
}
int som_item_ring_index(const struct som_state *s, int record);
int som_spirit_ring_index(const struct som_state *s, int hero, int spirit);

#endif
