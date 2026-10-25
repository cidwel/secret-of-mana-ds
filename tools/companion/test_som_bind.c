/* Unit tests of the shared binding engine (som_bind.c) and the pure parts of the hotkey executor (som_hotkey.c).
 * Plain C99, no emulator, no ROM.  Built and run on the PC and as an ARM binary under qemu-arm (tools/companion/run_bind_tests.sh). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "som_bind.h"
#include "som_hotkey.h"

#define IN(i) SOM_IN_BIT(i)
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

static struct som_bind mk(int mod, int key, int press, int kind, int a, int b, int c, int pol) {
  struct som_bind x = {(uint8_t)mod, (uint8_t)key, (uint8_t)press, (uint8_t)kind, (int16_t)a, (int16_t)b, (int16_t)c, (uint8_t)pol};
  return x;
}
static struct som_bind_ctx C;
static uint32_t T;
static struct som_bind_out O;
static void step(uint32_t held, uint32_t dt, int mode) { T += dt; som_bind_update(&C, held, T, mode, &O); }
static void fresh(int plat) { som_bind_init(&C, plat); T = 1000; memset(&O, 0, sizeof O); }

static void test_tables(void) {
  for (int p = 0; p < SOM_PLAT_COUNT; p++) {
    uint32_t have = som_bind_platform_inputs(p);
    CHECK(have & IN(SOM_IN_L1));
    for (int i = 1; i < SOM_IN_COUNT; i++) {
      CHECK(som_bind_input_info(i) && som_bind_input_info(i)->id == i);
      if (have & IN(i)) {
        CHECK(som_bind_label(p, i)[0]);
        CHECK(strlen(som_bind_label(p, i)) <= 4);
        for (int j = i + 1; j < SOM_IN_COUNT; j++) if (have & IN(j)) CHECK(strcmp(som_bind_label(p, i), som_bind_label(p, j)) != 0);
      }
    }
  }
  CHECK(!(som_bind_platform_inputs(SOM_PLAT_3DS_NEW) & IN(SOM_IN_THUMBL)));
  CHECK(som_bind_platform_inputs(SOM_PLAT_3DS_NEW) & IN(SOM_IN_RS_UP));
  CHECK(!(som_bind_platform_inputs(SOM_PLAT_3DS_OLD) & IN(SOM_IN_RS_UP)));
  CHECK(!(som_bind_platform_inputs(SOM_PLAT_3DS_OLD) & IN(SOM_IN_L2)));
  CHECK(som_bind_platform_inputs(SOM_PLAT_THOR) & IN(SOM_IN_MODE));
  /* L and R are read in no mode; every other SNES button is read in free play */
  for (int m = 0; m < SOM_BM_COUNT; m++) CHECK(!(som_bind_used_mask(m) & ((1u << SOM_SB_L) | (1u << SOM_SB_R))));
  CHECK(som_bind_used_mask(SOM_BM_FIELD) == 0x3FF);
  CHECK(!(som_bind_used_mask(SOM_BM_RING) & (1u << SOM_SB_B)));
  CHECK(som_bind_used_mask(SOM_BM_RING) & (1u << SOM_SB_Y));
  /* Thor mapping: south = SNES B (bit 0), west = Y (1), east = A (8), north = X (9) */
  CHECK(som_bind_game_mask(SOM_PLAT_THOR, IN(SOM_IN_A)) == 1u << 0);
  CHECK(som_bind_game_mask(SOM_PLAT_THOR, IN(SOM_IN_X)) == 1u << 1);
  CHECK(som_bind_game_mask(SOM_PLAT_THOR, IN(SOM_IN_B)) == 1u << 8);
  CHECK(som_bind_game_mask(SOM_PLAT_THOR, IN(SOM_IN_Y)) == 1u << 9);
  CHECK(som_bind_game_mask(SOM_PLAT_3DS_NEW, IN(SOM_IN_A)) == 1u << 8);
  CHECK(som_bind_game_mask(SOM_PLAT_3DS_NEW, IN(SOM_IN_B)) == 1u << 0);
  CHECK(som_bind_game_mask(SOM_PLAT_3DS_NEW, IN(SOM_IN_L1) | IN(SOM_IN_R1)) == ((1u << 10) | (1u << 11)));
  /* inputs the game never reads are never canonical */
  for (int p = 0; p < SOM_PLAT_COUNT; p++)
    for (int i = SOM_IN_L2; i < SOM_IN_COUNT; i++) {
      int free_in = i == SOM_IN_L2 || i == SOM_IN_R2 || i == SOM_IN_C || i == SOM_IN_Z || i == SOM_IN_THUMBL || i == SOM_IN_THUMBR || i == SOM_IN_MODE || i >= SOM_IN_LS_UP;
      if (free_in) { CHECK(!som_bind_canonical(p, i)); CHECK(som_bind_game_bit(p, i) < 0 || (i >= SOM_IN_LS_UP && i <= SOM_IN_LS_RIGHT)); }
    }
  CHECK(!som_bind_canonical(SOM_PLAT_THOR, SOM_IN_L1) && !som_bind_canonical(SOM_PLAT_THOR, SOM_IN_R1));
  CHECK(som_bind_canonical(SOM_PLAT_THOR, SOM_IN_A) && som_bind_canonical(SOM_PLAT_THOR, SOM_IN_START) && som_bind_canonical(SOM_PLAT_THOR, SOM_IN_DUP));
  CHECK(som_bind_swallow_ok(SOM_PLAT_THOR, SOM_IN_A, SOM_BM_FIELD) && !som_bind_swallow_ok(SOM_PLAT_THOR, SOM_IN_A, SOM_BM_RING));
  CHECK(som_bind_swallow_ok(SOM_PLAT_THOR, SOM_IN_L1, SOM_BM_RING));
}

