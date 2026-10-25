# som-dualscreen CLI, randomized games and the in-launcher flow (board items T4/T5)

Date: 2026-10-17. Code: `tools/companion/cli/som-dualscreen` (Python 3, standard library only), `tools/companion/cli/variants.json`,
`tools/companion/randomizer/` (headless build of the upstream randomizer, C ABI, build script). Source and licence audit,
feasibility and the C port plan: `docs/companion-cli-audit-20261017.md`. Variant facts: `docs/companion-ancientcave-20261017.md`.
Working data (ROMs, builds, reports) lives under `/home/cidwel/.local/state/som-decomp/companion-cli-20261017/`
(override with `SOM_CLI_STATE`); the CLI refuses to write a ROM anywhere inside the repository.

## Answers

- (A) A dual-screen version of any ROM is easy because the companion never patches the ROM: it reads WRAM and injects buttons.
  The CLI only recognises the variant, picks the address table and the behaviour record, and copies the ROM plus a small JSON to
  where each app looks for it. Recognised: vanilla US v1.0, the Magno Spanish translation, and the randomizer's 4 MiB ROMs.
  Any other ROM (Japanese, European, other hacks) is reported as `unknown` and the record says macros are refused.
- (B) Randomized games work end to end on the PC today: `randomize` builds the upstream generator from source and produces the
  ROM, `package` places it, `verify` plays it headless. Generating on the device needs a C port of the generator (plan, cost and
  licence in the audit); until then the launcher can open ROMs that the CLI made.

## Commands

```
som-dualscreen inspect <rom> [--json]
som-dualscreen setup-randomizer
som-dualscreen randomize --mode ancientcave|open|rando|bossrush|chaos --seed N --rom <vanilla us> --out <path>
                         [--difficulty casual|hard|reallyhard] [--option key=value]...
som-dualscreen package --rom <rom> --target android|3ds --out <dir>
som-dualscreen verify <rom>
```

| Command | What it does | Exit codes |
| --- | --- | --- |
| `inspect` | Strips a 512 byte copier header, hashes the image, reads the header, decides the variant and prints the record and the WRAM address table to use (34 fields; the Ancient Cave table prints its own column). `--json` prints everything. | 0 known, 3 unknown |
| `setup-randomizer` | Clones `Moppu/SecretOfManaRandomizer` at tag `v1.49` (commit `1404b7b`) into the state directory (or uses `SOM_RANDO_SRC`), builds `tools/companion/randomizer/headless` with `dotnet build` (SDK 10 found on `PATH`, `DOTNET_ROOT` or `~/.dotnet`). Needs network once. | 0 / 1 |
| `randomize` | Accepts only the US v1.0 2 MiB ROM (the generator refuses anything else), runs `somrando.dll`, writes `<out>.part` then renames it (the generator creates its output file before checking its input, so a failure used to leave an empty file), writes `<out>.json` with seed, mode, difficulty, options, source SHA-1, output SHA-256, generator commit and seconds, then re-runs `inspect` on the result. The seed is passed as the same string the GUI takes. Logs and the spoiler log stay in `work/<mode>_<seed>/`. | 0 / 1 / 2 |
| `package` | Writes the headerless ROM as `rom.sfc` and a per-variant config `som_variant.json` (Android) or `variant.json` (3DS) containing the variant record and the whole address table, plus `install.txt`. For Android it prints `adb shell mkdir` and two `adb push` lines and never runs them; for the 3DS it prints the SD card folder to copy. | 0 / 1 / 3 |
| `verify` | Headless smoke test on the libretro snes9x core through `tools/companion/common.py`, `build_fixture.py` (vanilla and Magno: input route to a controllable hero, fixtures cached by variant and SHA-1) or `ancientcave/build_ac_fixture.py` (name entry, floor 1). Then: mode `field` and safe to inject, map id, party read from WRAM, a WRAM write seen by the game (HP 20), the Candy "use item" macro, HP healed, ring closed. Writes `reports/verify_<variant>_<sha1>.json`. Other randomizer modes report "no headless fixture" and exit 3. | 0 pass, 1 fail, 2 environment, 3 no fixture |

Paths printed by `package` follow `tools/companion/android` (`/sdcard/Android/data/org.somdecomp.companion/files/rom.sfc`) and
`tools/companion/n3ds` (`sdmc:/som/rom.sfc`); they live in `variants.json` (`targets`) because the other agent may move them.
Nothing in the apps reads `som_variant.json` yet; it is the contract proposed in the next section.

## Proof, headless, 2026-10-17

All four ran through `som-dualscreen verify` with the snes9x 1.63 libretro core; seven checks each, all passed.

