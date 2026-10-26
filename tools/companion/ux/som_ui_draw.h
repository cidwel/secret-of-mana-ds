/* som_ui_draw: context-aware bottom-screen renderer and touch layout for the Secret of Mana companion.
 *
 * Successor candidate of som_status_draw (320x240 only).  It renders every game mode (field, event, ring, shop, Flammie
 * flight, title, name entry ...) at any size from 320x240 (3DS) to 1240x1080 / 1080x1240 (AYN Thor bottom screen).
 * Read-only: it only reads WRAM (and, optionally, the ROM image for names and the world map); it never writes either.
 * Depends on som_companion.h (state reader and macro API).  Pure C99, no OS dependency.
 *
 * Pixels are 0xAARRGGBB (alpha always 0xFF), `stride` is in pixels.  Layout is computed in "virtual pixels" (vp): the
 * canvas is scaled by an integer s = max(1, round(min(w, h) / 250)), so 320x240 is 1:1 and 1240x1080 is 4x
 * (310x270 vp).  One vp is about 0.19 mm on a 3.0 in 3DS panel and 0.24 mm on the Thor, so the same vp layout has the
 * same physical size on both.  Design and measured facts: docs/companion-ux-20261017.md.
 */
#ifndef SOM_UI_DRAW_H
#define SOM_UI_DRAW_H

#include <stddef.h>
#include <stdint.h>
#include "../som_companion.h"

#define SOM_UX_LOG_LINES 6
#define SOM_UX_LINE_CHARS 36
#define SOM_UX_ENEMIES 10
#define SOM_UX_NAMES 335
#define SOM_UX_NAME_LEN 20
#define SOM_UX_LAST_NAME 8                /* name entry letters + 1 (the game takes 6) */
#define SOM_UX_WORLD 512
#define SOM_UX_SPRITE_MAX 96               /* largest enemy sprite side in pixels (see som_ux_set_enemy_sprite) */
#define SOM_UX_SPRITES 16                  /* sprites kept at once, one per enemy id */

/* What the bottom screen is doing.  Derived from WRAM only (som_ux_classify). */
enum som_ux_mode {
  SOM_UX_BOOT = 0,     /* WRAM not initialised: title / logos */
  SOM_UX_FRONTEND,     /* game select, name entry, load: hero 0 active, max HP 0, $E4 == 0 */
  SOM_UX_FIELD,        /* free movement */
  SOM_UX_EVENT,        /* dialogue, cutscene, scripted flight ($D0 != 0) */
  SOM_UX_TRANSITION,   /* fade or warp in progress */
  SOM_UX_RING,         /* ring menu, status page, spell sub-ring, target selection ($7D == 1, $1847 == 0) */
  SOM_UX_SHOP,         /* buy / sell / upgrade ($7D == 1, $1847 in 1..3) */
  SOM_UX_FLAMMIE,      /* free Flammie flight ($E4 == 4, $E6 == 15, $D0 == 0) */
  SOM_UX_MODE_COUNT
};

enum som_ux_tab { SOM_UX_TAB_MAP = 0, SOM_UX_TAB_WEAPON, SOM_UX_TAB_ITEM, SOM_UX_TAB_MAGIC, SOM_UX_TAB_COUNT };   /* MAP is the first tab */

/* Options panel (the gear button in the header opens it in every mode).  page 0 = closed. */
enum som_ux_opt_page { SOM_UX_OPT_CLOSED = 0, SOM_UX_OPT_MAIN, SOM_UX_OPT_BINDINGS /* RESERVED: removed in round 5 (opening it shows the main page) */,
                       SOM_UX_OPT_QUICK /* quick-slot editor */,
                       SOM_UX_OPT_STATES /* save-state slots (Thor, see som_ux_set_options_ext) */, SOM_UX_OPT_CREDITS /* scrollable special-thanks text */ };
/* Option ids.  The host owns the values; the UI shows them (som_ux_set_option) and reports taps (SOM_UX_ACT_OPTION). */
enum som_ux_opt { SOM_UX_OPT_WIDE = 0 /* enumerated: text OFF / 17 / 27 / 46 */, SOM_UX_OPT_FALLBACK, SOM_UX_OPT_HUD_HIDE, SOM_UX_OPT_CORE, SOM_UX_OPT_UI_SOUNDS,
                  SOM_UX_OPT_HAPTICS, SOM_UX_OPT_LANGUAGE,
                  SOM_UX_OPT_FIT /* top-screen scaling: enumerated, text FILL / FIT / INTEGER / STRETCH */,
                  SOM_UX_OPT_SHADER /* top-screen filter: enumerated, text NONE / SHARP / SCANLINES / LCD GRID / CRT (only on the extended page) */,
                  SOM_UX_OPT_FFWD /* fast-forward button: text is the bound key or NONE; a tap asks the host to wait for a key (extended page) */,
                  SOM_UX_OPT_ENEMY_HP /* enemy HP bars: 0 FAIR (only enemies hit by Analyzer, the default), 1 ALWAYS, 2 HIDDEN */, SOM_UX_OPT_COUNT };
#define SOM_UX_STATE_SLOTS 8
#define SOM_UX_THUMB_W 128
#define SOM_UX_THUMB_H 72
#define SOM_UX_CREDITS_MAX 6144

/* Quick-slot entries: what a slot can hold (weapon, item, spell, spirit, ring, config screen).  Round 5: there is no per-button bind editor any
 * more; the four SHOULDER slots (slots 0..3) ARE bound to L1 / R1 / L2 / R2 by position (see enum som_ux_shoulder). */
enum som_ux_bind_kind { SOM_UX_BK_NONE = 0, SOM_UX_BK_WEAPON /* id = weapon type 0..7 */, SOM_UX_BK_ITEM /* id = item id 0..10 */,
                        SOM_UX_BK_SPELL /* id = spell id 0..41 (6 * spirit position + 3 * girl + slot) */,
                        SOM_UX_BK_SPIRIT /* id = spirit 0..7: opens that spirit's spell list */,
                        SOM_UX_BK_MENU /* id = ring category (enum som_ring_category): opens that ring */,
                        SOM_UX_BK_SUBMENU /* id = config ring entry 0..6 (enum som_ux_config_entry): opens that screen */,
                        SOM_UX_BK_LAST = SOM_UX_BK_SUBMENU };
