# Enemy sprites for the companion target selector

Date: 2026-10-17. Code: `tools/companion/enemysprites/`. API changes for the UX and Android sides: `docs/companion-enemysprites-api-20261017.md`.

The target selector must show the enemy and nothing else: the sprite on a transparent background, the idle frame, the palette the game uses, no scene and no
camera crop. There are two sources and one provider function that picks between them.

| Route | What it is | Needs |
| --- | --- | --- |
| Atlas | One clean idle picture per monster id, built on the PC from the owner's Godot project (ROM rips) and from the game's own code; shipped as a binary pack (`SESP`) | the pack file |
| Live render | `som_es_render_actor()` draws the sprites of one actor from the game's own OAM, VRAM and CGRAM | WRAM + VRAM of the running game |
| Provider | `som_es_sprite_for_id()`: pack picture of confidence >= 2, else live render of an on-screen actor with that id, else any pack picture | both, either may be missing |

No picture is stored in the repository. Everything extracted lives in `/home/cidwel/.local/state/som-decomp/companion-20261017/enemysprites/`.

## Licence of the sources

The Godot project (`/home/cidwel/projects/godot/doner-engine-godot/mana-magic-godot`) is GPL-3.0 for its code, but its README lists every sprite sheet as Square Enix
artwork kept only as a stand-in ("cannot be distributed"). The monster sheets (`assets/sprites/sheets/rom_mob_*.png`) are ROM rips made with this repository's
`tools/rip_monster_anims.py`. They are used for private local use only: the atlas, the pack and all images stay in the state directory, never in the repository or in a
published build. The live render has no such limit: it draws from the owner's own ROM at run time.

## 1. The id: what the actor table holds

The enemy identity is the byte at `+0x180` of the actor record (`$7E:E000 + 0x200 * slot`, slots 3..15), the monster object id of `data/monsters.json`
(0..0x56 ordinary monsters, 0x57..0x7F the boss-engine actors; the ROM name block index is `0xCF + id`). `som_ux_actor.id` already reads it.

Checks of the mapping:

* `tools/monster_model.py` writes `+0x180 = mid` when it creates a monster, and the game's spawner does the same.
* The Godot project keys its monsters by `romId`, which is this id. Of 83 monsters whose atlas could be cut, all 83 Godot names equal the ROM name of the id
  (name block `0xCF + id`), e.g. 44 Dinofish, 56 Blue Drop, 1 Buzz Bee, 4 Lullabud.
* On the PC harness (section 3), 312 live renders of 68 ids are pixel-equal to a picture of the Godot atlas of the same id (colours within 10 of 255, same
  opaque mask), so id, palette and drawing agree with the game.