| ROM | Variant detected | Boot to free play | Map | Party HP | Candy macro | Report |
| --- | --- | --- | --- | --- | --- | --- |
| Secret of Mana (USA) | `vanilla_us` (known SHA-1 `8133041a`) | route frame 23000, 18 s including the fixture | 131 | 50 / 45 / 40 | done, 255 frames, HP 20 to 50 | `verify_vanilla_us_8133041a.json` |
| Magno T-Esp 1.01.2 | `magno_es` (title `Secret of Mana (ESP)`) | route frame 20000, 16 s | 128 | 50 / 45 / 40 | done, 257 frames, HP 20 to 50 | `verify_magno_es_8d6ebfcf.json` |
| Ancient Cave, seed 777, made by `randomize` (casual preset, default options, music randomizer on) | `randomizer_ancientcave` (45 of 45 hook sites, ``ancient`cave``) | name entry then floor 1, 3.5 s | 0 | 50 / 45 / 40 | done, 244 frames, HP 20 to 50 | `verify_randomizer_ancientcave_4a44e9c3.json` |
| Ancient Cave, seed 4242, same command | same | 3.1 s | 0 | 50 / 45 / 40 | done, 244 frames, HP 20 to 50 | `verify_randomizer_ancientcave_b39bcff8.json` |

Generation timing from the CLI on lumina: 2.9 s (seed 777) and 4.1 s (seed 4242). The two Ancient Cave ROMs here use the real
casual difficulty preset read from the upstream resources (starting gold 5000, 3 consumables), unlike the ROMs of the earlier
Ancient Cave study, which set every difficulty field to 1.

What this does and does not prove: the address table and the "use item" macro work unchanged on the new ROMs, and detection
separates the four files. It does not play past floor 1, does not fight, and does not check the other randomizer modes beyond
generating them (all five were generated and `inspect` names each: rando 31 of 45 hook sites, open 32, bossrush and chaos 45,
distinguished by the title strings `randomizer`, `open world v1.49`, ``boss`rush``, ``chaos`mode``; no boot beyond the title was checked).

Other CLI checks: `randomize` on the Magno ROM exits 2 with the reason; an output path under the repository is refused for
`randomize` and `package`; `inspect /etc/hostname` returns `unknown` and exit 3; `verify` on an Open World ROM exits 3.

## Per-variant behaviour records (`tools/companion/cli/variants.json`)

One address table serves every variant (no address moved). The record carries what differs:

| Variant | Table | Macros | HUD first row | Notes |
| --- | --- | --- | --- | --- |
| `vanilla_us` | `data/wram_companion_map.json` | full | 199 | |
| `magno_es` | same | full | 199 | |
| `randomizer_ancientcave` | `data/wram_companion_map_ancientcave.json` (`ancient_cave_address`) | full | 184 | generated map ids, item ids 6 and 7 renamed, weapon slot holder bits, weapon ring order 1..7 then 0 with a cursor relative to the held weapon |
| `randomizer_open`, `_rando`, `_bossrush`, `_chaos` | `data/wram_companion_map.json` | `read_only_until_verified` | 199 | generated, not verified; boss rush and chaos use generated map ids |
| `unknown_hooked_4mib` | same | `read_only` | 199 | randomizer hooks without a known mode string |
| `unknown` | none | `refuse` | none | |

The detection rule is the one proposed in the Ancient Cave study, extended with the mode strings: size 2 MiB gives vanilla (known
SHA-1 or the US v1.0 header) or Magno (title); size 4 MiB needs the title `Secret of MANA`, at least half of the 45 `JSL` hook
sites in bank C0 pointing into bank `E0` or higher (list in `data/companion_ancientcave_20261017.json`) and a mode string in
`0x200000..0x23FFFF`. Whole-ROM hashes are not used for randomized ROMs.

## Design: "New game" in the dual-screen launcher

### Menu (wireframe in words)

Thor, top screen: the game list with a cover line per entry (name, variant, seed). Bottom screen (touch): the menu. 3DS: top
screen list, bottom screen menu, same text.

1. Title "Secret of Mana". Three large rows: `Normal game`, `Randomized game`, `Load`.
2. `Normal game`: shows the base ROMs found (vanilla US, Magno if present); tap one, it starts. A ROM that `inspect` calls
   `unknown` is listed greyed with the reason ("Unknown ROM, macros disabled") and can still be started read-only.
3. `Randomized game` opens a mode list: `Ancient Cave`, `Open World`, `Vanilla randomizer`, `Boss Rush`, `Chaos` (only the modes
   the installed generator supports; others hidden, not greyed).
4. Ancient Cave form, one screen, fields in this order: Seed (text, default a random 8 digit number, button `New seed`),
   Length (Short 8 floors, Medium 16, Long 24), Difficulty (Casual, Hard, Really hard), Boss frequency (Every floor, Every few
   floors, Final only), Characters (boy, girl, sprite switches), Biomes (five switches), Dialogue (Mitch Hedberg, Demetri
   Martin), Random music (switch), then `More options` (the common QoL toggles, collapsed). Buttons: `Generate and play`,
   `Generate only`, `Back`. The seed line under the title shows the share code (below).
5. `Load`: lists existing saves, each with variant, seed and play time; selecting one starts the ROM it belongs to.

### Where things are stored

| Item | Android (app files dir) | 3DS (SD card) |
| --- | --- | --- |
| Base ROMs, copied once from the picker or `/som/rom.sfc`, checked by SHA-1 | `roms/base/<sha1>.sfc` | `sdmc:/som/base/<sha1>.sfc` |
| Generated ROMs | `roms/gen/<key>.sfc` plus `<key>.json` (the same JSON the CLI writes) | `sdmc:/som/gen/<key>.sfc`, `<key>.json` |
| Saves (SRAM) and states | `saves/<rom sha1>.srm` | `sdmc:/som/saves/<rom sha1>.srm` |
| Logs and spoiler log | `logs/<key>.txt`, `<key>.spoiler.txt` (hidden behind a confirm, spoilers) | same |

The cache key is the first 16 hex digits of SHA-256 over `generator version | mode | seed | canonical sorted option list |
difficulty | source ROM SHA-1`, so a changed option or a different base ROM never reuses a ROM. An existing file with a valid JSON
and the right size is reused without regenerating. Eviction: keep the 8 newest unplayed ROMs; never delete a ROM that has a
save. The user deletes explicitly; there is no silent cleanup.

### Share code

`AC149-<seed>-<options>`, where `<options>` is empty for defaults or a base32 string of the changed keys (for example
`len=long,diff=hard`). Pasting it into the Seed field fills the form. Two people with the same code and generator version get the
same ROM; the original app uses the same seed string and options, so the code can also be typed into the Windows app (the GUI
needs the options set by hand). The generator version is part of the code: a different upstream version changes ROM bytes.

### Choosing the address table

At load, before the core starts, the launcher runs `inspect` (size, header, hook sites, mode string; the C version is about
80 lines and needs no JSON parser at run time because the records are compiled in) and picks the record from `variants.json`.
Macros are enabled only when the record says `full`. The result is also written next to the ROM as `som_variant.json` so a
frontend that does not link the detector can read it. A mismatch between the sidecar and the detector wins for the detector.

### Progress

The generator runs on a worker thread (Android) or a second thread on the 3DS and reports (step, total, name): the pipeline
has about 90 named steps (`Adding: [Weapon randomizer]` ... `Adding: [Title screen changes]` in the log), so the bar shows the
step name and a percentage. Cancel is cooperative between steps. During generation the core is not running, so the memory
budget is the whole application. Expected time: seconds on the Thor; minutes on an old 3DS (estimate, to be measured).

### Errors (message, then what to do)

| Situation | Message | Action offered |
| --- | --- | --- |
| No base ROM | "No US Secret of Mana ROM found." | `Choose ROM` |
| Base ROM is not US v1.0 (for example Magno) | "The randomizer needs the US v1.0 ROM. This one is the Spanish translation." | `Choose another ROM` |
| Not enough space | "Not enough space for a 4 MB ROM." | `Open storage` (Android) / delete a generated ROM |
| Generator error | "Generation failed at step N (name). Seed X was not saved." plus the generator message | `Retry`, `Copy log` |
| Output fails the sanity check (size not 4 MiB, hooks missing) | "The generated ROM looks wrong and was discarded." | `Retry` |
| Cancel | "Cancelled. Nothing was kept." | back to the form |

Writes go to `<key>.sfc.part` and are renamed only after the size and hook-site check, so an interrupted run leaves no valid
file. The launcher never starts a ROM whose detector result is `unknown` with macros on.

### Companion behaviour inside the game

After launch the status screen shows the variant label and, for Ancient Cave, the floor clock row. The HUD crop row and item
names come from the record (`hud_first_row`, `item_name_overrides`). Equip macros on Ancient Cave need the two fixes listed in
`docs/companion-ancientcave-20261017.md` section 3 (ring order, relative cursor); until applied to `som_companion.c` the weapon
equip macro is right only for 7 of 64 held/wanted pairs there.

## Reproduce

```
tools/companion/cli/som-dualscreen setup-randomizer
tools/companion/cli/som-dualscreen randomize --mode ancientcave --seed 777 --rom "<US ROM>" --out <dir outside the repo>/ac_777.smc
tools/companion/cli/som-dualscreen verify <dir>/ac_777.smc
tools/companion/cli/som-dualscreen package --rom <dir>/ac_777.smc --target android --out <dir>/pkg
tools/companion/randomizer/build_native.sh exe-aot     # optional: NativeAOT executable (needs gcc)
tools/companion/randomizer/build_native.sh lib-aot     # optional: C ABI shared library, test with randomizer/test_native.c
```

## Open items

- The launcher code (other agent) does not read `som_variant.json` yet; Phase 1 of the plan in the audit.
- Verify and fix the equip macro for Ancient Cave in `som_companion.c`.
- Play the Open World, vanilla randomizer, Boss Rush and Chaos ROMs through a fixture before enabling macros there.
- C port of the generator (audit section 2, phases 2 to 4) and the device measurements.
- Decide on the third-party quote tables and the music pack before any public release.