enum som_ux_config_entry { SOM_UX_CFG_EQUIP_ARMOR = 0, SOM_UX_CFG_TARGETING, SOM_UX_CFG_STATUS, SOM_UX_CFG_LEVELS,
                           SOM_UX_CFG_ACTION_GRID, SOM_UX_CFG_CONTROLLER, SOM_UX_CFG_WINDOW, SOM_UX_CFG_COUNT };
#define SOM_UX_QUICK_N 8
/* Target policy of a spell or item that needs a target (asked when the entry is bound, applied when its slot or hotkey is used):
 * SELECT shows the target selector, NEAR picks the nearest enemy, LESS_HP the ally with the lowest HP percentage, ALL every enemy or every ally
 * (only for entries that allow a group).  Entries that need no target never ask. */
enum som_ux_policy { SOM_UX_POL_SELECT = 0, SOM_UX_POL_NEAR, SOM_UX_POL_ALL, SOM_UX_POL_LESS_HP, SOM_UX_POL_COUNT };
struct som_ux_quick { uint8_t kind, id, policy; };   /* kind 0 = empty slot */
/* The four fixed shoulder slots are the quick slots 0..3: 0 = L1 (3DS: L), 1 = R1 (R), 2 = L2 (ZL), 3 = R2 (ZR).  Slots 4..7 are the free,
 * touch-only slots in the middle of the quick-slot area.  Pressing the physical button runs the slot: som_ux_shoulder_press(). */
enum som_ux_shoulder { SOM_UX_SH_L1 = 0, SOM_UX_SH_R1, SOM_UX_SH_L2, SOM_UX_SH_R2, SOM_UX_SHOULDERS };
#define SOM_UX_FIRST_FREE_SLOT 4

/* Same numeric values as `enum somd_class` of tools/companion/direct (checked there by a static assert in the glue). */
enum som_ux_tclass {
  SOM_UX_TC_NONE = 0, SOM_UX_TC_ALLY_ONE, SOM_UX_TC_ALLY_ONE_OR_ALL, SOM_UX_TC_ENEMY_ONE_OR_ALL, SOM_UX_TC_ENEMY_ONE,
  SOM_UX_TC_ALLY_DEAD_ONE, SOM_UX_TC_ALLY_STATUS_ONE, SOM_UX_TC_BOY_ONLY, SOM_UX_TC_UNSUPPORTED
};

/* What the highlighted ring entry is, decoded from WRAM ($1808 owner, $1809 category, $180A cursor, $180B count, $1803 bit 7
 * spell sub-ring, $1839 spell base id) and the ROM name and spell tables.  valid == 0 while the ring is still opening. */
#define SOM_UX_INFO_LINES 5
#define SOM_UX_INFO_CHARS 26
struct som_ux_selinfo {
  uint8_t valid;
  uint8_t kind, id;         /* bindable identity (enum som_ux_bind_kind), kind 0 when the entry cannot be bound */
  uint8_t menu;             /* ring category currently open (SOM_UX_BK_MENU id) */
  uint8_t owner;            /* hero slot whose ring it is */
  uint8_t sub_ring;         /* spell sub-ring of a spirit */
  char head[SOM_UX_INFO_CHARS];    /* "WEAPON RING", "SPIRIT RING", "GNOME SPELLS" ... */
  char title[SOM_UX_INFO_CHARS];   /* name of the highlighted entry */
  char line[SOM_UX_INFO_LINES][SOM_UX_INFO_CHARS];
  uint8_t n;
};

struct som_ux_actor {
  uint8_t slot;          /* object record index: 0..2 heroes, 3.. enemies ($E000 + 0x200 * slot) */
  uint8_t present;       /* active byte == 1 and a real identity */
  uint8_t alive;
  uint8_t on_screen;     /* 8 <= x < 0xF8 and y < 0xD8, the game's own visibility test */
  uint8_t boss;          /* +0x1FB bit 6 */
  uint8_t id;            /* +0x180 object identity (enemies: index into the ROM name block at 0xCF) */
  uint16_t hp, hp_max;
  uint16_t sx, sy;       /* screen position in the 256x224 game frame */
  uint16_t status;
};

/* Optional graphics pack (see tools/companion/ux/build_assets.py).  NULL pack = built-in placeholder art. */
struct som_ux_pack;
struct som_ux_pack *som_ux_pack_load(const uint8_t *blob, size_t len);   /* copies nothing: blob must outlive */
void som_ux_pack_free(struct som_ux_pack *p);