static void test_store(void) {
  char buf[4096], lab[96];
  fresh(SOM_PLAT_THOR);
  som_bind_defaults(&C);
  CHECK(C.n == 4);
  CHECK(som_bind_find(&C, 0, SOM_IN_L1, SOM_PRESS_SHORT) >= 0 && som_bind_find(&C, 0, SOM_IN_R2, SOM_PRESS_SHORT) >= 0);
  fresh(SOM_PLAT_3DS_OLD);
  som_bind_defaults(&C);
  CHECK(C.n == 2);
  fresh(SOM_PLAT_THOR);
  struct som_bind a = mk(0, SOM_IN_THUMBL, 0, SOM_BK_ITEM, 0, 0, 0, SOM_POL_LOWEST_ALLY), b = mk(SOM_IN_R1, SOM_IN_X, 1, SOM_BK_CAST, 2, 0, 0, SOM_POL_NEAREST_ENEMY);
  CHECK(som_bind_add(&C, &a) == 0 && som_bind_add(&C, &b) == 1);
  struct som_bind a2 = a; a2.a = 4;
  CHECK(som_bind_add(&C, &a2) == 0 && C.n == 2 && C.b[0].a == 4);               /* same trigger replaces */
  struct som_bind lg = a2; lg.press = 1;
  CHECK(som_bind_add(&C, &lg) == 2);                                              /* short and long are separate */
  struct som_bind bad = mk(0, 0, 0, SOM_BK_ITEM, 0, 0, 0, 0);
  CHECK(som_bind_add(&C, &bad) < 0);
  bad = mk(SOM_IN_X, SOM_IN_X, 0, SOM_BK_ITEM, 0, 0, 0, 0);
  CHECK(som_bind_add(&C, &bad) < 0);
  bad = mk(0, SOM_IN_X, 0, 99, 0, 0, 0, 0);
  CHECK(som_bind_add(&C, &bad) < 0);
  size_t n = som_bind_serialize(&C, buf, sizeof buf);
  CHECK(n > 0);
  struct som_bind_ctx D; som_bind_init(&D, SOM_PLAT_THOR);
  CHECK(som_bind_parse(&D, buf) == 3);
  CHECK(D.n == C.n && !memcmp(D.b, C.b, sizeof(struct som_bind) * (size_t)C.n));
  { char pad[sizeof buf + 16]; snprintf(pad, sizeof pad, "%s    ", buf); struct som_bind_ctx E2; som_bind_init(&E2, SOM_PLAT_THOR); CHECK(som_bind_parse(&E2, pad) == 3); }   /* trailing blanks from the prefs XML */
  CHECK(som_bind_parse(&D, "1 2 3\n") == -1 && D.n == 3);                         /* a bad text leaves the store untouched */
  CHECK(som_bind_parse(&D, "0 5 0 99 0 0 0 0\n") == -1);
  CHECK(som_bind_parse(&D, "") == 0 && D.n == 0);
  som_bind_action_label(&b, lab, sizeof lab);
  CHECK(!strcmp(lab, "CAST SPRITE GNOME 1 [NEAREST ENEMY]"));
  som_bind_trigger_label(SOM_PLAT_THOR, &b, lab, sizeof lab);
  CHECK(!strcmp(lab, "R1+X (LONG)"));
  som_bind_trigger_label(SOM_PLAT_3DS_NEW, &lg, lab, sizeof lab);
  CHECK(!strcmp(lab, "L3 (LONG)"));
  som_bind_badge(SOM_PLAT_THOR, &b, lab, sizeof lab);
  CHECK(!strcmp(lab, "R+X."));
  struct som_bind z = mk(0, SOM_IN_R2, 1, SOM_BK_EQUIP, 1, 0, 0, 0);
  som_bind_badge(SOM_PLAT_3DS_NEW, &z, lab, sizeof lab);
  CHECK(!strcmp(lab, "ZR."));
  CHECK(som_bind_find_entry(&C, SOM_BK_CAST, 2, 0, 0) == 1 && som_bind_find_entry(&C, SOM_BK_CAST, 2, 0, 1) < 0);
  som_bind_remove(&C, 0);
  CHECK(C.n == 2 && C.b[0].key == SOM_IN_X);
  for (int i = 0; i < SOM_BIND_MAX + 8; i++) { struct som_bind f = mk(0, SOM_IN_DUP + (i % 8), i / 8 % 2, SOM_BK_EQUIP, i, 0, 0, 0); (void)f; }
  som_bind_clear(&C);
  CHECK(C.n == 0);
  int added = 0;
  for (int m = 0; m < SOM_IN_COUNT; m++) for (int k = 1; k < SOM_IN_COUNT; k++) for (int pr = 0; pr < 2; pr++) {
    struct som_bind f = mk(m, k, pr, SOM_BK_EQUIP, 1, 0, 0, 0);
    if (som_bind_add(&C, &f) >= 0) added++;
  }
  CHECK(added == SOM_BIND_MAX && C.n == SOM_BIND_MAX);
}

