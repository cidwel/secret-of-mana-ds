# Companion direct control (no ring menu), 2026-10-17

Stage 6 of the T4 dual-screen companion. Goal: on the bottom screen one tap changes weapon, uses an item or casts a spell
on a chosen target, with no ring menu, no ring animation and no waiting. Works on vanilla US, Magno and Ancient Cave
with no ROM patching. All code is in `tools/companion/direct/` (pure C99 `som_direct.[ch]`, Python test harness).

Status: PC prototype complete and verified headless against the real ring on vanilla, Magno, AC seed 1 and AC seed 777.
Not yet integrated in the Android or 3DS app (patch proposal below). Stage 6 is therefore not marked done.

## 1. Result in one page

| Command | API | Frames from tap to effect | Ring opened |
|---|---|---|---|
| Equip weapon (incl. swap with another hero) | `somd_equip` | 2 (swap 4-5), 0 when already held | never |
| Use item on a hero | `somd_use_item` | 2 | never |
| Cast on one ally / one enemy | `somd_cast(..., {ONE, slot})` | cast pose starts at frame 1-2 | never |
| Cast on the whole party / all enemies | `somd_cast(..., {ALL_ALLIES / ALL_ENEMIES})` | 1-2 | never |

The ring path of the same actions (button macro, `tools/companion/som_companion.c`) needs 420-617 frames before the cast
starts. The direct cast reaches the game's "phase 0x80" (spell effect phase) 0-5 frames after the ring-driven one, so the
spell behaves identically; only the menu time is gone.

## 2. Reverse engineering: where the commit happens

Everything below was read from the recovered sources (`src/rom/*.s`) and confirmed in WRAM. Addresses are identical in
vanilla, Magno and AC (checked by byte prefix of each routine by `somd_init`, `SOMD_ERR_UNSUPPORTED_ROM` otherwise).

| What | Where |
|---|---|
| Rebuild stats of a hero | C0:0036 (X = hero*0x200) |
| Weapon graphics refresh | C0:E9F4 (X = record offset; needs `$E060 == 0`, clears `$E069`/`$E104`) |
| Item effect | C0:002A (A = item id, X = target hero offset) |
| Item quantity decrement and compaction | C0:78C3 (A = 0xFF, X = record) |
| Cast prepare (level, MP check, `A low == 0xFF` means MP short) | C0:001B |
| Cast start | C1:DA1E = `JSR $88C7 ; RTL` (long entry of C1:88C7, X = caster slot*0x200) |
| Weapon marks | `$CC54 + type`: owner bits `(hero+1) << 6`, tier `byte & 0x3F`; `$E1E3 = $E1E8 = type*9 + tier`, `$E1E4 = type` |
| Item records | `$CC48 + rec`: id = low 5 bits, quantity = high 3 bits |
| Cast request block (caster record `0xE000 + 0x200*hero`) | `+0x170` spell id, `+0x177` spirit position, `+0x176` target count, `+0x178..17A` target slot+1 (heroes 1-3, enemies 4-6) |
| Spell id | `6 * spirit_position + (girl ? 3 : 0) + slot`, position 0..6 |
| Spell table row (ROM `0x102AD0 + id*64`) | byte 8 target flags, byte 15 MP cost (the cost differs in AC, so it is read from the ROM at run time) |

Weapon swap: if another hero holds the wanted weapon the game gives that hero the weapon the caller holds and rebuilds
and refreshes both. If the caller holds nothing the swap is refused, exactly as the ring does (`bad-argument`).

Actor table (the "mini-map" data) is the object table at `$7E:E000 + 0x200*slot`:

| Field | Offset |
|---|---|
| active byte (1 = present) | +0x00 |
| world x / y | +0x02 / +0x04 |
| screen x / y | +0x20 / +0x22 |
| identity | +0x180 |
| hp / max hp | +0x182 / +0x184 |
| MP | +0x186 |
| status word | +0x190 |
| flags (bit 7 = dead) | +0x191 |

Slots 0-2 are the party, slots 3-5 are the monsters the game itself can target (`E600/E800/EA00`). The game's own target
test (reconstructed from D0:DAA1) is what `somd_scan_targets` reproduces: active == 1, screen x in `[8, 0xF8)`, screen y
`< 0xD8`, alive (flags bit 7 clear, or id 0x54/0x55), and for hostile spells flags bit 7 of `+0x180` clear.

## 3. Choosing the mechanism (measured)

Three options were evaluated against the ring as reference.

| Option | Verdict |
|---|---|
| (a) Pure WRAM writes | Not sufficient. Equip leaves stats and the weapon sprite stale until some unrelated refresh; items skip their effect routine; a cast request written to `$E170..` is only consumed by the cast routines. The ring mailbox `$19F0..19F3` is read only at ring exit (C1:DB6B from C1:CB38), so it is unusable without opening the ring. |
| (b) Emulator-side call of the game routine | Correct, but it needs CPU register access (set A/X/PC), which the libretro API does not expose and which differs per core, so it is not portable to the Android and 3DS cores. |
| (c) Inject a "ring closed" state | Rejected: faking ring state still runs the ring exit path and its animation, and needs the ring to be open first. |
| **Chosen: WRAM-resident call stub** (a + b without registers) | Writes the same state the ring would commit, then makes the game call its own routine from inside its own frame. 100 % differential match, no core feature needed beyond WRAM read/write. |

How the stub works (`somd_pre_frame` / `somd_post_frame`), second design (see section 11 for why it changed):