struct som_ux {
  /* ---- decoded each frame by som_ux_update() ---- */
  struct som_state st;
  enum som_ux_mode mode;
  uint8_t act0, e4, e6, d0, shop_mode;     /* raw bytes used by the classifier */
  struct som_ux_actor actor[16];           /* slots 0..15 */
  uint8_t n_enemies;                       /* enemy slots in `enemy_idx` */
  uint8_t enemy_idx[SOM_UX_ENEMIES];       /* actor[] indices of enemies, boss first, then by on-screen order */
  int8_t boss;                             /* actor index of a boss or -1 */
  uint8_t analyzed[32];                    /* enemy ids (bit per id) a hero cast Analyzer (spell 0x17) on in this game: their HP may be shown (SOM_UX_OPT_ENEMY_HP) */
  uint8_t cast_prev[SOM_PARTY][11], cast_primed;   /* each hero's last cast record ($E170..$E17A): a change is a new cast */
  uint16_t fl_x, fl_y, fl_heading;         /* Flammie: $FA, $FC (8 units = 1 world map pixel), $F6 (1024 = one turn) */
  uint8_t fl_world_ok;                     /* position inside the 4096 x 4096 main world */
  char line[SOM_UX_LINE_CHARS + 1];        /* current dialogue line ($A1A4) */
  char log[SOM_UX_LOG_LINES][SOM_UX_LINE_CHARS + 1];   /* most recent first; log[0] is the newest distinct line */
  uint8_t log_n;
  uint8_t name_cursor;                     /* name entry: letter under the cursor (0..25), 0xFF unknown */
  uint8_t fe_name_entry;                   /* a name entry is showing: the new game's (FRONTEND) or one opened in game (EVENT) */
  uint8_t name_hero;                       /* name entry: hero being named ($A22B, 0..2), 0xFF unknown */
  uint8_t name_wait_hero, name_wait_t;     /* a name entry just closed: frames left to read the confirmed name of that hero */
  uint8_t name_done;                       /* one-shot: hero + 1 whose name was confirmed (som_ux_name_confirmed) */
  char name_done_text[SOM_UX_LAST_NAME];
  char name_last[3][SOM_UX_LAST_NAME];     /* host-supplied last confirmed name per hero (A..Z, "" none): the LAST NAME key */
  uint16_t trail[32][2];                   /* Flammie: last positions in units, newest at trail[trail_n - 1] */
  uint8_t trail_n;
  /* ---- UI state (set by som_ux_touch) ---- */
  uint8_t tab, hero, spirit;               /* spirit 0xFF = choosing a spirit */
  uint8_t pend_kind;                       /* 0 none, 1 item, 2 spell: waiting for a target card */
  uint8_t pend_class, pend_arg, pend_spell, pend_hero;
  /* ---- optional inputs ---- */
  const uint8_t *rom; size_t rom_len;
  struct som_ux_pack *pack;
  uint8_t spell_class[42];
  char names[SOM_UX_NAMES][SOM_UX_NAME_LEN];
  uint8_t names_ok;
  uint32_t *world;                         /* 512x512 decoded world map, built lazily from the ROM */
  uint32_t *world_scaled; int world_scaled_n;
  uint8_t world_ok;
  uint32_t *zone;                          /* zone map of the current area for the MAP tab (som_ui_zone.c) */
  int zone_w, zone_h, zone_ppt, zone_map;  /* image size, image pixels per 16x16 tile, map id it shows */
  uint8_t zone_ok, zone_ext, zone_tiles_w, zone_tiles_h, map_small;  /* zone_ext: image supplied by the host; map_small 0 = LARGE view (default: whole zone over the whole content area), 1 = small view (close-up and enemy list); the player's last choice stays for the session */
  uint32_t zone_sum;
  uint16_t cam_x, cam_y;                   /* camera ($A8/$AA): world pixel of the top-left of the game view */
  uint8_t flags;                           /* SOM_UX_F_* */
  uint8_t frame_no;
  /* ---- shoulder slots and slot feedback ---- */
  char shoulder_label[SOM_UX_SHOULDERS][4];   /* printed permanently in the shoulder slots (default L1 R1 L2 R2) */
  uint8_t flash_t[SOM_UX_QUICK_N], flash_kind[SOM_UX_QUICK_N];   /* slot feedback animation: frames left, enum som_ux_flash_kind */
  uint8_t sflash_t, sflash_id;             /* spell-list button feedback: frames left, spell id */
  uint8_t arm_kind, arm_id;                /* entry waiting for a slot tap (long press on a list entry), quick_arm == 3 */
  /* ---- ring selection, quick slots, options (see the matching functions below) ---- */
  struct som_ux_selinfo sel;
  struct som_ux_quick quick[SOM_UX_QUICK_N];
  uint8_t nav_on, nav_kind, nav_a;         /* pad panel mode: the focused target (see som_ux_nav_toggle) */
  uint8_t press_on, press_kind, press_a;   /* pressed look: the target under the finger (enum som_ux_hit kind and parameter) */
  uint8_t pol_wait, pol_kind, pol_id, pol_dest, pol_slot;   /* target-policy chooser: entry being bound, destination (0 quick slot, 1 hotkey), slot */
  uint8_t qe_spirit;                       /* quick-slot editor SPELL tab: 0 = choosing a spirit, n = the spells of spirit n - 1 */
  uint32_t sprite[SOM_UX_SPRITES][SOM_UX_SPRITE_MAX * SOM_UX_SPRITE_MAX]; uint8_t spr_id[SOM_UX_SPRITES], spr_w[SOM_UX_SPRITES], spr_h[SOM_UX_SPRITES], spr_ok[SOM_UX_SPRITES], spr_next;
  uint8_t spell_avail[42];                 /* host-supplied per-spell state (enum som_ux_avail), 0 = derived from the party state */
  /* ---- round 3: what the player has discovered, the quiet bottom screen, game identity (see docs/thor-feedback-round3-20261018.md) ---- */
  uint8_t armed;                           /* 1 once the story flag $CF00 is set (the boy took the first sword, ring menus enabled); 0 at the title, in the name entry and in the intro.  While 0 the screen is blank outside the name entry */
  uint8_t seen_w, seen_s;                  /* bit n: weapon type n / spirit byte n was owned at least once in this playthrough */
  uint16_t seen_i;                         /* bit n: item id n */
  uint8_t key_names[36];                   /* the hero names ($CC00, 3 x 12) behind game_key */
  uint8_t fe_saw_name, game_event;         /* name entry seen during this frontend visit; one-shot SOM_UX_GAME_* event */
  uint8_t prev_mode;
  uint32_t game_key;                       /* FNV-1a of the three hero names ($CC00..$CC23), 0 while unknown (title, name entry) */
  uint8_t had_dialogue;                    /* a dialogue line is on screen (EVENT mode and $A1A4 holds text) */
  char hero_name[3][10];                   /* names the player gave (WRAM $CC00 + 12 * slot, decoded, upper case); empty = use the defaults */
  uint8_t notice_kind, notice_t; char notice_text[32];   /* small non-blocking chip, see som_ux_notice() */
  uint8_t drawn; uint16_t drawn_w, drawn_h; uint32_t drawn_hash;   /* first-frame / redraw bookkeeping, see som_ux_needs_redraw() */
  uint8_t rev_wait, rev_hero;              /* REVIVE chooser: open, and the fallen hero it is for */
  uint8_t quick_arm;                       /* 0 none, 1 ENTRY > SLOT, 2 MENU > SLOT, 3 a long-pressed list entry: the next slot tap stores it */
  uint8_t qe_slot, qe_cat, qe_page;        /* quick-slot editor: slot being edited, picker category and page */
  uint8_t qe_four;                         /* PC: the quick-slot editor shows only the four shoulder slots (som_ux_set_four_slots) */
  uint8_t opt_page;                        /* enum som_ux_opt_page */
  uint8_t quit_armed;                      /* MAIN MENU was tapped once: the next tap quits */
  uint8_t opt_from_main;                   /* the quick-slot editor was entered from the options page: BACK returns there, otherwise it closes */
  uint8_t opt_val[SOM_UX_OPT_COUNT];
  char opt_text[SOM_UX_OPT_COUNT][12];     /* value text of enumerated options (core name, language) */
  /* ---- Thor round 4: extended options page, save-state slots and credits (all zero = the classic page of the 3DS and the PC) ---- */
  uint8_t opt_ext;                         /* som_ux_set_options_ext(): the options page also lists SHADER, SAVE STATES and CREDITS */
  uint8_t st_sel, st_gen;                  /* slot shown selected on the save-state page; bumped by every som_ux_set_state_slot (redraw) */
  struct { uint8_t used; char time[16]; } st_slot[SOM_UX_STATE_SLOTS];
  uint32_t st_thumb[SOM_UX_STATE_SLOTS][SOM_UX_THUMB_W * SOM_UX_THUMB_H];
  char cr_text[SOM_UX_CREDITS_MAX];        /* credits: lines separated by \n, "# " starts a heading */
  int16_t cr_scroll, cr_lines, cr_vis;     /* first visible line; wrapped lines and lines per page of the last draw */
};
#define SOM_UX_F_SHOW_RADAR_ONLY_ONSCREEN 1
/* Game identity events, read and cleared with som_ux_game_event(): the frontend (title / game select / name entry) was left for the game.
 * NEW: the name entry was visited (a new game began: reset per-game settings); LOAD: a saved game was loaded. */
