# Randomizer C port (libsomrando) - progress, phase 1 (2026-10-18)

Board item: T9. Library: `tools/companion/randomizer/libsomrando/` (LGPL-2.1+, see `NOTICE`, `COPYING.LIB`).
Oracle: the C# generator v1.49 (`tools/companion/cli/som-dualscreen`). Contract: same ROM + config + seed gives a byte-identical ROM, or
`SOMRANDO_ERR_UNSUPPORTED` (5) with a message and no output file. Never a wrong ROM.

## Status in one line

Vanilla-randomizer ("rando") mode is byte-identical to the oracle for a defined option subset (below). **0 of the 17 menu presets were
covered at the end of phase 1** (7 of 17 after phase 2, 12 of 17 after phase 3, 17 of 17 after phase 4, see the sections below). Estimated overall port after phase 1: about 25 %.

## What is byte-identical today

Mode `rando` only, with every one of these (anything else is refused by preflight):

* Always-on common hacks ported: TitleScreen, NoSwordPull, HexasFix, PlayerAi default, Vineshroom, HighLevelEnemyNameFix,
  NumbersOnScreen, DamageCancelFix (all variants), InitialValues (empty), BossZPosition, InvisClearsSlow, ElementSwaps, OrbRewardEventGenerator
  (rOrbs any), ExperienceAdjust, GoldAdjust (rExp / rGold any), FirePalaceDoorFix, NamesOfThings, EventExpander.
* Toggle hacks ported, each verified with the toggle on and off (40 hacks, 34 mechanically transpiled by `tools/transpile_simple.py` and
  6 by hand): faster magic/weapon levels, faster top-of-screen dialogue, levelup MP restore, chest speedup, status lengths, sound effects
  (both), max items 7, fast bosses, hittable vampire, starter gear, boss death fix, enemy type damage, fast transitions, mode 7 fix,
  accurate percentages, no energy run/attack, gigases, whip fix, mana magic fix, OHKO, summon despawn fix, noclip, confuse fix, beak
  shield, mech rider death fix, always poisoned, percentage poison, obscure damage/HP/gold, evades as zero, name entry, enemy infinite MP,
  cup of wishes on 0 HP, candy healouts, magic rope death fix.
* Also identical: raceMode (functional stream reseed), unicode/empty seeds, cosmetic stream `seed_cosmetic`.
* Not yet (refused when set to the non-default value): rWeapons, magicRebalance, rainbowRabites, characterColors, leavePlayersBehind,
  funSpeedup, statusGlow, elemDamage, offscreenTarget, buyMultiple, adjustEnemyPos, aggEnemies, defRefactor, bossElementRando,
  showMinimap, rDialogue (shortening), rBosses, rEnemies, rMusic/muteMusic, rAutosave, rHoliday themes, rDifficulty != 100.
* Modes ancientcave, open, chaos, bossrush: refused (`mode '...' is not ported to C yet`).

## Verification (all run, all green)

`libsomrando/run_tests.sh` prints `ALL TESTS PASSED`:

| Check | Result |
|---|---|
| System.Random / seeding vs .NET vectors (27 seeds incl. unicode, empty) | identical, x86-64, ASan/UBSan, 32-bit ARM |
| LZ77 (SomLz77) round trips of both title blocks | identical |
| Math.Pow, 10 reachable sites, 57k vectors from .NET (glibc pow) | 0 bit differences on x86-64; on ARM glibc 7 of 6161 free-form "grid" exponents differ by 1 ulp (shipped exponent table: 0) |
| `test_core` total | 87,940 checks, 0 failures (x64), same under ASan and ARM (87,938) |
| Differential harness, 522 cases (options, 16 baseline seeds, every option x legal value, 150 random supported combos, API behaviours, presets) | 274 matrix identical, 64 refused as expected, 150 random identical, 0 mismatches |
| Harness under ASan/UBSan (372 cases) and 32-bit ARM under qemu (22 cases) | 0 failures |

The matrix flips every option against the oracle: supported values must give the same sha256, unsupported ones must be refused with no file.
On mismatch the harness prints the first differing offset, the diff count and the C hack responsible (`SOMRANDO_TRACE`). It caught one real bug
in this round (hand-ported sound-effects hack had PLP and JSL swapped); fixed.

## Coverage of the 17 presets (`tools/companion/randomizer/presets.json`)

| Preset(s) | Blocked by |
|---|---|
| ancientcave, -short, -marathon, -gauntlet, -hero (5) | mode not ported (map generator) |
| open, -mtr, -long, -casual, -owl (5) | mode not ported |
| chaos, chaos-short (2); bossrush, bossrush-hard (2) | mode not ported |
| rando | rWeapons, magicRebalance, rainbowRabites, characterColors, leavePlayersBehind, funSpeedup, statusGlow, elemDamage, offscreenTarget, buyMultiple, adjustEnemyPos, showMinimap, rDialogue, rBosses, rEnemies, rMusic, rAutosave |
| rando-plus | same minus rBosses, rEnemies |
| rando-hard | same as rando plus rDifficulty=150 |

C hack coverage: of the 65 common + 15 rando hacks, 40 toggle hacks plus the always-on ones are ported; remaining stubs are listed in
the next-phase list.

## Deviations from the plan

