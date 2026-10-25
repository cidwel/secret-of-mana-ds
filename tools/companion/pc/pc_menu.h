/* The in-game menu of the SoM PC frontend (board item T12): resume, save / load state slots, video settings (filter, window size, screen mode,
 * aspect, scaling, integer scaling, vsync), credits and the way out.  Pure model + software drawing (the 5x7 face of pc_gameplay.c); no SDL,
 * no files.  The host (som_pc.c) feeds buttons with pcm_key(), reads what to do with pcm_poll(), fills the slot cards from pc_states and
 * draws the page over the paused game.  Every control works with the keyboard, a pad (D-pad, A, B, L, R) or the mouse wheel, and nothing is
 * shown that does not need the player's attention: a saved state is visible in its card, not announced. */
#ifndef PC_MENU_H
#define PC_MENU_H

#include <stddef.h>
#include <stdint.h>
#include "pc_logic.h"
#include "pc_states.h"

enum pcm_page { PCM_ROOT = 0, PCM_SAVE, PCM_LOAD, PCM_VIDEO, PCM_CREDITS, PCM_PAGES };
enum pcm_key { PCM_UP = 0, PCM_DOWN, PCM_LEFT, PCM_RIGHT, PCM_ACCEPT, PCM_BACK, PCM_TAB_PREV, PCM_TAB_NEXT };
enum pcm_act {
  PCM_A_NONE = 0,
  PCM_A_CLOSE,         /* back to the game */
  PCM_A_SAVE,          /* arg: slot */
  PCM_A_LOAD,          /* arg: slot */
  PCM_A_VIDEO,         /* a video setting changed in the cfg: apply it (window size, screen mode, vsync, filter, aspect) */
  PCM_A_SLOTS,         /* open the quick slot editor (L1 R1 L2 R2) */
  PCM_A_LAUNCHER,      /* back to the start menu */
  PCM_A_QUIT
};

/* One slot card, filled by the host from pcs_info(). */
struct pcm_slot {
  int status;                  /* enum pcs_status */
  char when[32];               /* "2026-10-20 18:32" */
  int tw, th;                  /* thumbnail size, 0 when there is none */
  const uint32_t *thumb;       /* 0xAARRGGBB, tw * th, owned by the host */
};

struct pcm {
  int open, page;
  int cursor[PCM_PAGES];
  int scroll;                  /* credits: first visible line */
  int credits_lines;           /* set by the drawing code: total wrapped lines (for the scroll limit) */
  int credits_vis;
  int confirm;                 /* SAVE: slot + 1 whose overwrite waits for a second ACCEPT, 0 none */
  int can_launcher;            /* the menu has "start menu" */
  int in_game;                 /* a game is loaded: states and slots exist (0 before a ROM is chosen) */
  int single_page;             /* opened on one page from outside (the start menu's Display entry): B closes the menu instead of going to the root */
  struct pc_cfg *cfg;          /* the settings the video page edits in place */
  struct pcm_slot slot[PCS_SLOTS];
  const char *credits;         /* text of the credits page, lines separated by \n; owned by the host */
  char note[96];               /* one line of attention at the bottom ("COULD NOT WRITE THE STATE"), "" when nothing */
  int act, act_arg, act_pending;
  uint32_t rev;
};

void pcm_init(struct pcm *m, struct pc_cfg *cfg);
void pcm_open(struct pcm *m, int page);              /* opens on a page (PCM_ROOT, PCM_VIDEO, ...) */
void pcm_close(struct pcm *m);
void pcm_key(struct pcm *m, int key);
void pcm_scroll(struct pcm *m, int rows);            /* mouse wheel: positive goes down */
int pcm_poll(struct pcm *m, int *act, int *arg);
void pcm_note(struct pcm *m, const char *text);

/* Rows of the root page and of the video page, for the host and the tests. */
int pcm_root_count(const struct pcm *m);
const char *pcm_root_label(const struct pcm *m, int i);
enum { PCM_V_FILTER = 0, PCM_V_SIZE, PCM_V_SCREEN, PCM_V_ASPECT, PCM_V_SCALING, PCM_V_INTEGER, PCM_V_VSYNC, PCM_V_BACK, PCM_V_COUNT };
void pcm_video_value(const struct pcm *m, int row, char *out, size_t n);

/* Draws the whole page on a transparent canvas (alpha 0 outside the panel); w x h are the window pixels. */
void pcm_draw(struct pcm *m, uint32_t *px, int stride, int w, int h);

#endif