enum som_ux_game_ev { SOM_UX_GAME_NONE = 0, SOM_UX_GAME_NEW, SOM_UX_GAME_LOAD,
                      SOM_UX_GAME_RENAME /* the same game got a hero name (a hero joined): the key changed, the host keeps its data under the new key */ };
int som_ux_game_event(struct som_ux *ux);
uint32_t som_ux_game_key(const struct som_ux *ux);
/* Discovered entries packed as w | s << 8 | i << 16 (persist per game; set ORs them in). */
uint32_t som_ux_seen_get(const struct som_ux *ux);
void som_ux_seen_set(struct som_ux *ux, uint32_t packed);
int som_ux_is_armed(const struct som_ux *ux);
/* 0 = a normal screen, 1 = nothing to show (the host may put its own background behind it), 2 = only the faint "LOOK UP" hint. */
int som_ux_quiet(const struct som_ux *ux);

enum som_ux_act_kind { SOM_UX_ACT_NONE = 0, SOM_UX_ACT_CMD, SOM_UX_ACT_PAD, SOM_UX_ACT_NAME_LETTER,
                       SOM_UX_ACT_BIND_BEGIN_RESERVED /* removed in round 5, never returned */, SOM_UX_ACT_BIND_CLEAR_RESERVED,
                       SOM_UX_ACT_BIND_CANCEL_RESERVED,
                       SOM_UX_ACT_OPTION /* opt_id, opt_val: the host applies it and answers with som_ux_set_option */,
                       SOM_UX_ACT_AVATAR /* hero: make that hero the ring owner (press X until $1808 == hero) */,
                       SOM_UX_ACT_OPEN_MENU /* bind_kind MENU or SUBMENU + bind_id (+ hero): open that ring or screen */,
                       SOM_UX_ACT_QUICK_SET /* slot, bind_kind, bind_id (kind 0 = cleared): the host persists the slot */,
                       SOM_UX_ACT_QUICK_EDIT /* long press on a slot: the editor opened, informational (haptics) */,
                       SOM_UX_ACT_CLOSE_RING /* press B until the field is back (a hotlink that needs a target was tapped in the ring) */,
                       SOM_UX_ACT_CANCEL_QUEUED /* a queued spell button was tapped: drop bind_id (spell id) from the host's queue */,
                       SOM_UX_ACT_SWITCH_HERO /* hero: a living hero's card was tapped in the field: make it the controlled hero (somd_switch_hero) */,
                       SOM_UX_ACT_NAME_DELETE /* name entry: delete the last letter (somd_name_delete) */,
                       SOM_UX_ACT_STATE /* save-state page: opt_id = 0 save, 1 load, 2 slot selected; opt_val = slot (the host does the work) */,
                       SOM_UX_ACT_MAIN_MENU /* options page, MAIN MENU tapped twice: the host ends the game and returns to the start menu */,
                       SOM_UX_ACT_NAME_LAST /* name entry: hero = the hero being named; clear the typed letters, then type som_ux_last_name(hero) */ };

/* Result of a tap.  ACT_NONE: the tap only changed UI state (tab, hero, pending target) or hit nothing. */
struct som_ux_action {
  uint8_t kind;
  struct som_cmd cmd;       /* ACT_CMD: usable as is with som_macro_start(); a direct frontend maps cmd + target instead */
  uint8_t target_kind;      /* 0 none, 1 one, 2 all allies, 3 all enemies (enum somd_target_kind) */
  uint8_t target_slot;      /* ACT_CMD with target_kind 1: actor slot 0..2 heroes, 3.. enemies */
  uint16_t pad;             /* ACT_PAD: libretro button mask to tap */
  uint8_t letter;           /* ACT_NAME_LETTER: 0..25 = A..Z */
  uint8_t bind_kind, bind_id;   /* ACT_BIND_*, ACT_OPEN_MENU, ACT_QUICK_SET: the entry concerned (enum som_ux_bind_kind) */
  uint8_t hero;             /* ACT_AVATAR, ACT_OPEN_MENU: hero slot 0..2 */
  uint8_t slot;             /* ACT_QUICK_SET, ACT_QUICK_EDIT: quick slot 0..SOM_UX_QUICK_N-1 */
  uint8_t policy;           /* ACT_QUICK_SET: enum som_ux_policy chosen for the slot (0 for entries without a target) */
  uint8_t opt_id, opt_val;  /* ACT_OPTION: enum som_ux_opt and the value asked for (toggles 0/1, enumerations current + 1) */
};

