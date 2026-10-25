/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Derived from SoMRandomizer/processing/common/ElementSwaps.cs (Author: Moppleton), the Open World branch (processOpenWorld).  Translated to C 2026-10-18.
 * Consumes workingData girlMagicExists / spriteMagicExists (bool), publishes orbElement<mapNum> (int) in map insertion order.
 */
#include <stdio.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p4.h"

#define P(off, v) sr_poke(c, (off), (v))
#define SB(key) sr_settings_get_bool(c->settings, (key))

#define ORBMAP_MATANGO 307
#define ORBMAP_EARTHPALACE 291
#define ORBMAP_FIREPALACE3 240
#define ORBMAP_FIREPALACE1 348
#define ORBMAP_FIREPALACE2 345
#define ORBMAP_LUNAPALACE 35
#define ORBMAP_UPPERLAND 41
#define ORBMAP_GRANDPALACE1 420
#define ORBMAP_GRANDPALACE_FIRST 420
#define ORBMAP_GRANDPALACE_LAST 426

/* Dictionary<int, byte> in insertion order */
struct orb_map {
  int key[16];
  int val[16];
  int n;
};

static void om_set(struct orb_map *m, int key, int val) {
  for (int i = 0; i < m->n; i++)
    if (m->key[i] == key) { m->val[i] = val; return; }
  m->key[m->n] = key;
  m->val[m->n++] = val;
}

static int om_get(struct sr_ctx *c, const struct orb_map *m, int key) {
  for (int i = 0; i < m->n; i++)
    if (m->key[i] == key) return m->val[i];
  sr_fail(c, SOMRANDO_ERR_FAILED, "The given key was not present in the dictionary.");
}

/* StringValueSettings.getBool on a workingData key: missing key reads "" which is false */
static int work_bool(const struct sr_ctx *c, const char *key) {
  const char *v = sr_work_get(c, key);
  if (!v) return 0;
  char b[8];
  size_t n = 0;
  while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
  while (v[n] && n < 7) { b[n] = (char)((v[n] >= 'A' && v[n] <= 'Z') ? v[n] + 32 : v[n]); n++; }
  if (v[n]) return 0;
  while (n && (b[n - 1] == ' ' || b[n - 1] == '\t' || b[n - 1] == '\n' || b[n - 1] == '\r')) n--;
  b[n] = 0;
  return !strcmp(b, "yes") || !strcmp(b, "true") || !strcmp(b, "1");
}

static int pick(struct sr_ctx *c, const int *avail, int n) { return avail[sr_random_next(&c->rf) % n]; }

static void orb_script(struct sr_ctx *c, int event_id, int orb_color, const int conv[8], int kind, int jump_target, int progress_flag, int progress_lo) {
  struct sr_bytes *ev = sr_event_new(c, event_id);
  uint8_t cmd[2];
  int e = conv[orb_color - 0x81];
  sr_ev_jsr(c, ev, 0x350 + e);
  if (progress_flag >= 0) {
    sr_ev_jump_cmd(0, cmd);
    sr_ev_logic(c, ev, progress_flag, progress_lo, 0xF, cmd);
  }
  sr_ev_jump_cmd(jump_target, cmd);
  sr_ev_logic(c, ev, 0x98 + e, 0x1, 0x3, cmd);
  if (kind) sr_ev_jsr(c, ev, 0x799);
  sr_ev_end(c, ev);
}

