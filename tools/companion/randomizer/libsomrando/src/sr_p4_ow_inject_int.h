/* libsomrando: private declarations shared by sr_p4_ow_inject.c, sr_p4_ow_hints.c and sr_p4_ow_gift.c.  LGPL-2.1 or later. */
#ifndef SR_P4_OW_INJECT_INT_H
#define SR_P4_OW_INJECT_INT_H
#include "sr_core.h"

struct sr_bytes *sr_p4_inject_event_get(struct sr_ctx *c, int id);                       /* replacementEvents[id], throws when missing */
void sr_p4_inject_replace_event_data(struct sr_ctx *c, struct sr_bytes *e, const uint8_t *pat, int plen, const uint8_t *repl, size_t rlen);
int sr_p4_inject_work_int(struct sr_ctx *c, const char *key);                            /* workingData.getInt */
const char *sr_p4_inject_work_str(const struct sr_ctx *c, const char *key);              /* workingData.get ("" when missing) */
void sr_p4_inject_gift_events(struct sr_ctx *c);                                         /* GiftModeProcessing.makeGiftModeEvents */
char *sr_p4_inject_word_wrap(struct sr_ctx *c, const char *text);                        /* VanillaEventUtil.wordWrapText */
#endif
