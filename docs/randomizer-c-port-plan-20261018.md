# libsomrando: plan for the C port of the Moppu randomizer (board item T9)

Date: 2026-10-18. Refines the plan of `docs/companion-cli-audit-20261017.md` section 2 with measured line counts, an ordered module list,
the numeric risks and a test strategy. Nothing is ported in this round: the start menu (`docs/companion-startmenu-20261018.md`) already calls the
generator through `tools/companion/launcher/somrando_api.h`, and `somrando_stub.c` stands in for the library until this plan is executed.
Source: `Moppu/SecretOfManaRandomizer` v1.49 (commit `1404b7b`), LGPL-2.1. The port is a derivative work: keep the notices, the author comments
and the LGPL text, ship it as a separable library.

## 1. What has to be ported (measured)

Counts come from `tools/companion/randomizer/` scratch analysis of the upstream clone (throwaway script, not kept; rule: a file is
reachable from a mode when its generator file, `RomGenerator.cs` or the settings reach it through any type name; GUI folders `forms/`, `help/`, `config/ui/`
and `Program.cs` excluded). Name reachability over-approximates (registries and shared helpers pull in everything they mention), so read the
per-mode numbers as upper bounds and the union as the real size of "all five modes".

| Scope | C# files | Lines |
| --- | ---: | ---: |
| Whole repository (329 files) | 329 | 92,407 |
| Without GUI (`forms/`, `help/`, `config/ui/`, `Program.cs`) | 299 | 79,628 |
| Reachable by any mode (union of the five) | 293 | 78,912 |
| Ancient Cave (upper bound) | 208 | 53,644 |
| Open World | 208 | 51,909 |
| Vanilla Randomizer | 152 | 38,035 |
| Boss Rush | 217 | 55,409 |
| Chaos | 209 | 53,706 |

The 46,000 lines of the audit were Ancient Cave with its shipped hack set counted by hand; the name-reach upper bound is 53,600. Both agree
within 15 percent, so the planning number stays "about 50,000 lines of C# for the first mode, 79,000 for all modes".

### Module list by C# area (union, lines, files)

| Area (upstream path) | Lines | Files | Content and porting notes |
| --- | ---: | ---: | --- |
| `processing/hacks/common/*` | 21,543 | 99 | 65 hack classes registered by `RomGenerator` for every mode: `enhancement` 5,671 (StatusGlow 1,643, WeaponRandomizer 1,469), `procgen` 4,982 (Statistics, TimerDialogue, Layer3Changes, EnemyChanges), `qol` 3,886 (BuyMultipleItems 1,451), `other` 3,140 in 34 files, `util` 1,901 (CharacterPaletteRandomizer 863), `music` 1,133, `fix` 830. About 60 percent is code that writes 65816 machine code byte by byte (`outRom[ctx.workingOffset++] = 0xA9`): mechanical. |
| `processing/hacks/openworld/*` | 12,565 | 31 | Open World only. EnemiesAtYourLevel 2,500 and CharacterClasses 1,337 are the large ones; XmasMapPalettes, WinterTheme, Nesoberi and Pokedex data are optional and can be dropped (seeds that do not enable them are unaffected). |
| `processing/hacks/{vanillarando,bossrush,ancientcave}` | 1,883 | 11 | Small per-mode hacks. |
| `processing/ancientcave/` outside `mapgen` (generator, event maker, quotes) | 2,257 | 5 | `AncientCaveEventMaker` 1,091. `MitchHedbergQuotes` (341) and `DemetriMartinQuotes` (438) are third-party text: decide before publishing (see the audit, section 1). |
| `processing/ancientcave/mapgen/*` | 10,890 | 31 | Biome generators: Island 1,365, Cave 1,257, Forest 905, Ruins 884 (+ InsideRooms 807). Use `List`, `Dictionary`, the RNG and `Math.Floor/Round/Sqrt/Pow`: the part that needs real care. |
| `processing/openworld/*` | 10,890 | 39 | Logic, prizes (OpenWorldPrizes 1,405), events (OpenWorldSupportingEvents 1,013, PrizeEvents 878). LINQ-heavy (about 170 LINQ calls in `randomization/` and settings). |
| `processing/common/` (includes `structure/` 1,387 and `mapgen/` 110) | 9,624 | 33 | `RomGenerator` (445), `VanillaBossMaps` 2,305, `BossSwaps` 819, event/door/map/tileset/palette utilities, `SomMapCompressor`. Mostly tables and byte pokes. |
| `processing/chaos/` | 2,800 | 10 | `ChaosVanillaMaps` 1,106. |
| `processing/vanillarando/` | 2,240 | 2 | `FasterDialogueEvents` 2,181 (event text and bytecode tables). |
| `processing/bossrush/` | 1,250 | 5 | |
| `config/` (settings, difficulty, property files) | 1,600 | 10 | `StringValueSettings` is the key/value store behind every option; the catalogue in `tools/companion/randomizer/presets.json` already lists the names, values and defaults. |
| `util/` (RNG, hash, LZ77, compression, damage helpers) | 1,150 | 12 | `DotNet110Random` (222 lines, integer only), `HashcodeUtil`, `SomLz77`. Port first: everything depends on them. |
| `logging/` and `processing/RandoProcessor.cs` | 225 | 5 | Replace by a progress callback and an optional log buffer. |

