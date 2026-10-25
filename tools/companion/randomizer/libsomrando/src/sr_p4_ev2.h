/* libsomrando: helpers of the Open World supporting event modules (phase 4, owner "ev2").  LGPL-2.1 or later.
 *
 * Derived from processing/common/structure/EventScript.cs, processing/openworld/events/OpenWorldSupportingMapChanges.cs and
 * processing/common/ElementSwaps.cs (Author: Moppleton).  Translated to C 2026-10-18.
 * Included only by the sr_p4_ev2_*.c files; everything here is static so it adds no symbols. */
#ifndef SR_P4_EV2_H
#define SR_P4_EV2_H
#include <stdlib.h>
#include <string.h>

#include "sr_core.h"

/* EventScript.GetJumpCmd(n) as a 2-byte command (block scope compound literal) */
#define JC(n) ((const uint8_t[2]){(uint8_t)(EV_JUMP_BASE + ((n) >> 8)), (uint8_t)(n)})

/* ---- workingData typed reads (StringValueSettings semantics: a missing key reads as "") */
static inline const char *sr_p4_ev2_work_str(const struct sr_ctx *c, const char *key) {
  const char *v = sr_work_get(c, key);
  return v ? v : "";
}

static inline int sr_p4_ev2_work_bool(const struct sr_ctx *c, const char *key) {
  const char *v = sr_p4_ev2_work_str(c, key);
  while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
  size_t n = strlen(v);
  while (n && (v[n - 1] == ' ' || v[n - 1] == '\t' || v[n - 1] == '\n' || v[n - 1] == '\r')) n--;
  if (n == 1) return v[0] == '1';
  if (n == 3) return (v[0] | 32) == 'y' && (v[1] | 32) == 'e' && (v[2] | 32) == 's';
  if (n == 4) return (v[0] | 32) == 't' && (v[1] | 32) == 'r' && (v[2] | 32) == 'u' && (v[3] | 32) == 'e';
  return 0;
}

/* workingData.getInt: Int32.Parse, FormatException when missing or malformed */
static inline int sr_p4_ev2_work_int(struct sr_ctx *c, const char *key) {
  const char *v = sr_p4_ev2_work_str(c, key);
  char *end;
  long x;
  if (!*v) sr_fail(c, SOMRANDO_ERR_FAILED, "Input string was not in a correct format (workingData %s).", key);
  x = strtol(v, &end, 10);
  if (*end || x < -2147483647L - 1 || x > 2147483647L) sr_fail(c, SOMRANDO_ERR_FAILED, "Input string was not in a correct format (workingData %s).", key);
  return (int)x;
}

/* ElementSwaps.getCrystalOrbElementMap(context)[mapNum]: KeyNotFoundException when the map has no orb element */
static inline int sr_p4_ev2_crystal_orb(struct sr_ctx *c, int map_num) {
  char key[32];
  snprintf(key, sizeof key, "orbElement%d", map_num);
  if (!sr_work_get(c, key)) sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%d' was not present in the dictionary.", map_num);
  return sr_p4_ev2_work_int(c, key) & 0xFF;
}

/* SomVanillaValues.elementOrbByteToName(element, false) */
static inline const char *sr_p4_ev2_orb_element_name(int element) {
  switch (element) {
    case 0x81: return "gnome";
    case 0x82: return "undine";
    case 0x83: return "salamando";
    case 0x85: return "sylphid";
    case 0x84: return "lumina";
    case 0x86: return "shade";
    case 0x87: return "luna";
    case 0x88: return "dryad";
  }
  return "";
}

/* ---- EventScript dialogue helpers */
/* AddDialogue(text): text bytes, then sleep(0) to wait for a button */
static inline void sr_p4_ev2_dialogue(struct sr_ctx *c, struct sr_bytes *e, const char *text) {
  sr_event_text_bytes(c, e, text);
  sr_ev_sleep(c, e, 0);
}

static inline void sr_p4_ev2_dialogue_box(struct sr_ctx *c, struct sr_bytes *e, const char *text) {
  sr_bytes_push(c, e, EV_OPEN_DIALOGUE);
  sr_p4_ev2_dialogue(c, e, text);
  sr_bytes_push(c, e, EV_CLOSE_DIALOGUE);
}

