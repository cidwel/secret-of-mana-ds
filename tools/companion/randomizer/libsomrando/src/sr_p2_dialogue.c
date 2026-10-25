/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Phase 2: FasterDialogueEvents.  Author of every upstream hack: Moppleton.  Translated to C 2026-10-18.
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p2_dialogue_gen.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define S(key) sr_settings_get(c->settings, (key))
#define SB(key) sr_settings_get_bool(c->settings, (key))

const char *const sr_w5_dlg_element_names[8] = {"Gnome", "Undine", "Salamando", "Sylphid", "Lumina", "Shade", "Luna", "Dryad"};

static uint8_t dlg_byte(int s) {
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
    case '>': return 0x57;
  }
  return 0x80;
}

/* VanillaEventUtil.getBytesDelay(text, delay, 31) for the ASCII text used here, with the (boy) name indicator */
static void dlg_text_bytes(struct sr_ctx *c, struct sr_bytes *out, const char *text, int delay) {
  int num_newlines = 0, num_chars = 0;
  size_t len = strlen(text);
  for (size_t i = 0; i < len;) {
    int ch = (uint8_t)text[i];
    int name = -1;
    if (!strncmp(text + i, "(boy)", 5)) name = 0;
    if (name >= 0) {
      /* "\x03\x00" is two characters: the first has c == 3 (no counting, byte 0x57), the second is raw */
      num_chars += 1;
      if ((num_chars % 64) == 31) { sr_bytes_push(c, out, 0x80); sr_bytes_push(c, out, 0x80); num_chars += 2; }
      sr_bytes_push(c, out, 0x57);
      num_chars += 1;
      if ((num_chars % 64) == 31) { sr_bytes_push(c, out, 0x80); sr_bytes_push(c, out, 0x80); num_chars += 2; }
      sr_bytes_push(c, out, 0x00);
      i += 5;
      continue;
    }
    if (ch == '\n') {
      num_newlines++;
      if (num_newlines == 3 && i != len - 1) {
        num_newlines = 0;
        sr_bytes_push(c, out, 0x28);
        sr_bytes_push(c, out, delay);
      }
      num_chars = 0;
    } else {
      num_chars++;
      if ((num_chars % 64) == 31) {
        sr_bytes_push(c, out, dlg_byte(' '));
        sr_bytes_push(c, out, dlg_byte(' '));
        num_chars += 2;
      }
    }
    sr_bytes_push(c, out, dlg_byte(ch));
    i++;
  }
}

void sr_w5_dlg_dialogue(struct sr_ctx *c, struct sr_bytes *e, const char *text, int delay) {
  dlg_text_bytes(c, e, text, delay);
  sr_ev_sleep(c, e, delay);
}
void sr_w5_dlg_box(struct sr_ctx *c, struct sr_bytes *e, const char *text) {
  sr_bytes_push(c, e, EV_OPEN_DIALOGUE);
  sr_w5_dlg_dialogue(c, e, text, 0);
  sr_bytes_push(c, e, EV_CLOSE_DIALOGUE);
}
void sr_w5_dlg_auto_box(struct sr_ctx *c, struct sr_bytes *e, const char *text, int delay) {
  sr_bytes_push(c, e, EV_OPEN_DIALOGUE);
  sr_w5_dlg_dialogue(c, e, text, delay);
  sr_bytes_push(c, e, EV_CLOSE_DIALOGUE);
}

const char *sr_w5_dlg_fmt(struct sr_ctx *c, const char *fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  size_t n = strlen(buf) + 1;
  char *r = (char *)sr_alloc(c, n);
  memcpy(r, buf, n);
  return r;
}

void sr_w5_dlg_logic(struct sr_ctx *c, struct sr_bytes *e, int flag, int low, int high, int kind, int arg) {
  uint8_t cmd[2];
  if (kind == SRDLG_CMD_INCR) sr_ev_incr_cmd(arg, cmd);
  else {
    cmd[0] = (uint8_t)((kind == SRDLG_CMD_JUMP ? EV_JUMP_BASE : EV_JUMP_SUBR_BASE) + (arg >> 8));
    cmd[1] = (uint8_t)arg;
  }
  sr_ev_logic(c, e, flag, low, high, cmd);
}

