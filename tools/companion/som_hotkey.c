/* Executor of the hotkey bindings, see som_hotkey.h. */
#include "som_hotkey.h"

#include <string.h>

int som_hk_start_cmd(struct somd *d, const uint8_t *wram, const struct som_cmd *cmd, struct somd_target t) {
  switch (cmd->kind) {
  case SOM_CMD_EQUIP_WEAPON: return somd_equip(d, wram, cmd->hero, cmd->arg);
  case SOM_CMD_USE_ITEM: return somd_use_item(d, wram, cmd->arg, t);
  case SOM_CMD_CAST_MAGIC: return somd_cast(d, wram, cmd->hero, (cmd->hero == 1 && cmd->arg == 7) ? 6 : cmd->arg, cmd->spell, t);
  default: return SOMD_ERR_BAD_ARGUMENT;
  }
}

static int is_enemy_class(int cls) { return cls == SOMD_C_ENEMY_ONE_OR_ALL || cls == SOMD_C_ENEMY_ONE; }

/* Picks the target a policy asks for; returns 0 when nothing is valid. */
int som_hk_pick_target(const struct somd_targets *t, int cls, int caster, int policy, struct somd_target *out) {
  int best = -1;
  if (is_enemy_class(cls)) {
    if (policy == SOM_POL_ALL_ENEMIES && cls == SOMD_C_ENEMY_ONE_OR_ALL && somd_group_card(t) == 2) { out->kind = SOMD_T_ALL_ENEMIES; out->slot = 0; return 1; }
    long bd = 1L << 30;
    for (int i = 3; i < 6; i++) {
      if (!t->actor[i].valid) continue;
      long dx = (long)t->actor[i].sx - t->actor[caster].sx, dy = (long)t->actor[i].sy - t->actor[caster].sy, d = dx * dx + dy * dy;
      if (d < bd) { bd = d; best = i; }
    }
  } else {
    if (policy == SOM_POL_WHOLE_PARTY && cls == SOMD_C_ALLY_ONE_OR_ALL && somd_group_card(t) == 1) { out->kind = SOMD_T_ALL_ALLIES; out->slot = 0; return 1; }
    if (policy == SOM_POL_SELF && t->actor[caster].valid) best = caster;
    else {
      long bf = 1L << 30;
      for (int i = 0; i < 3; i++) {
        if (!t->actor[i].valid) continue;
        long f = t->actor[i].hp_max ? (long)t->actor[i].hp * 1000 / t->actor[i].hp_max : 0;
        if (f < bf) { bf = f; best = i; }
      }
    }
  }
  if (best < 0) return 0;
  out->kind = SOMD_T_ONE; out->slot = (uint8_t)best;
  return 1;
}

int som_hk_next_weapon(const struct som_state *st, int hero, int dir) {
  int cur = st->hero[hero].weapon_type;
  for (int k = 1; k <= 8; k++) {
    int t = ((cur + dir * k) % 8 + 8) % 8;
    if (st->weapon_slot[t] != 0xFF) return t == cur ? -1 : t;
  }
  return -1;
}
int som_hk_item_record(const struct som_state *st, int item_id) {
  for (int i = 0; i < SOM_ITEM_RECORDS; i++) if (st->item[i].qty && st->item[i].id == item_id) return i;
  return -1;
}
int som_hk_spell_id(int hero, int spirit, int slot) {
  int pos = (hero == 1 && spirit == 7) ? 6 : spirit;
  return somd_spell_id(hero, pos, slot);
}
void som_hk_spell_entry(int id, int *hero, int *spirit, int *slot) {
  *hero = (id % 6) >= 3 ? 1 : 2;
  *spirit = id / 6;
  *slot = id % 3;
}

static void toast(struct som_hk_env *e, const char *m) { if (e->toast) e->toast(e->u, m); }

