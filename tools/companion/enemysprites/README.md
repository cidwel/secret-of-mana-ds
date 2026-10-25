# Enemy sprites for the target selector

Shows ONLY the enemy sprite (transparent background, idle frame, game palette) instead of a camera crop.  Documentation:
`docs/companion-enemysprites-20261017.md` (method, coverage, pack format) and `docs/companion-enemysprites-api-20261017.md` (what the UX and
Android sides change).

| File | Purpose |
| --- | --- |
| `som_enemy_sprite.c/.h` | the C library: live render of one actor from OAM + VRAM + CGRAM, the SESP pack lookup, the one-call provider |
| `test_som_enemy_sprite.c` | synthetic tests (no ROM): `gcc -std=c99 -Wall -Wextra -fsanitize=address,undefined test_som_enemy_sprite.c som_enemy_sprite.c` |
| `godot_sprites.py` | cuts the idle picture of every monster out of the owner's Godot project |
| `rip_shim.py` | runs `tools/rip_monster_anims.py` on a snes9x state (game-drawn idle pictures for ids the Godot project lacks) |
| `harness.py`, `esrender.py`, `godot_cmp.py` | PC harness helpers (wide core, map warps, ctypes wrapper, comparison with the Godot atlas) |
| `survey_enemies.py`, `live_test.py`, `live_boss.py`, `test_provider.py` | the proofs on the PC harness |
| `build_atlas.py`, `coverage_report.py`, `contact.py` | atlas, pack, manifest, coverage and contact sheet |

All pictures are ROM-derived art and stay in `/home/cidwel/.local/state/som-decomp/companion-20261017/enemysprites/`; the repository holds code only.
