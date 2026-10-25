/* som_direct: instant weapon / item / spell commands for the Secret of Mana companion, with no ring menu.
 *
 * Pure C99.  The frontend gives the ROM image once (read-only, to learn the spell table and to place a return stub),
 * then calls somd_pre_frame() before and somd_post_frame() after every retro_run().  The module writes WRAM (the
 * state the ring would commit) and asks the game's own routines to finish the job through a small stub in WRAM that
 * is entered from the game's RAM NMI trampoline ($00:0100) while the main thread is parked in its frame-wait spin.
 * The ROM is never modified and no CPU register of the core is touched.  Design and evidence:
 * docs/companion-direct-20261017.md.
 */
#ifndef SOM_DIRECT_H
#define SOM_DIRECT_H

#include <stddef.h>
#include <stdint.h>

#define SOMD_WRAM_SIZE 0x20000u

enum somd_err {
  SOMD_OK = 0,
  SOMD_ERR_BUSY,            /* another direct command is still running */
  SOMD_ERR_UNSAFE_STATE,    /* not in field mode (title, dialogue, event, fade, ring menu, no living pad hero) */
  SOMD_ERR_BAD_ARGUMENT,    /* index out of range, empty record, unknown spirit */
  SOMD_ERR_NOT_OWNED,       /* weapon not owned */
  SOMD_ERR_NO_MP,
  SOMD_ERR_CASTER_BUSY,     /* the caster is swinging, hurt, casting or dead */
  SOMD_ERR_BAD_TARGET,      /* target slot not valid for this spell or item (off screen, dead, wrong side) */
  SOMD_ERR_NO_TARGET,       /* group target requested but the group is empty */
  SOMD_ERR_NEEDS_RING,      /* item with a modal effect (Magic Rope, Flammie Drum): not supported directly */
  SOMD_ERR_TIMEOUT,         /* the main thread never parked for the stub; the command was rolled back */
  SOMD_ERR_REJECTED,        /* the game refused the request (cast pose not entered) */
  SOMD_ERR_UNSUPPORTED_ROM, /* the routines the stub calls are not where the table says */
  SOMD_ERR_KO,              /* the caster is knocked out */
  SOMD_ERR_STATUS,          /* the caster has a disabling status (silence, sleep, frozen, stone, balloon, moogle, burning) */
  SOMD_ERR_EXPIRED,         /* a queued command waited longer than its timeout */
  SOMD_ERR_CANCELLED        /* a queued command was cancelled or replaced */
};

enum somd_state { SOMD_IDLE = 0, SOMD_RUNNING, SOMD_FINISHED, SOMD_FAILED };

enum somd_target_kind {
  SOMD_T_NONE = 0,
  SOMD_T_ONE,            /* slot 0..2 hero, 3..5 enemy */
  SOMD_T_ALL_ALLIES,
  SOMD_T_ALL_ENEMIES
};
struct somd_target { uint8_t kind; uint8_t slot; };

/* what kind of target a spell or an item asks for (data/companion_target_classes.json) */
enum somd_class {
  SOMD_C_NONE = 0,
  SOMD_C_ALLY_ONE,           /* one party member */
  SOMD_C_ALLY_ONE_OR_ALL,    /* one party member or the whole party */
  SOMD_C_ENEMY_ONE_OR_ALL,   /* one enemy or all enemies */
  SOMD_C_ENEMY_ONE,
  SOMD_C_ALLY_DEAD_ONE,      /* a fallen party member (Revivifier, Cup of Wishes) */
  SOMD_C_ALLY_STATUS_ONE,    /* a party member with a curable status (Medical Herb) */
  SOMD_C_BOY_ONLY,           /* the weapon-change spells: hero 0 holding the sword */
  SOMD_C_UNSUPPORTED
};

struct somd_actor {
  uint8_t slot;          /* 0..2 heroes, 3..5 enemy slots */
  uint8_t present;       /* active byte == 1 */
  uint8_t on_screen;     /* the game's own test: 8 <= x < 0xF8 and y < 0xD8 */
  uint8_t alive;         /* E191 bit 7 clear, or object id 0x54/0x55 */
  uint8_t id;            /* object identity byte (+0x180) */
  uint16_t sx, sy;       /* screen position (+0x20, +0x22) */
  int16_t wx, wy;        /* world position (+0x02, +0x04), for views larger than the screen */
  uint16_t hp, hp_max;   /* +0x182, +0x184 */
  uint16_t status;       /* +0x190 */
  uint8_t valid;         /* selectable by the current command class (set by somd_scan_targets) */
};