* For the 16 ids that could not be seen live or only in a different pose (6, 8, 11, 12, 40, 45, 46, 61, 62, 69, 76, 79, 80, 82, 83 and the boss 87), the game's own
  drawing code run by `rip_monster_anims.py` (state of map 246, the game's spawner creates the monster) gives the identical `ord00` picture for the 15 ordinary ids
  (pixel-exact, tolerance 0). Boss 87 stays at confidence 2 (see below).

## 2. Live render: `som_enemy_sprite.c` / `.h`

`int som_es_render_actor(const struct som_es_src *src, const uint8_t *wram, int slot, struct som_es_img *out)`.

Inputs: OAM (544 B), VRAM (64 KiB), CGRAM (512 B), OBSEL (`$2101`), and the 128 KiB WRAM image for the actor record. Output: straight-alpha RGBA, palette index 0 transparent,
cropped to the opaque pixels with a 1 px transparent border, plus the actor's foot point (its screen position) inside the picture. The caller provides
`SOM_ES_BUF_BYTES` of pixel memory; there is no allocation, no global state, and a render of an enemy takes about 3 us on x86-64 (`-O2`).

### Which OAM entries belong to the actor

A screen crop cannot answer that; the game's own data can. The game keeps the pieces of every actor in WRAM and emits them to OAM (`docs/graphics.md`):

* Ordinary monster: `+0x83 & 15` pieces at `+0x90`, four bytes each `{x, y, tile, attribute}` (signed offsets). The emitted OAM entry has the same tile and attribute, at
  `x = sx + dx`, `y = sy - (+0x74 & 0x7F) - (+0x45) + dy`. The shadow is in list 2 (`+0xD0`) and is not part of the picture.
* Boss-engine actor (count 0): count and pieces in the workspace `$F800 + 0x80 * (slot - 3)`. The emitter rewrites flips and priority, so only tile and palette
  (`attr & 0x0F`) are compared.

Every piece is looked up in the 128 OAM entries (tile and attribute equal, nearest to the expected position, each entry used once; a second pass relaxes the attribute
to the palette bits, for flashes). A piece that is not in the OAM (blink frame, clipped) is skipped and reported (`n_pieces`, `n_matched`). Entries are painted from the
highest OAM index to the lowest, so the lower index ends in front, as in hardware. OBSEL gives the name base, the name gap and the size pair; the 4 bpp decoding,
H and V flips, the 9-bit X and the Y wrap follow the PPU. `som_es_render_box()` is a fallback that takes every non-shadow sprite in a screen rectangle (neighbouring actors included).

### What the core has to expose (nothing new)

| Data | Stock snes9x libretro core | Wide core (`ws_snes9x.patch`) | Without any patch |
| --- | --- | --- | --- |
| VRAM | `retro_get_memory_data(RETRO_MEMORY_VIDEO_RAM)`, 65536 B (verified in `libretro.cpp` at the build commit) | same | yes |
| OAM | not exposed | `retro_som_oam_get()` (live `PPU.OAMData`, 544 B), `retro_som_oam_frame_get()` (snapshot taken when the frame was rendered) | game shadow `$7E:0800`, 544 B |
| CGRAM | not exposed | `retro_som_cgram_get()` (`PPU.CGDATA`, 256 words) | game shadow `$7E:0600` (`$7E:BE00` while `$7E:002A != 0`), 512 B |
| OBSEL | not exposed | `retro_som_regs_get()[0x2101]` (`Memory.FillRAM`) | `$7E:0091` (the value the game writes to `$2101`) |

So **no change to the widescreen patch is needed**, and no proposal patch was written: `som_es_src_wram()` builds the input from WRAM plus the VRAM the core already
exposes, and works on the stock core too. The shadows are what the NMI uploads next, so they can run one frame ahead of VRAM; of 420 renders the two routes gave the same
picture in 369 and a different animation frame in 51 (22 of those still pixel-equal to a Godot picture, against 28 for the hardware route). The wide core's hardware
copies are exact and may replace the shadows when present (same function, only the `som_es_src` pointers change).

## 3. Proof on the PC harness

`tools/companion/enemysprites/`: `survey_enemies.py` warps (the game's own warp event, as in `widescreen/survey.py`) to the first transition of every distinct map (397 maps) and
lists the actors; `live_test.py` moves the enemies next to the party (the game runs its own code), runs 6 frames, renders each enemy with the C function through
ctypes, saves the PNG and looks it up among all pictures of the same id in the Godot atlas.

| Measurement | Value |
| --- | --- |
| Maps with ordinary monsters rendered | 162 |
| Enemy actors | 420 (386 drawn, 34 off screen or blank; all 34 are the same ids that are drawn in other maps, except Spider Legs) |
| Distinct ordinary ids drawn live | 73 of 87 |
| Renders with every listed piece found in the OAM | 379 of 386 |
| Renders pixel-equal to a Godot picture of the same id | 312 (68 distinct ids) |
| Boss-engine ids drawn live with every piece found | 29 of 41 (maps with a boss, sampled every 20 frames for 700-900 frames, also while holding a direction to trigger the arena) |

Spider Legs (id 50) is a one-piece blank at rest (its `ord00` set draws nothing); the atlas uses the first set that draws (`ord06`).

Unit tests with no ROM: `test_som_enemy_sprite.c`, 39 checks under ASan/UBSan (colours, H/V flips, 32x32 sprites, name tables, priority order, negative X, Y wrap, boss workspace,
fit-to-square, pack lookup with bad input, provider order). `test_provider.py` runs the provider on 27 of the maps: with the pack it returns a sprite for 69 of 71 enemy calls (the other two are NPC id 235, outside 0..0x7F), live-only for 60 of 71 (the rest are Spider Legs, which draws nothing at rest, or actors off screen after the move).

## 4. Atlas: one picture per id

`build_atlas.py` picks per id the first available of: Godot idle picture, game-drawn rip, live render. The Godot idle picture is the first picture of the
set `ord00` facing down (the project's own "stand" set, `mob.gd ROM_POSE_STAND_SET`), un-mirrored if the atlas stores it as a flip. `rip_shim.py` runs
`tools/rip_monster_anims.py` on a snes9x state (the ripper wants a ZSNES state; only its WRAM block is used) for the ids the Godot project lacks.
Boss pictures are the live render with all pieces found that has the most opaque pixels among the samples.

Outputs in the state directory:

| File | Content |
| --- | --- |
| `png/<id>.png` | the chosen picture, RGBA, 1 px transparent border |
| `enemy_sprites.json` | per id: id, name (ROM name block), source, size, foot point, confidence, alternatives, verification notes |
| `som_enemy_sprites.bin` | the pack (format below), 115 pictures, 528,584 bytes, sha256 `a051cde1fc388dd1440f11dd921a34b7e9c8482c9c1f8534bf0ee9209356ad55` (changes with the sources) |
| `enemy_atlas.png`, `enemy_atlas_rects.json` | the same pictures shelf-packed in one 512-wide PNG, rectangle per id |
| `contact_sheet.png` | all pictures with id, source letter (G Godot, R game rip, L live), confidence and name |

Confidence: 3 verified (live render of the game equal to the Godot picture, or the game's own ripper draws the identical picture, or a complete live render of an ordinary monster),
2 probable (a boss with every piece found at the sampled frame; a game-drawn rip), 1 partial (not all pieces found).

### Pack format `SESP` v1

Little endian. Header, 16 bytes: `"SESP"`, u16 version = 1, u16 count, u32 total size, u32 reserved. Then `count` directory entries of 24 bytes: u8 id, u8 source (1 Godot ROM rip, 2 game rip,
3 live), u8 confidence, u8 reserved, u16 w, u16 h, i16 foot_x, i16 foot_y, u32 offset of the pixels from the start of the file, u32 byte count (`w * h * 4`), u32 reserved. The pixels follow:
straight-alpha RGBA, rows top to bottom, border included, foot = the actor's position inside the picture. `som_es_pack_find()` validates the header and every offset against the blob length.

### Coverage

| | ordinary (ids 0..0x56, 87) | boss-engine (0x57..0x7F, 41) |
| --- | ---: | ---: |
| Godot atlas (confidence 3 except the boss) | 83 | 1 (87 Mantis Ant, c2) |
| Game's own code (rip) | 2 (50 Spider Legs, 85 Crystal Orb) | 0 |
| Live render only | 0 | 29 (28 at c2, 107 Blue Spike at c1) |
| None | 2 (84 unused, 86 Treasure Chest: not monsters, the ripper picture is garbled) | 11 |

Ids without a picture: 84 (unused), 86 (chest) and the bosses 91 Spikey Tiger, 92 Jabberwocky, 95 Snap Dragon, 98 Vampire, 110 Aegagropilon, 111 Hexas, 112 Kettle Kin, 116 Fire Gigas, 121 Dark Lich,
122 Biting Lizard, 127 Mana Beast. Those bosses are not reachable by the warp harness (their fights start from events in the story); the live route draws them when they are on screen
in a real fight, and the selector shows the generic silhouette otherwise. The boss pictures are one pose at the sampled frame, not a chosen idle frame, and a multi-part boss may show
only the part that belongs to the actor record (Blue Dragon, Mech Rider show the body but not every attached piece); they are listed at confidence 2 or 1 so a frontend may prefer its silhouette.

Full list: `python3 tools/companion/enemysprites/coverage_report.py` (reads the manifest).

## 5. Known limits

* Idle frames of monsters that hold a weapon (Goblin, Water Thug, Ice Thug, Kid Goblin, Imp, Shellblast, Turtlance, Gremlin, Shadow X1) include it, as in the game's stand pose; the live render of a monster that has thrown it does not.
* The live render shows whatever frame the game is on (walk cycle, hit flash, status colours); the atlas is stable.
* Pieces of an actor that are clipped by the screen edge are not in the OAM; `n_matched < n_pieces` tells, and the provider then ignores the render.
* Only the vanilla US ROM was exercised; the Spanish translation and the Ancient Cave ROM were not.
* The ripper states a monster in slot 3 of map 246; a few ids (84, 86) draw garbage there because they are not monsters.