* No empty-hack milestone: the first slice was already "rando with everything optional off", since NamesOfThings and EventExpander always run in rando.
* Rando before Ancient Cave, because rando needs far less new code (no map generator) and exercises the whole pipeline.
* Gating design: each registry row has `fn` and `requires` pairs; preflight rejects states not reproduced; `run_hack` re-checks. `somrando_supports()` was added to the API.

## Build and seam

* Android: `-DSOMRANDO_LIB=ON` compiles libsomrando into `somjni`; otherwise `somrando_stub.c` stays. `SOMRANDO_ADVERTISE` OFF means
  `somrando_available()==0` and the menu keeps showing the unavailable screen. Needs `-ffp-contract=off` (set by CMake).
* Progress callback: 83 steps (65 + 15 + 3), nonzero return cancels (`SOMRANDO_ERR_CANCELLED`, no file). Verified in the harness.
* The Vineshroom tile resource is rebuilt from the user's ROM at run time (`src/sr_rom_recipes.c`, see "ROM-free binaries and robust registration, 2026-10-20" below); it used to be generated at build time from an upstream checkout.
* Android arm64 (NDK 28, android-26) library builds; not yet run on a device.

## Not verified

* Running on the Thor / any device, timing on device, 3DS (newlib pow), Windows UCRT pow.
* Math.Pow on ARM for non-shipped exponents (7 known 1-ulp cases are informational).
* ancientcave / open / chaos / bossrush (not implemented).

## Next phase (ordered)

1. Finish rando to cover the three rando presets: WeaponRandomizer (1469 lines, uses pow 1.25), MagicRebalance, RabiteColorRandomizer,
   CharacterPaletteRandomizer, FreeWalking, Speedup, StatusGlow, ElementalDamage, FreeTargetting, BuyMultipleItems,
   EnemyPositionAdjustments, Ff6StyleMiniMap, Autosave, CustomMusic; then FasterDialogueEvents shortening (2180 lines), BossSwaps,
   EnemySwaps and EnemyScaling real paths (+ VanillaBossMaps). Also FasterEnemies, DefenseRefactor (pow), BossElementRando, Fall/Winter themes.
2. Flip `SOMRANDO_ADVERTISE` once a full preset is identical for all harness seeds; add the three rando presets to a hashes regression file.
3. Boss Rush / Chaos, then Ancient Cave (map generation, MapPieceExpander, DoorReplacer, FullMapReplacer, MapPaletteSetReplacer, CustomEventManager), then Open World.
4. Dictionary-removal semantics for ordered maps; original names for NamesOfThings.
5. Device run on Android (arm64), then 3DS (newlib pow check).

---

# Phase 2 (2026-10-18): rando, Boss Rush and Chaos presets byte-identical

Result: **7 of the 17 menu presets** (`rando`, `rando-plus`, `rando-hard`, `bossrush`, `bossrush-hard`, `chaos`, `chaos-short`) give the same
SHA-256 as the C# oracle for every seed tried. The other 10 (5 Ancient Cave, 5 Open World) are refused (`SOMRANDO_ERR_UNSUPPORTED`, no file). Library:
about 23,800 lines of C in `libsomrando/src` (phase 1: 3,700). Work was split over parallel workers by hack group, each in a private copy of the
library, merged by the coordinator; the three registry `requires` gates that remain are `defRefactor`, `bossElementRando` and `characterColors=custom`.

## Coverage of the 17 presets

| Preset | Mode | Status | Seeds identical (final run) | Notes |
|---|---|---|---:|---|
| rando | rando | **supported** | 50 (+100 in an earlier run) | all 17 rando options of phase 1 now ported, incl. boss/enemy randomizer, dialogue shortening, music, autosave |
| rando-plus | rando | **supported** | 50 (+100) | |
| rando-hard | rando | **supported** | 50 (+100) | rDifficulty=150 enemy scaling |
| bossrush | bossrush | **supported** | 50 | difficulty casual |
| bossrush-hard | bossrush | **supported** | 50 | difficulty hard (reallyhard also verified) |
| chaos | chaos | **supported** | 50 | difficulty casual |
| chaos-short | chaos | **supported** | 50 | chFloors=few chBosses=few |
| ancientcave, -short, -marathon, -gauntlet, -hero | ancientcave | refused | - | map generator not ported |
| open, -mtr, -long, -casual, -owl | open | refused | - | open-world logic not ported |

`somrando_available()` is now 1 in builds with `SOMRANDO_ADVERTISE` (CMake default ON). Availability is per request: `somrando_supports()`
answers 1 only for requests that are byte-identical to the oracle, and the menu glue enables GO only for those (below). "50 seeds" are the
regression file `libsomrando/tests/regression_hashes.json` (50 seeds per supported preset = 350 hashes, checked without dotnet by
`diff_harness.py --verify-hashes`); the live oracle comparison of the same seeds is the `preset-seeds` section (20 seeds by default, 50 in the final run).

## What was ported (all byte-identical, mode-dependent paths included)

