/* Unit test of the shared bottom-screen renderer and touch layout (no emulator).  Optional arg: write a PPM. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "som_status_draw.h"

static uint8_t w[SOM_WRAM_SIZE];
static uint32_t px[SOM_UI_W * SOM_UI_H];

static int count_non_bg(void) { int n = 0; for (int i = 0; i < SOM_UI_W * SOM_UI_H; i++) n += px[i] != 0x101428u; return n; }

int main(int argc, char **argv) {
  struct som_state s; struct som_ui ui; struct som_cmd cmd;
  memset(w, 0, sizeof w);
  w[0xE000] = 1; w[0xE200] = 1; w[0xE400] = 1;
  w[0xE182] = 50; w[0xE184] = 50; w[0xE181] = 6; w[0xE1E4] = 2; w[0xE182 + 0x200] = 30; w[0xE184 + 0x200] = 45;
  w[0xE186 + 0x200] = 8; w[0xE187 + 0x200] = 9; w[0xE3C8] = 0x3F; w[0xE3CD] = 0x3F;
  w[0xE182 + 0x400] = 20; w[0xE184 + 0x400] = 40;
  w[0xE4] = 1; w[0xE6] = 0x0F; w[0xD9] = 1; w[0xDC] = 131;
  memset(w + 0xCC48, 0xFF, 12); w[0xCC48] = 0x60; w[0xCC4A] = 0x45;
  memset(w + 0xCC54, 0xFF, 8); w[0xCC55] = 0x40; w[0xCC56] = 2; w[0xCC5B] = 3;
  som_read_state(w, &s);
  som_ui_init(&ui);
  som_ui_draw(&s, &ui, NULL, px, SOM_UI_W);
  assert(count_non_bg() > 2000);

  {   /* title screen: empty WRAM, nothing selectable */
    uint8_t z[SOM_WRAM_SIZE]; struct som_state e; memset(z, 0, sizeof z); som_read_state(z, &e);
    assert(som_ui_touch(&ui, &e, 80 + 10, 130, &cmd) == 0);
  }
  /* weapon tab: cell 2 (axe, owned) = row 0 col 2 */
  assert(som_ui_touch(&ui, &s, 2 * 80 + 10, 130, &cmd) == 1);
  assert(cmd.kind == SOM_CMD_EQUIP_WEAPON && cmd.arg == 2 && cmd.hero == 0);
  assert(som_ui_touch(&ui, &s, 10, 130, &cmd) == 0);              /* glove not owned */
  /* item tab: record 2 = Royal Jam x2 */
  assert(som_ui_touch(&ui, &s, 2 + 106 + 10, 105, &cmd) == 0 && ui.tab == SOM_TAB_ITEM);
  assert(som_ui_touch(&ui, &s, 2 * 80 + 10, 130, &cmd) == 1 && cmd.kind == SOM_CMD_USE_ITEM && cmd.arg == 2);
  assert(som_ui_touch(&ui, &s, 80 + 10, 130, &cmd) == 0);       /* record 1 empty */
  /* magic tab: switches to the girl, spirit 0 then spell 1 */
  assert(som_ui_touch(&ui, &s, 2 + 212 + 10, 105, &cmd) == 0 && ui.tab == SOM_TAB_MAGIC && ui.hero == 1);
  assert(som_ui_touch(&ui, &s, 10, 130, &cmd) == 0 && ui.spirit == 0);
  assert(som_ui_touch(&ui, &s, 80 + 10, 130, &cmd) == 1);
  assert(cmd.kind == SOM_CMD_CAST_MAGIC && cmd.hero == 1 && cmd.arg == 0 && cmd.spell == 1 && ui.spirit == 0xFF);
  /* tapping a party row selects the hero */
  assert(som_ui_touch(&ui, &s, 100, 12 + 28 * 2 + 5, &cmd) == 0 && ui.hero == 2);
  som_ui_draw(&s, &ui, NULL, px, SOM_UI_W);
  if (argc > 1) {
    FILE *f = fopen(argv[1], "wb");
    fprintf(f, "P6\n%d %d\n255\n", SOM_UI_W, SOM_UI_H);
    for (int i = 0; i < SOM_UI_W * SOM_UI_H; i++) { fputc(px[i] >> 16, f); fputc(px[i] >> 8, f); fputc(px[i], f); }
    fclose(f);
  }
  puts("som_status_draw tests passed");
  return 0;
}