int sr_p4_shared_element_open(struct sr_ctx *c) {
  const int girl = work_bool(c, "girlMagicExists");
  const int sprite = work_bool(c, "spriteMagicExists");
  const int randomize_gp = SB("opGPEleRando");
  const int drum = SB("opFlammieDrumInLogic");
  if (sr_settings_get(c->settings, "plandoSettings")[0]) sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "unsupported option: plandoSettings (element plando)");

  int avail[8], n_avail = 0;
  if (sprite) {
    avail[n_avail++] = 0x81; avail[n_avail++] = 0x82; avail[n_avail++] = 0x83; avail[n_avail++] = 0x85;
    avail[n_avail++] = 0x86; avail[n_avail++] = 0x87; avail[n_avail++] = 0x88;
  }
  if (girl) {
    avail[n_avail++] = 0x84;
    if (!sprite) { avail[n_avail++] = 0x83; avail[n_avail++] = 0x85; }
  }
  if (n_avail == 0) avail[n_avail++] = 0xFF;

  struct orb_map m;
  m.n = 0;
  om_set(&m, ORBMAP_MATANGO, pick(c, avail, n_avail));
  om_set(&m, ORBMAP_EARTHPALACE, pick(c, avail, n_avail));
  om_set(&m, ORBMAP_FIREPALACE3, pick(c, avail, n_avail));
  om_set(&m, ORBMAP_FIREPALACE1, pick(c, avail, n_avail));
  om_set(&m, ORBMAP_FIREPALACE2, pick(c, avail, n_avail));
  om_set(&m, ORBMAP_LUNAPALACE, pick(c, avail, n_avail));

  om_set(&m, 420, sprite ? 0x81 : 0x84);
  om_set(&m, 421, sprite ? 0x82 : 0x84);
  om_set(&m, 422, 0x85);
  om_set(&m, 423, 0x83);
  om_set(&m, 424, girl ? 0x84 : 0x86);
  om_set(&m, 425, sprite ? 0x86 : 0x84);
  om_set(&m, 426, sprite ? 0x87 : 0x84);

  om_set(&m, ORBMAP_UPPERLAND, 0xFF);
  if (drum) om_set(&m, ORBMAP_UPPERLAND, pick(c, avail, n_avail));

  if (randomize_gp) {
    static const int pal_key[9] = {0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0xFF};
    static const int pal_val[9] = {89, 88, 91, 93, 47, 92, 95, 97, 0xFF};
    static const int pal_off[7] = {0x8DD19, 0x8DD41, 0x8DD69, 0x8DD91, 0x8DDB9, 0x8DDE1, 0x8DE09};
    for (int i = 0; i < 7; i++) {
      om_set(&m, 420 + i, pick(c, avail, n_avail));
      int col = om_get(c, &m, 420 + i), pv = -1;
      for (int k = 0; k < 9; k++)
        if (pal_key[k] == col) pv = pal_val[k];
      P(pal_off[i], 0x80 + pv);
    }
  }

  static const int conv[8] = {0, 1, 3, 4, 2, 5, 6, 7};
  static const int spell_ids[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 18, 19, 20, 21, 22, 23, 12, 13, 14, 15, 16, 17,
                                  39, 40, 41, 36, 37, 38, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35};
  int spell_idx = 0;
  for (int i = 0; i < 8; i++) {
    struct sr_bytes *ev = sr_event_new(c, 0x350 + i);
    sr_bytes_push(c, ev, 0x34);
    sr_bytes_push(c, ev, 0x04);
    sr_bytes_push(c, ev, 0x80);
    sr_bytes_push(c, ev, EV_WAIT_FOR_ANIM);
    sr_ev_set_flag(c, ev, 0x98 + i, 1);
    int max = (i == 4 || i == 5) ? 3 : 6;
    for (int j = 0; j < max; j++) {
      sr_bytes_push(c, ev, 0x4E);
      sr_bytes_push(c, ev, 0x04);
      sr_bytes_push(c, ev, 0x81);
      sr_bytes_push(c, ev, spell_ids[spell_idx]);
      sr_bytes_push(c, ev, 0x02);
      sr_bytes_push(c, ev, 0x01);
      spell_idx++;
    }
    sr_ev_set_flag(c, ev, 0x98 + i, 0);
    sr_bytes_push(c, ev, EV_RETURN);
    sr_ev_end(c, ev);
  }

  int v;
  if ((v = om_get(c, &m, ORBMAP_MATANGO)) != 0xFF) orb_script(c, 0x27e, v, conv, 1, 0x27c, 0x24, 0x4);
  if ((v = om_get(c, &m, ORBMAP_EARTHPALACE)) != 0xFF) orb_script(c, 0x239, v, conv, 0, 0x38, 0x18, 0x2);
  if ((v = om_get(c, &m, ORBMAP_FIREPALACE3)) != 0xFF) orb_script(c, 0x6ce, v, conv, 0, 0x6c7, -1, 0);
  if ((v = om_get(c, &m, ORBMAP_FIREPALACE1)) != 0xFF) orb_script(c, 0x2c8, v, conv, 0, 0x2c9, -1, 0);
  if ((v = om_get(c, &m, ORBMAP_FIREPALACE2)) != 0xFF) orb_script(c, 0x6cb, v, conv, 0, 0x6ca, -1, 0);
  if ((v = om_get(c, &m, ORBMAP_LUNAPALACE)) != 0xFF) orb_script(c, 0x2c4, v, conv, 0, 0x2c5, -1, 0);
  if (drum && (v = om_get(c, &m, ORBMAP_UPPERLAND)) != 0xFF) orb_script(c, 0x25a, v, conv, 0, 0x25b, -1, 0);

  /* upstream: "else if" palette branches for the other girl/sprite combinations can never run after this condition */
  if (!sprite || !girl || randomize_gp) {
    for (int map_id = ORBMAP_GRANDPALACE_FIRST; map_id <= ORBMAP_GRANDPALACE_LAST; map_id++) {
      int col = om_get(c, &m, map_id);
      if (col == 0xFF) continue;
      struct sr_bytes *ev = sr_event_new(c, 0x570 + (map_id - ORBMAP_GRANDPALACE_FIRST));
      uint8_t cmd[2];
      int e = conv[col - 0x81];
      sr_ev_jsr(c, ev, 0x350 + e);
      sr_ev_jump_cmd(0, cmd);
      sr_ev_logic(c, ev, 0x98 + e, 0x0, 0x0, cmd);
      sr_ev_incr_flag(c, ev, 0xE8 + (map_id - ORBMAP_GRANDPALACE_FIRST));
      sr_ev_jump(c, ev, 0x578);
      sr_ev_end(c, ev);
    }
  }

  for (int i = 0; i < m.n; i++) {
    char key[24];
    snprintf(key, sizeof key, "orbElement%d", m.key[i]);
    sr_work_set_int(c, key, m.val[i]);
  }
  return 1;
}
