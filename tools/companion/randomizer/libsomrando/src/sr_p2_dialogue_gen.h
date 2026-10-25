/* libsomrando: helpers shared by sr_p2_dialogue.c and the generated sr_p2_dialogue_gen.c (FasterDialogueEvents, Author: Moppleton). */
#ifndef SR_P2_DIALOGUE_GEN_H
#define SR_P2_DIALOGUE_GEN_H
#include "sr_core.h"

extern const char *const sr_w5_dlg_element_names[8];

/* processShortening (generated) */
void sr_w5_dlg_shortening(struct sr_ctx *c, const int *new_element_mapping);

/* EventScript text helpers (getBytesDelay based) */
void sr_w5_dlg_box(struct sr_ctx *c, struct sr_bytes *e, const char *text);
void sr_w5_dlg_dialogue(struct sr_ctx *c, struct sr_bytes *e, const char *text, int delay);
void sr_w5_dlg_auto_box(struct sr_ctx *c, struct sr_bytes *e, const char *text, int delay);
const char *sr_w5_dlg_fmt(struct sr_ctx *c, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void sr_w5_dlg_set_attribute(struct sr_ctx *c, struct sr_bytes *e, const int *new_element_mapping, int element_id);

enum { SRDLG_CMD_JUMP, SRDLG_CMD_JSR, SRDLG_CMD_INCR };
/* Logic(flag, low, high, GetJumpCmd/GetJsrCmd(target) or GetIncrCmd(flag)) */
void sr_w5_dlg_logic(struct sr_ctx *c, struct sr_bytes *e, int flag, int low, int high, int kind, int arg);
#endif