struct somd_targets {
  struct somd_actor actor[6];
  uint8_t n_allies_valid, n_enemies_valid;
  uint8_t cls;                    /* enum somd_class used for the valid flags */
};

struct somd_step { uint8_t type; uint8_t rts; uint32_t a, b; uint16_t a_reg, x_reg; uint8_t has_a, has_x; };

/* ------------------------------------------------------------------ cast availability and queue */
/* Why a spell cannot be cast now.  The game itself blocks casting through the magic ring when the owner's status word & 0xD1F0
 * is non-zero (table at D9:FD10, category 1): KO 8000, burning 4000, moogle 1000, balloon 0100, silence 0080, stone 0040,
 * frozen 0020, sleep 0010; the Barrel disguise 0400 is added (the macro cannot cast with it).  Permanent reasons are not queued (the UI disables the button); transient ones can be. */
enum somd_cast_reason {
  SOMD_CAST_OK = 0,
  SOMD_CAST_BAD_ARGUMENT,   /* hero, spirit or slot out of range, or no spell there */
  SOMD_CAST_NOT_OWNED,      /* permanent: the hero does not have that spirit */
  SOMD_CAST_KO,             /* permanent until revived */
  SOMD_CAST_NO_MP,          /* permanent for now: MP regenerates slowly, the UI should show the cost */
  SOMD_CAST_SILENCED,       /* transient: status 0x0080, clears when its timer runs out */
  SOMD_CAST_STATUS,         /* transient: asleep, frozen, stone, balloon, moogle or burning (see info.status) */
  SOMD_CAST_BUSY,           /* transient: the caster is swinging, casting or hurt */
  SOMD_CAST_COMMAND,        /* transient: a direct command is still running */
  SOMD_CAST_RING_OPEN,      /* transient: a ring, spell list or page is open */
  SOMD_CAST_EVENT,          /* transient: dialogue, cutscene or map script running */
  SOMD_CAST_FADE,           /* transient: screen fading */
  SOMD_CAST_INPUT_LOCK,     /* transient: the game blocks the pad */
  SOMD_CAST_UNSAFE          /* transient: no game running or no living pad hero */
};
struct somd_cast_info {
  uint8_t reason;           /* enum somd_cast_reason */
  uint8_t permanent;        /* 1: do not queue (NOT_OWNED, KO, NO_MP, BAD_ARGUMENT) */
  uint8_t will_clear;       /* transient reasons: 1 when the game clears it by itself */
  uint16_t eta_frames;      /* estimated frames until it clears, 0 = unknown (statuses: 20 frames per timer unit) */
  uint16_t status;          /* the caster's status word */
  uint8_t spell_id, mp_have, mp_cost, cls;
};
enum somd_policy_kind { SOMD_POL_SLOT = 0 /* the given slot */, SOMD_POL_NEAREST, SOMD_POL_LOWEST_HP, SOMD_POL_ALL };
struct somd_policy { uint8_t kind; uint8_t slot; };
enum somd_queue_status { SOMD_Q_EMPTY = 0, SOMD_Q_WAITING, SOMD_Q_FIRED, SOMD_Q_DROPPED };
struct somd_queue_info {
  uint8_t state;            /* enum somd_queue_status */
  uint8_t reason;           /* WAITING: the transient reason it waits for */
  uint8_t err;              /* DROPPED: enum somd_err (EXPIRED, CANCELLED, BAD_TARGET, NO_TARGET, NO_MP, KO ...) */
  uint8_t spell_id;
  uint16_t waited, timeout; /* frames */
  uint8_t replaced;         /* 1: this entry replaced a different waiting spell of the same caster; replaced_id is that spell */
  uint8_t replaced_id;
};
struct somd_queued {
  uint8_t state, reason, err, spirit, slot, pol_kind, pol_slot, spell_id;
  uint16_t waited, timeout;
  uint8_t replaced, replaced_id;
};