static void test_engine(void) {
  /* free input: fires on press, never swallowed from the game map (it does not drive any game button other than L/R) */
  fresh(SOM_PLAT_THOR);
  struct som_bind r2 = mk(0, SOM_IN_R2, 0, SOM_BK_CYCLE_SPIRIT, 1, 0, 0, 0);
  som_bind_add(&C, &r2);
  step(0, 10, SOM_BM_FIELD); CHECK(O.nfire == 0);
  step(IN(SOM_IN_R2), 10, SOM_BM_FIELD); CHECK(O.nfire == 1 && O.fire[0] == 0 && (O.swallow & IN(SOM_IN_R2)));
  step(IN(SOM_IN_R2), 600, SOM_BM_FIELD); CHECK(O.nfire == 0 && (O.swallow & IN(SOM_IN_R2)));   /* held: no repeat */
  step(0, 10, SOM_BM_FIELD); CHECK(O.nfire == 0 && !O.swallow);
  step(IN(SOM_IN_R2), 10, SOM_BM_RING); CHECK(O.nfire == 1);                                      /* free inputs fire in every mode */

  /* a game button: only in free play; elsewhere it goes to the game */
  fresh(SOM_PLAT_THOR);
  struct som_bind xb = mk(0, SOM_IN_X, 0, SOM_BK_EQUIP, 0, 0, 0, 0);
  som_bind_add(&C, &xb);
  step(IN(SOM_IN_X), 10, SOM_BM_RING); CHECK(O.nfire == 0 && !O.swallow);
  step(0, 10, SOM_BM_RING);
  step(IN(SOM_IN_X), 10, SOM_BM_FIELD); CHECK(O.nfire == 1 && (O.swallow & IN(SOM_IN_X)));
  step(0, 10, SOM_BM_FIELD); CHECK(!O.swallow);
  /* entering the ring while it is held does not turn an already-swallowed press into a game press */
  step(IN(SOM_IN_X), 10, SOM_BM_FIELD); step(IN(SOM_IN_X), 10, SOM_BM_RING); CHECK(O.swallow & IN(SOM_IN_X));
  step(0, 10, SOM_BM_RING);

  /* chord: the modifier passes through as it is (free), the key fires the chord and is swallowed */
  fresh(SOM_PLAT_THOR);
  struct som_bind ch = mk(SOM_IN_R1, SOM_IN_X, 0, SOM_BK_CAST, 2, 0, 0, 1), solo = mk(0, SOM_IN_R1, 0, SOM_BK_CYCLE_WEAPON, 1, 0, 0, 0);
  som_bind_add(&C, &ch); som_bind_add(&C, &solo);
  step(IN(SOM_IN_R1), 10, SOM_BM_FIELD); CHECK(O.nfire == 0 && (O.swallow & IN(SOM_IN_R1)));       /* deferred: it is a modifier */
  step(IN(SOM_IN_R1) | IN(SOM_IN_X), 50, SOM_BM_FIELD); CHECK(O.nfire == 1 && O.fire[0] == 0 && (O.swallow & IN(SOM_IN_X)));
  step(IN(SOM_IN_R1), 50, SOM_BM_FIELD); CHECK(O.nfire == 0);
  step(0, 50, SOM_BM_FIELD); CHECK(O.nfire == 0);                                                  /* chord used: the solo does not fire */
  step(IN(SOM_IN_R1), 50, SOM_BM_FIELD); step(0, 50, SOM_BM_FIELD); CHECK(O.nfire == 1 && O.fire[0] == 1);   /* solo tap fires on release */
  step(IN(SOM_IN_X), 50, SOM_BM_FIELD); CHECK(O.nfire == 0);                                      /* X alone has no binding */
  step(0, 10, SOM_BM_FIELD);
  /* same-frame chord (3DS polls whole frames): both orders work */
  step(IN(SOM_IN_R1) | IN(SOM_IN_X), 16, SOM_BM_FIELD); CHECK(O.nfire == 1 && O.fire[0] == 0);
  step(0, 16, SOM_BM_FIELD); CHECK(O.nfire == 0);

  /* short vs long on one input */
  fresh(SOM_PLAT_3DS_NEW);
  struct som_bind sh = mk(0, SOM_IN_L2, 0, SOM_BK_CYCLE_SPIRIT, -1, 0, 0, 0), lg = mk(0, SOM_IN_L2, 1, SOM_BK_CYCLE_SPIRIT, 1, 0, 0, 0);
  som_bind_add(&C, &sh); som_bind_add(&C, &lg);
  step(IN(SOM_IN_L2), 10, SOM_BM_FIELD); CHECK(O.nfire == 0);
  step(IN(SOM_IN_L2), 100, SOM_BM_FIELD); CHECK(O.nfire == 0);
  step(0, 10, SOM_BM_FIELD); CHECK(O.nfire == 1 && O.fire[0] == 0);                                  /* tap: short on release */
  step(IN(SOM_IN_L2), 10, SOM_BM_FIELD);
  step(IN(SOM_IN_L2), SOM_BIND_LONG_MS - 5, SOM_BM_FIELD); CHECK(O.nfire == 0);
  step(IN(SOM_IN_L2), 10, SOM_BM_FIELD); CHECK(O.nfire == 1 && O.fire[0] == 1);                      /* held: long at the threshold */
  step(IN(SOM_IN_L2), 900, SOM_BM_FIELD); CHECK(O.nfire == 0);
  step(0, 10, SOM_BM_FIELD); CHECK(O.nfire == 0);                                                     /* and no short after it */

  /* a long-only binding leaves the short press unbound */
  fresh(SOM_PLAT_3DS_NEW);
  som_bind_add(&C, &lg);
  step(IN(SOM_IN_L2), 10, SOM_BM_FIELD); step(0, 100, SOM_BM_FIELD); CHECK(O.nfire == 0);

  /* long chord */
  fresh(SOM_PLAT_THOR);
  struct som_bind lc = mk(SOM_IN_L1, SOM_IN_THUMBR, 1, SOM_BK_ITEM, 4, 0, 0, 3);
  som_bind_add(&C, &lc);
  step(IN(SOM_IN_L1), 10, SOM_BM_FIELD); step(IN(SOM_IN_L1) | IN(SOM_IN_THUMBR), 10, SOM_BM_FIELD); CHECK(O.nfire == 0);
  step(IN(SOM_IN_L1) | IN(SOM_IN_THUMBR), SOM_BIND_LONG_MS + 1, SOM_BM_FIELD); CHECK(O.nfire == 1);
  step(0, 10, SOM_BM_FIELD);

  /* sticks as buttons and the Thor extras behave like any free input */
  fresh(SOM_PLAT_THOR);
  const int extras[] = {SOM_IN_C, SOM_IN_Z, SOM_IN_THUMBL, SOM_IN_THUMBR, SOM_IN_MODE, SOM_IN_L2, SOM_IN_LS_UP, SOM_IN_LS_RIGHT, SOM_IN_RS_DOWN, SOM_IN_RS_LEFT};
  for (size_t i = 0; i < sizeof extras / sizeof *extras; i++) { struct som_bind e = mk(0, extras[i], 0, SOM_BK_EQUIP, 1, 0, 0, 0); som_bind_add(&C, &e); }
  for (size_t i = 0; i < sizeof extras / sizeof *extras; i++) {
    step(IN(extras[i]), 10, SOM_BM_EVENT); CHECK(O.nfire == 1);
    step(0, 10, SOM_BM_EVENT);
  }
}