/* ------------------------------------------------------------------ lifecycle */
void som_ux_init(struct som_ux *ux);
void som_ux_set_pack(struct som_ux *ux, struct som_ux_pack *p);
/* The ROM image (headered or not) gives names and the world map.  The memory must outlive the struct. */
void som_ux_set_rom(struct som_ux *ux, const uint8_t *rom, size_t len);
void som_ux_free(struct som_ux *ux);              /* releases the world buffers */
size_t som_ux_sizeof(void);
struct som_ux *som_ux_new(void);                  /* malloc + init, for foreign-function callers */
void som_ux_delete(struct som_ux *ux);

/* Call once per emulated frame (cheap).  Read-only on wram. */
void som_ux_update(struct som_ux *ux, const uint8_t *wram);
enum som_ux_mode som_ux_classify(const uint8_t *wram);
const char *som_ux_mode_name(enum som_ux_mode m);
/* Free-flight position on the 512x512 world map, in map pixels (units / 8); compass bearing clockwise from north in degrees. */
void som_ux_flammie_map_pos(const struct som_ux *ux, int *mx, int *my, int *bearing_deg);
const char *som_ux_flammie_zone(const struct som_ux *ux);
/* Town i on the world map (pixels of the 512 x 512 map) and its name; returns the town count, 0 without a ROM. */
int som_ux_flammie_town(const struct som_ux *ux, int i, int *mx, int *my, const char **name);   /* place under Flammie ("NORTHTOWN", "OPEN SEA"); NULL without a ROM */

/* ------------------------------------------------------------------ drawing and touch */
/* Renders the whole screen.  `m` (may be NULL) supplies the macro status for the header chip. */
void som_ux_draw(struct som_ux *ux, const struct som_macro *m, uint32_t *px, int stride, int w, int h);
/* A tap at device pixel (x, y) on a w x h screen.  Updates UI state; returns 1 and fills *out when something must be done. */
int som_ux_touch(struct som_ux *ux, int x, int y, int w, int h, struct som_ux_action *out);
/* Pressed look.  The host calls som_ux_touch_down() on finger down: it returns 1 when an enabled target is under the finger (play the click and
 * the haptic tick then) and that target is drawn depressed until som_ux_touch_up().  som_ux_touch_move() un-presses it while the finger is
 * outside the target.  The action itself still comes from som_ux_touch() on finger up.  som_ux_press_hit() presses a target by name (a hardware
 * hotkey flashing its quick slot: hit = SOM_UX_HIT_QUICK, a = slot).  som_ux_pressed() reads the state. */
int som_ux_touch_down(struct som_ux *ux, int x, int y, int w, int h);
void som_ux_touch_move(struct som_ux *ux, int x, int y, int w, int h);
void som_ux_touch_up(struct som_ux *ux);
void som_ux_press_hit(struct som_ux *ux, int hit, int a, int on);
int som_ux_pressed(const struct som_ux *ux, int *hit, int *a);
/* The Android JNI's single entry point (declared weak there): down = 1 is som_ux_touch_down(x, y), down = 0 is som_ux_touch_up(). */
void som_ux_press(struct som_ux *ux, int x, int y, int w, int h, int down);
/* UI state setters for foreign-function callers and tests. */
void som_ux_set_view(struct som_ux *ux, int tab, int hero, int spirit);
void som_ux_set_pending(struct som_ux *ux, int kind, int cls, int arg, int spell);
/* ---- pad control of the panel ("panel mode") ----
 * The host calls som_ux_nav_toggle() for its panel-mode button (the Thor: a click of the left stick).  While it is on, the D-pad / stick move a
 * visible focus ring over the touch targets of the current screen (nearest target in that direction, wrapping at the edges), A presses the focused
 * target exactly like a tap on it, B presses the screen's back / cancel target (or closes the options page).  The host keeps the pad away from the
 * game while the mode is on.  dir: 0 up, 1 down, 2 left, 3 right. */
int som_ux_nav_toggle(struct som_ux *ux);                 /* returns the new state */
int som_ux_nav_on(const struct som_ux *ux);
int som_ux_nav_move(struct som_ux *ux, int dir, int w, int h);   /* 1 when the focus moved */
int som_ux_nav_activate(struct som_ux *ux, int w, int h, struct som_ux_action *out);   /* as som_ux_touch() */
int som_ux_nav_back(struct som_ux *ux, int w, int h, struct som_ux_action *out);

/* Integer upscale factor used for a w x h screen. */
int som_ux_scale(int w, int h);
/* Area name of a map id for the header ("PURE LAND"), NULL when the evidence is not good enough: no name is shown then. */
const char *som_ux_area_name(int map_id);
/* Name of an item, weapon, spell or enemy from the ROM name block (NULL when no ROM). */
const char *som_ux_name(const struct som_ux *ux, int index);
/* Name entry helper: the pad mask for the next step towards `letter` (0..25) given the cursor; 0 when it is there.
 * The caller taps the returned LEFT/RIGHT, waits ~28 frames and calls again; when it returns 0 it taps B. */
uint16_t som_ux_name_next_pad(const struct som_ux *ux, int letter);
/* Hero being named (0..2) while a name entry shows, else -1. */
int som_ux_name_hero(const struct som_ux *ux);
/* LAST NAME key: the host keeps the last confirmed name per hero and gives it here (A..Z, up to 7 letters; NULL / "" / other characters clear it). */
void som_ux_set_last_name(struct som_ux *ux, int hero, const char *name);
const char *som_ux_last_name(const struct som_ux *ux, int hero);
/* One-shot: a name was just confirmed in a name entry.  Returns the hero (0..2) and writes the name (A..Z) to out, or -1.  Names with other glyphs are not reported. */
int som_ux_name_confirmed(struct som_ux *ux, char *out, int n);