struct somd {
  const uint8_t *rom; size_t rom_size;
  int rom_ok;
  /* running command */
  enum somd_state state;
  enum somd_err err;
  uint8_t kind;                    /* 1 equip, 2 item, 3 cast, 4 sound */
  struct somd_step step[24];
  int nstep, cur;
  int armed;                       /* stub written and hook installed */
  int armed_frames;
  /* menu commands (kind 5 open, 6 close): a state machine over WRAM edges and pad taps, no stub */
  uint8_t mkind, mhero, marg, mphase, hide, fast;
  uint16_t mtimer, mpad, mtotal;
  uint8_t mtries;
  struct somd_queued queue[3];     /* one queued cast per hero */
  uint16_t queue_timeout;          /* default timeout in frames for queued casts (600 = 10 s) */
  int release;                     /* stub bytes still to be given back (one frame after the call finished) */
  uint32_t frames;                 /* frames from start to completion (latency) */
  uint8_t orig_nmi[4];
  uint8_t stub_save[112];          /* WRAM bytes under the mailbox and stub */
  uint8_t undo_addr_n;             /* rollback list */
  uint32_t undo_addr[12]; uint8_t undo_val[12];
  uint8_t cast_hero;
  uint8_t last_a_low;
  char note[48];
};

void somd_init(struct somd *d, const uint8_t *rom, size_t rom_size);
int somd_busy(const struct somd *d);
const char *somd_err_name(enum somd_err e);

/* Commands.  Each returns SOMD_OK when accepted (it then runs over the next frames) or the refusal reason. */
int somd_equip(struct somd *d, const uint8_t *wram, int hero, int weapon_type);
int somd_use_item(struct somd *d, const uint8_t *wram, int item_record, struct somd_target t);
/* spirit = the hero's magic-ring index 0..6 (Gnome, Undine, Salamando, Sylphid, Luna, Dryad, then Lumina for the girl
 * or Shade for the sprite), spell_slot 0..2.  hero 1 = girl, 2 = sprite.  Spell id = 6 * spirit + 3 * (hero == 1) + slot. */
int somd_cast(struct somd *d, const uint8_t *wram, int hero, int spirit, int spell_slot, struct somd_target t);

/* Plays one of the game's own menu sounds (ids 0C 0D 0E 29 2A 2B 2C 4B 28 A3, parameter 0F, pan 0) through the same stub.
 * No field-mode check (the ring and menus are fine); BUSY while another command runs; BAD_ARGUMENT for other ids. A call
 * that never reaches its safe point times out after 30 frames, restores the NMI vector and leaves state FAILED/TIMEOUT. */
#define SOMD_HAS_SOUND 1
int somd_sound(struct somd *d, const uint8_t *wram, int sfx_id);

/* Direct menu jumps (no intermediate rings).  The game's own state machine does the work: the saved per-hero category and the
 * saved cursor ranks are written first, then the field's Y-button edge flag is raised, so the ring opens ONCE, already on the
 * target category, owner and entry.  While a menu command runs somd_hide_video() is 1: the host should keep running frames
 * (unthrottled, audio and video of those frames dropped) and present nothing until it turns 0, so no ring animation, no
 * hero switch and no page fade is ever shown.  The host ORs somd_menu_pad() into the pad mask of every frame while the
 * command runs (it is non-zero only on the confirm and cancel taps).
 *   SOMD_MENU_RING   arg = ring category (0 item, 1 magic, 2 config, 3 weapon); opens that ring for `hero`
 *   SOMD_MENU_SPIRIT arg = spirit mask index 0..7; opens the girl's / sprite's magic ring on it and enters its spell list
 *   SOMD_MENU_PAGE   arg = config entry 0..6 (Equip Armor, Targeting, Status, Levels, Action Grid, Controller, Window)
 * hero 0 boy, 1 girl, 2 sprite; the pad is bound to `hero` first with the game's own field hero switch.  Allowed from the
 * field or from an idle top-level ring (which is closed first).  Errors: UNSAFE_STATE, BAD_ARGUMENT (hero absent, boy has no
 * magic, empty ring), TIMEOUT. */
enum somd_menu_kind { SOMD_MENU_NONE = 0, SOMD_MENU_RING, SOMD_MENU_SPIRIT, SOMD_MENU_PAGE };
#define SOMD_HAS_MENU 1
int somd_open_menu(struct somd *d, const uint8_t *wram, int kind, int hero, int arg);
/* Closes whatever is open (spell list, page, ring) and returns to the field. */
int somd_close_menu(struct somd *d, const uint8_t *wram);
uint16_t somd_menu_pad(const struct somd *d);
int somd_hide_video(const struct somd *d);   /* 0 unless somd_set_fast_menu(1): menu jumps are NOT hidden or sped up any more (round 3) */
void somd_set_fast_menu(struct somd *d, int on);
/* Control swap: makes `hero` the controlled one with the game's own X-edge hero switch, in real time (state kind 7). */
int somd_switch_hero(struct somd *d, const uint8_t *wram, int hero);
/* Name entry direct text (see som_direct.c): letter 0..25 appended / last letter deleted, no cursor movement. */
int somd_name_letter(struct somd *d, uint8_t *wram, int letter);
int somd_name_delete(struct somd *d, uint8_t *wram);

