/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Derived from (Author: Moppleton for all of them), translated to C 2026-10-18:
 *   SoMRandomizer/processing/common/structure/EventScript.cs      - sr_ev_*
 *   SoMRandomizer/processing/common/VanillaEventUtil.cs            - sr_event_text_bytes, sr_event_start_offset
 *   SoMRandomizer/processing/hacks/common/util/NamesOfThings.cs    - sr_names_*  (spell-name branch, used by no mode, is not ported)
 *   SoMRandomizer/processing/hacks/common/util/EventExpander.cs    - sr_event_expander_process
 *   SoMRandomizer/processing/hacks/common/util/CustomEventManager.cs is only needed once a hack registers a custom command.
 */
#include <stdlib.h>
#include <string.h>

#include "sr_core.h"

/* ---------------------------------------------------------------------------------------------------------- EventScript */
void sr_ev_door(struct sr_ctx *c, struct sr_bytes *e, int target_door) {
  sr_bytes_push(c, e, EV_DOOR_BASE + (target_door >> 8));
  sr_bytes_push(c, e, target_door);
}
void sr_ev_jump(struct sr_ctx *c, struct sr_bytes *e, int target_event) {
  sr_bytes_push(c, e, EV_JUMP_BASE + (target_event >> 8));
  sr_bytes_push(c, e, target_event);
}
void sr_ev_jsr(struct sr_ctx *c, struct sr_bytes *e, int target_event) {
  sr_bytes_push(c, e, EV_JUMP_SUBR_BASE + (target_event >> 8));
  sr_bytes_push(c, e, target_event);
}
void sr_ev_sleep(struct sr_ctx *c, struct sr_bytes *e, int ticks) {
  sr_bytes_push(c, e, EV_SLEEP_FOR);
  sr_bytes_push(c, e, ticks);
}
void sr_ev_set_flag(struct sr_ctx *c, struct sr_bytes *e, int flag, int value) {
  sr_bytes_push(c, e, EV_SET_FLAG);
  sr_bytes_push(c, e, flag);
  sr_bytes_push(c, e, value);
}
void sr_ev_incr_flag(struct sr_ctx *c, struct sr_bytes *e, int flag) {
  sr_bytes_push(c, e, EV_INCREMENT_FLAG);
  sr_bytes_push(c, e, flag);
}
void sr_ev_decr_flag(struct sr_ctx *c, struct sr_bytes *e, int flag) {
  sr_bytes_push(c, e, EV_DECREMENT_FLAG);
  sr_bytes_push(c, e, flag);
}
void sr_ev_end(struct sr_ctx *c, struct sr_bytes *e) { sr_bytes_push(c, e, EV_END); }

void sr_ev_logic(struct sr_ctx *c, struct sr_bytes *e, int flag, int low, int high, const uint8_t cmd[2]) {
  sr_bytes_push(c, e, EV_EVENT_LOGIC);
  sr_bytes_push(c, e, flag);
  sr_bytes_push(c, e, (low << 4) | (high & 0x0F));
  sr_bytes_push(c, e, cmd[0]);
  sr_bytes_push(c, e, cmd[1]);
}
void sr_ev_jump_cmd(int target_event, uint8_t out[2]) {
  out[0] = (uint8_t)(EV_JUMP_BASE + (target_event >> 8));
  out[1] = (uint8_t)target_event;
}
void sr_ev_incr_cmd(int flag, uint8_t out[2]) {
  out[0] = EV_INCREMENT_FLAG;
  out[1] = (uint8_t)flag;
}

/* ---------------------------------------------------------------------------------------------------- VanillaEventUtil */
static uint8_t event_byte(int s) {
  if (s == '\n') return 0x7F;
  if (s == ' ') return 0x80;
  if (s >= 0x61 && s <= 0x7A) return (uint8_t)(s + 0x20);
  if (s >= 0x41 && s <= 0x5A) return (uint8_t)(s + 0x5A);
  if (s >= 0x30 && s <= 0x39) return (uint8_t)(s + 0x85);
  switch (s) {
    case '.': return 0xBF;
    case ',': return 0xC0;
    case '/': return 0xC1;
    case '\'': return 0xC2;
    case '[': return 0xC3;
    case ']': return 0xC4;
    case ':': return 0xC5;
    case '-': return 0xC6;
    case '%': return 0xC7;
    case '!': return 0xC8;
    case '&': return 0xC9;
    case '?': return 0xCA;
    case '(': return 0xCB;
    case ')': return 0xCC;
    case '*': return 0xCD;
    case '^': return 0xCE;
    case 149: return 0xCF;
    case 150: return 0xD0;
    case 151: return 0xD1;
    case 152: return 0xD2;
    case '>': return 0x57;
  }
  return 0x80;
}