* Common hacks: WeaponRandomizer (rando path and the non-vanilla path used by Boss Rush/Chaos, `pow` from libm matches the oracle), MagicRebalance, ElementalDamage,
  RabiteColorRandomizer, CharacterPaletteRandomizer (`none`, `rando`), StatusGlow, FreeWalking, Speedup, FreeTargetting, BuyMultipleItems (plus the InitialValues
  emission it needs), EnemyPositionAdjustments, FasterEnemies, Ff6StyleMiniMap, CustomMusic (no `music/` pack), Autosave.
* Rando-mode hacks: FasterDialogueEvents with shortening (85 events, generated table), BossSwaps (+ VanillaBossMaps data, both `rPreserve` values), EnemySwaps (all `rStatusAilments`
  values), EnemyScaling (all `rDifficulty` values), FallTheme and WinterTheme.
* Boss Rush and Chaos: both generators, their event makers, generated maps/doors/map pieces/palettes, CustomEventManager, the "non-vanilla mode" hacks (stats, drops, shops, statistics,
  layer 3 timer, door expansion, ...), final passes of `RomGenerator`, `--difficulty` casual/hard/reallyhard. Generated tables (weapons, bosses, dialogue, chaos maps, bossrush data)
  come from small generator scripts kept in `libsomrando/tools/` (`gen_*.py`, `w8_*`, `w9_*`).
* Hack rows: 78 of 80 are fully ported (`sr_cli --list-hacks`); `characterColors=custom` is refused (the oracle itself fails on it); `DefenseRefactor` and
  `BossElementRando` (Open World options) are still gated.

## Verification (all green, final tree)

| Check | Result |
|---|---|
| `test_core` vs .NET vectors (RNG, hash, LZ77, `Math.Pow`) | 87,940 checks, 0 failures on x86-64, ASan/UBSan; 87,938 on 32-bit ARM (qemu) |
| Differential harness, `-O2` | 2,240 cases, 0 failures: baseline 16, option matrix 332 identical + 6 refused, 150 random combos, 392 rando feature cases, 484 Boss Rush and 564 Chaos option cases (each non-default value x 2 seeds x 3 difficulties + random combos), 7 presets, 140 preset-seed comparisons |
| Features section | every phase-2 option alone (3 seeds, every legal value), every pair of supported features, 30 random subsets x 3 seeds, all supported at once x 5 seeds; unsupported ones must be refused without a file |
| Harness under ASan/UBSan | 1,728 cases, 0 failures |
| Harness under 32-bit ARM (qemu-arm) | 1,324 cases, 0 failures |
| Regression hashes (C only) | 350 recorded, 0 differ |
| Mutation checks (break the code, the harness must fail) | by the coordinator: boss JSL opcode (34 feature + 6 preset cases fail, first differing offset 134,027 attributed to "Boss randomizer"), enemy swap draw bound (first offset 561,269, "Non-boss enemy randomizer"), dialogue refresh opcode (274 cases fail, attributed to "Event table"); by the workers: palette ids, music tables, weapon formula exponent 1.25 to 1.26, stats tables, shop formula, and others, each failed then was restored |
| `supports()` consistency with `generate()` | checked over about 1,100 requests per non-vanilla mode: no case where supports said yes and generate refused (a mode that is not fully reproducible is refused up front) |
| Menu glue (`launcher/test_somrando_menu.c`) | 7 of 17 presets supported, 10 refused, no unported mode reported as supported; launcher tests still pass (153,580 checks, 108 configurations) |
| Android arm64 (NDK 28, android-26) | `libsomrando.so` and `libsomjni.so` link with `-DSOMRANDO_LIB=ON` (not run on a device) |

On a mismatch the harness prints the first differing offset, the number of differing bytes and the hacks whose write ranges cover the offset.
The harness found real bugs during the phase (workers report): description padding bound in the weapon randomizer, swapped tables in the music expander, a missing
InitialValues emission when `buyMultiple` is on; all fixed.

## Generation time (estimates)

Wall clock of one CLI process (ROM read, generation, 4 MiB write), median of 15 runs, x86-64 `-O2`; ARM column is `qemu-arm` user emulation of the 32-bit static build
on the same host (5 runs). The ARM numbers are an emulator ratio, not device timings: a real Cortex-A (Thor, phones) will be roughly in the same order of magnitude as
native x86 times 2 to 6, an old 3DS far slower; both are estimates.

| Preset (seed 12345) | x86-64 native | qemu-arm | ratio |
|---|---:|---:|---:|
| rando | 0.058 s | 0.414 s | 7.1x |
| rando-plus | 0.059 s | 0.378 s | 6.4x |
| rando-hard | 0.058 s | 0.365 s | 6.3x |
| bossrush | 0.057 s | 0.375 s | 6.5x |
| chaos | 0.057 s | 0.371 s | 6.5x |

The C# oracle takes 0.10 to 0.17 s per run under the same harness (NativeAOT). No preset needs more than half a second even under emulation.

## Menu seam (launcher glue)

* `launcher/somrando_menu.{h,c}`: `somrando_menu_supports(L, why, n)` (current menu configuration) and `somrando_menu_preset_supported(i, why, n)`.
* JNI (`android/app/src/main/cpp/som_launcher_jni.c`, no Java change): `generatorAvailable()` now means "advertised AND the menu's current configuration is
  supported", so the existing Java check before generating enables GO only for supported presets and shows the existing "not available" screen otherwise.
  New exports `generatorWhy()` and `presetSupported(int)` are ready for the Java side (not wired: `app/src/main/java` belongs to another agent).