1. The game keeps its NMI vector in WRAM at `$00:0100` (`JML C0:C0A8`). `somd_pre_frame` writes a two-part stub and a 4-byte
   mailbox at `7F:FF90..FFFF` and redirects that JML to the stub.
2. Part 1 (runs on the NMI): if the interrupted PC is the main loop spin `BIT $EC; BNE` (C0:B079/B07B) it copies the stacked
   return address into part 2, replaces the stacked return with part 2 (bank 7F), puts the trampoline back and jumps to
   the game's original NMI. If the PC is anywhere else it jumps to the original NMI and the hook stays armed.
3. The game's own NMI body runs to completion inside vblank and its RTI returns into part 2. Part 2 runs in the main
   thread's context: saves A/X/Y/D/B, sets DB=7E and D=0, calls ONE routine with JSL, stores A and status 2 in the mailbox,
   restores everything and jumps back to the interrupted spin.
4. One routine per frame; multi-call commands (item effect then quantity compaction, equip rebuild then refresh) run over
   consecutive frames, which is where the 2 frame latency comes from.
5. `somd_post_frame` reads the mailbox. If part 1 never ran for 30 frames the command is rolled back from an undo list
   (`SOMD_ERR_TIMEOUT`); if it ran but part 2 has not finished it waits up to 120 frames. The bytes under the stub are
   given back one frame after the call finished (part 2 may still be executing its last instructions).

The ROM is never modified. The stub area `7F:FF90-FFFF` must be free (it is unused by the game in all four ROMs; the
original bytes are saved and restored).

## 4. Target classes (data: `tools/companion/direct/companion_target_classes.json`)

`somd_target {kind, slot}`: `kind` is `NONE`, `ONE` (slot 0-2 hero, 3-5 enemy), `ALL_ALLIES` or `ALL_ENEMIES`.

| Class | Meaning | Spells (ids, same in all four ROMs) | Bottom-screen cards |
|---|---|---|---|
| enemy-one-or-all | one enemy or all enemies | 24 spells: 0-2, 6-8, 12-14, 16-20, 22, 24-26, 30-31, 36-38, 40 | enemy cards + "all enemies" |
| ally-one-or-all | one ally or the whole party | 13 spells: 3-5, 9-11, 15, 21, 27-28, 34, 39, 41 | party cards + "whole party" |
| ally-one | one ally only | 29 | party cards |
| enemy-one | one enemy only | 23 (Analyzer) | enemy cards |
| ally-dead-one | a fallen ally | 33 | cards of fallen heroes |
| boy-only | caster-bound weapon-change spells | 32, 35 | one card (the boy) |

Items (`somd_item_class`): ids 0-3, 8-10 ally-one; id 4 ally-status-one (target must have a curable status, mask
`0x73FC`); id 5 ally-dead-one; ids 6 and 7 modal (`SOMD_ERR_NEEDS_RING`, they open their own UI); ids 11-12 have no field
effect. Ids 8 and 9 keep their quantity. Item targets are always one hero. Equip needs no target.

MP costs per spell and per variant are in the JSON (`mp`), read from each ROM. The classes are identical in all four ROMs;
costs differ in AC (ids 8, 13, 14, 19, 20, 22, 24, 26, 34, 36-38, 40).

Per-spell identity vs ring: hero = girl for `(id % 6) >= 3`, sprite otherwise; `spirit_pos = id / 6`; `slot = id % 3`.
The E3C8 mask index of the macro path equals `spirit_pos`, except the girl's Lumina (mask index 7 = position 6).

## 5. Tap-to-target mapping and bottom-screen layout

Rendered reference (`render_minimap.py`, 320 x 240 bottom screen):

* Mini-map area: 320 x 200 at the top, showing the 256 x 224 game screen scaled to fit (`somd_minimap_point`). Enemy
  boxes are numbered, valid targets get a white outline and an HP bar; the touch point maps to the nearest valid target
  within a radius via `somd_pick` (default 24 px; hostile classes prefer enemies).
* The three heroes normally stand on the same pixel, so they are not pickable on the map. Allies are chosen with the
  card strip at y 200-240 (card i at x = 2 + 72*i, 70 x 36). The reference renderer fans the three hero markers out
  by 12 px only for display.
* Group card: `somd_group_card()` returns 1 ("whole party") or 2 ("all enemies") when the class allows a group and at
  least one member is valid; the app draws it as a button after the cards and sends `ALL_ALLIES` / `ALL_ENEMIES`.
* Enemy cards: one per valid enemy slot, "ENEMY n, HP". Tapping a card or its map marker sends `{ONE, slot}`.

Evidence PNGs: `tools/companion/direct/evidence/minimap/` (vanilla map 344, Magno map 365, AC1 injected).

## 6. Measured results

All runs headless with the snes9x libretro core used by the companion, state fixtures made at the maps listed.
Each test runs the ring macro on a copy of the same state as reference and compares semantic WRAM fields (HP, MP, status,
timers, item records, weapon marks, cast request block).

| Test | vanilla | Magno | AC seed 1 | AC seed 777 |
|---|---|---|---|---|
| Equip, all weapon/hero combinations (ring vs direct) | 48/48 | 16/16 | 192/192 | 192/192 |
| Item use (ring vs direct) | 12/12 | 12/12 | 14/14 | 14/14 |
| All 42 spells (ring vs direct) | 41/42 first run, 42/42 after fix | 39/42 first run, 42/42 on rerun | 36/42 first run, 42/42 on rerun | 41/42 (see note) |
| Target-class suite (21-22 checks) on maps 313/344/357/365 | 20/20 on each of 4 maps | 20/20 on each of 3 maps (344/357/365) | 22/22 (injected enemies) | 22/22 (injected enemies) |
| Ring flag `$7D` during direct casts | 0 on every frame | 0 | 0 | 0 |

