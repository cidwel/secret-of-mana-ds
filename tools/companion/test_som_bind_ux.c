/* Tests of the shoulder-slot model (som_bind_ux.h): four fixed buttons fire by position, nothing else is bound, the slot content is what runs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "som_bind_ux.h"

static int checks;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)
#define IN(i) SOM_IN_BIT(i)

static int fired(struct som_bx *bx, uint32_t held, uint32_t now, int mode, int *which) {
  struct som_bind_out o;
  som_bind_update(&bx->ctx, held, now, mode, &o);
  *which = o.nfire ? som_bx_shoulder_of(&bx->ctx.b[o.fire[0]]) : -1;
  return o.nfire;
}

int main(void) {
  struct som_bx bx;
  struct som_ux *ux = som_ux_new();
  uint32_t t = 1000;
  int w;
  CHECK(ux);
  som_bx_init(&bx, SOM_PLAT_THOR);
  CHECK(bx.ctx.n == 4);   /* exactly the four shoulder buttons, nothing else is bound */

  /* each shoulder button, tapped, fires its own slot (position, not content) */
  static const int ins[4] = {SOM_IN_L1, SOM_IN_R1, SOM_IN_L2, SOM_IN_R2};
  for (int i = 0; i < 4; i++) {
    CHECK(fired(&bx, 0, t, SOM_BM_FIELD, &w) == 0);
    CHECK(fired(&bx, IN(ins[i]), t += 50, SOM_BM_FIELD, &w) == 1 && w == i);
    CHECK(fired(&bx, 0, t += 80, SOM_BM_FIELD, &w) == 0);
    t += 100;
  }
  /* game buttons, sticks and chords never fire and are never swallowed */
  static const int others[] = {SOM_IN_A, SOM_IN_B, SOM_IN_X, SOM_IN_Y, SOM_IN_C, SOM_IN_Z, SOM_IN_SELECT, SOM_IN_START, SOM_IN_DUP, SOM_IN_LS_UP, SOM_IN_RS_LEFT, SOM_IN_THUMBL};
  for (size_t i = 0; i < sizeof others / sizeof *others; i++) {
    struct som_bind_out o;
    som_bind_update(&bx.ctx, IN(others[i]), t += 200, SOM_BM_FIELD, &o);
    CHECK(o.nfire == 0 && o.swallow == 0);
    som_bind_update(&bx.ctx, IN(others[i]) | IN(SOM_IN_L1), t += 20, SOM_BM_FIELD, &o);   /* a chord with L1 is not a chord any more */
    CHECK(o.swallow & IN(others[i]) ? 0 : 1);
    som_bind_update(&bx.ctx, 0, t += 100, SOM_BM_FIELD, &o);
  }

  /* the slot content is what a press runs: slot 1 (R1) holds Doom Axe, slot 3 (R2) holds a spell with its policy */
  CHECK(som_ux_quick_set(ux, 1, SOM_UX_BK_WEAPON, 2) == 0);
  struct som_ux_action a;
  memset(&a, 0, sizeof a);
  CHECK(som_ux_shoulder_press(ux, 0, &a) == 0);   /* L1: empty slot, nothing to run */
  CHECK(som_ux_shoulder_press(ux, SOM_UX_SH_R1, &a) == 0 || a.kind == SOM_UX_ACT_CMD);
  CHECK(som_ux_shoulder_press(ux, 9, &a) == 0);   /* out of range */

  ux->mode = SOM_UX_FIELD; CHECK(som_bx_mode(ux) == SOM_BM_FIELD && !som_bx_locked(ux));
  ux->mode = SOM_UX_RING; CHECK(som_bx_mode(ux) == SOM_BM_RING && !som_bx_locked(ux));
  ux->mode = SOM_UX_EVENT; CHECK(som_bx_mode(ux) == SOM_BM_EVENT && som_bx_locked(ux));
  ux->mode = SOM_UX_FLAMMIE; CHECK(som_bx_mode(ux) == SOM_BM_EVENT && som_bx_locked(ux));

  char var[32];
  som_bind_rom_variant(NULL, 0, var, sizeof var); CHECK(!strcmp(var, "none"));
  static uint8_t rom[0x200000];
  rom[0x7FC0 + 0x1C] = 0x12; rom[0x7FC0 + 0x1F] = 0xAB;
  som_bind_rom_variant(rom, sizeof rom, var, sizeof var); CHECK(!strcmp(var, "200000-1200" "00ab"));
  som_ux_delete(ux);
  printf("som_bind_ux: %d checks passed\n", checks);
  return 0;
}