Third-party and data that is not code: about 12 KB of embedded resources for Ancient Cave (3 difficulty property files, 13 palette sets, 2 custom
maps, `vineshroom_tiles.bin`), converted to C arrays at build time by a small script (the property files are read by `PropertyFileReader`,
17 `File` calls, which become a table lookup).

## 2. Order of work

Each step ends with a byte-for-byte comparison against the C# output (section 4); a step is done only when its seeds match.

| Step | Work | Output | Lines (C#) |
| --- | --- | --- | ---: |
| 0 | Comparison tool: `somrando_cmp.py` runs the C# headless generator (`tools/companion/randomizer/headless`) and the C build on the same request and reports the first differing offset with the hack that wrote it (a write log in both builds records `(hack, offset, length)`). | tests, CI script | - |
| 1 | `util/`: `DotNet110Random`, `HashcodeUtil` (32-bit wrap), `SomLz77`, byte-buffer helpers; `config/`: key/value store, difficulty presets, settings per mode, option list identical to `presets.json`. | `libsomrando-core` | 2,750 |
| 2 | `RomGenerator` pipeline: header check (the same rules as the audit: 2 MiB or +512, title, region 1, version 0), copy to the 4 MiB output, hack ordering as logged, progress and cancel callback, error strings, `RandoContext`, `VanillaRomOffsets`, `VanillaMapUtil`, `VanillaEventUtil`, `VanillaDoorUtil`, `VanillaPaletteUtil`, `VanillaTilesetUtil`, `SomMapCompressor`. | pipeline runs with an empty hack list and reproduces the C# bytes for that case | 9,600 |
| 3 | The 65 common hacks, in the order they appear in the generator log, smallest first (`fix`, `qol`, `other`), then `enhancement`, `util`, `procgen`, `music` (the music pack is optional: without `./music/` the C# continues, so the port can too). | all hack toggles bit-identical on a vanilla input with no mode logic | 21,500 |
| 4 | Ancient Cave: event maker, the four biome generators and `InsideRooms`, enemy/drop/shop changes, quotes. | AC seeds identical | 13,150 + 210 |
| 5 | Vanilla Randomizer, then Boss Rush, then Chaos (small, reuse of step 3 and `common`). | three more modes | 7,900 |
| 6 | Open World (largest logic, LINQ to loops, plando and holiday themes last, optional). | Open World seeds identical | 12,565 + 10,890 |
| 7 | Library packaging: C ABI of `somrando_api.h`, LGPL notices, build for Android (NDK), 3DS (devkitARM) and PC, replacement of `somrando_stub.c`, device measurements. | `libsomrando.so` / `.a` | - |

Estimate, kept from the audit and unchanged: about 55,000 to 65,000 lines of C for all modes, 4 to 8 weeks for a coding agent working module by module with the
C# as oracle (factor 2 uncertainty), 3 to 5 months for one human. Steps 1 to 4 (Ancient Cave) are about 60 percent of the total and deliver the first
playable generator on device; steps 5 and 6 are independent of each other.

## 3. Numeric and language risks

| Risk | Where (reachable code) | Handling |
| --- | --- | --- |
| `Math.Pow` | 10 calls in 6 files: `EnemiesAtYourLevel` 3, `SomLz77` 2, `DefenseRefactor` 2, `EnemyChanges`, `WeaponRandomizer`, `AncientCaveEventMaker` 1 each. (The audit counted 26: 21 of them are in `util/DamageCalculations.cs`, which no mode reaches; it is used by GUI helpers.) | For integer bases and exponents use an exact integer power. For real exponents compare libm `pow` with the .NET result on the full argument range the code can produce (the arguments are small integers or fixed fractions); a mismatch near an integer boundary changes one stat by 1, the differential test finds it. If libm differs, port the .NET algorithm (CoreCLR calls the platform C runtime `pow`, so glibc/bionic usually agree; newlib on the 3DS may not: test it on the target). |
| Other `Math.*` | 38 calls in 13 files (`CaveGenerator` 10, `StatusGlow` 6, `PerlinNoise` 6). `Floor`, `Round` (banker's rounding by default in .NET: `Math.Round(x)` rounds half to even, `rint` in C), `Sqrt` are exact; `Sin/Cos/Log` in `PerlinNoise` and `StatusGlow` feed integer casts. | `Math.Round` becomes `rint` with the default rounding mode; check each `Sin/Cos/Log` use for the `(int)` cast boundary like `Pow`. |
| `double` and `float` | 359 declarations in 45 files (`LimeSlimeFlavors` 41, `EnemiesAtYourLevel` 40, `BossElementRando` 31). | Use `double` everywhere the C# does; compile with `-ffp-contract=off` and no `-ffast-math` so the C compiler does not fuse multiply-add (a fused result differs from .NET in the last bit). On 3DS (VFPv2, hard-float) and Android arm64 the IEEE results equal x86-64 SSE2 for `+ - * /` and `sqrt`. |
| RNG | `DotNet110Random` (Knuth subtractive, integer) plus `HashcodeUtil`: 502 uses in 72 files. Seed string to hash must use the same 32-bit wrap. | Port exactly; unit-test the first 1,000 outputs for 100 seeds against the C#. Keep the second generator seeded with `seed + "_cosmetic"`. |
| Iteration order | 561 `Dictionary/HashSet` uses in 67 files. .NET `Dictionary` enumerates in insertion order when nothing was removed; code that removes then adds changes that order, and a seeded generator that iterates a dictionary then draws random numbers depends on it. | Implement an insertion-ordered map with the same removal semantics (free list reuse) wherever a loop over a dictionary precedes an RNG call. Find those sites by a grep for `foreach` over dictionaries; the write-log comparison localises any miss. |
| LINQ | 1,002 calls in 70 files (`XmasMapPalettes` 157, `ChaosVanillaMaps` 132, `VanillaBossMaps` 132). `OrderBy` is a stable sort; `Distinct`, `GroupBy` keep first-occurrence order. | Translate to loops with a stable sort (merge sort); never `qsort` (not stable). |
| Strings | 1 `string.Format` or interpolation; dialogue wrapping and text tables use `string` and `char` arithmetic; the game's text encoding is single-byte. | Work on `uint8_t` text buffers with the existing tables. |
| Bitmap/Drawing | 10 uses in 3 files (`Nesoberi`, a fan-art feature of Open World). | Drop the feature (documented option loss), keep the seed otherwise identical. |
| Resources | 32 `File` calls in 9 files, mostly `PropertyFileReader`. | Embedded arrays; `./music/` optional. |

## 4. Test strategy: byte-identical ROMs per seed against the C# app

1. Oracle: the existing headless build (`som-dualscreen setup-randomizer`, NativeAOT variant proven byte-identical to the CLR build on five modes) and
   the release exe of v1.49 (compared byte-for-byte on two Ancient Cave seeds in the audit).
2. Inputs: the catalogue `tools/companion/randomizer/presets.json` gives every option, its legal values and the pools used by "randomize all". A
   generator of test requests produces: every preset of the menu with 200 seeds each (3,400 ROMs), 500 randomized configurations (the C start menu's
   `sl_randomize_all` is the generator, so the test also covers what a player can actually pick), every single option flipped from its default for the
   five modes, and the 3 Ancient Cave difficulties by 3 lengths by 3 boss frequencies.
3. Comparison: `cmp` of the 4 MiB output, then SHA-256 recorded in a table `data/randomizer_oracle_*.json` (hashes only, never ROM bytes in the repository).
   A failure prints the first differing offset and the hack that wrote it (step 0 write log).
4. Unit level: RNG streams, `HashcodeUtil`, `SomLz77` round trips on all vanilla map blobs, `Math.Pow` argument sweep, the `StringValueSettings`
   behaviour (default, set, enumeration index).
5. Playability: every generated ROM must pass `som-dualscreen generate`'s checks already in place (size, title, variant by hook sites, 1,800 frames headless) and a
   sample of 20 ROMs per mode the Ancient Cave floor-1 fixture (`verify`) before a release.
6. Sanitisers on PC (ASan, UBSan, `-fsanitize=float-divide-by-zero`), and a 32-bit ARM static build under `qemu-arm` (as `run_bind_tests.sh` does) to catch 32-bit
   `int`/`long` assumptions: the C# `int` is 32 bits, `long` 64; use `int32_t` and `int64_t`, never plain `long`.

Seed compatibility is the goal, not a requirement for playing: if a module cannot be made identical it can ship as a documented difference (the seed then
differs from the Windows app), as the audit already allows.

## 5. Unknowns for the 3DS (not measured)

| Unknown | Why it matters | How to measure |
| --- | --- | --- |
| Generation time | An ARM11 at 268 MHz (old 3DS) is 40 to 100 times slower than a desktop for integer, branchy code. The PC takes 0.3 to 4 s (this round: Ancient Cave 0.7 to 1.0 s, Open World 0.3 s, headless CLR including start-up). Estimate 1 to 4 minutes old 3DS, 20 to 60 s New 3DS (804 MHz). | Run the C core on a device after step 4 with the progress callback logging per hack times; the menu already shows step and percent. |
| Memory | Input 2 MiB, output 4 MiB, tables and maps under 10 MiB in C. Old 3DS apps have 64 MB, New 3DS 124 MB; generation runs before the emulator core starts. | `mallinfo`/high-water mark in the library; budget test with the core unloaded. |
| Stack | The map generators recurse and keep arrays on the stack in C#. The default 3DS thread stack is small (32 KB to 1 MB depending on the creation call). | Run the generator on a thread created with a 1 MiB stack, check the high-water mark, move large arrays to the heap. |
| Newlib `pow`/`sqrt` accuracy | Seed compatibility of the `Math.Pow` sites. | Argument sweep on the device or qemu-arm against the C# table. |
| SD card write speed | 4 MiB per ROM, an 8-ROM cache is 32 MiB. | Time the write; keep the temporary `.part` rename rule. |

## 6. Licence checklist for the port

- Every ported file keeps `Author: Moppleton` and the upstream copyright; add a header line "derived from Moppu/SecretOfManaRandomizer v1.49, translated to C, changed <date>".
- `COPYING.LIB` (LGPL-2.1 text) in the library directory, a `NOTICE` that lists the MIT-licensed `DotNet110Random` header and the unknown-origin `HashcodeUtil`.
- The library has its own directory and build target (`libsomrando`), linked dynamically on Android and as an object archive with the application source on the 3DS.
- Decide on the comedian quote tables and the fan data (`pokedata3.xml`, `nesoberi`) before any public build; neither is needed by the menu or by the tests.
- Ask the author before publishing a derived library (courtesy, recommended in the audit), and offer the headless build upstream.

Progress: see randomizer-c-port-progress-20261018.md.

## 7. Open World plan (phase 4, written 2026-10-18 at the end of phase 3; nothing of it is ported yet)

Measured on the v1.49 sources. `OpenWorldGenerator` registers 39 mode hacks after the 65 common ones; the five `open*` presets use the options
`opGoal` (mtr, vlong), `opNumSeeds`, `opDifficulty` (easy, kindahard, hard), `opExp`, `opGold`, `opStartChar`, `opCharacters`, `opEnemies=oops` with
`oopsAllThis`, `opLogic=restrictive`, `confusionFix`. Total new C# to read: 10,851 lines in `processing/openworld/**` and 12,534 lines in
`processing/hacks/openworld/**` (about 23,400 lines, the largest mode). Work order, each step ending in a byte comparison with the oracle:

| Step | Files (lines) | Notes |
| --- | --- | --- |
| 1 settings + small selectors | OpenWorldSettings options, `OpenWorldCharacterSelection` 274, `OpenWorldGoalProcessor` 63, `OpenWorldClassSelection` 134, `OpenWorldMtrSeedNumSelection` 59, `OpenWorldDifficultyProcessor` 109, `OpenWorldSillyEnemyNamePicker` 174, `PlandoProperties`/`PlandoUtils` 482 (plando refused at first) | RNG draws and workingData publication only |
| 2 shared mode hacks already ported for rando | ElementSwaps, BossSwaps, EnemySwaps, InvisClearsSlow, BossZPosition, FirePalaceDoorFix, Autosave, CustomMusic | need the open-world branches (the C code gates them on mode rando today) |
| 3 randomizer core | `randomization/*` 4,200 lines: OpenWorldRandomizer 163, Locations 383, Prizes 1,404, LocationPrizeMatcher 321, Simulator 377, Hints 377, GiftModeProcessing 581, StartingWeaponRandomizer 151, Spoilers 140, ResultInjector 86 | LINQ and Dictionary/HashSet iteration order: needs the .NET Dictionary emulation (`ac_dict`, phase 3) and a stable sort; the simulator is a graph search driven by the RNG |
| 4 events | `events/*` 4,900 lines: PrizeEvents 877, SupportingEvents 1,012, SupportingMapChanges 592, ReindeerIntro 491, MiscDialogue 344, GiftDeliveryIntro 272, WaterfallIntro 233, HintEvents 200, SwordGrab 177, NekoEvents 154, InitialEventFlagStates 151 and others | byte-poking, mostly mechanical (`transpile_simple.py`) |
| 5 hacks | CharacterJoinChanges 327, MinMaxLevel 307, EnemiesAtYourLevel 2,499 (uses `Math.Pow` 3x), CharacterClasses 1,336, CrystalOrbColors 527, DropOnlyConsumables 392, MagicRopeRefactor 321, FastWhipPosts 659, ShopRandomizer 145, LimeSlimeFlavors 264, MenuIconsIfArmorImprovement 243, CustomTopOfScreenText 218, MapPaletteRandomizations 218, WattsWeaponLevelFix, InvincibleRabite, DontAutoResetAtFirstMenu, TitleMenuSeedHashDisplay 72 (last, prints the RNG state) | the open-world options of the common hacks (`DefenseRefactor` 364 with `Math.Pow`, `BossElementRando` 826, `characterColors=custom`) come with it |
| 6 optional / droppable | Plando 814, Nesoberi 626 (Bitmap, drop), XmasMapPalettes 821, OpenWorldWinterTheme 401, XmasRandoData, Pokedex data, AISwap, BlackMapPalettes, RemoveItemEventCommand 584 | seeds that do not enable them are unaffected: refuse them up front |

Method (as in phase 3): a per-component replay against an instrumented oracle (dump hooks in a private copy of the C# tree, as `oracle-dbg`), workers per
step in private workspaces, whole-ROM comparison for the five presets and the option matrix last. Estimated effort: 2 to 3 times the Ancient Cave phase.