/* Read-only accessors for foreign-function callers.  what: 0 enemies, 1 boss actor (-1 none), 2 flammie x, 3 flammie y,
 * 4 flammie heading, 5 flammie in main world, 6 present heroes, 7 log lines. */
int som_ux_info(const struct som_ux *ux, int what);
const char *som_ux_line(const struct som_ux *ux);   /* current dialogue line */

/* ---- target policy and hotkey execution ----
 * Items and spells that need a target ask for a policy when they are bound (screen with SELECT / NEAR or LESS HP / ALL, see enum som_ux_policy);
 * som_ux_entry_policies() gives the bit mask of the choices an entry offers, 0 when it needs no target.  A quick slot stores its policy
 * (ACT_QUICK_SET carries it, som_ux_quick_set_policy restores it; shoulder slots too).  The host runs a pressed shoulder button with
 * som_ux_shoulder_press() (or any entry with som_ux_run_entry()): it returns 1 with an
 * ACT_CMD (target_kind / target_slot already resolved for NEAR, LESS_HP and ALL; the only ally is chosen when the player is alone), or 0 when
 * the target selector opened (SELECT) or nothing could be done; in the ring a pending selector yields ACT_CLOSE_RING. */
int som_ux_entry_policies(const struct som_ux *ux, int kind, int id);
int som_ux_run_entry(struct som_ux *ux, int kind, int id, int policy, struct som_ux_action *out);
int som_ux_quick_set_policy(struct som_ux *ux, int slot, int policy);
int som_ux_quick_policy(const struct som_ux *ux, int slot);
/* Shoulder slots: a physical L1 / R1 / L2 / R2 press.  Runs the entry of slot `which` (enum som_ux_shoulder) with the policy stored for it and
 * flashes the slot.  Returns 1 with the dispatch in *out (same contract as som_ux_run_entry), 0 when the slot is empty or the target selector
 * opened.  Labels default to L1 R1 L2 R2; a 3DS build passes L R ZL ZR. */
int som_ux_shoulder_press(struct som_ux *ux, int which, struct som_ux_action *out);
void som_ux_set_shoulder_labels(struct som_ux *ux, const char *l1, const char *r1, const char *l2, const char *r2);
const char *som_ux_shoulder_label(const struct som_ux *ux, int which);

/* ---- slot feedback animation ----
 * A slot (or the spell button of the spell list) plays a ~0.3 s animation when its action fires: a bright flash, an expanding ring and a small
 * glyph (check, or the cast star).  Touch taps on a slot start it by themselves; the host starts it for hotkeys (som_ux_shoulder_press does) and
 * for queued spells that fire later (som_ux_flash_entry).  som_ux_update() advances it, so it needs one update per emulated frame. */
enum som_ux_flash_kind { SOM_UX_FLASH_NONE = 0, SOM_UX_FLASH_OK /* check: equip, item, menu jump */, SOM_UX_FLASH_CAST /* star: spell */,
                         SOM_UX_FLASH_QUEUED /* a queued spell fires: star with a clock */, SOM_UX_FLASH_REFUSED /* cross */ };
#define SOM_UX_FLASH_FRAMES 18
void som_ux_slot_flash(struct som_ux *ux, int slot, int kind);                        /* slot 0..7 (0..3 are the shoulder buttons) */
void som_ux_flash_entry(struct som_ux *ux, int kind, int entry_kind, int entry_id);   /* every slot holding the entry + the spell list button */

/* ---- enemy sprites for the target selector ----
 * The selector shows ONLY the enemy sprite on a plain card.  The frontend gives one sprite per enemy id (the id of the actor, as in
 * som_ux_actor.id): straight (non-premultiplied) ARGB with a transparent background, w and h up to SOM_UX_SPRITE_MAX, any trim.  The card
 * centres the opaque part, scales it by the largest integer factor that fits (nearest neighbour, never blurred) and blends it over the card
 * with its alpha, with a soft drop shadow under the feet.  Enemies without a sprite get a generic silhouette.
 * som_ux_enemy_sprites_wanted() lists the enemy ids on screen that still have no sprite (returns how many were written, at most `max`). */
int som_ux_set_enemy_sprite(struct som_ux *ux, int enemy_id, const uint32_t *argb, int w, int h);   /* NULL removes; 0 ok, -1 bad size */
void som_ux_clear_enemy_sprites(struct som_ux *ux);
int som_ux_enemy_sprites_wanted(const struct som_ux *ux, uint8_t *ids, int max);

/* Name of hero 0..2 as the player entered it (WRAM $CC00 + 12 * slot: game glyph codes, one byte each, 00 or FF ends it), upper case; BOY / GIRL / SPRITE
 * while the slot is empty.  Glyphs the UI font lacks (accented letters of other ROMs) are dropped. */
const char *som_ux_hero_name(const struct som_ux *ux, int slot);

/* ---- notices (queue feedback) ----
 * A small chip under the header, SoM window style, non-blocking (no hit target), never clipped (condensed faces, reason cut to 14 characters),
 * fading out over its last 24 frames; SOM_UX_NOTICE_FRAMES emulated frames in all (som_ux_update ticks it).  Replaces the host's red toast for
 * queue events: QUEUED -> "QUEUED" with a clock, CANCELLED -> "CANCELLED", DROPPED -> "DROPPED: <reason>" (reason: e.g. "NO MP", "KO"),
 * INFO / ERROR -> the text as given. */
enum som_ux_notice_kind { SOM_UX_NOTICE_NONE = 0, SOM_UX_NOTICE_QUEUED, SOM_UX_NOTICE_CANCELLED, SOM_UX_NOTICE_DROPPED, SOM_UX_NOTICE_INFO, SOM_UX_NOTICE_ERROR };
#define SOM_UX_NOTICE_FRAMES 120
void som_ux_notice(struct som_ux *ux, int kind, const char *text_or_reason);
void som_ux_notice_clear(struct som_ux *ux);

