/* Gameplay tweaks page of the SoM PC start menu (board item T10, options of T8).  Reads tools/gameplay/gameplay_menu.json, keeps the
 * chosen values, writes the options file that `som_gameplay.py apply --options` takes and draws a full-window page.  No SDL, no files
 * (som_pc.c supplies the text); the generator and the randomizer sources are not touched.  Pure C99. */
#ifndef PC_GAMEPLAY_H
#define PC_GAMEPLAY_H

#include <stddef.h>
#include <stdint.h>

#define PG_MAX_OPT 64
#define PG_MAX_VAL 8
#define PG_MAX_PRE 8

enum pg_type { PG_ENUM = 0, PG_BOOL, PG_NUM };
struct pg_value { char v[32], l[40]; };
struct pg_opt {
  char key[40], label[48], group[24], help[420];
  int type, advanced, optional, integer;   /* integer: an enum whose values are written as numbers */
  int min, max, nvals;
  struct pg_value val[PG_MAX_VAL];
  int def, cur;                            /* enum: value index; bool: 0 / 1; num: -1 = unset, else the number */
};
struct pg_preset { char id[24], name[40], desc[160]; short val[PG_MAX_OPT]; };   /* val: the value of every option once the preset is applied */
struct pg {
  struct pg_opt o[PG_MAX_OPT];
  int n, show_adv, cursor, done;
  struct pg_preset pre[PG_MAX_PRE];
  int npre;
};
/* PG_PRESET applies the next preset of the menu file (vanilla, quick combat, arcade, quality of life...) over every option. */
enum pg_key { PG_UP = 0, PG_DOWN, PG_LEFT, PG_RIGHT, PG_FAST_LEFT, PG_FAST_RIGHT, PG_ACCEPT, PG_BACK, PG_RESET, PG_PRESET };

int pg_load_menu(struct pg *g, const char *menu_json);          /* 0 ok, -1 not the gameplay menu file */
int pg_load_values(struct pg *g, const char *options_json);     /* the file pg_to_json wrote; unknown keys and bad values are ignored; 0 ok, -1 not JSON */
int pg_to_json(const struct pg *g, char *out, size_t n);        /* {"options": {...}} with the values that differ from the default; length or -1 */
int pg_is_default(const struct pg *g);
int pg_preset_match(const struct pg *g);                         /* index of the preset the current values equal, or -1 */
void pg_reset(struct pg *g);
void pg_key(struct pg *g, int key);
int pg_entries(const struct pg *g);                              /* rows the cursor moves over: visible options + advanced toggle + reset + done */
void pg_value_text(const struct pg_opt *o, int cur, char *out, size_t n);
/* Full-window page.  `hover_note` (may be NULL) is a one-line message at the bottom. */
void pg_draw(const struct pg *g, uint32_t *px, int stride, int w, int h, const char *hover_note);
/* Draws a short text with the page font (used for the hint on the main menu).  mul = pixel scale. */
void pg_text(uint32_t *px, int stride, int w, int h, int x, int y, const char *s, uint32_t col, int mul);
int pg_text_width(const char *s, int mul);

#endif
