/* Executor of the hotkey bindings, shared by the Android JNI and the 3DS app.  It resolves an action of som_bind.h against the
 * live game state and runs it through the direct layer (direct/som_direct.h).  The platform supplies the few things that
 * differ (toast, command runner, target cards, spell-list focus) as callbacks. */
#ifndef SOM_HOTKEY_H
#define SOM_HOTKEY_H

#include "som_bind.h"
#include "som_companion.h"
#include "direct/som_direct.h"

struct som_hk_env {
  void *u;
  const uint8_t *wram;
  struct som_state *st;        /* refreshed from wram before every action */
  struct somd *direct;
  void (*toast)(void *u, const char *msg);
  /* Runs a resolved command (direct call, with the ring macro as the platform's fallback). */
  void (*run)(void *u, const struct som_cmd *cmd, struct somd_target t);
  /* Opens the target cards for the command (policy ASK); NULL when the platform has none (ASK then picks like a default). */
  void (*ask)(void *u, int is_spell, int cls, const struct som_cmd *c);
  /* Spell-list focus of the bottom screen (hero 1 or 2, spirit 0..7 or 0xFF); NULL when the platform has none. */
  int (*get_focus)(void *u, int *hero, int *spirit);
  void (*set_focus)(void *u, int hero, int spirit);
};

/* Runs one binding.  Returns 0 when it ran or was refused with a toast. */
int som_hotkey_run(const struct som_bind *b, struct som_hk_env *env);

/* Starts a macro-style command through the direct layer; returns a `enum somd_err`.  The girl's Lumina is spirit 7 of the macro
 * numbering and position 6 of the direct layer. */
int som_hk_start_cmd(struct somd *d, const uint8_t *wram, const struct som_cmd *cmd, struct somd_target t);

/* Pure helpers (unit-tested on the PC). */
int som_hk_pick_target(const struct somd_targets *t, int cls, int caster, int policy, struct somd_target *out);   /* 1 found */
int som_hk_next_weapon(const struct som_state *st, int hero, int dir);      /* next owned weapon type in a direction, -1 none */
int som_hk_item_record(const struct som_state *st, int item_id);            /* record index of an owned item, -1 none */
/* The bottom-screen spell ids (6 * spirit position + 3 * girl + slot) to the binding's (hero, spirit, slot). */
void som_hk_spell_entry(int spell_id, int *hero, int *spirit, int *slot);
int som_hk_spell_id(int hero, int spirit, int slot);

#endif
