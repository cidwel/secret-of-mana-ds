/* som_launcher: platform-neutral model of the start menu (board item T9).
 *
 * Screens: MAIN (SoM original / Create randomized), PRESETS, TWEAK (options), MULTI (a multiple-choice option), SEED, GENERATE, PLAY.
 * Input comes from buttons (sl_key) or touch (sl_tap, sl_scroll); the host reads what to do with sl_poll().  The model never touches a file,
 * a thread or the generator: it produces the chosen configuration (sl_config_json / sl_config_cli / sl_config_lines) and a cache key, and
 * the host answers the GENERATE action with sl_gen_cached(), sl_gen_begin() or sl_gen_unavailable().  That is the whole seam between the
 * UI and the generator: the PC runs som-dualscreen, the Android app looks for a pre-generated ROM today and will call libsomrando later
 * (somrando_api.h) without any change in this file or in the drawing code.
 *
 * Option names and values are the generator's own (Moppu/SecretOfManaRandomizer v1.49, LGPL-2.1), see som_launcher_catalog.h.
 * Pure C99, no allocation, no OS dependency.
 */
#ifndef SOM_LAUNCHER_H
#define SOM_LAUNCHER_H

#include <stddef.h>
#include <stdint.h>
#include "som_launcher_catalog.h"

#define SL_SEED_DIGITS 8
#define SL_MAX_ROWS 224
#define SL_TEXT 96

enum sl_screen { SL_MAIN = 0, SL_PRESETS, SL_TWEAK, SL_MULTI, SL_SEED, SL_GENERATE, SL_PLAY, SL_SAVED, SL_CREDITS, SL_SCREEN_COUNT };

enum sl_button { SL_UP = 0, SL_DOWN, SL_LEFT, SL_RIGHT, SL_A, SL_B, SL_X, SL_Y, SL_L, SL_R, SL_START, SL_SELECT };

enum sl_action_kind {
  SL_ACT_NONE = 0,
  SL_ACT_PLAY_ORIGINAL,    /* start the user's original ROM */
  SL_ACT_GENERATE,         /* the configuration is final: look it up in the cache, then answer with sl_gen_cached / sl_gen_begin / sl_gen_unavailable */
  SL_ACT_CANCEL,           /* the player cancelled a running generation */
  SL_ACT_PLAY,             /* start the ROM at sl_rom_path() */
  SL_ACT_QUIT,             /* back out of the main screen */
  SL_ACT_DELETE_SAVED,     /* the player confirmed deleting a saved game: slot in sl_act_arg(); the host moves its files to a trash folder, then re-lists */
  SL_ACT_TWEAKS,           /* extras: open the gameplay tweaks page (QOL / fixes) */
  SL_ACT_DISPLAY           /* extras: open the display settings (filter, window size, screen mode) */
};

/* Optional main menu entries after the three standard ones, enabled by the host with sl_set_extras().  Without them the menu is the three-entry
 * menu of every earlier build, pixel for pixel. */
#define SL_X_TWEAKS 1
#define SL_X_DISPLAY 2
#define SL_X_CREDITS 4

/* Pad focus on the list screens: the list itself, the footer buttons, and (SAVED) the delete button of the selected slot. */
enum sl_focus { SF_LIST = 0, SF_BACK, SF_GO, SF_UP, SF_DOWN, SF_DEL };

/* One saved randomized game: a pre-generated ROM the host found (roms/gen/<key>.sfc with its sidecar).  At most SL_SAVED_MAX are shown. */
#define SL_SAVED_MAX 4
struct sl_saved {
  uint8_t used;
  char key[17], preset[32], mode[24], seed[12], diff[16], date[16], path[512], lines[2048];   /* preset: catalogue id or ""; lines: "key=value\n" options */
};

enum sl_gen_state { SL_GEN_ASK = 0 /* waiting for the host's answer */, SL_GEN_RUNNING, SL_GEN_UNAVAILABLE, SL_GEN_ERROR, SL_GEN_DONE };

enum sl_row_kind { SR_HEADER = 0, SR_ACTION, SR_DIFF, SR_OPT, SR_TOGGLE_ADV, SR_PRESET, SR_MULTI_ITEM };
enum sl_row_action { SA_GENERATE = 0, SA_SEED, SA_RANDOM_ALL, SA_RESET, SA_SURPRISE };

struct sl_row { uint8_t kind; int16_t a; };      /* a: option index, preset index (-1 = surprise me), action id, value index, or group header id */

struct sl_config {
  int preset;                                     /* index in sl_presets, -1 custom */
  int mode;                                       /* index in sl_modes */
  int diff;                                       /* difficulty index, -1 when the mode has none */
  char seed[SL_SEED_DIGITS + 1];                  /* digits, no leading zeros ("0" at least) */
  int32_t val[SL_MAX_OPTS];                       /* current values, see struct sl_option.def for the encoding */
};