static void test_capture(void) {
  fresh(SOM_PLAT_THOR);
  step(IN(SOM_IN_START), 10, SOM_BM_FIELD);        /* held when the capture starts: ignored, hidden until released */
  som_bind_capture_begin(&C, T);
  CHECK(som_bind_capturing(&C));
  step(IN(SOM_IN_START), 10, SOM_BM_FIELD); CHECK(O.event == 0 && (O.swallow & IN(SOM_IN_START)));
  step(0, 10, SOM_BM_FIELD); CHECK(O.event == 0);
  /* free key, short */
  step(IN(SOM_IN_R2), 10, SOM_BM_FIELD); CHECK(O.event == 0 && (O.swallow & IN(SOM_IN_R2)));
  step(0, 100, SOM_BM_FIELD); CHECK(O.event == SOM_BE_CAPTURE_DONE && O.trigger.key == SOM_IN_R2 && O.trigger.mod == 0 && O.trigger.press == SOM_PRESS_SHORT);
  CHECK(!som_bind_capturing(&C));
  /* free key, long (still held) */
  som_bind_capture_begin(&C, T);
  step(IN(SOM_IN_THUMBL), 10, SOM_BM_FIELD); step(IN(SOM_IN_THUMBL), SOM_BIND_LONG_MS + 5, SOM_BM_FIELD);
  CHECK(O.event == SOM_BE_CAPTURE_DONE && O.trigger.key == SOM_IN_THUMBL && O.trigger.press == SOM_PRESS_LONG);
  step(IN(SOM_IN_THUMBL), 10, SOM_BM_FIELD); CHECK(O.swallow & IN(SOM_IN_THUMBL));   /* still down after the capture: hidden */
  step(0, 10, SOM_BM_FIELD); CHECK(!O.swallow);
  /* chord */
  som_bind_capture_begin(&C, T);
  step(IN(SOM_IN_L1), 10, SOM_BM_FIELD); step(IN(SOM_IN_L1) | IN(SOM_IN_C), 30, SOM_BM_FIELD); CHECK(O.event == 0);
  step(IN(SOM_IN_L1), 30, SOM_BM_FIELD); CHECK(O.event == SOM_BE_CAPTURE_DONE && O.trigger.mod == SOM_IN_L1 && O.trigger.key == SOM_IN_C && O.trigger.press == SOM_PRESS_SHORT);
  step(0, 10, SOM_BM_FIELD);
  /* a game button needs the same press twice */
  som_bind_capture_begin(&C, T);
  step(IN(SOM_IN_X), 10, SOM_BM_FIELD); step(0, 50, SOM_BM_FIELD); CHECK(O.event == SOM_BE_CAPTURE_CONFIRM && som_bind_capturing(&C) && O.trigger.key == SOM_IN_X);
  step(IN(SOM_IN_Y), 10, SOM_BM_FIELD); step(0, 50, SOM_BM_FIELD); CHECK(O.event == SOM_BE_CAPTURE_CONFIRM && O.trigger.key == SOM_IN_Y);   /* another key: asks again */
  step(IN(SOM_IN_Y), 10, SOM_BM_FIELD); step(0, 50, SOM_BM_FIELD); CHECK(O.event == SOM_BE_CAPTURE_DONE && O.trigger.key == SOM_IN_Y);
  /* the first press of a canonical chord modifier also needs confirmation */
  som_bind_capture_begin(&C, T);
  step(IN(SOM_IN_START), 10, SOM_BM_FIELD); step(IN(SOM_IN_START) | IN(SOM_IN_R1), 10, SOM_BM_FIELD); step(IN(SOM_IN_START), 50, SOM_BM_FIELD);
  CHECK(O.event == SOM_BE_CAPTURE_CONFIRM);
  /* cancel and timeout */
  som_bind_capture_cancel(&C); CHECK(!som_bind_capturing(&C));
  som_bind_capture_begin(&C, T);
  step(0, SOM_BIND_CAPTURE_MS + 1, SOM_BM_FIELD); CHECK(O.event == SOM_BE_CAPTURE_CANCEL && !som_bind_capturing(&C));
  /* 3DS whole-frame chord: both pressed in the same frame, the lower id is the modifier */
  fresh(SOM_PLAT_3DS_NEW);
  som_bind_capture_begin(&C, T);
  step(IN(SOM_IN_L1) | IN(SOM_IN_L2), 16, SOM_BM_FIELD); step(IN(SOM_IN_L1), 16, SOM_BM_FIELD);
  CHECK(O.event == SOM_BE_CAPTURE_DONE && O.trigger.mod == SOM_IN_L1 && O.trigger.key == SOM_IN_L2);
}

