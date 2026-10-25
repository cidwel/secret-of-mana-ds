# QOL options in som_gameplay (T11), 2026-10-20 (round 2)

Twelve options in the group "Quality of life" of `tools/gameplay/gameplay_menu.json` (all default off), implemented in
`tools/gameplay/som_gameplay.py` like the T8 options: records with expected bytes applied to a COPY of the ROM, routines in
the free area `DC:0540`, `--on-conflict skip` drops an option whose bytes were changed by a hack. Each option is also in the C port
(`tools/companion/gameplay_c/`, used by the 3DS and Thor builds) with a byte-for-byte parity test. Survey and credits:
`docs/qol-patches-survey-20261020.md`, `data/credits.json`.

| Option | Values | Sites (US v1.0 offsets) | Behaviour |
|---|---|---|---|
| `qol_chest_fast` | bool | `D1:5FB8`, `D1:ED01`, `D1:EC84` -> `FF` | the three chest-opening animation scripts end at once |
| `qol_unready_damage` | bool | `C0:5383` `LSR $89` -> NOP (signature search; Magno `+0x17`) | a swing with the gauge not empty keeps twice the damage (vanilla halves it and then scales by the gauge ratio) |
| `qol_fast_transitions` | bool | `C0:8B76` `LDA $E6` -> `LDA $E7` | the fade routine jumps to the target brightness |
| `qol_fast_fades` | bool | `C0:8B71` `LDA $E5 ; STA $E4` -> `JSL` (halves the step timer, never 0) | every fade runs twice as fast |
| `qol_item_limit` | 4, 7 | `C0:6559` `CMP #$9F` -> NOP NOP | stack of one consumable up to 7; the `BCS` now sees the carry of the add |
| `qol_levelup_mp_restore` | bool | `C0:44D3` (signature search) -> `JSL levelup`; skipped when the ROM already has the randomizer hook (same routine bytes) | a level-up refills MP |
| `qol_dead_exp` | bool | `C0:446B` `BEQ` -> NOP and a `JSL` at the end of the level-up step (`C0:44DB`) that puts a hero with the dead flag back to 0 HP | a hero at 0 HP gets experience and levels up but stays at 0 HP; independent of the MP hook, so it composes with the randomizer hook |
| `qol_magic_level_rate` | 1, 2, 3 | `D0:4DEC` -> `JSL magic_rate`; skipped when the randomizer x4 hook is there | spell progress per cast times 1/2/3 |
| `qol_scroll_deadzone` | vanilla, tight, tighter | `C0:D903 D90A D952 D959` | camera window: x 56-199 y 72-175 (vanilla), x 80-175 y 80-159, x 96-159 y 96-143 |
| `qol_ring_speed` | 1, 2 | ring code `C0:6BF8..6D00`: rotation step `$1824` 0x12 -> 0x24 (3 literals) and the open/close step on `$181E` +-10 -> +-20 | the ring turns and opens twice as fast; the rest layout is identical. x3 is not offered: its step does not divide the layout and the final positions break |
| `qol_message_speed` | 1, 2, 4 | `C0:5CF4` (signature search) lifetime of the top-of-screen message records 0x60 -> 0x30 / 0x18 | level-up, item-found, weapon and spell level notices stay 1/2 or 1/4 as long. Magno rewrote the builder (reported as a conflict); the randomizer's `fasterMessages` already uses 0x30 |
| `qol_shop_fill` | bool | buy shop `C0:7AF2` (`BNE fail ; JSR $7D0D`, signature search, also found on Magno) -> `JSL shopfill` | buying a consumable (ids `$BA..$C5`) keeps buying the same item while the stack has room and the money covers it, one confirmation instead of four |

`gameplay_menu.json` stays backward compatible: the T8 options and their order are untouched, `groups` has a fourth entry, the preset list ends with
`qol` (it now also sets ring speed 2, message speed 2 and shop fill). New fields (ignored by older consumers): per option `conflicts` (`{other option: relation}`),
`randomizer_already` (randomizer settings that already apply the same behaviour), top level `equivalents` (community patch name -> existing T8 option
values). `apply.integer_enums` also lists `qol_item_limit`, `qol_magic_level_rate`, `qol_ring_speed` and `qol_message_speed`. Bool options use `yes`/`no`,
as the T8 booleans. The option count of the file is 25 (13 T8 + 12 QOL); `tools/gameplay/gameplay_options.json` lists every default.

## Verification (`tools/gameplay/qol_verify.py`, headless snes9x libretro core, vanilla US v1.0)

```
SOM_ROM=vanilla.sfc SOM_QOL_EXTRA_ROMS=<Magno>:mode_rando:mode_ancientcave:mode_chaos:mode_open:mode_bossrush \
  python3 tools/gameplay/qol_verify.py --report out.json
```
Takes about 10 minutes; ROM copies and traces go to `~/.local/state/som-decomp/qol-20261020/`, nothing is written in the repository.
Result of the last run: **42 checks, all pass**.