* CMake: `SOMRANDO_ADVERTISE` default ON in `libsomrando`; the Android CMake option `SOMRANDO_LIB` still defaults OFF (stub). `file(GLOB ... CONFIGURE_DEPENDS)` so new sources are picked up.
* Debug aids `SOMRANDO_SKIP` and `SOMRANDO_DUMP_DIR` exist only in builds with `-DSOMRANDO_DEBUG` (tests); release builds ignore them, so no environment can make `supports()` lie.

## Not verified / known limits

* No device run (Thor, phones), no 3DS (newlib `pow`), no Windows. ARM timing is under qemu only.
* Music: only the "no `./music/` pack" behaviour of the oracle is reproduced.
* `characterColors=custom`, `defRefactor`, `bossElementRando`, Boss Rush/Chaos `--difficulty custom`, imported music packs: refused.
* Ancient Cave and Open World (10 presets) not started; Open World options of the common hacks are not exercised outside rando/bossrush/chaos.
* `Math.Pow` on ARM for exponents outside the shipped set (phase 1 note) is unchanged.
* The Java side does not yet show per-preset support in the preset list (only GO is gated through `generatorAvailable()`).

## Next phase

1. Ancient Cave (map generator, MapPieceExpander, DoorReplacer, FullMapReplacer, CustomEventManager is already there from Boss Rush), then Open World (largest logic).
2. DefenseRefactor, BossElementRando, `characterColors=custom`, dictionary-removal semantics for ordered maps (needed by Open World).
3. Wire `presetSupported`/`generatorWhy` in the Java menu; run on the Thor (arm64) and measure; 3DS newlib `pow` check.


---

# Phase 3 (2026-10-18): Ancient Cave, 5 more presets byte-identical (12 of 17)

Result: **12 of the 17 menu presets** give the same SHA-256 as the C# oracle for every seed tried; the 5 Open World presets are the only ones
refused (`SOMRANDO_ERR_UNSUPPORTED`, no file). New C: about 5,500 lines of map generators and Ancient Cave plumbing (`src/sr_p3_*.c`), library total about 29,400 lines.

## Coverage of the 17 presets

| Preset | Mode | Status | Seeds identical (preset-seeds) | C time (x86-64) | Oracle time |
|---|---|---|---:|---:|---:|
| ancientcave | ancientcave | **supported** | 50 | 0.26 s (max 0.38) | 1.9 s |
| ancientcave-short | ancientcave | **supported** | 50 | 0.17 s (max 0.25) | 0.9 s |
| ancientcave-marathon | ancientcave | **supported** | 50 | 0.37 s (max 0.53) | 3.1 s |
| ancientcave-gauntlet | ancientcave | **supported** | 50 | 0.27 s (max 0.47) | 2.2 s |
| ancientcave-hero | ancientcave | **supported** | 50 | 0.29 s (max 0.47) | 2.1 s |
| rando, rando-plus, rando-hard, bossrush, bossrush-hard, chaos, chaos-short | | supported (phase 2) | 50 each | 0.07 to 0.08 s | 0.10 s |
| open, open-mtr, open-long, open-casual, open-owl | open | refused | - | - | - |

Times are wall clock of one CLI process in a 6-job parallel harness run (indicative). Single runs, seed 12345, idle machine: default 16 floors 0.35 s,
8 floors 0.18 s, 24-floor marathon 0.32 s. The 32-bit ARM static build under `qemu-arm` (emulator, not a device): 3.8 s for 16 floors, 7.0 s for the
24-floor hard marathon, about 10 to 20 times the native time (the generators are branchy integer code with a few `double` Perlin and trigonometry calls).
Peak resident memory (`getrusage` maxrss, whole process including the 2+4+4 MiB ROM buffers): 12 MB (8 floors), 15 MB (16), 22 MB (24 floors) on x86-64;
24 to 30 MB for the 32-bit ARM build. No generator keeps big arrays on the stack (layers are arena allocated), so the 3DS stack budget is not an issue; the
whole generation is in one arena that is released at the end.

## What was ported

* Map generators (`processing/ancientcave/mapgen/**`): Forest, Ruins (courtyard, inside and outside rooms, path generator), Island (huts), Cave (tile edges),
  Manafort Interior (room makers; the whip room has weight 0 and is unreachable), the shared base (`AncientCaveMapGenerator.generate`, vanilla boss arena choice),
  `TileReplacer` (string pattern tables, same parser semantics), `PerlinNoise`, palette sources and the embedded Ancient Cave resources
  (13 palette sets, 2 custom maps; `src/sr_p3_ac_resdata.c` generated by `tools/gen_ac_resources.py`).
