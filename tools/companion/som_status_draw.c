#include <stdio.h>
#include <string.h>
#include "som_status_draw.h"

#define BG 0x101428u
#define FG 0xEBEBEBu
#define DIM 0x787D96u
#define ACC 0xFFC83Cu
#define RED 0xE65A5Au
#define GRN 0x5AC85Au
#define BLU 0x5A8CE6u
#define CELL 0x20284Cu
#define SEL 0x405090u

/* classic 5x7 font, columns LSB = top row; lower case is drawn as upper case */
static const struct { char c; uint8_t col[5]; } font[] = {
  {' ', {0, 0, 0, 0, 0}}, {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}}, {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
  {'2', {0x42, 0x61, 0x51, 0x49, 0x46}}, {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}}, {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
  {'5', {0x27, 0x45, 0x45, 0x45, 0x39}}, {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}}, {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
  {'8', {0x36, 0x49, 0x49, 0x49, 0x36}}, {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
  {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}}, {'B', {0x7F, 0x49, 0x49, 0x49, 0x36}}, {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
  {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}}, {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}}, {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
  {'G', {0x3E, 0x41, 0x49, 0x49, 0x7A}}, {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}}, {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
  {'J', {0x20, 0x40, 0x41, 0x3F, 0x01}}, {'K', {0x7F, 0x08, 0x14, 0x22, 0x41}}, {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
  {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}}, {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}}, {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
  {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}}, {'Q', {0x3E, 0x41, 0x51, 0x21, 0x5E}}, {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
  {'S', {0x46, 0x49, 0x49, 0x49, 0x31}}, {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}}, {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},
  {'V', {0x1F, 0x20, 0x40, 0x20, 0x1F}}, {'W', {0x3F, 0x40, 0x38, 0x40, 0x3F}}, {'X', {0x63, 0x14, 0x08, 0x14, 0x63}},
  {'Y', {0x07, 0x08, 0x70, 0x08, 0x07}}, {'Z', {0x61, 0x51, 0x49, 0x45, 0x43}},
  {'/', {0x20, 0x10, 0x08, 0x04, 0x02}}, {'%', {0x23, 0x13, 0x08, 0x64, 0x62}}, {':', {0x00, 0x36, 0x36, 0x00, 0x00}},
  {'.', {0x00, 0x60, 0x60, 0x00, 0x00}}, {'-', {0x08, 0x08, 0x08, 0x08, 0x08}}, {'+', {0x08, 0x08, 0x3E, 0x08, 0x08}},
  {'?', {0x02, 0x01, 0x51, 0x09, 0x06}}, {'#', {0x14, 0x7F, 0x14, 0x7F, 0x14}}, {'>', {0x41, 0x22, 0x14, 0x08, 0x00}},
  {'<', {0x08, 0x14, 0x22, 0x41, 0x00}},
};

static const char *const item_name[] = {"CANDY", "CHOCOLATE", "ROYAL JAM", "WALNUT", "HERB", "CUP WISHES", "MAGIC ROPE",
                                        "FL DRUM", "MOOGLE BELT", "MALLET", "BARREL"};
static const char *const weapon_name[] = {"GLOVE", "SWORD", "AXE", "SPEAR", "W4", "W5", "W6", "JAVELIN"};
static const char *const spirit_name[] = {"GNOME", "UNDINE", "SALAMANDO", "SYLPHID", "LUNA", "DRYAD", "SHADE", "LUMINA"};
static const char *const hero_name[] = {"BOY", "GIRL", "SPRITE"};

struct cv { uint32_t *px; int stride; };