struct sl_launcher {
  enum sl_screen screen, back_to;
  struct sl_config cfg;
  struct sl_row rows[SL_MAX_ROWS]; int nrows;
  int cursor, scroll, vis_rows;                   /* selected row, first visible row, rows that fit (set by sl_set_view) */
  int ret_cursor, ret_scroll;                     /* TWEAK position to return to from MULTI */
  int preset_row;                                 /* PRESETS: row of the last choice, where the cursor returns to */
  int multi_opt;                                  /* SL_MULTI: option being edited */
  uint8_t show_adv;                               /* TWEAK: advanced options shown */
  int seed_pos;                                   /* SEED: digit being edited */
  enum sl_gen_state gen; int gen_step, gen_total; char gen_name[SL_TEXT], gen_msg[SL_TEXT * 2], rom_path[512];
  char status_original[SL_TEXT], status_generator[SL_TEXT];
  char notice[SL_TEXT]; int notice_t;
  uint64_t rng;
  int act, act_pending, act_arg;
  int focus;                                      /* enum sl_focus */
  struct sl_saved saved[SL_SAVED_MAX]; int saved_hidden;   /* saved games listed by the host; how many more exist but are not shown */
  int8_t preset_ok[SL_MAX_PRESETS];               /* host: 1 the on-device generator makes this preset, 0 not (PC only), -1 unknown */
  int8_t gen_ok; char gen_why[SL_TEXT];           /* host: can the current configuration be generated on the device (1/0/-1) and why not */
  int del_confirm;                                /* SAVED: slot + 1 waiting for the second press, 0 none */
  unsigned extras;                                /* SL_X_* */
  const char *credits;                            /* text of the credits screen (host owned, lines separated by \n) */
  char status_tweaks[SL_TEXT];                    /* line under the tweaks entry, "" for none */
  int cred_scroll, cred_vis;                      /* credits screen: first line shown, lines that fit */
  uint32_t rev;                                   /* bumped on every visible change (redraw test) */
};

/* ---- lifecycle ---- */
void sl_init(struct sl_launcher *L, uint64_t entropy);
void sl_seed_rng(struct sl_launcher *L, uint64_t s);
/* One-line status strings shown on the main screen under the two entries ("ROM OK", "GENERATOR PENDING"); may be NULL. */
void sl_set_status(struct sl_launcher *L, const char *original, const char *generator);
void sl_show(struct sl_launcher *L, enum sl_screen s);        /* jump to a screen with its rows built (tests, restoring a session) */
void sl_set_view(struct sl_launcher *L, int w, int h);        /* rows that fit on a w x h screen */

/* ---- extras: tweaks / display / credits entries ---- */
void sl_set_extras(struct sl_launcher *L, unsigned flags, const char *credits_text);   /* credits_text must outlive the launcher; NULL keeps the previous text */
void sl_set_tweaks_status(struct sl_launcher *L, const char *text);
int sl_main_count(const struct sl_launcher *L);                  /* entries on the main screen: 3 plus the enabled extras */
int sl_main_kind(const struct sl_launcher *L, int i);            /* SH_MAIN_* of entry i */
int sl_credits_count(const struct sl_launcher *L);               /* credits lines after wrapping at SL_CREDITS_COLS characters */
int sl_credits_line(const struct sl_launcher *L, int i, char *out, size_t n);   /* 1 and the text of line i, 0 past the end */
#define SL_CREDITS_COLS 40

/* ---- saved games (host side lists, the model shows and asks) ---- */
void sl_set_preset_ok(struct sl_launcher *L, int preset, int ok);
void sl_set_gen_support(struct sl_launcher *L, int ok, const char *why);
void sl_saved_clear(struct sl_launcher *L);
/* Adds a saved game (the newest first); returns the slot or -1 when the four are used.  `preset` is a catalogue id or "", `lines` the options that differ
 * from the mode defaults ("key=value\n", sl_config_lines format). */
int sl_saved_add(struct sl_launcher *L, const char *key, const char *preset, const char *mode, const char *seed, const char *diff,
                 const char *date, const char *path, const char *lines);
void sl_saved_set_hidden(struct sl_launcher *L, int n);
int sl_saved_count(const struct sl_launcher *L);
/* Makes `cfg` the configuration described by the arguments (preset id may be ""); returns 0, or -1 when the mode is unknown (the cfg is then untouched). */
int sl_load_config(struct sl_launcher *L, const char *preset, const char *mode, const char *seed, const char *diff, const char *lines);
int sl_act_arg(const struct sl_launcher *L);
const char *sl_pad_hint(const struct sl_launcher *L);   /* one line telling which pad buttons do what on the current screen */