Notes on the non-clean first runs:
* vanilla spell 25: MP changes by RNG at payment; the metric was changed to MP at payment time (`mp_paid`).
* Magno 27/28/18 and AC1 10/11/29/24/31/32: the first runs shared scratch file names between parallel jobs. Reruns with
  per-process names are 0 mismatches (`evidence/cast_*_run1.json` hold the first run, the reruns are in the log).
* AC777 spell 5: the ring run leaves a stale byte at request `+0x175` (`d0` vs `00`). Target bytes, MP, timing and
  effect are identical; deterministic, not a behaviour difference.

Target-class suite checks (per map): scan of enemy and ally classes, single enemy cast, cast on all enemies, cast on all
allies, single ally that is not the caster, refusal of an ally spell on an enemy and of an enemy spell on an ally,
refusal of group target for single-target spell (Analyzer), refusal of the boy as caster, no MP, unowned spirit, busy
(second command while running), caster busy, unsafe states (event, ring open, fade, input lock), rollback on an event
starting mid-command, modal items 6 and 7, no target. Logs: `evidence/targets/*.log`.

Maps with enemies used (id: variant): 313, 344, 357, 365 in vanilla; 344, 357, 365 in Magno; AC1/AC777 use a vanilla
mob map injected into the AC ROM (see limits). Tap-to-cast demonstration (`demo_minimap.py`): for each of 8 fixtures a
tap on an enemy marker resolves to the right slot, the cast request block names that slot, and a tap on the third party
card heals that hero, with `$7D == 0` throughout. 8/8 fixtures, both cases OK.