static void rect(struct cv *c, int x, int y, int w, int h, uint32_t col) {
  for (int j = y < 0 ? 0 : y; j < y + h && j < SOM_UI_H; j++)
    for (int i = x < 0 ? 0 : x; i < x + w && i < SOM_UI_W; i++) c->px[j * c->stride + i] = col;
}
static void frame(struct cv *c, int x, int y, int w, int h, uint32_t col) {
  rect(c, x, y, w, 1, col); rect(c, x, y + h - 1, w, 1, col); rect(c, x, y, 1, h, col); rect(c, x + w - 1, y, 1, h, col);
}
static void text(struct cv *c, int x, int y, const char *s, uint32_t col) {
  for (; *s; s++, x += 6) {
    char ch = (*s >= 'a' && *s <= 'z') ? (char)(*s - 32) : *s;
    for (unsigned f = 0; f < sizeof font / sizeof font[0]; f++) {
      if (font[f].c != ch) continue;
      for (int i = 0; i < 5; i++)
        for (int j = 0; j < 7; j++)
          if (font[f].col[i] & (1 << j)) {
            int px = x + i, py = y + j;
            if (px >= 0 && px < SOM_UI_W && py >= 0 && py < SOM_UI_H) c->px[py * c->stride + px] = col;
          }
      break;
    }
  }
}
static void bar(struct cv *c, int x, int y, int w, int h, unsigned num, unsigned den, uint32_t col) {
  frame(c, x, y, w, h, DIM);
  if (den == 0) return;
  if (num > den) num = den;
  rect(c, x + 1, y + 1, (int)((w - 2) * num / den), h - 2, col);
}

/* layout */
#define PARTY_Y 12
#define PARTY_H 28
#define TAB_Y 100
#define TAB_H 18
#define GRID_Y 122
#define CELL_W 80
#define CELL_H 38

static int party_slots(const struct som_state *s, int *slot) {
  int n = 0;
  for (int i = 0; i < SOM_PARTY; i++) if (s->hero[i].present) slot[n++] = i;
  return n;
}

void som_ui_init(struct som_ui *ui) { ui->tab = SOM_TAB_WEAPON; ui->hero = 0; ui->spirit = 0xFF; }

/* What each grid cell means in the current tab: label, enabled flag.  Returns the number of cells (0..12). */
static int cells(const struct som_state *s, const struct som_ui *ui, char label[12][14], uint8_t on[12]) {
  int n = 0;
  memset(on, 0, 12);
  int live = s->hero[0].present;   /* nothing is selectable on the title screen or while loading */
  if (ui->tab == SOM_TAB_WEAPON) {
    for (int t = 0; t < SOM_WEAPON_TYPES; t++, n++) {
      snprintf(label[n], 14, "%s", weapon_name[t]);
      on[n] = live && s->weapon_slot[t] != 0xFF;
    }
  } else if (ui->tab == SOM_TAB_ITEM) {
    for (int r = 0; r < SOM_ITEM_RECORDS; r++, n++) {
      if (s->item[r].qty) snprintf(label[n], 14, "%s x%d", s->item[r].id < 11 ? item_name[s->item[r].id] : "?", s->item[r].qty);
      else label[n][0] = 0;
      on[n] = live && s->item[r].qty != 0;
    }
  } else if (ui->spirit == 0xFF) {
    for (int sp = 0; sp < SOM_SPIRITS; sp++, n++) {
      snprintf(label[n], 14, "%s", spirit_name[sp]);
      on[n] = som_spirit_ring_index(s, ui->hero, sp) >= 0;
    }
  } else {
    for (int k = 0; k < 3; k++, n++) { snprintf(label[n], 14, "SPELL %d", k + 1); on[n] = 1; }
    snprintf(label[n], 14, "BACK"); on[n++] = 1;
  }
  return n;
}

