#include <stdio.h>
#include <string.h>
#include <time.h>
#include "screens.h"
#include "gen_data.h"
#include "fb.h"

static uint32_t top_px[400 * 240], bot_px[320 * 240];

void screens_present(const uint32_t *top400, const uint32_t *bottom320) {
  fb_blit(gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), top400, 400);
  fb_blit(gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL), bottom320, 320);
  gfxFlushBuffers();
  gfxSwapBuffers();
  gspWaitForVBlank();
}

/* ---- credits */
void screens_credits(void) {
  static struct sui_text t;
  sui_text_clear(&t, GD_CREDITS_TITLE);
  for (int i = 0; i < GD_NCREDITS; i++) {
    const struct gd_credit *c = &GD_CREDITS[i];
    if (i) sui_text_blank(&t);
    sui_text_add(&t, c->who, 31, 1, 0);
    if (c->what[0]) sui_text_add(&t, c->what, 31, 0, 0);
    if (c->patch[0]) { char b[200]; snprintf(b, sizeof b, "Patch: %s", c->patch); sui_text_add(&t, b, 31, 0, 0); }
    if (c->license[0]) { char b[120]; snprintf(b, sizeof b, "License: %s", c->license); sui_text_add(&t, b, 31, 0, 1); }
    if (c->url[0]) sui_text_add(&t, c->url, 31, 0, 1);
  }
  sui_draw_grid(bot_px, 320, 320, 240);
  hidSetRepeatParameters(15, 3);
  int dirty = 1;
  while (aptMainLoop()) {
    hidScanInput();
    u32 down = hidKeysDown(), rep = hidKeysDownRepeat();
    if (down & (KEY_B | KEY_A | KEY_START)) break;
    if (rep & KEY_DOWN) { sui_text_scroll(&t, 1); dirty = 1; }
    if (rep & KEY_UP) { sui_text_scroll(&t, -1); dirty = 1; }
    if (down & KEY_R) { sui_text_scroll(&t, sui_text_visible_lines()); dirty = 1; }
    if (down & KEY_L) { sui_text_scroll(&t, -sui_text_visible_lines()); dirty = 1; }
    if (dirty) { sui_draw_text(top_px, 400, &t); dirty = 0; screens_present(top_px, bot_px); }
    else gspWaitForVBlank();
  }
}

/* ---- generic list loop */
typedef int (*ui_handler)(struct sui_list *l, u32 down, u32 rep, void *user);   /* 1 closes the list */
static void ui_loop(struct sui_list *l, ui_handler h, void *user) {
  int dirty = 1;
  sui_draw_grid(bot_px, 320, 320, 240);
  hidSetRepeatParameters(15, 4);
  while (aptMainLoop()) {
    hidScanInput();
    u32 down = hidKeysDown(), rep = hidKeysDownRepeat();
    if (rep & KEY_DOWN) { sui_list_move(l, 1); dirty = 1; }
    if (rep & KEY_UP) { sui_list_move(l, -1); dirty = 1; }
    if (down & KEY_R) { sui_list_page(l, 1); dirty = 1; }
    if (down & KEY_L) { sui_list_page(l, -1); dirty = 1; }
    if (down | rep) {
      int before = l->n;
      if (h(l, down, rep, user)) return;
      (void)before;
      dirty = 1;
    }
    if (dirty) { sui_draw_list(top_px, 400, l); dirty = 0; screens_present(top_px, bot_px); }
    else gspWaitForVBlank();
  }
}

/* ---- settings */
enum { ID_FILTER = 1, ID_PRESET, ID_ADV, ID_CREDITS, ID_GP = 100 };
struct settings_ui { struct n3_settings *s; int adv; };

