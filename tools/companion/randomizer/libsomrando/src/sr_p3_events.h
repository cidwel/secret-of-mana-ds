/* libsomrando: Ancient Cave event maker, difficulty rows.  LGPL-2.1 or later, see sr_core.h and COPYING.LIB. */
#ifndef SR_P3_EVENTS_H
#define SR_P3_EVENTS_H
#include "sr_core.h"
#include "sr_p2_bossrush_data.h"

/* Ancient Cave difficulty presets (Resources/difficulty_{casual,hard,reallyhard}.properties), same keys and order as sr_w8_diff_rows */
extern const struct sr_w8_diff_row sr_p3_ev_diff_rows[];
extern const int sr_p3_ev_diff_row_count;

/* settings.getDouble("difficulty_" + key) of the preset in the request; fails for a difficulty other than casual, hard and reallyhard */
double sr_p3_ev_diff(struct sr_ctx *c, const char *key);
#endif