/* Replace(pattern, repl) on a counted buffer; returns the new length. */
static size_t replace_all(char *buf, size_t len, const char *pat, const char *repl, size_t repl_len) {
  size_t pl = strlen(pat), i = 0, o = 0;
  char *tmp = (char *)malloc(len + 1);
  if (!tmp) return len;
  while (i < len) {
    if (i + pl <= len && !memcmp(buf + i, pat, pl)) {
      memcpy(tmp + o, repl, repl_len);
      o += repl_len;
      i += pl;
    } else {
      tmp[o++] = buf[i++];
    }
  }
  memcpy(buf, tmp, o);
  free(tmp);
  return o;
}

void sr_event_text_bytes(struct sr_ctx *c, struct sr_bytes *out, const char *text) {
  const uint8_t delay = 0;
  const int weapon_descrip_index = 31;
  size_t len = strlen(text);
  char *s = (char *)sr_alloc(c, len + 1);
  memcpy(s, text, len);
  len = replace_all(s, len, "(boy)", "\x03\x00", 2);
  len = replace_all(s, len, "(girl)", "\x03\x01", 2);
  len = replace_all(s, len, "(sprite)", "\x03\x02", 2);
  int num_newlines = 0, num_chars = 0;
  for (size_t i = 0; i < len; i++) {
    int ch = (uint8_t)s[i];
    if (ch == '\n') {
      num_newlines++;
      if (num_newlines == 3 && i != len - 1) {
        num_newlines = 0;
        sr_bytes_push(c, out, 0x28);
        sr_bytes_push(c, out, delay);
      }
      num_chars = 0;
    } else if (ch == '\t') {
      sr_bytes_push(c, out, 0x10);
      sr_bytes_push(c, out, 0x00);
    } else {
      num_chars++;
      if ((num_chars % 64) == weapon_descrip_index) {
        sr_bytes_push(c, out, event_byte(' '));
        sr_bytes_push(c, out, event_byte(' '));
        num_chars += 2;
      }
    }
    if (ch > 3) sr_bytes_push(c, out, event_byte(ch));
    else if (ch == 0x03) sr_bytes_push(c, out, 0x57);
    else sr_bytes_push(c, out, ch);
  }
}

void sr_ev_add_dialogue(struct sr_ctx *c, struct sr_bytes *e, const char *text) { sr_event_text_bytes(c, e, text); }

int sr_event_start_offset(struct sr_ctx *c, const uint8_t *rom, int event_num) {
  (void)c;
  int base = 0x90000, id_in_bank = event_num;
  if (event_num >= 0x400) {
    base = 0xA0000;
    id_in_bank -= 0x400;
  }
  return base + sr_ushort_from_bytes(rom, base + id_in_bank * 2);
}

/* ------------------------------------------------------------------------------------------------------ NamesOfThings */
void sr_names_set(struct sr_ctx *c, int index, const char *name) {
  size_t n = strlen(name) + 1;
  char *copy = (char *)sr_alloc(c, n);
  memcpy(copy, name, n);
  for (int i = 0; i < c->n_names; i++)
    if (c->names[i].index == index) { c->names[i].text = copy; return; }
  if (c->n_names == c->cap_names) {
    int nc = c->cap_names ? c->cap_names * 2 : 16;
    struct sr_name *nn = (struct sr_name *)sr_alloc(c, (size_t)nc * sizeof *nn);
    if (c->n_names) memcpy(nn, c->names, (size_t)c->n_names * sizeof *nn);
    c->names = nn;
    c->cap_names = nc;
  }
  c->names[c->n_names].index = index;
  c->names[c->n_names].text = copy;
  c->n_names++;
}

static const char *name_lookup(const struct sr_ctx *c, int index) {
  for (int i = 0; i < c->n_names; i++)
    if (c->names[i].index == index) return c->names[i].text;
  return NULL;
}