/* ---- input ---- */
void sl_key(struct sl_launcher *L, int button);
void sl_tap(struct sl_launcher *L, int x, int y, int w, int h);    /* device pixels */
void sl_scroll(struct sl_launcher *L, int rows);                   /* touch drag: positive scrolls down */
int sl_poll(struct sl_launcher *L, int *action);                   /* 1 and the action when one is waiting */
void sl_tick(struct sl_launcher *L);                               /* once per frame: expires the notice */

/* ---- host answers to SL_ACT_GENERATE and progress while running ---- */
void sl_gen_cached(struct sl_launcher *L, const char *rom_path);      /* a valid ROM for this key exists: go to PLAY */
void sl_gen_begin(struct sl_launcher *L);                             /* generation started */
void sl_gen_progress(struct sl_launcher *L, int step, int total, const char *name);
void sl_gen_done(struct sl_launcher *L, const char *rom_path);        /* generation finished: go to PLAY */
void sl_gen_fail(struct sl_launcher *L, const char *message);
void sl_gen_unavailable(struct sl_launcher *L, const char *message);  /* nothing can make this ROM on this device (yet) */
const char *sl_rom_path(const struct sl_launcher *L);

/* ---- configuration ---- */
int sl_apply_preset(struct sl_launcher *L, int preset);               /* 0 ok, -1 bad index or a catalogue entry that does not resolve */
void sl_randomize_all(struct sl_launcher *L);                         /* every option marked randomize, from its pool or uniformly */
void sl_random_seed(struct sl_launcher *L);
int sl_set_seed(struct sl_launcher *L, const char *digits);           /* 0 ok, -1 not 1..8 digits */
int sl_set_option(struct sl_launcher *L, const char *key, const char *value);   /* by generator name and value string; 0 ok */
int sl_nopts(const struct sl_launcher *L);
int sl_find_opt(const struct sl_launcher *L, const char *key);          /* option index by generator name, -1 if none */
void sl_sha256(const void *p, size_t n, uint8_t out[32]);               /* FIPS 180-4, used for the cache key */
const struct sl_option *sl_opt(const struct sl_launcher *L, int i);
int sl_opt_visible(const struct sl_launcher *L, int i);               /* requirements met (the advanced flag is not considered) */
void sl_value_text(const struct sl_launcher *L, int i, char *buf, size_t n);   /* display text of the current value */
void sl_value_string(const struct sl_launcher *L, int i, char *buf, size_t n); /* generator string of the current value */
const char *sl_diff_label(const char *diff);
int sl_difficulty(const struct sl_launcher *L, const char **name);    /* 1 and the name when the mode has one */

/* Canonical outputs, byte-identical to what tools/companion/cli/launcher_config.py produces for the same choices. */
int sl_config_json(const struct sl_launcher *L, char *buf, size_t n);   /* compact JSON, sorted keys; returns the length or -1 when it does not fit */
int sl_config_cli(const struct sl_launcher *L, char *buf, size_t n);    /* som-dualscreen randomize arguments */
int sl_config_lines(const struct sl_launcher *L, char *buf, size_t n);  /* "key=value\n" lines for somrando_generate(options) */
int sl_config_keystring(const struct sl_launcher *L, char *buf, size_t n);
void sl_config_key(const struct sl_launcher *L, char out[17]);          /* first 16 hex digits of SHA-256 of the key string */

/* ---- layout shared by the drawing code and the touch handler (virtual pixels) ---- */
struct sl_metrics { int s, W, H, col_x, col_w, head_h, foot_h, row_h, rows; };
int sl_focus_has(const struct sl_launcher *L, int focus);   /* 1 when the footer button / control exists on the current screen */
void sl_metrics_for(int w, int h, struct sl_metrics *m);

enum sl_hit_kind { SH_NONE = 0, SH_MAIN_ORIGINAL, SH_MAIN_RANDOM, SH_ROW, SH_ROW_DEC, SH_ROW_INC, SH_BACK, SH_SCROLL_UP, SH_SCROLL_DOWN,
                   SH_KEY /* a: 0..9 digit, 10 backspace, 11 random, 12 OK */, SH_SEED_DIGIT /* a: position */, SH_CANCEL, SH_RETRY, SH_GO,
                   SH_MAIN_SAVED, SH_SAVED_SLOT /* a: slot */, SH_SAVED_DEL /* a: slot */,
                   SH_MAIN_TWEAKS, SH_MAIN_DISPLAY, SH_MAIN_CREDITS };
struct sl_rect { int x, y, w, h; };
struct sl_hit { struct sl_rect r; int kind, a; };
#define SL_MAX_HITS 64
int sl_layout(const struct sl_launcher *L, const struct sl_metrics *m, struct sl_hit *hits, int max);

/* Text for a row ("label" on the left, value on the right); returns 0 for rows without a value. */
void sl_row_texts(const struct sl_launcher *L, const struct sl_row *r, char *label, size_t ln, char *value, size_t vn);
const char *sl_screen_name(int screen);

#endif