/* AddDialogueBoxWithChoices (open_box) / AddDialogueWithChoices (dialogue box already open); every command is 2 bytes */
static inline void sr_p4_ev2_choices(struct sr_ctx *c, struct sr_bytes *e, int open_box, const char *text, int n, const uint8_t *xs,
                                     const char *const *labels, const uint8_t cmds[][2], const uint8_t cancel[2]) {
  if (open_box) sr_bytes_push(c, e, EV_OPEN_DIALOGUE);
  sr_event_text_bytes(c, e, text);
  sr_bytes_push(c, e, 0x58);   /* OPTION_SELECTION */
  for (int i = 0; i < n; i++) {
    sr_bytes_push(c, e, 0x5A);   /* OPTION_AT */
    sr_bytes_push(c, e, xs[i]);
    sr_event_text_bytes(c, e, labels[i]);
  }
  sr_bytes_push(c, e, 0x5B);   /* OPTION_SELECTION_END */
  sr_bytes_push(c, e, cancel[0]);
  sr_bytes_push(c, e, cancel[1]);
  for (int i = 0; i < n; i++) {
    sr_bytes_push(c, e, cmds[i][0]);
    sr_bytes_push(c, e, cmds[i][1]);
  }
}

/* List<string>.RemoveAt */
static inline void sr_p4_ev2_remove_at(const char **v, int *n, int idx) {
  for (int i = idx; i < *n - 1; i++) v[i] = v[i + 1];
  (*n)--;
}

/* VanillaMapUtil.getTriggerOffset(outRom, mapNum) */
static inline int sr_p4_ev2_trigger_offset(struct sr_ctx *c, int map_num) {
  return 0x80000 + sr_peek(c, 0x84000 + map_num * 2) + (sr_peek(c, 0x84000 + map_num * 2 + 1) << 8);
}

/* ---- map object helpers of OpenWorldSupportingMapChanges (they read and write the output ROM) */
static inline int sr_p4_ev2_obj_off(struct sr_ctx *c, int map_num, int obj_num, int index) {
  return sr_vanilla_object_offset(c->out, map_num) + 8 + 8 * obj_num + index;   /* + 8: skip header */
}

static inline int obj_getObjectData(struct sr_ctx *c, int map_num, int obj_num, int index) { return sr_peek(c, sr_p4_ev2_obj_off(c, map_num, obj_num, index)); }

static inline void obj_setObjectData(struct sr_ctx *c, int map_num, int obj_num, int index, int value) {
  sr_poke(c, sr_p4_ev2_obj_off(c, map_num, obj_num, index), value);
}

static inline int obj_getObjectX(struct sr_ctx *c, int m, int o) { return obj_getObjectData(c, m, o, 2); }
static inline int obj_getObjectY(struct sr_ctx *c, int m, int o) { return obj_getObjectData(c, m, o, 3); }

static inline void obj_setObjectXY(struct sr_ctx *c, int m, int o, int x, int y) {
  obj_setObjectData(c, m, o, 2, x);
  obj_setObjectData(c, m, o, 3, y);
}

static inline void obj_setObjectVisibility(struct sr_ctx *c, int m, int o, int flag, int min_value, int max_value) {
  obj_setObjectData(c, m, o, 0, flag);
  obj_setObjectData(c, m, o, 1, (uint8_t)(max_value | (min_value << 4)));
}

static inline void obj_setObjectAlwaysVisible(struct sr_ctx *c, int m, int o) { obj_setObjectVisibility(c, m, o, 0x00, 0x00, 0x0F); }
static inline void obj_setObjectNeverVisible(struct sr_ctx *c, int m, int o) { obj_setObjectVisibility(c, m, o, 0x00, 0x0F, 0x0F); }

static inline void obj_setObjectEvent(struct sr_ctx *c, int m, int o, int event_num) {
  obj_setObjectData(c, m, o, 6, (uint8_t)event_num);
  {
    int old_msb = obj_getObjectData(c, m, o, 7) & 0xF8;
    obj_setObjectData(c, m, o, 7, ((event_num >> 8) & 0x07) | old_msb);
  }
}
#endif