void sr_names_set_all(struct sr_ctx *c) {
  if (c->n_names == 0) return;   /* if nothing set, do nothing */
  for (int i = 0; i < c->n_names; i++)
    if (c->names[i].index >= 0x1000) sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "custom spell names are not ported");

  int nco = c->working_offset;
  /* increment bank if close to end, for safety */
  int nco2 = nco + 0x2000;
  if ((nco & 0xFFFF0000) != (nco2 & 0xFFFF0000)) nco = (int)(nco2 & 0xFFFF0000);

  sr_poke(c, 0x1931, 0x22);
  sr_poke(c, 0x1932, (uint8_t)nco);
  sr_poke(c, 0x1933, (uint8_t)(nco >> 8));
  sr_poke(c, 0x1934, (uint8_t)((nco >> 16) + 0xC0));

  /* LDA #bank: the C# evaluates the array index (and its increment) before the right-hand side */
  sr_poke(c, nco++, 0xA9);
  {
    int o = nco++;
    sr_poke(c, o, (uint8_t)((nco >> 16) + 0xC0));
  }
  sr_poke(c, nco++, 0x48);   /* PHA */
  sr_poke(c, nco++, 0xAB);   /* PLB */
  sr_poke(c, nco++, 0x6B);   /* RTL */

  for (int event_id = 0x800; event_id <= 0x94E; event_id++) {
    int item_id = event_id - 0x800;
    const char *nm = name_lookup(c, item_id);
    if (nm) {
      struct sr_bytes b = {0};
      sr_event_text_bytes(c, &b, nm);
      sr_ushort_to_bytes(c->out, 0xA0000 + (event_id - 0x400) * 2, (uint16_t)nco);
      for (size_t i = 0; i < b.len; i++) sr_poke(c, nco++, b.p[i]);
      sr_poke(c, nco++, 0x00);
    } else {
      int event_offset_offset = 0xA0000 + (event_id - 0x400) * 2;
      int event_offset = sr_ushort_from_bytes(c->out, event_offset_offset) + 0xA0000;
      sr_ushort_to_bytes(c->out, event_offset_offset, (uint16_t)nco);
      uint8_t v = sr_peek(c, event_offset);
      while (v != 0) {
        sr_poke(c, nco++, v);
        event_offset++;
        v = sr_peek(c, event_offset);
      }
      sr_poke(c, nco++, v);
    }
  }
  c->working_offset = nco;
}

/* ---------------------------------------------------------------------------------------------------- EventExpander */
void sr_event_expander_process(struct sr_ctx *c) {
  if (c->n_events == 0) return;   /* no event changes?  don't touch anything */
  uint8_t *rom = c->out;
  int *new_offsets = (int *)sr_alloc(c, (size_t)(0x400 + 0x601) * sizeof(int));
  for (int i = 0; i < 0x400; i++) new_offsets[i] = 0x90000 + rom[0x90000 + i * 2] + (rom[0x90000 + i * 2 + 1] << 8);
  for (int i = 0; i < 0x601; i++) new_offsets[0x400 + i] = 0xA0000 + rom[0xA0000 + i * 2] + (rom[0xA0000 + i * 2 + 1] << 8);

  for (int k = 0; k < c->n_events; k++) {
    const struct sr_bytes *d = c->events[k].data;
    sr_ensure_space_in_bank(&c->working_offset, (int)d->len);
    if (c->events[k].id < 0 || c->events[k].id >= 0x400 + 0x601) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was outside the bounds of the array (event id).");
    new_offsets[c->events[k].id] = c->working_offset;
    for (size_t i = 0; i < d->len; i++) sr_emit(c, d->p[i]);
  }

  int offsets_new_start = c->working_offset + 0xC00000;
  int offsets_new_start2 = offsets_new_start + 2;
  for (int i = 0; i < 0x400 + 0x601; i++) {
    sr_emit(c, (uint8_t)new_offsets[i]);
    sr_emit(c, (uint8_t)(new_offsets[i] >> 8));
    sr_emit(c, (uint8_t)(0xC0 + (new_offsets[i] >> 16)));
  }

  /* modify code to read offsets from the new 24 bit table (see the comment block in EventExpander.cs) */
  for (int a = 0x1E796; a <= 0x1E79C; a++) rom[a] = 0xEA;
  rom[0x1E79E] = 0x18;
  rom[0x1E79F] = 0x65;
  rom[0x1E7A0] = 0x00;
  rom[0x1E7A1] = 0xAA;
  rom[0x1E7A2] = 0xBF;
  rom[0x1E7A3] = (uint8_t)offsets_new_start;
  rom[0x1E7A4] = (uint8_t)(offsets_new_start >> 8);
  rom[0x1E7A5] = (uint8_t)(offsets_new_start >> 16);
  rom[0x1E7A6] = 0x85;
  rom[0x1E7A7] = 0xD1;
  rom[0x1E7A8] = 0xE2;
  rom[0x1E7A9] = 0x20;
  rom[0x1E7AA] = 0xBF;
  rom[0x1E7AB] = (uint8_t)offsets_new_start2;
  rom[0x1E7AC] = (uint8_t)(offsets_new_start2 >> 8);
  rom[0x1E7AD] = (uint8_t)(offsets_new_start2 >> 16);
  rom[0x1E7AE] = 0x85;
  rom[0x1E7AF] = 0xD3;
  for (int a = 0x1E7B0; a <= 0x1E7B9; a++) rom[a] = 0xEA;
}