static void build_settings(struct sui_list *l, struct settings_ui *u) {
  char v[64];
  int keep = l->sel;
  sui_list_clear(l, "Settings");
  sui_list_add(l, SUI_ROW_HEADER, "VIDEO", "", "")->id = 0;
  sui_list_add(l, SUI_ROW_ITEM, "Filter", n3_filter_name(u->s->filter),
               "None: sharp pixels. Smooth: blends neighbouring pixels (slower on the Old 3DS). Scanlines: darkens every other output line.")->id = ID_FILTER;
  sui_list_add(l, SUI_ROW_HEADER, "GAMEPLAY", "", "")->id = 0;
  const char *pn = "Custom";
  for (int i = 0; i < n3_gp_preset_count(); i++) if (n3_gp_preset_matches(u->s, i)) { pn = n3_gp_preset_name(i); break; }
  int pi = -1;
  for (int i = 0; i < n3_gp_preset_count(); i++) if (n3_gp_preset_matches(u->s, i)) { pi = i; break; }
  sui_list_add(l, SUI_ROW_ITEM, "Preset", pn, pi >= 0 ? n3_gp_preset_desc(pi) : "A mix of options that matches no preset. Gameplay options apply when a game starts.")->id = ID_PRESET;
  sui_list_add(l, SUI_ROW_ITEM, "Advanced options", u->adv ? "shown" : "hidden", "Shows the numeric tuning options.")->id = ID_ADV;
  for (int g = 0; g < GD_NGROUPS; g++) {
    int any = 0;
    for (int i = 0; i < GD_NOPTS; i++) {
      const struct gd_opt *o = &GD_OPTS[i];
      if (strcmp(o->group, GD_GROUPS[g]) != 0 || n3_gp_sgp(i) < 0 || (o->advanced && !u->adv)) continue;
      if (!any) { sui_list_add(l, SUI_ROW_HEADER, GD_GROUPS[g], "", "")->id = 0; any = 1; }
      n3_gp_text(u->s, i, v, sizeof v);
      sui_list_add(l, SUI_ROW_ITEM, o->label, v, o->help)->id = ID_GP + i;
    }
  }
  sui_list_add(l, SUI_ROW_HEADER, "ABOUT", "", "")->id = 0;
  sui_list_add(l, SUI_ROW_ITEM, "Credits", "", "Who made this version and whose work it builds on.")->id = ID_CREDITS;
  snprintf(l->hint, sizeof l->hint, "D-PAD move  LEFT RIGHT change  A open  B back");
  l->sel = keep < l->n ? keep : 0;
  if (l->n && l->rows[l->sel].kind != SUI_ROW_ITEM) sui_list_move(l, 1);
  if (l->sel < l->scroll) l->scroll = l->sel;
  while (l->sel >= l->scroll + sui_list_visible_rows()) l->scroll++;
}

static int settings_handler(struct sui_list *l, u32 down, u32 rep, void *user) {
  struct settings_ui *u = user;
  if (down & (KEY_B | KEY_START)) return 1;
  int dir = ((rep & KEY_RIGHT) ? 1 : 0) - ((rep & KEY_LEFT) ? 1 : 0), act = (down & KEY_A) ? 1 : 0;
  if (!dir && !act) return 0;
  int id = l->rows[l->sel].id;
  if (id == ID_FILTER) u->s->filter = (u->s->filter + (dir ? dir : 1) + N3_FILTER_COUNT) % N3_FILTER_COUNT;
  else if (id == ID_PRESET) {
    int n = n3_gp_preset_count(), cur = -1;
    for (int i = 0; i < n; i++) if (n3_gp_preset_matches(u->s, i)) cur = i;
    n3_gp_apply_preset(u->s, ((cur < 0 ? (dir < 0 ? 0 : -1) : cur) + (dir ? dir : 1) + n) % n);
  } else if (id == ID_ADV) u->adv = !u->adv;
  else if (id == ID_CREDITS) { if (act) screens_credits(); }
  else if (id >= ID_GP) n3_gp_step(u->s, id - ID_GP, dir ? dir : 1, 0);
  build_settings(l, u);
  return 0;
}

void screens_settings(struct n3_settings *s) {
  static struct sui_list l;
  struct settings_ui u = {s, 0};
  build_settings(&l, &u);
  ui_loop(&l, settings_handler, &u);
  n3_save(s, SETTINGS_PATH);
}

/* ---- pause menu */
enum { P_RESUME = 1, P_SLOT, P_SAVE, P_LOAD, P_FILTER, P_CREDITS, P_EXIT };
struct pause_ui {
  struct pause_ctx *p;
  int result, confirm_save;
  uint16_t thumb565[SF_THUMB_W * SF_THUMB_H];
  uint32_t thumb[SF_THUMB_W * SF_THUMB_H];
  struct sf_info info;
  int has_thumb;
};

static void slot_path(const struct pause_ui *u, int slot, char *out, size_t n) { sf_state_path(u->p->state_dir, u->p->key, slot, out, n); }

static void refresh_slot(struct pause_ui *u) {
  char path[200];
  slot_path(u, u->p->settings->slot, path, sizeof path);
  sf_state_info(path, &u->info);
  u->has_thumb = u->info.exists && sf_state_thumb(path, u->thumb565) == 0;
  if (u->has_thumb) sf_thumb_to_rgb(u->thumb565, u->thumb);
}