static void test_analog(void) {
  CHECK(som_bind_stick_dirs(0, 0, 0) == 0);
  CHECK(som_bind_stick_dirs(0, 0.5f, 0) == 0 && som_bind_stick_dirs(0, 0.6f, 0) == 8);
  CHECK(som_bind_stick_dirs(8, 0.4f, 0) == 8 && som_bind_stick_dirs(8, 0.3f, 0) == 0);      /* hysteresis */
  CHECK(som_bind_stick_dirs(0, -0.7f, -0.7f) == (4 | 1));
  CHECK(som_bind_stick_dirs(0, 0, 1.0f) == 2);
  CHECK(som_bind_trigger(0, 0.5f) == 0 && som_bind_trigger(0, 0.7f) == 1 && som_bind_trigger(1, 0.5f) == 1 && som_bind_trigger(1, 0.3f) == 0);
}

/* Random input streams: whatever the bindings, the engine keeps its invariants. */
static unsigned rnd(unsigned *s) { *s = *s * 1103515245u + 12345u; return (*s >> 16) & 0x7FFF; }
static void test_fuzz(void) {
  unsigned seed = 12345;
#define RND() rnd(&seed)
  for (int round = 0; round < 400; round++) {
    int plat = round % SOM_PLAT_COUNT;
    fresh(plat);
    uint32_t have = som_bind_platform_inputs(plat);
    int nb = 1 + (int)(RND() % 10);
    for (int i = 0; i < nb; i++) {
      int key = 1 + (int)(RND() % (SOM_IN_COUNT - 1)), mod = RND() % 3 ? 0 : 1 + (int)(RND() % (SOM_IN_COUNT - 1));
      if (!(have & IN(key)) || (mod && !(have & IN(mod)))) continue;
      struct som_bind b = mk(mod, key, (int)(RND() % 2), 1 + (int)(RND() % (SOM_BK_COUNT - 1)), (int)(RND() % 8), 0, 0, (int)(RND() % SOM_POL_COUNT));
      som_bind_add(&C, &b);
    }
    uint32_t held = 0;
    int caps = 0;
    for (int s = 0; s < 300; s++) {
      if (RND() % 4 == 0) { int k = 1 + (int)(RND() % (SOM_IN_COUNT - 1)); if (have & IN(k)) held ^= IN(k); }
      if (!caps && RND() % 40 == 0) { som_bind_capture_begin(&C, T); caps = 1; }
      int mode = (int)(RND() % SOM_BM_COUNT);
      step(held, 1 + RND() % 200, mode);
      CHECK((O.swallow & ~held) == 0);                           /* only held inputs are hidden */
      CHECK(O.nfire >= 0 && O.nfire <= SOM_BIND_FIRES);
      for (int f = 0; f < O.nfire; f++) CHECK(O.fire[f] >= 0 && O.fire[f] < C.n);
      if (som_bind_capturing(&C)) CHECK(O.nfire == 0);          /* nothing fires while learning */
      else caps = 0;
      /* an unbound input that is not a game button is never hidden */
      for (int i = 1; i < SOM_IN_COUNT; i++) {
        if (!(O.swallow & IN(i)) || som_bind_capturing(&C)) continue;
        int bound = 0;
        for (int j = 0; j < C.n; j++) if (C.b[j].key == i || C.b[j].mod == i) bound = 1;
        CHECK(bound || (C.swallowed & IN(i)));
      }
    }
  }
}