/* ---- first frame and redraw ----
 * som_ux_draw() works on a fresh object before any WRAM frame (it shows the title block), so a host draws once right after init and after every
 * surface (re)creation without waiting for a touch.  som_ux_needs_redraw() answers whether the picture would differ from the last som_ux_draw():
 * 1 when nothing was drawn yet, the size changed (pass w, h), the game state, a pulse, a press or a notice changed.  Touches always redraw. */
int som_ux_needs_redraw(const struct som_ux *ux, int w, int h);

/* ---- spell availability and the REVIVE chooser ----
 * enum som_ux_avail: what a spell button shows.  ENABLED: normal.  DISABLED: dimmed, taps do nothing (not enough MP, spirit not owned, caster
 * KO).  QUEUED: the cast is waiting for its moment; the button shows a clock badge and a tap asks the host to cancel the wait
 * (ACT_CANCEL_QUEUED).  The host (direct layer: somd_can_cast reasons and the queue) hands one state per spell id 0..41 to
 * som_ux_set_spell_avail(); an entry of 0 (SOM_UX_AV_AUTO), or no array at all, derives ENABLED / DISABLED from the party state in WRAM
 * (caster alive, MP, spirit owned).  Hotkey slots and the spell lists share the same state. */
enum som_ux_avail { SOM_UX_AV_AUTO = 0, SOM_UX_AV_ENABLED, SOM_UX_AV_DISABLED, SOM_UX_AV_QUEUED };
void som_ux_set_spell_avail(struct som_ux *ux, const uint8_t *avail42);   /* NULL clears */
int som_ux_spell_state(const struct som_ux *ux, int spell_id);            /* enum som_ux_avail, never AUTO */
/* Tapping a fallen hero's card (field) or avatar (ring) opens the REVIVE chooser: Cup of Wishes with its quantity, the Dryad spell
 * (Revivifier) with its MP cost and caster, and any other revive item of the item table.  A choice runs at once on that hero. */
struct som_ux_revive_opt { uint8_t kind /* 1 item, 2 spell */, id /* item id or spell id */, enabled, queued, caster /* hero slot casting or using */, qty, mp_cost, reason /* 0 ok, 1 none left, 2 no MP, 3 spirit not owned, 4 caster KO or absent */; };
int som_ux_revive_open(struct som_ux *ux, int hero);                      /* options shown; 0 (and nothing opens) when the hero is not fallen */
int som_ux_revive_count(const struct som_ux *ux);
int som_ux_revive_option(const struct som_ux *ux, int idx, struct som_ux_revive_opt *o);
int som_ux_revive_choose(struct som_ux *ux, int idx, struct som_ux_action *out);   /* 1 with the command in *out (or the queue cancel) */
void som_ux_revive_close(struct som_ux *ux);

/* ---- zone map (MAP tab) ----
 * By default the renderer builds the map of the current area from WRAM (tile ids and tile words) and the ROM (graphics, palette); it needs the ROM
 * passed to som_ux_set_rom().  A host that already has a picture of the area (for example rendered by the core) hands it over instead: ARGB pixels,
 * px_per_tile image pixels per 16x16 map tile, valid for `map_id` until the map changes. */
void som_ux_set_zone_image(struct som_ux *ux, const uint32_t *argb, int w, int h, int map_id, int px_per_tile);
void som_ux_clear_zone_image(struct som_ux *ux);                       /* back to the built-in rendering */
const uint32_t *som_ux_zone_pixels(const struct som_ux *ux);           /* ARGB pixels of the zone image, or NULL */
int som_ux_zone_info(const struct som_ux *ux, int *w, int *h, int *map_id);   /* 1 when a zone image exists */

/* ---- slot assignment ----
 * There is no bind editor: an entry is put in a slot (quick slots 0..7).  Ways in: the slot editor pickers, the ring (the highlighted entry or
 * the open ring itself, 'ENTRY > SLOT' / 'MENU > SLOT'), or a long press on a list entry followed by a slot tap.  Entries that need a target
 * ask the target policy first.  A long press at (x, y) on a slot opens the slot editor (ACT_QUICK_EDIT), on a list entry it arms the entry
 * for the next slot tap (returns 0, a notice tells the player), on an avatar it asks for that hero's STATUS screen. */
int som_ux_longpress(struct som_ux *ux, int x, int y, int w, int h, struct som_ux_action *out);
/* Display name of a quick-slot entry, or NULL. */
const char *som_ux_bind_name(const struct som_ux *ux, int kind, int id, char *buf, size_t n);

/* ---- ring selection ---- */
/* Decoded each frame while the ring is open; NULL-safe.  `valid` is 0 until the ring has finished opening. */
const struct som_ux_selinfo *som_ux_selection(const struct som_ux *ux);

/* ---- quick slots (hotlinks) ----
 * Eight slots shown in field mode below the party and in ring mode under the selected-entry info.  The host owns persistence:
 * it restores the slots with som_ux_quick_set() at start and stores them again on every SOM_UX_ACT_QUICK_SET action. */
int som_ux_quick_set(struct som_ux *ux, int slot, int kind, int id);   /* kind 0 clears; 0, or -1 for a bad argument */
int som_ux_quick_get(const struct som_ux *ux, int slot, int *kind, int *id);   /* 1 when the slot holds an entry */
int som_ux_quick_bind_selected(struct som_ux *ux, int slot);   /* store the highlighted ring entry; 0, or -1 when none */
int som_ux_quick_bind_menu(struct som_ux *ux, int slot);       /* store the open ring itself as a menu hotlink */