/* setAttribute: girl and sprite get the element's attribute value, with the shade/lumina/dryad exceptions */
void sr_w5_dlg_set_attribute(struct sr_ctx *c, struct sr_bytes *e, const int *new_element_mapping, int element_id) {
  static const int attribute_values[8] = {0xC8, 0xC9, 0xCA, 0xCB, 0xCF, 0xCE, 0xCC, 0xCD};
  int new_id = new_element_mapping[element_id];
  int new_value = attribute_values[new_id];
  sr_bytes_push(c, e, 0x39);
  sr_bytes_push(c, e, 0x02);
  sr_bytes_push(c, e, new_value);
  sr_bytes_push(c, e, new_id == 5 ? 0x00 : new_id == 7 ? 0x18 : 0x38);
  sr_bytes_push(c, e, 0x39);
  sr_bytes_push(c, e, 0x03);
  sr_bytes_push(c, e, new_value);
  sr_bytes_push(c, e, new_id == 4 ? 0x00 : new_id == 7 ? 0x03 : 0x07);
}

int sr_hack_faster_dialogue_events(struct sr_ctx *c) {
  int mapping[16];
  int n = sr_work_get_int_array(c, "vanillaRandoElementList", mapping, 16);
  if (n < 8) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range (element list).");
  if (SB("rDialogue")) {
    sr_w5_dlg_shortening(c, mapping);
    return 1;
  }

  /* processNoShortening: 0x1e7 spikey intro, adjusted for the flag */
  const int WITCHCASTLE_FLAG = 0x17;
  struct sr_bytes *e = sr_event_new(c, 0x1E7);
  uint8_t cmd[2];
  sr_ev_jump_cmd(0, cmd);
  sr_ev_logic(c, e, WITCHCASTLE_FLAG, 0x4, 0xF, cmd);
  sr_ev_set_flag(c, e, WITCHCASTLE_FLAG, 3);   /* set this because we removed the npc, due to spawning issues */
  sr_bytes_push(c, e, EV_REFRESH_MAP);         /* apply event flag 17 */
  sr_bytes_push(c, e, EV_MOVE_CHARACTER);
  sr_bytes_push(c, e, 0x00);
  sr_bytes_push(c, e, 0x20);
  sr_bytes_push(c, e, EV_WAIT_FOR_ANIM);
  for (int i = 0; i < 3; i++) {   /* dyluck flash */
    sr_ev_incr_flag(c, e, WITCHCASTLE_FLAG);
    sr_bytes_push(c, e, EV_REFRESH_OBJECTS);
    sr_ev_sleep(c, e, 2);
    sr_ev_decr_flag(c, e, WITCHCASTLE_FLAG);
    sr_bytes_push(c, e, EV_REFRESH_OBJECTS);
    sr_ev_sleep(c, e, 2);
  }
  sr_ev_incr_flag(c, e, WITCHCASTLE_FLAG);
  sr_bytes_push(c, e, EV_REFRESH_OBJECTS);
  sr_ev_incr_flag(c, e, WITCHCASTLE_FLAG);
  sr_bytes_push(c, e, EV_REFRESH_OBJECTS);
  sr_bytes_push(c, e, EV_MOVE_CHARACTER);
  sr_bytes_push(c, e, 0x00);
  sr_bytes_push(c, e, 0x20);
  sr_bytes_push(c, e, EV_WAIT_FOR_ANIM);
  sr_bytes_push(c, e, EV_GRAPHICAL_EFFECT);
  sr_bytes_push(c, e, 0x08);   /* center cam */
  for (int i = 0; i < 3; i++) {   /* the slow open of the spikey gate */
    sr_ev_jsr(c, e, 0x1F0);
    sr_ev_sleep(c, e, 0xC);
  }
  sr_ev_door(c, e, 0x13A);   /* spikey fight */
  sr_ev_jump(c, e, 0x704);   /* boss theme */
  sr_ev_end(c, e);

  /* fix mana tree destruction scrolling for scroll hack */
  P(0xa7a6f, 0x14);
  return 1;
}