/* Can `hero` cast the spell (spirit position 0..6, slot 0..2) right now?  Returns the reason, fills `info` when not NULL. */
int somd_can_cast(const struct somd *d, const uint8_t *wram, int hero, int spirit, int slot, struct somd_cast_info *info);
/* Casts now when possible, else queues the cast for a transient reason.  It fires on the first frame (somd_pre_frame) on which
 * somd_can_cast() is OK and no other direct command runs; the target is resolved then from `policy` (SLOT: that slot must still be
 * valid; NEAREST to the caster; LOWEST_HP; ALL = whole group).  A target that is gone drops the request with BAD_TARGET or
 * NO_TARGET.  One queued cast per hero: a new request replaces the old one.  timeout_frames 0 = d->queue_timeout (600).
 * Returns SOMD_OK (fired or queued; see somd_queue_state) or the refusal.  A permanent reason is not queued: it returns
 * SOMD_ERR_NO_MP, SOMD_ERR_KO, SOMD_ERR_BAD_ARGUMENT (not owned or invalid) and `info` carries the reason. */
#define SOMD_HAS_QUEUE 1
int somd_queue_cast(struct somd *d, const uint8_t *wram, int hero, int spirit, int slot, struct somd_policy policy, int timeout_frames,
                    struct somd_cast_info *info);
int somd_queue_state(const struct somd *d, int hero, struct somd_queue_info *out);   /* returns the state */
void somd_queue_cancel(struct somd *d, int hero);
/* Resets a FIRED or DROPPED entry to EMPTY once the UI has read it. */
void somd_queue_ack(struct somd *d, int hero);

/* Ways to bring a KO hero back.  Items: Cup of Wishes (item id 5; the only item whose effect revives, item_effects 16).  Spells:
 * Revivifier (spell id 33, the girl's Dryad spell slot 0).  One entry per item record that holds the item and one per spell
 * caster; `reason` is the somd_can_cast reason for spell entries (0 for items). */
struct somd_revive_option { uint8_t kind /* 1 item, 2 spell */, id, qty, record, caster, spirit, slot, mp_cost, reason, usable; };
int somd_revive_options(const struct somd *d, const uint8_t *wram, int hero, struct somd_revive_option out[4]);

/* Call before every retro_run() (may write WRAM) and after it (reads the mailbox). */
void somd_pre_frame(struct somd *d, uint8_t *wram);
void somd_post_frame(struct somd *d, uint8_t *wram);

/* Target classes.  spell_id comes from somd_spell_id(); the table is read from the ROM so balance hacks follow. */
int somd_spell_id(int hero, int spirit, int slot);
enum somd_class somd_spell_class(const struct somd *d, int spell_id);
enum somd_class somd_item_class(int item_id);
int somd_spell_mp_cost(const struct somd *d, int spell_id);
/* Fills the actor cards and marks which ones the game would let the player pick for the class. */
void somd_scan_targets(const uint8_t *wram, enum somd_class cls, int item_or_spell_id, int is_spell, struct somd_targets *out);

/* Mini-map helpers.  The playfield is the 256 x 224 game screen; the app scales it into its touch area of map_w x map_h
 * pixels.  somd_minimap_point() places an actor; somd_pick() returns the valid target nearest to a tap within `radius`
 * pixels (1 = found), preferring enemies when the class is hostile.  The "whole group" and "all enemies" cards are plain
 * buttons built by the app when somd_group_card() says so (class allows a group and the group has valid members). */
void somd_minimap_point(const struct somd_actor *a, int map_w, int map_h, int *px, int *py);
int somd_pick(const struct somd_targets *t, int tap_x, int tap_y, int map_w, int map_h, int radius, struct somd_target *out);
/* 0 none, 1 "whole party" card, 2 "all enemies" card */
int somd_group_card(const struct somd_targets *t);

#endif