/* ---- options panel ---- */
void som_ux_set_option(struct som_ux *ux, int id, int value, const char *text);   /* text: optional value label */
int som_ux_get_option(const struct som_ux *ux, int id);
/* ---- Thor round 4 ----
 * Extended options page (SHADER, SAVE STATES, CREDITS, no LANGUAGE).  The host fills the slots after every save / load and when the page opens:
 * `time` is a short label ("10-20 18:30"), `thumb` SOM_UX_THUMB_W x SOM_UX_THUMB_H 0xAARRGGBB pixels (NULL for an empty slot).  `text` of the
 * credits uses \n for line breaks and "# " for headings.  The page scrolls with the buttons or with the pad (nav up / down). */
void som_ux_set_options_ext(struct som_ux *ux, int on);
/* 1 when the HP of the enemy in actor slot `slot` may be shown (SOM_UX_OPT_ENEMY_HP and the Analyzer casts of this game). */
int som_ux_enemy_hp_shown(const struct som_ux *ux, int slot);
void som_ux_set_state_slot(struct som_ux *ux, int slot, int used, const char *time, const uint32_t *thumb);
void som_ux_set_state_sel(struct som_ux *ux, int slot);
int som_ux_state_sel(const struct som_ux *ux);
void som_ux_set_credits(struct som_ux *ux, const char *text);
void som_ux_open_options(struct som_ux *ux, int page);   /* enum som_ux_opt_page; 0 closes */
int som_ux_options_page(const struct som_ux *ux);

/* ---- tap targets by name (tests, scripted taps, accessibility) ---- */
enum som_ux_hit { SOM_UX_HIT_NONE = 0, SOM_UX_HIT_PARTY, SOM_UX_HIT_TAB, SOM_UX_HIT_CELL, SOM_UX_HIT_T_ALLY, SOM_UX_HIT_T_ENEMY,
                  SOM_UX_HIT_T_ALL_ALLIES, SOM_UX_HIT_T_ALL_ENEMIES, SOM_UX_HIT_CANCEL, SOM_UX_HIT_PAD, SOM_UX_HIT_LETTER,
                  SOM_UX_HIT_NEXT, SOM_UX_HIT_KEYROW, SOM_UX_HIT_KEYS_SCROLL, SOM_UX_HIT_BIND_CANCEL, SOM_UX_HIT_BIND_CLEAR,
                  SOM_UX_HIT_OPT /* gear */, SOM_UX_HIT_OPTION, SOM_UX_HIT_OPT_NAV /* a=2 slot editor, 0 back */,
                  SOM_UX_HIT_AVATAR, SOM_UX_HIT_QUICK, SOM_UX_HIT_BIND_ENTRY, SOM_UX_HIT_BIND_MENU, SOM_UX_HIT_QE_TAB,
                  SOM_UX_HIT_QE_CELL, SOM_UX_HIT_QE_PAGE, SOM_UX_HIT_QE_CLEAR, SOM_UX_HIT_QE_KEY,
                  SOM_UX_HIT_MAPVIEW /* the zone map: a tap toggles the large view and the small view with the enemy list */,
                  SOM_UX_HIT_POLICY /* a = enum som_ux_policy */, SOM_UX_HIT_POL_CANCEL, SOM_UX_HIT_QE_BACK /* spell picker: back to the spirits */,
                  SOM_UX_HIT_REVIVE /* a = option index of the REVIVE chooser */, SOM_UX_HIT_REV_CANCEL,
                  SOM_UX_HIT_ST_SLOT, SOM_UX_HIT_ST_SAVE, SOM_UX_HIT_ST_LOAD, SOM_UX_HIT_CR_SCROLL /* a = 0 up, 1 down */,
                  SOM_UX_HIT_NAME_LAST /* name entry: types the last name of the hero being named */ };
/* Centre (device pixels) of the first enabled-or-not target of `hit` with parameter `a`; 1 when found. */
int som_ux_target_center(struct som_ux *ux, int w, int h, int hit, int a, int *x, int *y);
/* Smallest side in vp of the targets of one kind (0 when absent), for the touch-size audit. */
int som_ux_target_min_vp(struct som_ux *ux, int w, int h, int hit);

/* Layout audit for tests: number of texts cut, labels that did not fit their cell and text rows that overlapped since the last
 * reset, counted while drawing.  som_ux_audit(1) returns the count and resets it; the note describes the first problem. */
int som_ux_audit(int reset);
int som_ux_mp_bars(int slot);   /* MP bars drawn for hero 0..2 since the last som_ux_audit(1) (girl and sprite must have one on every standing card) */
int som_ux_screen_has_text(const char *s);   /* tests: was this exact string drawn by the last som_ux_draw() */
const char *som_ux_audit_note(void);
/* Text boxes of the hero cards drawn since the last som_ux_audit(1): label, number, name, level (tests).  Returns 0 past the end. */
int som_ux_text_log(int i, char *s, size_t sn, int *x, int *y, int *w, int *h);

/* Test sheet of the text faces (alphabet, digits, sample words) for the font review and the mocks. */
void som_ux_text_sheet(struct som_ux *ux, uint32_t *px, int stride, int w, int h);

/* Touch target audit: the smallest tappable rectangle of the current layout, in vp (for tests and the doc). */
int som_ux_min_target_vp(struct som_ux *ux, int w, int h);

/* ---- PC frontend support (tools/companion/pc) ----
 * som_ux_draw_shoulders(): the four shoulder slots L1 R1 L2 R2 in one row (icon, button label, flash, dimmed when unavailable, queued clock) on a
 * transparent canvas (alpha 0 where nothing is drawn) at the integer `scale`; `names` adds a row with the entry name under each slot.
 * som_ux_shoulders_size() gives the canvas size that needs.  som_ux_hit_list() lists the tap targets of the current screen in device pixels. */
struct som_ux_hitrect { int x, y, w, h; uint8_t kind, a, enabled; };
void som_ux_shoulders_size(int scale, int names, int *w, int *h);
void som_ux_draw_shoulders(struct som_ux *ux, uint32_t *px, int stride, int w, int h, int scale, int names);
int som_ux_hit_list(struct som_ux *ux, int w, int h, int max, struct som_ux_hitrect *out);
void som_ux_set_four_slots(struct som_ux *ux, int on);

#endif