* Shared model `src/sr_p3_ac.h`: checked array/division helpers (the C# throws, the C fails with a message instead of reading garbage), `ac_list` and
  `ac_dict` reproducing the .NET enumeration order including free-slot reuse after Remove (needed by Open World as well), MapHeader/MapObject setters with the
  unmasked `VanillaStructure.set` semantics.
* `AncientCaveRandomizer` (door pairing with the (y,x) ordered `SortedDictionary`, NPC replacement, boss entry/death doors), `AncientCaveEventMaker`, the three
  `Ac*TilesetChanges` hacks, the Ancient Cave difficulty tables, and the Ancient Cave branches of the shared hacks (StartingDoor, DoorExpansion, EnemyChanges,
  DropChanges, Layer3Changes, TitleScreenAddition, CustomMusic, MagicRopeDeathFix, WeaponRandomizer path ...). MapPieceExpander, DoorReplacer, FullMapReplacer,
  MapPaletteSetReplacer and CustomEventManager are those of Boss Rush/Chaos (limits raised: 256 objects, 512 triggers, 320 pieces, 1100 doors, 128 palettes).
* Options: `acBoy acGirl acSprite acFilter acMusic acLength BossFrequency acDialogue acBiomeTypes` (comma list, any non-empty subset of the five biomes) and
  `--difficulty casual|hard|reallyhard`. `acDifficulty=custom`, a custom difficulty file, `characterColors=custom`, `defRefactor`, `bossElementRando` stay refused.

## Quotes (third-party text) as a build option

The Hedberg and Martin lines are not covered by the LGPL. The mechanism (pool selection, `acFilter` replacement, one event per quote) is in `src/sr_p3_events.c`; the text
lives only in `data/sr_quotes_mitch.c` and `data/sr_quotes_demetri.c` (generated by `tools/gen_p3_quotes.py`, see `data/README.quotes.md`). Build option `SOMRANDO_QUOTES`
(CMake, default ON; `SOMRANDO_QUOTES=0 ./build.sh lib`): OFF links `data/sr_quotes_placeholder.c` (original neutral lines, pools of 45 and 50 entries), the
library still generates valid Ancient Cave ROMs but they are no longer byte-identical to the C# generator (dialogue bytes and every address behind them move), and
`somrando_supports()` still answers 1 in that build: a distribution without the quotes must not claim seed compatibility for Ancient Cave. Both Android arm64
variants link (`libsomrando.so` 2.48 MB with, 2.46 MB without; `libsomjni.so` links with `-DSOMRANDO_LIB=ON`), not run on a device.

## Verification (all green, final tree)

| Check | Result |
|---|---|
| Floor level (`tests/ac_check.py`, instrumented oracle with dump hooks, replay of each floor from the dumped RNG state) | in the final tree 10 seeds x 24 floors per biome (240 floors each, 1,200 in all, incl. text and non-ASCII seeds): 0 differing; during development the workers ran 40+ seeds per biome and length with ASan/UBSan and 32-bit ARM replays and per-biome mutation checks (reported by the workers, Manafort: 1,176 + 784 + 392 floors, Island 960 + 688 + 344) |
| Whole ROM, differential harness `-O2` (`diff_harness.py --jobs 6 --seeds 2 --random 10 --preset-seeds 50`) | 3,203 cases, 0 failures: Ancient Cave baseline 12, option matrix 477 identical + 18 refused as expected, pairs 75, random subsets 30, API 44, 5 presets x 50 seeds |
| Harness under ASan/UBSan (reduced) | 2,415 cases, 0 failures |
| Harness under 32-bit ARM / qemu-arm (reduced) | 1,996 cases, 0 failures |
| `run_tests.sh` | `ALL TESTS PASSED` (also with `SOMRANDO_QUOTES=0 ./build.sh lib` building) |
| `test_core` | 87,940 checks, 0 failures (x86-64, ASan); 87,938 (ARM) |
| Regression hashes (`tests/regression_hashes.json`, C only) | 600 recorded (350 from phase 2 + 250 for the 5 Ancient Cave presets x 50 seeds), 0 differ |
| Mutation check on the whole pipeline (copy of the library) | the continue-door transition byte 0x81 -> 0x80 in `sr_p3_ac_rand.c`: the preset-seeds comparison fails for the Ancient Cave presets and prints the first differing offset (about 427,000) attributed to "Door changes for non-vanilla modes" / "Custom event commands" |
| `supports()` vs `generate()` | 12 request pairs in the AC API section plus the phase 2 checks: never yes-then-refused |
| Cancel / progress | steps 96 (8 floors), 104 (16), 112 (24) with a constant total; cancel at the first step, in the floor loop, after the loop and at the last step returns `SOMRANDO_ERR_CANCELLED` and writes nothing; a cancelled run keeps an existing destination; a successful run replaces it atomically (temporary name + rename); an unwritable destination reports `ERR_FAILED` without a partial file |
| Menu glue (`launcher/test_somrando_menu.c`) | 12 of 17 supported, 5 Open World refused, 0 failures |

## Not verified / known limits

* No device run (Thor, phones), no 3DS (newlib `pow`/`sin`/`atan2`): the Ancient Cave generators use `Math.Sin/Cos/Atan2/Sqrt/Floor`; they match under glibc x86-64 and
  under glibc ARM 32-bit (qemu), but not tested against bionic or newlib.
* The oracle needs a few seconds per Ancient Cave seed, so the preset-seeds sample is 50 per preset; the option matrix uses 1 to 3 seeds per value.
* The instrumented oracle (`oracle-dbg`, hooks in a private copy of the C# tree, not in the repository) is a test aid; the product comparison is always the release oracle.
* `getMapLogString` (spoiler text) is not ported; if the C# ever threw inside it for a seed, the oracle would fail where the C does not (none seen in 600+ seeds).
* Open World (5 presets), `DefenseRefactor`, `BossElementRando`, `characterColors=custom` remain.

## Next phase

Open World, see section 7 of `randomizer-c-port-plan-20261018.md` (file list, line counts and order); then DefenseRefactor, BossElementRando, `characterColors=custom`.


---

# Phase 4 (2026-10-18): Open World, 5 more presets byte-identical (17 of 17)

Result: **all 17 menu presets** give the same SHA-256 as the C# oracle for every seed tried. Open World adds about 21,500 lines of C
(`src/sr_p4_*`, generated tables included; library total about 51,000 lines with generated data). Work was split over nine parallel workers by
module (selectors, shared-hack open branches, randomization core, injector/hints/gift, events 1 and 2, three groups of hacks) plus one for
`BossElementRando`; the coordinator wrote the skeleton (registry in OpenWorldGenerator order, option table, weak stubs, per-hack log) first.

## Coverage of the 17 presets

| Preset | Mode | Status | Seeds identical (preset-seeds) | C time (x86-64, CLI process) | Oracle time |
|---|---|---|---:|---:|---:|
| open | open | **supported** | 50 | 0.09 s | 0.17 s |
| open-mtr | open | **supported** | 50 | 0.09 s | 0.18 s |
| open-long | open | **supported** | 50 | 0.06 s | 0.17 s |
| open-casual | open | **supported** | 50 | 0.06 s | 0.17 s |
| open-owl | open | **supported** | 50 | 0.10 s | 0.16 s |
| the other 12 presets | rando, bossrush, chaos, ancientcave | supported (phases 2 and 3) | 50 each | unchanged | |

Peak resident memory of one Open World run: 13.4 MB (x86-64, whole process). Times: median of 15 runs, seed 12345, indicative.

## What was ported

* Skeleton (coordinator): `sr_p4.h`, `sr_p4_open.c` (hack list in the order of `OpenWorldGenerator`, nested-hack helper `sr_p4_nested`, per-owner `*_supports()`
  preflight), `sr_p4_opts.h` (the 60 Open World options incl. numeric ones, checked against `somrando --mode open --list-options`), `sr_p4_ow.h` (prize/location model).
* Selectors: character, goal, class, silly enemy names, MTR seed count, difficulty processor, ice country fixes, crystal orb colors, auto-reset removal,
  title hash display (prints the RNG state), Christmas hacks (`opXmasMaps`, `opXmasItems`).
* Shared hacks with Open World branches: WeaponRandomizer, TitleScreen, OrbRewardEventGenerator, EnemyPositionAdjustments, ExperienceAdjust/GoldAdjust, ElementSwaps,
  BossSwaps (vanilla/swap/random), EnemySwaps (vanilla/swap/random/oops/none, status ailments), CustomMusic, Autosave.
* Randomization: locations, prizes (generated tables), matcher, simulator (1000-attempt search, easy/hard/dontcare), starting weapons, injector, hints, gift mode.
* Events: new game, intros (waterfall, flammie, gift delivery, reindeer), flag initialisation, sword grab, prize and hint events, pokedex lady, Neko, supporting events,
  map changes, misc dialogue.
* Hacks: EnemiesAtYourLevel (all `opStatGrowth` modes), MinMaxLevel, CustomTopOfScreenText, CharacterClasses, CharacterJoinChanges, FastWhipPosts,
  InvincibleRabite, ShopRandomizer (+NoNekoOverflow), WattsWeaponLevelFix, LimeSlimeFlavors, DropOnlyConsumables, MenuIconsIfArmorImprovement, MagicRopeRefactor,
  MapPaletteRandomizations, AISwap, BlackMapPalettes, Nesoberi (68 plush images decoded at build time).
* Common hacks that were still gated: `DefenseRefactor` (hand port, `Math.Pow` from libm) and `BossElementRando` (HSV doubles), both verified in every mode.
* Build-time resources (not committed, generated from an upstream checkout): the pokedex table (`tools/gen_p4_ev1_resources.py`) and the Nesoberi
  images (`tools/gen_p4_hmisc_resources.py`). A build without them refuses Open World (`supports()` answers 0). `build.sh`, `CMakeLists.txt` and `run_tests.sh` wire them in.
* Nesoberi uses the local calendar date (character birthdays override the pick); the regression run therefore fixes the date (`SOMRANDO_FAKE_DATE`, debug builds).

## Verification (final tree)

| Check | Result |
|---|---|
| Per-hack comparison (`tests/hack_cmp.py`: instrumented oracle log with ROM hash, working offset and both random states after every hack, 119 hacks) | first divergent hack named; all identical for the presets |
| Preset-seeds, 50 seeds x 5 Open World presets against the live oracle | 250 / 250 identical |
| Open World harness sections (`--only open`): baseline 20 seeds, every option x every legal value x 2 seeds, every preset with one option flipped, 150 + 50 random combinations, every pair of options, API | 8,462 cases, 0 failures (identical or refused as expected) |
| Christmas / gift / reindeer sweep (goal x maps x items x gifts 4,6,8 x logic x 3 seeds) | 288 / 288 identical |
| `BossElementRando` and `DefenseRefactor` in rando, bossrush, chaos, ancientcave and Open World | identical (workers: 32 rando seeds; coordinator: 24 Open World runs) |
| Regression hashes (`tests/regression_hashes.json`, C only) | 850 recorded (600 + 250 Open World), 0 differ |
| Mutation checks | injector starter-weapon byte changed: preset-seeds fails, first differing offset 22,464 (0x57C0), attributed to "Open world randomizations"; plus the workers' per-module mutations (about 60, each caught by the log of the right hack) |
| Menu glue (`launcher/test_somrando_menu.c`) | 17 of 17 supported, 0 failures; `run_launcher_tests.sh` builds and runs it |
| Android arm64 (NDK 28, android-26) | `libsomrando.so` (4.0 MB) and `libsomjni.so` link with `-DSOMRANDO_LIB=ON` (not run on a device) |
| `run_tests.sh` (final tree) | `ALL TESTS PASSED`: `test_core` 87,940 checks (ARM 87,938); harness -O2 11,429 cases, 0 failures; ASan/UBSan reduced 10,705 cases, 0 failures; 32-bit ARM under qemu-arm reduced 10,261 cases, 0 failures; regression hashes 850, 0 differ |

## Not supported (refused, never a wrong ROM)

`plandoSettings` non-empty (plando), `characterColors=custom`, `acDifficulty=custom` and `--difficulty custom` (the menu does not offer them; the oracle itself fails without
the per-field data), imported music packs, `opStartGold=65535` (outside the 100-step grid the option layer accepts). Quote tables of Ancient Cave and Nesoberi/pokedata are
build-time resources; a distribution without them must not claim seed compatibility.

## Not verified

* No device run (Thor, phones), no 3DS (newlib), no Windows. ARM numbers are `qemu-arm` only.
* Plando; Open World with `music/` packs; behaviour on a calendar date with a Nesoberi birthday is verified against an oracle with a faked date by the worker only.
* `getMapLogString`/spoiler text is not ported (logging only; analysed not to throw).

## 3DS (devkitARM r68, newlib, ARM11), 2026-10-20

Checked by the 3DS companion stream (`docs/companion-3ds-20261020.md`; build `tools/companion/n3ds/emu/build_libsomrando.sh`, test `tools/companion/n3ds/test/rando_batch.c`):

* The 96 sources plus the generated resources compile for `armv6k` hard float (`-ffp-contract=off`) with no source change, in about 4 s with 4 processes; the static library is 0.9 MB of code.
* **Superseded 2026-10-20: no special linker flag is needed any more** (see the section below). The first build had to use `-Wl,--whole-archive`: the hacks were registered through weak stubs; from a plain static library the linker kept the stubs (the strong definitions are in members nothing else
  references) and Open World fails at generation with "open world hack sr_p4_hack_character_selection is not ported yet" although `somrando_supports()` said yes. The Android `SHARED` library is not affected.
* The 3DS build run in Azahar (an emulator) makes **the same ROM SHA-256 as the host build for all 17 presets** (seed 12345), and the Ancient Cave ROM made through the in-app menu is byte-identical to the host's, so newlib's `libm` (`pow` in DefenseRefactor and BossElementRando) and VFP doubles agree with glibc on these inputs.
* Cost in Azahar (emulated 268 MHz ARM11, not calibrated against a console): Ancient Cave 12.6 to 29.6 s, Open World 3.1 s, Randomizer 5.5 s, Chaos 5.5 s, Boss Rush 5.6 s (host: 0.05 to 0.75 s). Peak malloc heap: Ancient Cave 12.9 to 22.0 MB, the others 9 to 11 MB.
* Still not done: a run on a real 3DS (time, memory with the app's other data, the progress screen over a long run).

## ROM-free binaries and robust registration, 2026-10-20 (T9)

### 1. No ROM bytes in any binary

Found by the PC stream: the Vineshroom tile data and other build-time tables ended up in every binary that links the library (`som-pc` matched the owner's ROM in about 3.7 KB).
`tests/scan_rom_bytes.py` (new) looks up every 32-byte window of the vanilla ROM (windows with fewer than 6 different bytes, or monotone counting sequences, are ignored) in a
file at any alignment, also inside `.a`, APK/ZIP members, `.3dsx` and PC binaries. Run on the old `sr_cli`/`som-pc` it found 2,349 windows in these tables, which are the only
ROM-derived data of the library (a scan with 16-byte windows adds only chance matches of 16 to 25 bytes in unrelated tables):

| Table | Bytes | Provenance |
| --- | --- | --- |
| `vineshroom_tiles.bin` | 3,584 (1,929 identical to ROM 0x190500..) | upstream edit of the game's own tiles |
| Ancient Cave map palette sets `mapPalettes/ac*` | 13 x 210 | edited dumps of the ROM palettes (0xC8000 table) |
| `bossRushRestMapPal.bin` | 210 (180 identical) | same |
| `acManafortBossArenaL1.bin`, `acManafortNpcRoomL1.bin` | 1,024 and 624 (171 identical in the second) | edited map dumps |

The tables were literal arrays in `sr_p3_ac_resdata.c` / `sr_p2_bossrush_data.c` (committed!) and a generated file (`sr_resources_gen.c`). Now `tools/gen_rom_recipes.py` writes
`src/sr_rom_recipes.c` (needs the ROM and the upstream checkout once; the output is committed): per resource, runs of at least 12 bytes with at least 4 different values that exist in the ROM are stored as
`(ROM offset, length)`; the bytes that differ (the upstream author's edits) stay literal; a CRC-32 of the whole resource is kept. At generation time `sr_rom_res(ctx, name, &len)`
(`src/sr_rom_res.c`) rebuilds the resource from `ctx->orig`, caches it in the generation arena and fails with `SOMRANDO_ERR_BAD_ROM` ("The source ROM is not the unmodified US v1.0
image") when the CRC does not match, so a tampered ROM can never give a different result silently. The bytes are exactly those of before.

Not ROM data (checked with the scanner, documented in `NOTICE`): the Ancient Cave quote text (`data/`), boss rush rest map layers, difficulty presets and the other constants of the upstream
sources (LGPL-2.1+), the pokedex text and the Nesoberi fan art (third-party content of the upstream repository; still generated at build time from `SOM_RANDO_SRC`, never stored here).
The old generator scripts `gen_resources.py` and `gen_ac_resources.py`, the file `sr_p3_ac_resdata.c` and the fallback `sr_p4_ev1_nodata.c` were moved out of the tree
(`~/.local/state/som-decomp/pending-delete-20261020/libsomrando/`). `pc/build_rando.sh` and `emu/build_libsomrando.sh` now archive an explicit object list, so a stale object of
the old layout cannot get into a library.

Verification: all 850 regression hashes unchanged (x86-64 -O2, ASan/UBSan, 32-bit ARM under qemu-arm: 850 / 850 each); `test_core` 87,940 checks, 0 failures (-O2 and ASan);
`pc/test_rando_parity.py`: 34 / 34 (17 presets x seeds 12345 and 777) identical between `som-pc`, `sr_cli` and the C# oracle. `scan_rom_bytes.py --selftest` plants a ROM chunk in noise to prove the scanner
can fail. Scan of the new `som-pc`, `libsomrando.a` and `sr_cli`: 0 files with ROM bytes.

### 2. Registration without linker flags

Cause of "open world hack sr_p4_hack_character_selection is not ported yet" in PC builds: `sr_p4_open.c` defined weak stubs for the Open World hacks and their `supports()` functions; with a
static archive the linker resolves the reference to the weak symbol inside the member already loaded, so the strong definitions (in members nothing else references) were never pulled.
Fix: the weak stubs are gone. `sr_p4_open.c` references every hack and every `supports()` function directly (strong symbols), so a normal `ar`/`gcc ... libsomrando.a` link pulls the
whole Open World module chain. The two weak data fallbacks (`sr_res_pokedex`, `sr_p4_neso_data`) are gone as well: the generated files are always linked. `--whole-archive` is still used by the 3DS Makefile
and is harmless. Result: 17 of 17 presets generate on the PC (the earlier 12), byte-identical to `sr_cli` and the oracle; 3DS (Azahar, `emu/build_rando_batch.sh` with `NO_WHOLE_ARCHIVE=1`, i.e. a plain archive) see below.

### 3. Platform checks of this round

| Check | Result |
| --- | --- |
| PC, `pc/test_rando_parity.py` (som-pc vs `sr_cli` vs C# oracle, 17 presets x seeds 12345 and 777) | 34 identical, 0 pending, 0 failed (before: 24, 5 presets pending) |
| Regression hashes, x86-64 -O2 / ASan+UBSan / ARM32 under qemu-arm | 850 recorded, 0 differ each |
| `test_core` (-O2, ASan/UBSan) | 87,940 checks, 0 failures |
| 3DS: `build_libsomrando.sh` + `build_rando_batch.sh` with `NO_WHOLE_ARCHIVE=1` (plain archive), run in Azahar, `test/compare_rando_batch.py` | 17 presets on the host, 17 on the 3DS, 17 identical (including the 5 Open World ones) |
| 3DS app `make RANDO_A=... ` (whole archive kept) | links, `-Wall -Wextra` clean in the checked files |
| Android arm64 + x86_64 `libsomrando.so` and `libsomjni.so` (`-PsomRandoSrc`, CMake `SOMRANDO_LIB`) | link; APK built, not installed |
| `tests/scan_rom_bytes.py` (32-byte windows) on `sr_cli` (-O2, ASan, ARM32), `libsomrando.a` (PC and 3DS), `rando_batch.3dsx/.elf`, `som_companion.3dsx/.elf`, `som-pc`, and every file of the Thor APK (both libsomrando.so, libsomjni.so, the cores) | 0 files with ROM bytes (the old binaries: 2,349 windows) |

Scanner notes: `SOM_ROM` selects the ROM (SKIP when unset); `--selftest` plants a ROM chunk in noise and expects to find it; `--window N` (multiple of 8). Windows with fewer than 6 different byte values and windows that hold a long
monotone stretch are ignored, because 16-bit counting tables (0,1,2,..) occur by chance in unrelated code (two of the third-party Snes9x cores in the APK contain one). `run_tests.sh`, `pc/run_pc_tests.sh`
and `pc/test_release.sh` call it. The Thor APK of this round is built by `~/.local/state/som-decomp/thor-r4-20261020/build_apk.sh`.