static void build_pause(struct sui_list *l, struct pause_ui *u) {
  char v[64];
  int keep = l->sel;
  char notice[96];
  snprintf(notice, sizeof notice, "%s", l->notice);
  sui_list_clear(l, "Paused");
  snprintf(l->notice, sizeof l->notice, "%s", notice);
  sui_list_add(l, SUI_ROW_ITEM, "Resume", "", "Back to the game.")->id = P_RESUME;
  snprintf(v, sizeof v, "< %d >", u->p->settings->slot);
  sui_list_add(l, SUI_ROW_ITEM, "State slot", v, "LEFT and RIGHT choose one of the 8 slots; the picture shows what is stored there.")->id = P_SLOT;
  sui_list_add(l, SUI_ROW_ITEM, u->confirm_save ? "Overwrite? press A again" : "Save state", "", "Writes the whole game to the SD card (saves/ keeps the normal battery save separately).")->id = P_SAVE;
  int usable = u->info.exists && u->info.rom_crc == u->p->rom_crc;
  sui_list_add(l, usable ? SUI_ROW_ITEM : SUI_ROW_DISABLED, "Load state", "", u->info.exists && !usable ? "This state belongs to another ROM or another set of gameplay options." : "Loads the slot and resumes there.")->id = P_LOAD;
  sui_list_add(l, SUI_ROW_ITEM, "Filter", n3_filter_name(u->p->settings->filter), "None, Smooth or Scanlines.")->id = P_FILTER;
  sui_list_add(l, SUI_ROW_ITEM, "Credits", "", "Who made this version.")->id = P_CREDITS;
  sui_list_add(l, SUI_ROW_ITEM, "Exit game", "", "Saves the battery RAM and leaves the app.")->id = P_EXIT;
  snprintf(l->hint, sizeof l->hint, "D-PAD move  LEFT RIGHT change  A select  B resume");
  l->sel = keep < l->n ? keep : 0;
  if (l->rows[l->sel].kind != SUI_ROW_ITEM) sui_list_move(l, 1);
  l->thumb = u->has_thumb ? u->thumb : NULL;
  usable = u->info.exists && u->info.rom_crc == u->p->rom_crc;
  if (u->info.exists) {
    time_t t = (time_t)u->info.stamp;
    struct tm *tm = u->info.stamp > 86400 * 365 ? gmtime(&t) : NULL;
    if (tm) snprintf(l->thumb_caption, sizeof l->thumb_caption, "%04d-%02d-%02d %02d:%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);
    else snprintf(l->thumb_caption, sizeof l->thumb_caption, "slot %d", u->p->settings->slot);
    if (!usable) snprintf(l->thumb_caption, sizeof l->thumb_caption, "other ROM or options");
  } else snprintf(l->thumb_caption, sizeof l->thumb_caption, "empty");
}

static int pause_handler(struct sui_list *l, u32 down, u32 rep, void *user) {
  struct pause_ui *u = user;
  struct n3_settings *s = u->p->settings;
  l->notice[0] = 0;
  if (down & KEY_B) { u->result = PAUSE_RESUME; return 1; }
  int dir = ((rep & KEY_RIGHT) ? 1 : 0) - ((rep & KEY_LEFT) ? 1 : 0), act = (down & KEY_A) ? 1 : 0;
  if (!dir && !act) return 0;
  int id = l->rows[l->sel].id;
  if (id != P_SAVE) u->confirm_save = 0;
  switch (id) {
  case P_RESUME: if (act) { u->result = PAUSE_RESUME; return 1; } break;
  case P_SLOT: s->slot = (s->slot - 1 + (dir ? dir : 1) + SF_SLOTS) % SF_SLOTS + 1; refresh_slot(u); break;
  case P_SAVE:
    if (!act) break;
    if (u->info.exists && !u->confirm_save) { u->confirm_save = 1; break; }
    u->confirm_save = 0; u->result = PAUSE_SAVE; return 1;
  case P_LOAD: if (act && u->info.exists && u->info.rom_crc == u->p->rom_crc) { u->result = PAUSE_LOAD; return 1; } break;
  case P_FILTER: s->filter = (s->filter + (dir ? dir : 1) + N3_FILTER_COUNT) % N3_FILTER_COUNT; break;
  case P_CREDITS: if (act) screens_credits(); break;
  case P_EXIT: if (act) { u->result = PAUSE_EXIT; return 1; } break;
  }
  build_pause(l, u);
  return 0;
}

int screens_pause(struct pause_ctx *p) {
  static struct sui_list l;
  static struct pause_ui u;
  memset(&u, 0, sizeof u);
  u.p = p;
  u.result = PAUSE_RESUME;
  refresh_slot(&u);
  memset(&l, 0, sizeof l);
  snprintf(l.notice, sizeof l.notice, "%s", p->notice);
  p->notice[0] = 0;
  build_pause(&l, &u);
  ui_loop(&l, pause_handler, &u);
  n3_save(p->settings, SETTINGS_PATH);
  return u.result;
}
