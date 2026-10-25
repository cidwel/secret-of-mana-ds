/* Small pad-driven screens of the 3DS frontend (settings, credits, pause menu): 5x7 text on a 0x00RRGGBB pixel buffer.
 * Rules from the console tests: lists live on the top screen, the bottom screen is only a grid background, only things that need attention
 * are shown (no "ROM found" style messages).  Pure C99, no 3DS calls, so tools/companion/n3ds/test/test_sui.c renders it on the PC. */
#ifndef SOM_N3DS_SUI_H
#define SOM_N3DS_SUI_H
#include <stdint.h>
#include <stddef.h>

#define SUI_ROW_TEXT 48
#define SUI_HELP_TEXT 200
#define SUI_MAX_ROWS 64
#define SUI_THUMB_W 128
#define SUI_THUMB_H 112

enum sui_row_kind { SUI_ROW_ITEM = 0, SUI_ROW_HEADER, SUI_ROW_DISABLED };

struct sui_row {
  uint8_t kind;
  int id;                     /* caller's action id */
  char label[SUI_ROW_TEXT];
  char value[SUI_ROW_TEXT];   /* right aligned; empty for plain actions */
  char help[SUI_HELP_TEXT];   /* shown under the list while the row is selected */
};

struct sui_list {
  char title[40];
  struct sui_row rows[SUI_MAX_ROWS];
  int n, sel, scroll;
  const uint32_t *thumb;      /* optional SUI_THUMB_W x SUI_THUMB_H picture drawn right of the list (save state slot) */
  char thumb_caption[48];
  char hint[96];              /* pad hint line at the very bottom, "" for none */
  char notice[96];            /* something that needs attention (a failed save, a refused state): drawn in a red bar, "" for none */
};

void sui_list_clear(struct sui_list *l, const char *title);
struct sui_row *sui_list_add(struct sui_list *l, int kind, const char *label, const char *value, const char *help);
void sui_list_move(struct sui_list *l, int delta);     /* skips headers and disabled rows, wraps */
void sui_list_page(struct sui_list *l, int dir);       /* L / R page jump */
int sui_list_visible_rows(void);

/* top screen (400x240) */
void sui_draw_list(uint32_t *px, int stride, const struct sui_list *l);
/* bottom screen (320x240): grid background only */
void sui_draw_grid(uint32_t *px, int stride, int w, int h);

/* Word-wrapped scrolling text (credits).  `lines` is filled by sui_wrap(); the scroll offset is in lines. */
#define SUI_WRAP_LINES 160
#define SUI_WRAP_COLS 48
struct sui_text {
  char title[40];
  char line[SUI_WRAP_LINES][SUI_WRAP_COLS + 1];
  uint8_t dim[SUI_WRAP_LINES];   /* 1 = secondary line (url, license) */
  uint8_t head[SUI_WRAP_LINES];  /* 1 = entry heading (who) */
  int n, scroll;
};
void sui_text_clear(struct sui_text *t, const char *title);
void sui_text_add(struct sui_text *t, const char *s, int cols, int head, int dim);   /* wraps at `cols` characters */
void sui_text_blank(struct sui_text *t);
int sui_text_visible_lines(void);
void sui_text_scroll(struct sui_text *t, int delta);
void sui_draw_text(uint32_t *px, int stride, const struct sui_text *t);

/* primitives */
int sui_text_width(const char *s, int scale);
void sui_text_draw(uint32_t *px, int stride, int w, int h, int x, int y, int scale, uint32_t color, const char *s);
void sui_rect(uint32_t *px, int stride, int w, int h, int x, int y, int rw, int rh, uint32_t color);

#endif
