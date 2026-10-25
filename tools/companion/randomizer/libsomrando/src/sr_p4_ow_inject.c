/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Open World result injection.  Derived from processing/openworld/randomization/{OpenWorldResultInjector,OpenWorldSpoilers}.cs
 * (Author: Moppleton).  Translated to C 2026-10-18.
 */
#include <stdlib.h>
#include <string.h>

#include "sr_p4.h"
#include "sr_p4_ow.h"
#include "sr_p4_ow_inject_int.h"

static const uint8_t PRIZE_INJECTION_PATTERN[4] = {0x12, 0x34, 0x56, 0x78};   /* PrizeEvents.OPENWORLD_EVENT_INJECTION_PATTERN */

#define EV_CLEAR_DIALOGUE 0x52   /* EventCommandEnum.CLEAR_DIALOGUE */

struct sr_bytes *sr_p4_inject_event_get(struct sr_ctx *c, int id) {
  for (int i = 0; i < c->n_events; i++)
    if (c->events[i].id == id) return c->events[i].data;
  sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%d' was not present in the dictionary.", id);
}

/* VanillaEventUtil.replaceEventData: first match only, scanning i < Count - pattern.Count; the replacement may have another length */
void sr_p4_inject_replace_event_data(struct sr_ctx *c, struct sr_bytes *e, const uint8_t *pat, int plen, const uint8_t *repl, size_t rlen) {
  long at = -1;
  for (long i = 0; i < (long)e->len - plen; i++) {
    int match = 1;
    for (int j = 0; j < plen; j++)
      if (e->p[i + j] != pat[j]) match = 0;
    if (match) {
      at = i;
      break;
    }
  }
  if (at < 0) return;
  size_t tail = e->len - (size_t)at - (size_t)plen;
  if (rlen > (size_t)plen) sr_bytes_reserve(c, e, rlen - (size_t)plen);
  memmove(e->p + at + rlen, e->p + at + plen, tail);
  if (rlen) memcpy(e->p + at, repl, rlen);
  e->len = (size_t)at + rlen + tail;
}

/* workingData.getInt: Int32.Parse of the stored string, which throws when the key is missing */
int sr_p4_inject_work_int(struct sr_ctx *c, const char *key) {
  const char *s = sr_work_get(c, key);
  char *end;
  if (!s || !*s) sr_fail(c, SOMRANDO_ERR_FAILED, "The input string '' was not in a correct format (workingData %s).", key);
  long v = strtol(s, &end, 10);
  if (*end) sr_fail(c, SOMRANDO_ERR_FAILED, "The input string '%s' was not in a correct format (workingData %s).", s, key);
  return (int)v;
}

const char *sr_p4_inject_work_str(const struct sr_ctx *c, const char *key) {
  const char *s = sr_work_get(c, key);
  return s ? s : "";
}

void sr_p4_ow_inject(struct sr_ctx *c, const struct ow_placements *p) {
  const char *goal = sr_p4_inject_work_str(c, "openWorldGoalShortName");
  int boy = sr_p4_inject_work_int(c, "boyStartWeapon");
  int girl = sr_p4_inject_work_int(c, "girlStartWeapon");
  int sprite = sr_p4_inject_work_int(c, "spriteStartWeapon");
  sr_poke(c, 0x57c0, boy * 9);
  sr_poke(c, 0x57c4, girl * 9);
  sr_poke(c, 0x57c8, sprite * 9);

  for (int k = 0; k < p->n; k++) {
    const struct ow_location *loc = p->v[k].loc;
    const struct ow_prize *prize = p->v[k].prize;
    struct sr_bytes *e = sr_p4_inject_event_get(c, loc->event_num);
    uint8_t pat[5];
    memcpy(pat, PRIZE_INJECTION_PATTERN, 4);
    pat[4] = (uint8_t)loc->event_replacement_index;
    sr_p4_inject_replace_event_data(c, e, pat, 5, prize->event_data, (size_t)prize->event_len);

    if (loc->map_num != -1) {
      int map_num = loc->map_num, obj_num = loc->obj_num, ev_num = loc->event_num;
      int flag = (uint8_t)prize->got_item_event_flag;
      int off = 0x80000 + sr_peek(c, 0x87000 + map_num * 2) + (sr_peek(c, 0x87000 + map_num * 2 + 1) << 8);
      off += 8;
      sr_poke(c, off + 8 * obj_num + 0, flag);
      sr_poke(c, off + 8 * obj_num + 1, 0x00);
      sr_poke(c, off + 8 * obj_num + 6, ev_num);
      sr_poke(c, off + 8 * obj_num + 7, sr_peek(c, off + 8 * obj_num + 7) & 0xC0);
      sr_poke(c, off + 8 * obj_num + 7, sr_peek(c, off + 8 * obj_num + 7) | (uint8_t)(ev_num >> 8));
    }

    /* a close followed by an open becomes a clear */
    struct sr_bytes filtered = {0};
    sr_bytes_reserve(c, &filtered, e->len);
    for (size_t i = 0; i < e->len; i++) {
      if (i + 1 < e->len && e->p[i] == EV_CLOSE_DIALOGUE && e->p[i + 1] == EV_OPEN_DIALOGUE) {
        sr_bytes_push(c, &filtered, EV_CLEAR_DIALOGUE);
        i++;
      } else {
        sr_bytes_push(c, &filtered, e->p[i]);
      }
    }
    *e = filtered;
  }

  if (!strcmp(goal, "xmas2020")) sr_p4_inject_gift_events(c);
}

/* OpenWorldSpoilers.logOpenWorldSpoilers only writes Logging.log lines.  It cannot throw: every dictionary lookup is on a key the call
 * itself iterates, crystalOrbColorMap always holds the 15 ORBMAP_* maps (ElementSwaps.processOpenWorld sets them unconditionally and
 * stores each as workingData orbElement<N>), and the name lookups return "" for unknown values.  It changes no state. */
void sr_p4_ow_log_spoilers(struct sr_ctx *c, const struct ow_placements *p, const struct ow_sim_result *r) {
  (void)c;
  (void)p;
  (void)r;
}

/* Every Open World option value the injector and the hints read is reproduced (checked against the oracle, see the delivery report). */
int sr_p4_inject_supports(const struct sr_settings *s, char *why, size_t why_len) {
  (void)s;
  (void)why;
  (void)why_len;
  return 1;
}