static void test_hotkey_helpers(void) {
  struct somd_targets t;
  struct somd_target out;
  memset(&t, 0, sizeof t);
  for (int i = 0; i < 6; i++) { t.actor[i].slot = (uint8_t)i; t.actor[i].present = 1; t.actor[i].valid = 1; t.actor[i].alive = 1; }
  t.actor[0].sx = 100; t.actor[0].sy = 100; t.actor[0].hp = 90; t.actor[0].hp_max = 100;
  t.actor[1].hp = 20; t.actor[1].hp_max = 100; t.actor[2].hp = 100; t.actor[2].hp_max = 100;
  t.actor[3].sx = 200; t.actor[3].sy = 100; t.actor[4].sx = 120; t.actor[4].sy = 110; t.actor[5].sx = 10; t.actor[5].sy = 10;
  t.cls = SOMD_C_ENEMY_ONE_OR_ALL; t.n_enemies_valid = 3; t.n_allies_valid = 3;
  CHECK(som_hk_pick_target(&t, SOMD_C_ENEMY_ONE_OR_ALL, 0, SOM_POL_NEAREST_ENEMY, &out) && out.kind == SOMD_T_ONE && out.slot == 4);
  CHECK(som_hk_pick_target(&t, SOMD_C_ENEMY_ONE_OR_ALL, 0, SOM_POL_ALL_ENEMIES, &out) && out.kind == SOMD_T_ALL_ENEMIES);
  CHECK(som_hk_pick_target(&t, SOMD_C_ENEMY_ONE, 0, SOM_POL_ALL_ENEMIES, &out) && out.kind == SOMD_T_ONE && out.slot == 4);   /* single-target spell degrades */
  t.actor[4].valid = 0;
  CHECK(som_hk_pick_target(&t, SOMD_C_ENEMY_ONE, 0, SOM_POL_NEAREST_ENEMY, &out) && out.slot == 3);
  t.actor[3].valid = t.actor[5].valid = 0;
  CHECK(!som_hk_pick_target(&t, SOMD_C_ENEMY_ONE, 0, SOM_POL_NEAREST_ENEMY, &out));
  t.cls = SOMD_C_ALLY_ONE_OR_ALL;
  CHECK(som_hk_pick_target(&t, SOMD_C_ALLY_ONE_OR_ALL, 0, SOM_POL_LOWEST_ALLY, &out) && out.kind == SOMD_T_ONE && out.slot == 1);
  CHECK(som_hk_pick_target(&t, SOMD_C_ALLY_ONE_OR_ALL, 0, SOM_POL_WHOLE_PARTY, &out) && out.kind == SOMD_T_ALL_ALLIES);
  CHECK(som_hk_pick_target(&t, SOMD_C_ALLY_ONE_OR_ALL, 2, SOM_POL_SELF, &out) && out.slot == 2);
  t.actor[1].valid = 0;
  CHECK(som_hk_pick_target(&t, SOMD_C_ALLY_ONE_OR_ALL, 0, SOM_POL_LOWEST_ALLY, &out) && out.slot == 0);

  struct som_state st;
  memset(&st, 0, sizeof st);
  memset(st.weapon_slot, 0xFF, sizeof st.weapon_slot);
  st.weapon_slot[1] = 3; st.weapon_slot[4] = 2; st.weapon_slot[7] = 1;
  st.hero[0].weapon_type = 1;
  CHECK(som_hk_next_weapon(&st, 0, 1) == 4 && som_hk_next_weapon(&st, 0, -1) == 7);
  st.hero[0].weapon_type = 7;
  CHECK(som_hk_next_weapon(&st, 0, 1) == 1);
  memset(st.weapon_slot, 0xFF, sizeof st.weapon_slot); st.weapon_slot[7] = 1;
  CHECK(som_hk_next_weapon(&st, 0, 1) == -1);
  st.item[3].id = 4; st.item[3].qty = 2; st.item[5].id = 4; st.item[5].qty = 0;
  CHECK(som_hk_item_record(&st, 4) == 3 && som_hk_item_record(&st, 0) == -1);

  int h, sp, sl;
  for (int id = 0; id < 42; id++) { som_hk_spell_entry(id, &h, &sp, &sl); CHECK(som_hk_spell_id(h, sp, sl) == id); }
  som_hk_spell_entry(3, &h, &sp, &sl); CHECK(h == 1 && sp == 0 && sl == 0);   /* girl, Gnome, slot 0 */
  som_hk_spell_entry(0, &h, &sp, &sl); CHECK(h == 2 && sp == 0 && sl == 0);   /* sprite */
}

int main(void) {
  test_tables(); test_store(); test_engine(); test_capture(); test_analog(); test_fuzz(); test_hotkey_helpers();
  printf("som_bind: %d checks passed\n", checks);
  return 0;
}