static void run_cmd(struct som_hk_env *e, const struct som_cmd *c, int cls, int id, int is_spell, int caster, int policy) {
  struct somd_target t = {SOMD_T_NONE, 0};
  if (cls == SOMD_C_UNSUPPORTED || cls == SOMD_C_NONE) { e->run(e->u, c, t); return; }
  if (cls == SOMD_C_BOY_ONLY) { t.kind = SOMD_T_ONE; t.slot = 0; e->run(e->u, c, t); return; }
  if (policy == SOM_POL_ASK && e->ask) { e->ask(e->u, is_spell, cls, c); return; }
  if (policy == SOM_POL_ASK) policy = is_enemy_class(cls) ? SOM_POL_NEAREST_ENEMY : SOM_POL_LOWEST_ALLY;   /* no target cards here */
  struct somd_targets tg;
  somd_scan_targets(e->wram, (enum somd_class)cls, id, is_spell, &tg);
  if (!som_hk_pick_target(&tg, cls, caster, policy, &t)) { toast(e, "NO_TARGET"); return; }
  e->run(e->u, c, t);
}

int som_hotkey_run(const struct som_bind *b, struct som_hk_env *e) {
  struct som_state *st = e->st;
  if (!e->direct) { toast(e, "NO_DIRECT"); return 0; }
  som_read_state(e->wram, st);
  if (b->kind == SOM_BK_SELECT_SPIRIT) {   /* a = hero, b = spirit index */
    if (e->set_focus) e->set_focus(e->u, b->a == 2 ? 2 : 1, b->b & 7);
    toast(e, som_bind_spirit_name(b->b));
    return 0;
  }
  if (b->kind == SOM_BK_CYCLE_SPIRIT) {   /* only moves the spell list focus */
    int hero = 1, cur = -1;
    if (!e->get_focus || !e->set_focus) return 0;
    e->get_focus(e->u, &hero, &cur);
    if (hero != 1 && hero != 2) hero = 1;
    if (cur == 0xFF) cur = -1;
    for (int k = 1; k <= 8; k++) {
      int sp = ((cur + b->a * k) % 8 + 8) % 8;
      if (som_spirit_ring_index(st, hero, sp) >= 0) { e->set_focus(e->u, hero, sp); toast(e, som_bind_spirit_name(sp)); return 0; }
    }
    return 0;
  }
  if (!st->safe_to_inject) { toast(e, "UNSAFE_STATE"); return 0; }
  if (somd_busy(e->direct)) { toast(e, "BUSY"); return 0; }
  int pad = st->pad_hero < 3 ? st->pad_hero : 0;
  struct som_cmd cmd;
  struct somd_target none = {SOMD_T_NONE, 0};
  memset(&cmd, 0, sizeof cmd);
  if (b->kind == SOM_BK_EQUIP || b->kind == SOM_BK_CYCLE_WEAPON) {
    int type = b->a;
    if (b->kind == SOM_BK_CYCLE_WEAPON) {
      type = som_hk_next_weapon(st, pad, b->a);
      if (type < 0) return 0;
    } else if (type < 0 || type > 7 || st->weapon_slot[type] == 0xFF) { toast(e, "NOT_OWNED"); return 0; }
    cmd.kind = SOM_CMD_EQUIP_WEAPON; cmd.hero = (uint8_t)pad; cmd.arg = (uint8_t)type;
    e->run(e->u, &cmd, none);
  } else if (b->kind == SOM_BK_ITEM) {
    int r = som_hk_item_record(st, b->a);
    if (r < 0) { toast(e, "NO_ITEM"); return 0; }
    cmd.kind = SOM_CMD_USE_ITEM; cmd.hero = (uint8_t)pad; cmd.arg = (uint8_t)r;
    run_cmd(e, &cmd, somd_item_class(b->a), b->a, 0, pad, b->policy);
  } else if (b->kind == SOM_BK_CAST) {
    if (b->a < 1 || b->a > 2 || !st->hero[b->a].present) { toast(e, "BAD_ARGUMENT"); return 0; }
    cmd.kind = SOM_CMD_CAST_MAGIC; cmd.hero = (uint8_t)b->a; cmd.arg = (uint8_t)b->b; cmd.spell = (uint8_t)b->c;
    int id = som_hk_spell_id(b->a, b->b, b->c);
    if (st->hero[b->a].mp < somd_spell_mp_cost(e->direct, id)) { toast(e, "NO_MP"); return 0; }
    run_cmd(e, &cmd, somd_spell_class(e->direct, id), id, 1, b->a, b->policy);
  }
  return 0;
}