void som_ui_draw(const struct som_state *s, const struct som_ui *ui, const struct som_macro *m, uint32_t *px, int stride) {
  struct cv c = {px, stride};
  char b[48], label[12][14];
  uint8_t on[12];
  int slot[SOM_PARTY], n;
  rect(&c, 0, 0, SOM_UI_W, SOM_UI_H, BG);

  snprintf(b, sizeof b, "MAP %u  %s  %s", s->map_id, som_mode_name(s->mode), s->safe_to_inject ? "READY" : "LOCKED");
  text(&c, 4, 2, b, s->safe_to_inject ? ACC : RED);
  if (m && m->result != SOM_RUNNING && m->frames) { text(&c, 232, 2, som_result_name(m->result), m->result == SOM_DONE ? GRN : RED); }
  else if (m && som_macro_busy(m)) text(&c, 232, 2, "RUNNING", ACC);

  n = party_slots(s, slot);
  for (int k = 0; k < n; k++) {
    const struct som_hero *h = &s->hero[slot[k]];
    int y = PARTY_Y + k * PARTY_H;
    frame(&c, 2, y, 316, PARTY_H - 2, slot[k] == ui->hero ? ACC : DIM);
    snprintf(b, sizeof b, "%s L%d%s", hero_name[slot[k]], h->level, h->alive ? "" : " KO");
    text(&c, 6, y + 3, b, FG);
    snprintf(b, sizeof b, "HP %d/%d", h->hp, h->hp_max);
    text(&c, 6, y + 14, b, FG);
    bar(&c, 84, y + 4, 96, 7, h->hp, h->hp_max, GRN);
    if (h->mp_max) {
      snprintf(b, sizeof b, "MP %d/%d", h->mp, h->mp_max);
      text(&c, 84, y + 14, b, FG);
      bar(&c, 190, y + 14, 40, 7, h->mp, h->mp_max, BLU);
    }
    text(&c, 240, y + 3, h->weapon_type < 8 ? weapon_name[h->weapon_type] : "?", ACC);
    bar(&c, 240, y + 14, 70, 7, h->recharge_pct, 100, ACC);
  }

  static const char *const tab_name[] = {"WEAPON", "ITEM", "MAGIC"};
  for (int t = 0; t < 3; t++) {
    int x = 2 + t * 106;
    rect(&c, x, TAB_Y, 104, TAB_H, t == ui->tab ? SEL : CELL);
    frame(&c, x, TAB_Y, 104, TAB_H, t == ui->tab ? ACC : DIM);
    text(&c, x + 52 - 3 * (int)strlen(tab_name[t]), TAB_Y + 5, tab_name[t], FG);
  }

  n = cells(s, ui, label, on);
  for (int i = 0; i < n; i++) {
    int x = i % 4 * CELL_W, y = GRID_Y + i / 4 * CELL_H;
    rect(&c, x + 2, y + 2, CELL_W - 4, CELL_H - 4, on[i] ? CELL : BG);
    frame(&c, x + 2, y + 2, CELL_W - 4, CELL_H - 4, on[i] ? FG : 0x30344Cu);
    if (label[i][0]) text(&c, x + 6, y + CELL_H / 2 - 3, label[i], on[i] ? FG : DIM);
  }
}

int som_ui_touch(struct som_ui *ui, const struct som_state *s, int x, int y, struct som_cmd *out) {
  char label[12][14];
  uint8_t on[12];
  int slot[SOM_PARTY], np = party_slots(s, slot);
  memset(out, 0, sizeof *out);
  if (y >= PARTY_Y && y < PARTY_Y + np * PARTY_H) { ui->hero = (uint8_t)slot[(y - PARTY_Y) / PARTY_H]; ui->spirit = 0xFF; return 0; }
  if (y >= TAB_Y && y < TAB_Y + TAB_H) {
    int t = (x - 2) / 106;
    if (t < 0 || t > 2) return 0;
    ui->tab = (uint8_t)t; ui->spirit = 0xFF;
    if (t == SOM_TAB_MAGIC && ui->hero == 0) ui->hero = s->hero[1].present ? 1 : 2;
    return 0;
  }
  if (y < GRID_Y || x < 0 || x >= SOM_UI_W) return 0;
  int i = (y - GRID_Y) / CELL_H * 4 + x / CELL_W;
  int n = cells(s, ui, label, on);
  if (i < 0 || i >= n || !on[i]) return 0;
  if (ui->tab == SOM_TAB_WEAPON) { out->kind = SOM_CMD_EQUIP_WEAPON; out->hero = ui->hero; out->arg = (uint8_t)i; return 1; }
  if (ui->tab == SOM_TAB_ITEM) { out->kind = SOM_CMD_USE_ITEM; out->hero = ui->hero; out->arg = (uint8_t)i; return 1; }
  if (ui->spirit == 0xFF) { ui->spirit = (uint8_t)i; return 0; }
  if (i == 3) { ui->spirit = 0xFF; return 0; }
  out->kind = SOM_CMD_CAST_MAGIC; out->hero = ui->hero; out->arg = ui->spirit; out->spell = (uint8_t)i;
  ui->spirit = 0xFF;
  return 1;
}

void som_ui_text(uint32_t *px, int stride, int x, int y, const char *s, uint32_t col) {
  struct cv c = {px, stride};
  text(&c, x, y, s, col);
}