| Check | Result |
|---|---|
| Differential, 9,000 frames of `tools/som_input_newgame.txt`, WRAM SHA-1 every 50 frames (180 checkpoints) | chest, unready, item limit, MP, dead exp, magic x2/x3, the combination of them, ring speed, message speed x2/x4, dead exp + MP and shop fill: **bit identical to vanilla** for the whole route (none of those behaviours occurs in the prologue). fades/transitions first diverge at frame 1,100, deadzone tight at 1,700 and tighter at 1,650, as expected |
| Chest hero pose (event command `ACTOR_ANIM`, pose 0x9B, script `D1:5FB8`) | vanilla 142 frames away from the idle animation, patched **1**. The two chest-object scripts (`D1:EC84`, `D1:ED01`) could not be driven (a dropped chest could not be opened by walking, B or A, and event `1F 0C` did nothing): those two bytes stay verified by bytes only |
| Spell progress in a real cast (girl, ring menu: Y, X, UP to the magic ring, B, B, B; story cap `$CFFC` = 8; spirits 0-4) | gain per cast vanilla 8 7 6 5 4; x2 16 14 12 10 8; x3 24 21 18 15 12 |
| Ring speed (ring menu opened with Y, then LEFT/RIGHT) | opening 45 -> 35 frames, a turn 12.8 -> 7.8 frames; the rest layout of every owner and category is identical to vanilla, the selection held during a turn is the same |
| Message time (3 heroes level up on 2,000 experience, frames until the banner leaves the screen) | vanilla 750; half 494; quarter 365 |
| Shop fill (event `2E 00 00`, empty inventory, buy Candy, price 10) | money 3: nothing bought in either; money 25: vanilla 1, patched 2 (money 5); money 100,000: vanilla 1, patched 4 (limit 4) or 7 (limit 7), money drops by 10 each; money 65,541 (borrow across the low word): same counts |
| Fades (frames with current != target brightness, first five fades of the route) | vanilla 18 105 103 90 116; `qol_fast_fades` 18 77 75 62 88; `qol_fast_transitions` 4 35 33 20 46 |
| Deadzone (hero screen x after walking right / left until the camera scrolls) | vanilla 198 / 56; tight 174 / 80; tighter 158 / 96 |
| Unready damage (real hit resolver, 10 hits) | ready hits (gauge 0): identical; gauge 40: vanilla 2 (6 on a critical), patched 4 (12) |
| Level-up MP (monster reward 2,000 experience, MP set to 0 first) | vanilla MP 0/0 after the level-ups; patched 10/10 = max MP; HP and max HP identical |
| Dead hero experience (girl at 0 HP) | vanilla girl experience 0, max HP 45; patched experience 2,000, max HP 104, HP stays 0 |
| Item limit (nine Candy through the game's own event opcode `1E`) | vanilla stack stops at 4; patched 5, 6, 7, then "full" |
| Spell progress routine `D0:4DCD` on the project's 65816 interpreter, levels 0-7 | vanilla 9 8 7 6 5 4 3 2; x2 exactly double; x3 exactly triple |
| Randomizer composition (Rando, Ancient Cave, Chaos, Open World, Boss Rush) | the `mpAtLevel` and `fasterMagicLevel` hooks are recognised by the routine they jump to (not by their address); `qol_dead_exp` applies on top (it does not use the MP hook) with nothing skipped; the interpreter's spell progress on the patched ROM is exactly 4x vanilla (36 32 ... 8) |
| Other ROMs (Magno, Ancient Cave, Chaos, Boss Rush, Open World, Rando), all options on, conflicts skipped | all boot; first divergence at the first screen fade (1,100-1,200) |

Not frame-verified: the two chest-object scripts (see above), and the shop fill on Magno (the site and the failure target are found by
signature and the unit test applies it, but the snes9x fixtures are vanilla only).

Tests:
* `python3 -m pytest tools/gameplay` (31 tests; the ROM tests need `SOM_ROM`, `SOM_ROM_MAGNO`);
* `tools/companion/gameplay_c/test_gameplay_c.py` (C port used by PC, 3DS and the launcher): 488 outputs byte-identical on vanilla, Magno, Rando and Boss Rush, with and without a copier header;
* `tools/companion/android/tests/gameplay_parity.py` (the separate C port `tools/companion/som_gameplay_apply.c` of the Thor app): 250 comparisons, 0 differences;
* `tools/companion/pc/test_gpc_parity.py`: 197 identical; `tools/companion/n3ds/test/run_n3ds_tests.sh`: ok.