Visual cleanliness: top-screen frames before/after the direct commands (`*_top_after.png`) show the field with the cast
banner only (the game's own spell-name line), no ring, no HUD overlay and no menu. The ring flag stayed 0.

Latency (unchanged by the black strip fix): equip 2 frames (swap 4-5); item 2 frames; casts reach the pose at frame 1 (vanilla, Magno) or 1-2 (AC), phase
0x80 0-5 frames after the ring-driven timeline; the ring route takes 420-617 frames.

## 7. Failure handling (error path for the apps)

`som_direct` commands return immediately with a `somd_err`; if accepted they run for a few frames and then
`d->state` is `SOMD_FINISHED` or `SOMD_FAILED` with `d->err`.

| Error | Cause | UI behaviour |
|---|---|---|
| `UNSAFE_STATE` | not in field mode: title, dialogue/event (`$D0 != 0`), fade, ring menu open, input lock (`$F1` bit 7), no living pad hero | grey out the screen (the UX agent already shows this per game mode) |
| `NO_MP` | MP below the ROM cost | flash the MP bar |
| `CASTER_BUSY` | caster swinging, hurt, casting, dead | retry or ignore |
| `BAD_TARGET` | off-screen, dead, wrong side, group not allowed | refresh the cards |
| `NO_TARGET` | group requested with no valid member | hide the group card |
| `NOT_OWNED` / `BAD_ARGUMENT` | weapon not owned, empty record, unknown spirit | refresh lists |
| `NEEDS_RING` | modal items (Magic Rope, Flammie Drum) | fall back to the macro path |
| `BUSY` | previous command still running | queue in the app (a command lasts 2-5 frames) |
| `TIMEOUT` | main thread never parked in 30 frames (rolled back) | retry |
| `REJECTED` | the game refused the cast pose | refresh state |
| `UNSUPPORTED_ROM` | routine prefixes differ (unknown hack) | disable direct mode, use the macro |

A command that fails after its first write rolls the writes back from the undo list, so no half-applied equip or item use
remains (tested: event starting mid-command).

## 8. Limits

* AC enemy fixtures are a vanilla mob map injected into the AC ROM (monster records written into the object table);
  AC floors beyond the first were not reached. The classes, spell table and routines are proven on AC; real AC monsters
  at several floors were not.
* Items 6 and 7 (Magic Rope, Flammie Drum) are modal and return `NEEDS_RING`.
* Taking another hero's weapon while holding none is refused (the ring does the same).
* Only monsters in slots 3-5 are targetable (a game limit). Off-screen enemies (outside `[8,0xF8) x [0,0xD8)`) are not
  valid; the cards should show them greyed or hide them.
* The call needs the main thread parked at the main-loop spin. During heavy-lag frames the stub waits (30 frame timeout with rollback).
* `7F:FF90-FFFF` must stay free. All four ROMs leave it unused.
* A dead pad hero counts as unsafe (the game is in a revive/game-over path).
* One ROM per process in the PC harness (libretro core keeps global state); the apps already run one core.
* Not verified on hardware yet: the Thor, 3DS builds.

## 9. Integration API for Android and 3DS

Files to add to the app build: `tools/companion/direct/som_direct.c` and `som_direct.h` (pure C99, no libc beyond
`string.h`). Optional glue patch for `som_companion.[ch]`: `tools/companion/direct/som_companion_direct.patch` (applies
clean with `git apply -p1`; compile-checked). The patch adds `som_cmd_direct()` which converts the macro's `struct som_cmd`
(hero, arg, spell) to a direct call, so the existing command layer can switch per action.

Per core instance:

```c
struct somd direct;
somd_init(&direct, rom_bytes, rom_size);        /* ROM is read-only; read once, keep the pointer alive */

/* every emulated frame */
somd_pre_frame(&direct, wram);                  /* before retro_run() */
retro_run();
somd_post_frame(&direct, wram);                 /* after retro_run() */
```

`wram` is the 128 KiB WRAM image `$7E:0000-$7F:FFFF` (read and write access is required: snes9x, snes9x2005 and the 3DS
core already expose it through `retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM)`). The write access is the only core
capability needed.

Commands (call from the UI thread between frames):

| Call | Notes |
|---|---|
| `somd_equip(&d, wram, hero, weapon_type)` | hero 0-2, type 0-7 |
| `somd_use_item(&d, wram, record, (struct somd_target){SOMD_T_ONE, hero})` | record 0-11 |
| `somd_cast(&d, wram, hero /*1 girl 2 sprite*/, spirit_pos /*0-6*/, spell_slot /*0-2*/, target)` | target as in section 4 |
| `somd_sound(&d, wram, sfx_id)` | game menu sounds, see section 12 |
| `somd_busy(&d)`, `d.state`, `d.err`, `d.frames` | poll; `somd_err_name(err)` for logs |

Target cards: before drawing, call

```c
enum somd_class c = somd_spell_class(&d, somd_spell_id(hero, spirit_pos, slot));  /* or somd_item_class(id) */
struct somd_targets t;
somd_scan_targets(wram, c, id, /*is_spell*/1, &t);    /* once per frame while the target screen is shown */
/* t.actor[0..5]: slot, present, on_screen, alive, id, sx, sy, hp, hp_max, status, valid */
/* somd_group_card(&t): 0 none, 1 whole party card, 2 all enemies card */
/* somd_minimap_point(&t.actor[i], map_w, map_h, &px, &py)  -> where to draw */
/* somd_pick(&t, tap_x, tap_y, map_w, map_h, radius_px, &target) -> 1 when a valid target is under the tap */
```

Data for UI lists: `somd_spell_mp_cost(&d, id)` (per ROM), `companion_target_classes.json` for the static table (ids and
classes only; names stay in the app's own assets).

Safe flag for greying out the screen: a command returns `SOMD_ERR_UNSAFE_STATE` immediately; the app can poll with a
no-op equip (`somd_equip` of the held weapon returns OK/finished with note "already held" in the same state) or reuse
`som_read_state().safe_to_inject`.

Latency budget for the app: command accepted at frame N, effect visible at N+2 (equip, item) or the cast pose at N+1..2.
Do not feed `retro_run` pad input for the command; the stub works with whatever pad state the user is holding.

Patch for `som_companion.[ch]` (not applied): `tools/companion/direct/som_companion_direct.patch`.

## 10. Reproducing

```
cd tools/companion/direct
python3 test_direct_equip.py VARIANT      # ring vs direct, weapons
python3 test_direct_item.py VARIANT       # ring vs direct, items
python3 test_direct_cast.py VARIANT STATE # ring vs direct, 42 spells
python3 test_direct_targets.py VARIANT STATE   # target classes, groups, failures
python3 demo_minimap.py VARIANT STATE OUTDIR   # tap-to-cast with mini-map PNGs
python3 gen_target_classes.py             # companion_target_classes.json
```
Needs the legally owned ROMs (paths in `tools/companion/common.py`) and the snes9x libretro core. Fixture states live in
`~/.local/state/som-decomp/companion-direct/` (not in the repo); `make_mob_states.py` builds the enemy maps.

## 11. Black strip on the top screen (found on the Thor, fixed)

Symptom: a black strip on the top screen for one frame when a weapon was changed.

Reproduced headless (`blackhost.py`, `direct_host.c` command `BLACKSCAN`, which lists fully black rows per frame):

| Case (old stub, routine inside the NMI) | Black rows |
|---|---|
| vanilla, equip, frame 5 (second call, weapon graphics refresh) | rows 155-167 (13 rows) |
| vanilla, cast (second call, cast start) | rows 141-155 and 163-177 (15 rows, position varies) |
| AC1 / AC777, equip | rows 155-166 (12 rows) on top of the static dungeon black rows |
| vanilla item (second call is the quantity compaction, no graphics) | none |
| real ring equip, 240 + 120 frames | none |

Cause: the old stub called the game routine at the start of the NMI, before the game's own NMI body. The weapon-graphics
refresh and the cast start load graphics and queue VRAM/palette work, so the routine ran long enough that the original NMI
body started late, after the beam had left vblank. That body does its DMA under forced blank, and the forced blank showed
as a band of black rows at the beam position (the start row changes with the routine's run time, 12-15 rows long). Routines
without graphics work (rebuild stats, item effect, quantity compaction) were short enough to stay inside vblank. The real
ring path never does this because the main thread runs the routine, after the NMI, like any other game code.

Fix (som_direct.c): the stub now lets the game's NMI body run first and runs the routine in the main thread by redirecting
the NMI's RTI (section 3). No work is done inside the NMI any more. Cast start uses the long entry C1:DA1E because the
JSR-style routine no longer needs a hand-built return frame.

Verification (`blackall.py`, compares per-frame black rows against a no-command control run of the same state):

| Variant | Cases (equip x2-3, item x2, cast enemy one/all, cast ally one/all) | Black frames added by the command |
|---|---|---|
| vanilla | 8 | 0 |
| Magno | 8 | 0 |
| AC seed 1 | 9 | 0 |
| AC seed 777 | 9 | 0 |

Evidence: `tools/companion/direct/evidence/blackrows_after_fix.jsonl`. Differential tests re-run on the new stub with the
same results as before: equip 48/16/192/192, item 12/12/14/14, 42 spells per ROM (AC777 spell 5 byte `+0x175` as before),
target-class suite 20/20/22/22 on every map listed in section 6. Latency is unchanged (equip/item 2 frames, swap 4-5).
The criterion is fully black rows; a smaller colour glitch would not be seen by it. If the core is the widescreen build
and a strip still shows, repeating the previous frame during the commit is the documented fallback, but nothing in the PC
core needs it.

The Android build must be rebuilt with the new `som_direct.c/.h` (the struct `somd` gained a field and the stub changed;
the API of the commands is unchanged).

## 12. Menu sounds (`somd_sound`, requested by the Android agent)

`int somd_sound(struct somd *d, const uint8_t *wram, int sfx_id)`, `#define SOMD_HAS_SOUND 1`. Plays one of the ten
fixed menu presets (D0:CEF8 + 8*n for ids 0C 0D 0E 29 2A 2B 2C 4B 28 A3; they preserve A and P) through the same stub.
No field check, so it works in the ring, menus and events; `BUSY` while another command runs; `BAD_ARGUMENT` for other
ids; `UNSUPPORTED_ROM` if the preset bytes are not where expected. Safe points: the field spin C0:B079 and the ring
and menu vblank wait C0:694A..694E (found by sampling the interrupted PC in ring mode). A call that never reaches a safe
point times out after 30 frames, restores the NMI vector and leaves `FAILED / TIMEOUT`; music and requests are untouched.
Test `test_direct_sound.py`: vanilla, Magno and AC1, field, ring open (`$7D == 1`) and event-flag states, 5 ids each,
all finished in 1-2 frames with audio output different from a no-command control; bad id and busy checks pass.

## 13. Direct menu jumps (quick slots that open a ring, a spirit list or a page)

User feedback: a quick slot that opened a menu showed several rings and animations in a row before reaching the target. Now
`somd_open_menu` goes straight to the target and `somd_close_menu` leaves in one step, with no intermediate ring ever
displayed. API in `som_direct.h` (`SOMD_HAS_MENU`):

```c
int      somd_open_menu(struct somd *d, const uint8_t *wram, int kind, int hero, int arg);
int      somd_close_menu(struct somd *d, const uint8_t *wram);
uint16_t somd_menu_pad(const struct somd *d);      /* pad bits to OR into the core input this frame */
int      somd_hide_video(const struct somd *d);    /* 1: do not present this frame */
```

| kind | arg | Quick-slot binding (`som_ux_bind_kind`) | Result |
|---|---|---|---|
| `SOMD_MENU_RING` | ring category 0 item, 1 magic, 2 config, 3 weapon | `SOM_UX_BK_MENU` (id = category) | that ring of `hero`, ready, on its saved entry |
| `SOMD_MENU_SPIRIT` | spirit mask index 0..7 | `SOM_UX_BK_SPIRIT` | the girl's or the sprite's magic ring on that spirit and its 3-spell list open (`$1803` bit 7, `$180B` = 3, `$1839` = first spell id) |
| `SOMD_MENU_PAGE` | config entry 0..6 (Equip Armor, Targeting, Status, Levels, Action Grid, Controller, Window) | `SOM_UX_BK_SUBMENU` (id = entry) | that screen open |

hero 0 boy, 1 girl, 2 sprite. The boy has no magic (`BAD_ARGUMENT`). Errors: `UNSAFE_STATE` (not field and no ring open:
event, fade, input lock), `BAD_ARGUMENT` (hero absent or dead, boy magic, bad kind or arg), `NOT_OWNED` (spirit not owned),
`NO_TARGET` (empty item or magic ring; Targeting with no enemy on screen, in which case the ring stays as it was), `BUSY`,
`TIMEOUT`. Close from an already closed state finishes at once.

### 13.1 Reverse engineering (state machine of the ring)

Everything below is from `src/rom` and confirmed at run time on the four ROMs.

* The ring is a blocking loop in the main thread: the main loop's input routine (C0:B136) sees the Y edge (`$CC` bit 2, pad 1;
  `$CD`, `$CE` for the other pads), takes the hero from the pad binding (`$D9`, bit n = hero n) and calls C0:BEFC -> C1:CB38 ->
  C0:6594, which runs C0:65D7 until `$1826` (ring active) is 0. Every frame of that loop waits at C0:694A..694E for vblank,
  then polls the pad into `$1800` and dispatches the phase in `$1812` (5 = opening, 6 = closing, 0 = idle).
* What the ring shows at opening is not fixed: the category is read from `$1870 + hero` (saved per hero, default 3) and the
  entry from saved ranks: items `$1873`, the girl's spirit `$1874`, the sprite's spirit `$1875`, config `$1876` (the game
  stores them again on every close). The weapon ring has no saved rank.
* The old behaviour (macros and the first quick-slot glue) opened the default ring (weapon) and then walked it: X for the
  owner (rebuilds the ring, an animation), UP for each category (another animation), LEFT/RIGHT for the cursor. Each step
  was a visible ring change.
* Owner change in the field is the game's own hero switch: `$CC` bit 1 (X) runs `cycle_controller_to_first_eligible_party_actor`
  (C0:BCF3), boy -> girl -> sprite -> boy, skipping heroes that cannot take the pad; `$D9` is the result.
* Config entries: Equip Armor is a sub-ring (category 5, `$1803` = 8). Targeting is a ring mode (`$1803` = 0x20) that needs an
  enemy on screen. Status, Levels, Action Grid, Controller Edit and Window Edit are pages the game runs after the ring
  leaves: the confirm writes the mailbox (`$19F0` = C0) and clears `$1826`, and C1:DB6B dispatches the page. Pages leave
  with Y (status), B/Y (most) or SELECT/START (Controller Edit only); a hold of at least 6 frames is needed.
* Closing: a top-level ring leaves by itself when `$1826` is written 0 (the loop's own exit test); the game then runs its
  close path (14 frames) with no shrink animation. Sub-rings need Y, pages need their button.

### 13.2 Design

No NMI stub is needed: the commands only write the game's own latch bytes and press buttons through the core input.

1. If a ring, list or page is open it is closed first (retargeting works from any menu state).
2. The pad is bound to `hero` with the game's field hero switch (the same thing X does in the field or in the ring), at most
   a few cycles; this is the same final state as choosing that hero's ring in the game.
3. The saved category and the saved rank (spirit rank = position among non-zero mask bytes, skipping Lumina for the girl and
   Shade for the sprite; config rank = entry) are written, then `$CC` bit 2 is set. The game opens the ring itself, once, on
   the target. Nothing else is written (no ring flags), so every flag is the game's own.
4. For the spirit list and the pages one confirm (B for 3 frames) is sent through `somd_menu_pad()`.
5. While the command runs `somd_hide_video()` is 1. The host must run frames without presenting them and drop their
   audio, and present the first frame whose hide flag is 0. So the user sees the field, then the finished target.
6. Safety nets: a pending Y/X latch is removed on every failure and on every finish of a close (a late tap from a page exit
   would reopen the ring); every phase has a timeout; the first refusal leaves the game untouched.

Host loop (3DS and Android):

```c
do {
  somd_pre_frame(&d, wram);
  core_set_pad(user_pad | somd_menu_pad(&d));
  retro_run();
  somd_post_frame(&d, wram);
} while (somd_hide_video(&d) && ++ff < 160);     /* run unthrottled, no present, drop audio */
present();                                        /* the first visible frame is already the target */
```

Cost in emulated frames (hidden, so nothing is seen): ring 44 (+6 or +17 more when the pad must be switched to the girl or the
sprite), spirit list 62-75, pages 64 (armor) to 118 (status), close 14 (ring), 34 (spirit list or armor), 30-115 (pages; the
Controller page waits for the SELECT turn). At the 6-12 ms per frame measured for the core on the Thor, a ring is about
0.3-0.5 s of unthrottled emulation and a page about 1 s; if the core exposes a fast-forward/no-render mode the host should
turn it on while `somd_hide_video()` is 1.

### 13.3 Verification (frame by frame)

`test_direct_menu.py` (39 cases per variant) and `test_direct_menu_extra.py` (21 checks per variant), evidence in
`tools/companion/direct/evidence/menu/`. Party fixtures: vanilla and Magno party of three, AC seed 1 and 777 party.

| Check | vanilla | Magno | AC1 | AC777 |
|---|---|---|---|---|
| Rings: 3 heroes x item/config/weapon, girl and sprite x magic (11 cases) | pass | pass | pass | pass |
| Spirit lists: every owned spirit for girl and sprite (14 cases) | pass | pass | pass | pass |
| Pages: 7 entries x boy and girl (14 cases) | 12 pass, 2 `no-target` (Targeting, no enemy) | same | same | same |
| Only one frame shown in the whole open (the finished target); black rows in shown frames | 0 | 0 | 0 | 0 |
| No ring of another category or owner exists at any frame, hidden or not | pass | pass | pass | pass |
| Close: `$7D`, `$1826`, `$1812`, `$1803`, `$1847`, `$CC` edge bits, event, fade all back to field values | pass | pass | pass | pass |
| The pad moves the hero afterwards and the plain Y button reopens the ring | pass | pass | pass | pass |
| Retarget from an open menu (weapon ring -> sprite spirit list -> girl item ring), rapid open/close sequence | pass | pass | pass | pass |
| Refusals: boy magic, bad kind or page, unowned spirit, event, fade, dead hero, busy | pass | pass | pass | pass |
| Spirit-list spell id equals `6 * position + 3 * girl` for each spirit | pass | pass | pass | pass |

Notes: the matrix counts a Targeting page with no enemy as the designed refusal. After Targeting the game itself leaves
`$1804` bit 4 set (same with a real Y press); that is not a stuck flag of the direct path. Black rows are measured against the
black rows of the field frame itself (the AC dungeon frame has a static black band). The contact sheet
`evidence/menu/<variant>_open_sequence.png` shows 10 frames of one open with the hidden ones marked: the hidden frames are the
screen fade and the growing ring, the only shown frame is the finished list.

Limits: pages open only after about 64-118 hidden frames; the Controller Edit page closes with SELECT (not START, which would
set the assignment); Targeting needs an enemy on screen; the first button taps need the game to be polling (a ring that is
still animating is waited for, not interrupted).

### 13.4 What the Android and 3DS apps integrate

Rebuild with the current `som_direct.c/.h` (this section, the black-strip fix and `somd_sound` are all in them) and wire:

* quick slot `MENU` -> `somd_open_menu(SOMD_MENU_RING, acting_hero, category)`
* quick slot `SPIRIT` -> `somd_open_menu(SOMD_MENU_SPIRIT, acting_hero, spirit)`
* quick slot `SUBMENU` -> `somd_open_menu(SOMD_MENU_PAGE, acting_hero, entry)` (long press on an avatar = entry 2 for that hero)
* avatar tap in the ring -> `somd_open_menu(SOMD_MENU_RING, hero, current category)` (replaces the old close-and-reopen glue)
* the ring's close action (`ACT_CLOSE_RING`) -> `somd_close_menu`
* in the frame loop: OR `somd_menu_pad()` into the pad, run until `somd_hide_video()` is 0 without presenting, and drop the
  audio of the hidden frames; treat `d.state == SOMD_FAILED` as "stay where you were" and show `somd_err_name(d.err)`.

While a menu command runs the app must not send other commands (`BUSY`); `somd_sound` may be used again once the state is
finished.

## 14. Cast availability, cast queue and revive

Everything here is in `som_direct.c/.h` (`SOMD_HAS_QUEUE`); the lib must be rebuilt. Tests: `test_direct_castcheck.py`,
`test_direct_queue.py`, `test_direct_revive.py` (evidence in `tools/companion/direct/evidence/{castcheck,queue,revive}_<variant>.json`).
All pass on vanilla, Magno, AC seed 1 and AC seed 777.

### 14.1 `somd_can_cast(d, wram, hero, spirit, slot, &info)`

Returns `enum somd_cast_reason`; `info` carries `permanent`, `will_clear`, `eta_frames`, `status`, `spell_id`, `mp_have`, `mp_cost`, `cls`.

| Reason | Kind | Clears by itself | Notes |
|---|---|---|---|
| `OK` | | | the cast can start now |
| `BAD_ARGUMENT` | permanent | no | hero is not 1 or 2, spirit or slot out of range, hero absent |
| `NOT_OWNED` | permanent | no | the spirit mask at `+0x1C8` is 0 |
| `KO` | permanent | no | status `0x8000` / dead bit; use the revive options |
| `NO_MP` | permanent (until MP is restored) | no | `mp_have < mp_cost` |
| `SILENCED` | transient | yes, `eta_frames` = 20 x status timer | status `0x0080` |
| `STATUS` | transient | yes for timed statuses, no for Barrel `0x0400` | frozen, stone, balloon, sleep, burning, moogle, Barrel |
| `BUSY` | transient | yes, about 90 frames | the caster is attacking, hurt, casting (`+0x60` or `+0x1C` non-zero) |
| `COMMAND` | transient | yes, 4 frames | another direct command still runs |
| `RING_OPEN`, `EVENT`, `FADE`, `INPUT_LOCK` | transient | yes (FADE about 30 frames) | the field is not free |
| `UNSAFE` | transient | yes (about 10 frames) when another hero lives | no game running, or the controlled hero just died and the pad has not moved yet; wiped party never clears |

Evidence from the game itself: the magic ring (category 1) greys out a hero when `status & 0xD1F0` (table C0:FD10, checked by
D9:FD46): KO `8000`, burning `4000`, moogle `1000`, balloon `0100`, silence `0080`, stone `0040`, frozen `0020`, sleep `0010`. The
castcheck test drives the real ring macro with each of the 14 status bits and compares: `can_cast` says OK exactly when the ring
completes a cast. The one addition to the game mask is Barrel `0x0400`: the real ring macro never completes a cast with it, so
it is reported as `STATUS` (not timed, `will_clear` = 0). With MP short the ring refuses the spell and nothing is spent; `NO_MP`
reproduces that (`mp-zero` and `mp-exact` checks). A silenced caster is refused by the ring and by `somd_cast` (error `status`).

Timers: an injected status word restarts its timer at 20 on the next frame (the game normalises it), so the silence test takes
about 390 frames. Natural statuses applied by the game have the real remaining time in `eta_frames`.

### 14.2 The queue

```c
int somd_queue_cast(d, wram, hero, spirit, slot, struct somd_policy pol, int timeout_frames /*0 = d->queue_timeout = 600*/, struct somd_cast_info *info);
int somd_queue_state(d, hero, struct somd_queue_info *q);   /* EMPTY, WAITING, FIRED, DROPPED */
void somd_queue_cancel(d, hero);                            /* WAITING -> DROPPED with CANCELLED */
void somd_queue_ack(d, hero);                               /* clears a FIRED or DROPPED entry back to EMPTY */
```

* Policies: `SOMD_POL_SLOT` (given slot), `SOMD_POL_NEAREST`, `SOMD_POL_LOWEST_HP`, `SOMD_POL_ALL`. They are resolved on the frame the cast fires, not
  when it is queued. If the target is gone the entry is DROPPED with `bad-target` (slot) or `no-target` (nearest, lowest HP, all).
* One entry per hero (1 and 2). A new request replaces the waiting one (`rapid-replacement` check shows the new spell id fires).
* A direct `somd_cast` for the same hero drops a waiting entry (CANCELLED).
* Permanent reasons are not queued; the function returns `ko`, `no-mp` or `bad-argument` (not owned) and `info->reason` tells which. The UI disables the button.
* A transient reason queues. `queue_state` shows `WAITING` with `reason` (what it waits for), `waited` and `timeout` in frames. It fires on the first
  `somd_pre_frame` where `cast_reason` is OK, one cast per frame. Reaching the timeout drops it with `expired`. A reason that turns permanent while
  waiting (the caster dies, MP is spent) drops it with `ko` or `no-mp`.
* Queueing is safe because the fire path is the same `somd_cast` (stub, no ring) behind the same gates; there is no fallback needed. Fired entries stay `FIRED`
  until `queue_ack`.

Tested on the four variants: silence + queue fires when the status ends (MP drops), busy + queue, KO and no MP are not queued, not-owned is not queued,
rapid replacement, cancel, timeout (`expired`), immediate fire when possible, direct cast cancelling the queue, the four policies resolving at fire time,
and the three vanished-target cases (nearest, slot, all).

### 14.3 Revive

```c
int somd_revive_options(d, wram, hero, struct somd_revive_option out[4]);   /* 0 entries unless hero is KO */
```

Entries: `kind 1` item (Cup of Wishes, item id 5: `id`, `qty`, inventory `record`), `kind 2` spell (Revivifier, spell 33, girl, Dryad spirit position 5 slot 0:
`caster`=1, `spirit`, `slot`, `mp_cost`, `reason` = `can_cast` reason, `usable`). Item 5 is the only revive item in the table (items 0-4 do nothing on a dead hero,
8-10 toggle statuses), so there are no other entries. The girl cannot cast it on herself (`usable` 0, reason KO); the boy cannot cast it at all.

Use: Cup of Wishes -> `somd_use_item(d, wram, record, SOMD_T_ONE slot hero)`; Revivifier -> `somd_cast(d, wram, 1, 5, 0, ONE slot hero)` or `somd_queue_cast(...)`.
Both accept a KO ally target (class `ALLY_DEAD_ONE`; a living ally gives `bad-target`).

End to end, all four variants: boy killed (HP 0, game sets the dead bit, pad passes to the girl) -> Cup of Wishes returns him with full HP and quantity -1;
Revivifier from the girl returns him alive (HP > 0, MP -10); girl killed -> Cup of Wishes returns her at full HP; a queued Revivifier on a busy girl fires when free and revives.

### 14.4 What UX and Android must integrate

1. Rebuild with the new `som_direct.c/.h`. Call `somd_can_cast` for every spell button each time the party state is refreshed (cheap, no ROM read besides the table).
2. Button states: `OK` enabled; permanent reason disabled with the reason text (no MP, not owned, KO); transient reason enabled but a tap queues (`somd_queue_cast`),
   showing a clock with `eta_frames` and the reason; a second tap on a queued button calls `somd_queue_cancel`.
3. Poll `somd_queue_state` each frame for the quick slots and ack FIRED/DROPPED after showing them; DROPPED shows `somd_err_name(err)` ("expired", "no-target"...).
4. Target policy choice goes into `struct somd_policy` (the slot is used only for `POL_SLOT`).
5. KO hero card/avatar tap -> `somd_revive_options`: a Cup of Wishes button with `qty`, and when `kind 2` is present and `usable`, a Revivifier button with `mp_cost`;
   grey the Revivifier when `reason` is not OK and queue it when the reason is transient.
6. After a revive the app must refresh the party state (HP, status) from WRAM; the hero keeps the pad only if it was the controlled one.

Limits: the queue is lost on load state; `eta_frames` is an estimate; spell availability does not cover the "no target on screen" case until fire time (the entry is dropped
with `no-target`).

### 14.5 Full regression on the current `som_direct.c/.h`

Run on vanilla, Magno, AC seed 1 and AC seed 777 (one process per ROM, `mob_*` fixtures for enemies). `blackall.py` gained
revive (Cup of Wishes and Revivifier on a KO boy) and queue-fired cases (`DQUEUE` / `DQWAIT` in `direct_host.c`); the black
rows are compared with a no-command control run.

| Suite | vanilla | Magno | AC1 | AC777 |
|---|---|---|---|---|
| `blackall.py` (equip, item, cast, revive, queued cast), frames with black rows | 0 of 12 cases | 0 of 12 | 0 of 13 | 0 of 13 |
| `test_direct_equip.py` pairs, mismatches | 48, 0 | 16, 0 | 192, 0 | 192, 0 |
| `test_direct_item.py` cases, mismatches | 12, 0 | 12, 0 | 14, 0 | 14, 0 |
| `test_direct_cast.py` (42 spells, ring vs direct) | 42/42 | 42/42 | 42/42 | 41/42 |
| `test_direct_targets.py` checks | 20/20 | 20/20 | 22/22 | 22/22 |
| `test_direct_sound.py` | 2/2 checks, all ids finished | same | same | same |
| `test_direct_menu.py` | 37 pass + 2 designed refusals | same | same | same |
| `test_direct_menu_extra.py` | 20/20 | 20/20 | 20/20 | 20/20 |
| `test_direct_castcheck.py` | 26/26 | 26/26 | 26/26 | 26/26 |
| `test_direct_queue.py` | 19/19 | 19/19 | 19/19 | 19/19 |
| `test_direct_revive.py` | 9/9 | 9/9 | 9/9 | 9/9 |

Known, unchanged: AC777 spell 5 differs from the ring only in the stale request byte `+0x175` (`d0` vs `00`; section 6). The two menu
refusals per ROM are the Targeting page with no enemy on screen (`no-target`, section 13). No regression was found, so no code
changed because of this run.
