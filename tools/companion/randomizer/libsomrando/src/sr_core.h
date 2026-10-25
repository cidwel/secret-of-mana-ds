/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).
 *
 * Secret of Mana Randomizer, Copyright (C) 2024 Moppleton (original C# source).
 * Translated to C in 2026 for the som-decomp companion; each file names the upstream file it derives from.
 *
 * This library is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the License, or (at your option) any later version.  This library is
 * distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.  You should have received a copy of the GNU Lesser
 * General Public License along with this library (COPYING.LIB); if not, write to the Free Software Foundation, Inc., 51 Franklin Street,
 * Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Internal declarations shared by the modules of libsomrando.  Not part of the public API (that is somrando_api.h).
 */
#ifndef SR_CORE_H
#define SR_CORE_H

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "somrando_api.h"

#define SR_ROM_OUT_SIZE (4 * 1024 * 1024)
#define SR_ROM_IN_SIZE (2 * 1024 * 1024)

/* ---------------------------------------------------------------------------------------------------------------- arena
 * Everything allocated during one generation lives in an arena released at the end, so a fatal error (longjmp) leaks nothing. */
struct sr_block;
struct sr_arena {
  struct sr_block *head;
};
void *sr_arena_alloc(struct sr_arena *a, size_t n);   /* zeroed; NULL on failure */
void sr_arena_free(struct sr_arena *a);

/* ----------------------------------------------------------------------------------------------- byte list (List<byte>) */
struct sr_ctx;
struct sr_bytes {
  uint8_t *p;
  size_t len, cap;
};
void sr_bytes_push(struct sr_ctx *c, struct sr_bytes *b, int v);          /* Add((byte)v) */
void sr_bytes_push_n(struct sr_ctx *c, struct sr_bytes *b, const uint8_t *p, size_t n);
void sr_bytes_reserve(struct sr_ctx *c, struct sr_bytes *b, size_t n);

/* ---------------------------------------------------------------------------------------------- DotNet110Random (sr_random.c) */
struct sr_random {
  int inext, inextp;
  int32_t seed_array[56];
  long calls;               /* number of InternalSample() draws, debugging only */
};
void sr_random_init(struct sr_random *r, int32_t seed);
int32_t sr_random_next(struct sr_random *r);                               /* Next() */
int32_t sr_random_next_max(struct sr_random *r, int32_t max);              /* Next(max); max < 0 is an error (returns -1) */
int32_t sr_random_next_range(struct sr_random *r, int32_t min, int32_t max);
double sr_random_next_double(struct sr_random *r);
void sr_random_next_bytes(struct sr_random *r, uint8_t *buf, size_t n);
/* HashcodeUtil.GetDeterministicHashCode over the UTF-16 code units of a UTF-8 string. */
int32_t sr_hash_string(const char *utf8);

/* ---------------------------------------------------------------------------------------- settings (sr_settings.c)
 * One value per option of the mode (common options first); lookups by key. */
struct sr_settings;
struct sr_settings *sr_settings_new(struct sr_arena *a, const char *mode);
/* Applies "key=value\n" lines.  Returns 0, or SOMRANDO_ERR_UNSUPPORTED with a message in err. */
int sr_settings_apply_lines(struct sr_settings *s, const char *lines, char *err, size_t err_len);
const char *sr_settings_get(const struct sr_settings *s, const char *key);   /* "" when unknown, like StringValueSettings.get */
int sr_settings_get_bool(const struct sr_settings *s, const char *key);      /* yes/true/1 (case-insensitive, trimmed) */
double sr_settings_get_double(const struct sr_settings *s, const char *key);   /* Double.Parse, NaN on failure */
int sr_settings_get_int(const struct sr_settings *s, const char *key);       /* Int32.Parse, 0 on failure */
void sr_settings_set(struct sr_settings *s, const char *key, const char *val);
int sr_settings_known_mode(const char *mode);
/* Prints "key=default" lines of the mode's options (the same order as `somrando --list-options`, minus internal keys). */
void sr_settings_dump_defaults(const char *mode, FILE *f, int with_legal);

/* ------------------------------------------------------------------------ small ordered maps (Dictionary insertion order) */
struct sr_event {
  int id;
  struct sr_bytes *data;
};
struct sr_kv {
  char *key;
  char *val;
};

/* ------------------------------------------------------------------------------------------------- generation context */
typedef int (*sr_hack_fn)(struct sr_ctx *c);

struct sr_ctx {
  struct sr_arena arena;
  jmp_buf fatal;
  int fail_code;            /* SOMRANDO_* when longjmp'ed */
  char err[256];
  const struct somrando_request *req;
  somrando_progress_fn progress;
  void *progress_user;
  int step, total_steps;
  const char *step_name;

  struct sr_settings *settings;
  const char *seed;
  struct sr_random rf;      /* randomFunctional */
  struct sr_random rc;      /* randomCosmetic */
  uint8_t *orig;            /* originalRom, 4 MiB (first 2 MiB filled) */
  uint8_t *out;             /* outputRom, 4 MiB */
  int working_offset;       /* RandoContext.workingOffset */

  /* RandoContext.workingData (only the entries hacks exchange) */
  struct sr_kv *work;
  int n_work, cap_work;

  /* RandoContext.replacementEvents, insertion ordered */
  struct sr_event *events;
  int n_events, cap_events;

  /* NamesOfThings.newNames, insertion ordered (index -> text) */
  struct sr_name {
    int index;
    char *text;
  } *names;
  int n_names, cap_names;

  /* CustomEventManager: number of registered custom commands (port is only needed once a hack registers one) */
  int custom_events;

  /* Maps, doors, palettes and map pieces generated by procedural modes: the slice never fills them. */
  int unsupported_containers;

  /* resources rebuilt from the user's ROM by sr_rom_res(), cached for the lifetime of the generation */
  struct { const char *name; const uint8_t *data; size_t len; } rom_res[24];
  int n_rom_res;

  /* state of the boss rush port (sr_p2_bossrush*.c) */
  void *w8;
  int debug_dump_index;
  int hacklog_idx;

  /* debugging: per-hack trace (SOMRANDO_TRACE) */
  FILE *trace;
  uint8_t *trace_snapshot;
};

/* Development aid: appends a line to $SOMRANDO_HACKLOG (SOMRANDO_DEBUG builds only); nested hacks call it themselves. */
void sr_hacklog(struct sr_ctx *c, const char *name, int success);

/* Fatal error: records code and message and unwinds to somrando_generate. */
void sr_fail(struct sr_ctx *c, int code, const char *fmt, ...) __attribute__((noreturn, format(printf, 3, 4)));
void *sr_alloc(struct sr_ctx *c, size_t n);

/* bounds-checked outRom[off] = (byte)v */
void sr_poke(struct sr_ctx *c, int off, int v);
/* outRom[workingOffset++] = v.  v is evaluated before the offset moves; the C# evaluates the index first, so a value that reads
 * workingOffset itself must be written with an explicit temporary (see sr_names_set_all). */
void sr_emit(struct sr_ctx *c, int v);
uint8_t sr_peek(struct sr_ctx *c, int off);
uint8_t sr_peek_orig(struct sr_ctx *c, int off);

/* DataUtil */
int sr_ushort_from_bytes(const uint8_t *rom, int index);                       /* little endian */
void sr_ushort_to_bytes(uint8_t *rom, int index, int value);
int sr_clamp_to_endpoints(int value, int min_inclusive, int max_inclusive);

/* CodeGenerationUtils.ensureSpaceInBank */
int sr_ensure_space_in_bank(int *new_code_offset, int num_bytes);

/* workingData */
void sr_work_set(struct sr_ctx *c, const char *key, const char *val);
const char *sr_work_get(const struct sr_ctx *c, const char *key);   /* NULL when missing */
void sr_work_set_int(struct sr_ctx *c, const char *key, int v);
void sr_work_set_int_array(struct sr_ctx *c, const char *key, const int *v, int n);
int sr_work_get_int_array(const struct sr_ctx *c, const char *key, int *out, int max);   /* returns count, -1 when missing */

/* replacementEvents[id] = new EventScript(): returns the (empty) list stored under id, keeping the first insertion position */
struct sr_bytes *sr_event_new(struct sr_ctx *c, int id);

/* ------------------------------------------------------------------------------------------- EventScript (sr_events.c) */
#define EV_END 0x00
#define EV_RETURN 0x02
#define EV_WAIT_FOR_ANIM 0x08
#define EV_REFRESH_OBJECTS 0x09
#define EV_REFRESH_MAP 0x0A
#define EV_JUMP_BASE 0x10
#define EV_DOOR_BASE 0x18
#define EV_JUMP_SUBR_BASE 0x20
#define EV_SLEEP_FOR 0x28
#define EV_INCREMENT_FLAG 0x29
#define EV_DECREMENT_FLAG 0x2A
#define EV_GRAPHICAL_EFFECT 0x2D
#define EV_SET_FLAG 0x30
#define EV_MOVE_CHARACTER 0x32
#define EV_EVENT_LOGIC 0x42
#define EV_OPEN_DIALOGUE 0x50
#define EV_CLOSE_DIALOGUE 0x51
#define EV_MULTI_SPACE 0x59

void sr_ev_door(struct sr_ctx *c, struct sr_bytes *e, int target_door);
void sr_ev_jump(struct sr_ctx *c, struct sr_bytes *e, int target_event);
void sr_ev_jsr(struct sr_ctx *c, struct sr_bytes *e, int target_event);
void sr_ev_sleep(struct sr_ctx *c, struct sr_bytes *e, int ticks);
void sr_ev_set_flag(struct sr_ctx *c, struct sr_bytes *e, int flag, int value);
void sr_ev_incr_flag(struct sr_ctx *c, struct sr_bytes *e, int flag);
void sr_ev_decr_flag(struct sr_ctx *c, struct sr_bytes *e, int flag);
void sr_ev_end(struct sr_ctx *c, struct sr_bytes *e);
/* Logic(): runs the two-byte command when flag is in [low, high] */
void sr_ev_logic(struct sr_ctx *c, struct sr_bytes *e, int flag, int low, int high, const uint8_t cmd[2]);
void sr_ev_jump_cmd(int target_event, uint8_t out[2]);
void sr_ev_incr_cmd(int flag, uint8_t out[2]);
/* AddDialogue(text, null): VanillaEventUtil.getBytes(text) without a trailing sleep */
void sr_ev_add_dialogue(struct sr_ctx *c, struct sr_bytes *e, const char *text);
/* VanillaEventUtil.getBytes(text) (weaponDescripHandlingIndex 31) appended to list */
void sr_event_text_bytes(struct sr_ctx *c, struct sr_bytes *out, const char *text);
int sr_event_start_offset(struct sr_ctx *c, const uint8_t *rom, int event_num);   /* VanillaEventUtil.getEventStartOffset */

/* NamesOfThings */
#define NAMES_INDEX_ENEMIES_START 0xCF
void sr_names_set(struct sr_ctx *c, int index, const char *name);
void sr_names_set_all(struct sr_ctx *c);                 /* setAllNames(outRom, ref workingOffset) */
void sr_event_expander_process(struct sr_ctx *c);        /* EventExpander.process, no-op without replacement events */

/* SomLz77 */
void sr_lz77_decode(struct sr_ctx *c, const uint8_t *rom, int location, struct sr_bytes *out);
void sr_lz77_encode(struct sr_ctx *c, const uint8_t *block, size_t len, int window_size, struct sr_bytes *out);

/* ------------------------------------------------------------------------------------------- Vanilla structures */
struct sr_map_object { uint8_t d[8]; };
struct sr_door { uint8_t d[4]; };
int sr_vanilla_object_offset(const uint8_t *rom, int map_num);                       /* VanillaMapUtil.getObjectOffset */
int sr_door_get_ypos(const struct sr_door *d);
void sr_door_set_ypos(struct sr_door *d, int y);
int sr_obj_get_ypos(const struct sr_map_object *o);
void sr_obj_set_ypos(struct sr_map_object *o, int y);
int sr_obj_get_vis_flag(const struct sr_map_object *o);
void sr_obj_set_vis_flag(struct sr_map_object *o, int v);
void sr_obj_set_spawn_priority(struct sr_map_object *o, int v);

/* ------------------------------------------------------------------------------------------------- hacks (sr_hacks*.c) */
struct sr_hack {
  const char *name;         /* RandoProcessor.getName() */
  sr_hack_fn fn;            /* NULL: not ported; see gate */
  const char *upstream;     /* upstream class */
  /* Conditions under which the C reproduces the C# for this hack: a NULL terminated list of pairs {key, value-list}, value-list '|'
   * separated, every pair must hold.  For a hack with fn == NULL (not ported) they are exactly the cases in which the C# hack returns
   * without touching anything, so skipping it is correct; any other state of those options is an "unsupported option" error.
   * For a ported hack they list the options it does not handle yet.  NULL: always supported. */
  const char *const *requires;
};
extern const struct sr_hack sr_common_hacks[];
extern const int sr_common_hack_count;
extern const struct sr_hack sr_rando_hacks[];
extern const int sr_rando_hack_count;
/* One line per hack: "index list name status" with status ported / partial (ported with requirements) / stub (skipped under its requirements). */
void sr_hacks_dump(FILE *f);

/* implemented hacks */
#include "sr_hacks_simple.h"
int sr_hack_no_footstep_sound(struct sr_ctx *c);
int sr_hack_summon_despawn_fix(struct sr_ctx *c);
int sr_hack_confuse_fix(struct sr_ctx *c);
int sr_hack_percentage_poison(struct sr_ctx *c);
int sr_hack_magic_rope_death_fix(struct sr_ctx *c);
int sr_hack_boss_death_fix(struct sr_ctx *c);
int sr_hack_title_screen(struct sr_ctx *c);
int sr_hack_no_sword_pull(struct sr_ctx *c);
int sr_hack_hexas_fix(struct sr_ctx *c);
int sr_hack_orb_reward_events(struct sr_ctx *c);
int sr_hack_high_level_enemy_name_fix(struct sr_ctx *c);
int sr_hack_numbers_on_screen(struct sr_ctx *c);
int sr_hack_boss_element_rando(struct sr_ctx *c);
int sr_hack_damage_cancel_fix(struct sr_ctx *c);
int sr_hack_vineshroom(struct sr_ctx *c);
int sr_hack_player_ai_default(struct sr_ctx *c);
int sr_hack_weapon_randomizer(struct sr_ctx *c);
int sr_hack_initial_values(struct sr_ctx *c);
int sr_hack_element_swaps(struct sr_ctx *c);
int sr_hack_faster_dialogue_events(struct sr_ctx *c);
int sr_hack_boss_swaps(struct sr_ctx *c);
int sr_hack_enemy_swaps(struct sr_ctx *c);
int sr_hack_invis_clears_slow(struct sr_ctx *c);
int sr_hack_boss_z_position(struct sr_ctx *c);
int sr_hack_experience_adjust(struct sr_ctx *c);
int sr_hack_gold_adjust(struct sr_ctx *c);
int sr_hack_enemy_scaling(struct sr_ctx *c);
int sr_hack_fire_palace_door_fix(struct sr_ctx *c);
int sr_hack_defense_refactor(struct sr_ctx *c);

#include "sr_p2.h"

/* Upstream resources that are partly vanilla ROM data are never stored whole: sr_rom_recipes.c keeps ROM offsets plus the author's own edits and
 * sr_rom_res() rebuilds them from c->orig (checked by CRC-32; a ROM that differs fails with SOMRANDO_ERR_BAD_ROM).  Names: see tools/gen_rom_recipes.py.
 * The result lives in the arena of c; *len receives its size.  NULL when the name is unknown. */
struct sr_rom_seg { uint32_t rom; uint32_t len; };   /* rom == 0xFFFFFFFF: the next len bytes of the literal array */
struct sr_rom_recipe { const char *name; uint32_t len, crc; const struct sr_rom_seg *segs; int nseg; const uint8_t *lit; };
extern const struct sr_rom_recipe sr_rom_recipes[];
extern const int sr_rom_recipe_count;
const uint8_t *sr_rom_res(struct sr_ctx *c, const char *name, size_t *len);

#endif
