# Secret of Mana QOL and bug-fix patches: survey (2026-10-20)

Scope (T11): which community patches exist, what they change, whether we can offer the same behaviour without distributing
anybody's bytes, and what is implemented in `tools/gameplay/som_gameplay.py` (options `qol_*`, group "Quality of life" of
`tools/gameplay/gameplay_menu.json`). Design and verification: `docs/qol-gameplay-20261020.md`.

Sources read: the romhacking.net game page 799 (77 hacks, listing and description pages, 2026-10-20), the Moppleton
Secret of Mana Randomizer sources (`SoMRandomizer/processing/hacks/common/{qol,fix,other}`, LGPL-2.1, GitHub `Moppu/SecretOfManaRandomizer`)
and the libsomrando C port in this repository. romhacking.net pages state no licence; the patches are IPS/BPS files whose
author asks for credit. **Rule used here: no third-party patch byte is stored or distributed.** Each accepted behaviour is
re-implemented as a record (address, expected bytes, new bytes) or as our own 65816 snippet, written against the ROM structure
we mapped ourselves, applied at run time to the user's own ROM copy. Ideas and research are credited in `data/credits.json`.

## Candidates

| # | Patch (author, source) | What it does | Can it be rules without third-party bytes | Status here |
|---|---|---|---|---|
| 1 | Secret of Mana Fixed (Gigasoft, RHDN 9886, Aug 2026) | About 30 bug fixes: dialogue state, RAM initialisation, duplicate starting equipment, exp from own character, Mana Sword damage, weapon weaknesses, Wisdom bonus, hit up/down, equipment sale/trash corruption, overcharge, snowman abduction, Neko save location, Mana Beast loops, wall clipping | yes, each fix is a small code change; every one needs its own reverse engineering and a trigger to verify | not implemented (largest remaining source of value; list kept for a later round) |
| 2 | Secret of Mana Plus, Climate Crisis (Tomm, RHDN 8491, 7940) | new dungeons, content, rebalance | n/a (content, not QOL) | out of scope |
| 3 | Secret of Mana, Turbo (Queue, RHDN forum 27890) | gameplay overhaul with bug fixes, zhaDe patch format | no (format and content) | out of scope |
| 4 | Stamina Removed (Faydgamer, RHDN 9581) | attack without waiting for the gauge | yes | **covered by T8** `weapon_recharge=instant` + `run_recharge=none` (`equivalents` in the menu) |
| 5 | Faster Weapon Grinding (Kethinov, RHDN 4430) | four weapon-skill gain rates | yes | **covered by T8** `weapon_level_rate` 1..3 |
| 6 | Faster or Slower Spell Grinding (Kethinov, RHDN 4419) | twelve spell-level pace presets (x0.48 to x1.5) | yes | **`qol_magic_level_rate`** x1..x3 (faster side only) |
| 7 | Item Limit "9" Patch (Masterflow, RHDN 672) | consumable stack 4 -> 9 | partly: the count is a 3-bit field of the inventory byte, so 9 needs a new byte layout and menu code | **`qol_item_limit`** 4 or 7 (7 is the field's maximum); 9 rejected |
| 8 | Better Scrolling (Binarynova, RHDN 2913) | screen scrolls sooner, hero keeps farther from the edge | yes | **`qol_scroll_deadzone`** vanilla/tight/tighter |
| 9 | Dead party members gain EXP (AdmiralStiff, RHDN 6662) | fallen heroes still receive experience | yes (needs a guard so the level-up does not revive them) | **`qol_dead_exp`** |
| 10 | Remove Barrels on Run / Fix Barrel Frame Glitches / Fix Dark Lich Head Bang / No Glitching Into Pandora Ruins (Kethinov, RHDN 4515, 4514, 4780, 4763) | barrel and glitch fixes | yes | not implemented: needs barrel and boss fixtures to verify |
| 11 | Gigas Bosses Disassemble Less Often (Kethinov, RHDN 4857); randomizer GigasesNeverSplit | Gigas bosses split into orbs less often (one byte in the action table) | yes | not implemented: needs a Gigas fight to verify |
| 12 | No Text Boxes, Better Default Text Boxes, Text-Box-Fix (Kethinov 4791, 4310; N.Dpendent 4885, German) | window frame and background defaults | yes, but purely cosmetic and the frontends draw their own windows | not implemented |
| 13 | No HP Restored At Level Up (Kethinov, RHDN 4319) | removes the level-up heal | yes | not implemented (difficulty, not QOL) |
| 14 | Randomizer FasterWeaponLevels / FasterMagicLevels (Moppleton) | x4 weapon / spell progress per use | yes | `weapon_level_rate` (T8), `qol_magic_level_rate` |
| 15 | Randomizer LevelupMpRestore (Moppleton) | level-up refills MP too | yes | **`qol_levelup_mp_restore`** |
| 16 | Randomizer MaxItemsIncrease (Moppleton) | consumable stack 4 -> 7 | yes | **`qol_item_limit`** |
| 17 | Randomizer FastTransitions (Moppleton) | no fade on map transitions (may glitch) | yes | **`qol_fast_transitions`**; plus our own `qol_fast_fades` (all fades twice as fast, keeps the look) |
| 18 | Randomizer DamagePercentFix (Moppleton, from the SoM editor community) | a swing before the gauge is full no longer loses half its damage twice | yes | **`qol_unready_damage`** |
| 19 | Randomizer ChestOpeningSpeedup (Zhade) | the chest lock-breaking animation ends at once | yes | **`qol_chest_fast`** (hero pose 142 -> 1 frames verified in snes9x; the two chest-object scripts verified by bytes) |
| 20 | Randomizer FasterTopOfScreenDialogue (Moppleton) | shorter top-of-screen text lifetime | yes | **`qol_message_speed`** (round 2): the site is the lifetime byte of the 12-record ring at `$7E:FE00` that holds the top-of-screen messages (level-ups, items found); measured 750 -> 494 -> 365 frames. The round 1 attempt had no screen message to show; the level-up banner is the fixture |
| 21 | Randomizer NoSwordPullAnimation (Moppleton) | skips the sword-pull scene | yes | tried and **rejected**: the handler (`C1:EC78`, JSL `$02:C976`) is not reached in 9,000 frames of the new game and is a generic event handler; not shown to affect only that scene |
| 22 | Randomizer CandyHealouts, CupWishesOnZeroHp (Moppleton) | consumables usable on a hero at 0 HP | yes | **not implemented, by design**: the dead flag (`$E191` bit 7) is set within a frame of HP 0 and cleared only by the Cup of Wishes (item 5), and the game's heal items do nothing on a flagged hero on purpose (`docs/rom-combat.md` section 16). The randomizer only NOPs the HP check. Reviving with Candy changes the difficulty rather than removing a chore |
| 23 | Randomizer BuyMultipleItems (Moppleton, 1,451 lines) | buy several items at once | yes | **`qol_shop_fill`** (round 2): our own routine, not the 1,451-line one. After the first purchase of a consumable it keeps buying the same item while the stack has room and the money covers it. Verified in snes9x (money edge cases, limit 4 and 7) |
| 24 | Randomizer OverchargeFix, WhipFix, ConfuseFix, DamageCancelFix, BeakShieldDisable, Gigas fix, Autosave | bug fixes and Autosave | yes | candidates; Autosave is replaced by the PC/Thor/3DS save systems |
| 25 | Randomizer NoEnergyToAttack / NoEnergyToRun / FasterEnemies | no stamina, faster enemies | yes | T8 `weapon_recharge`, `run_recharge` (conflicts recorded) |

Items of the project scope that are not community patches (round 2):

* **Ring menu speed**: `qol_ring_speed` (x2). The ring code (`C0:6BF8..6D00`) was mapped for this: the turn step `$1824` is 0x12 per frame (three literals) and the open/close step on `$181E` is +-10. x2 keeps every rest layout identical; x3 (0x36 / 30) breaks the final positions, so it is not offered.
* **Text speed**: not implemented. Dialogue pages render in about 8 frames with no per-character delay, so there is nothing to speed up.
* **Skip the level-up screen**: there is no such screen. The level-up is the top-of-screen banner, which `qol_message_speed` shortens.
* **Barrel and Dark Lich fixes** (candidate 10), **Gigas split** (11) and the **Gigasoft fixes** (1): not implemented. They need fixtures (a barrel room, the Dark Lich and Gigas fights, the triggers of each bug) that the harness does not have, and nothing was guessed. The weapon-weakness quirk (`docs/rom-combat.md` section 6) is a balance change and was left alone.

## Conflicts

* With T8 options: none by bytes. `qol_magic_level_rate` and `weapon_level_rate` use different routines. `qol_fast_fades` and
  `qol_fast_transitions` are compatible and redundant (transitions already jump to the target brightness; `fades` still halves the
  other fades). `qol_dead_exp` and `qol_levelup_mp_restore` are independent (round 2): dead exp gates the experience share and puts a flagged hero back to 0 HP at the end of the level-up step, the MP option owns the `C0:44D3` hook.
* With randomizer hacks (probe on Magno and the five randomizer modes, `tools/gameplay/qol_verify.py other_roms`): the randomizer applies
  `fasterChests`, `moreItems`, `mpAtLevel` (hook at `C0:44D3`) and `fasterMagicLevel` (hook at `D0:4DEC`) by default. Chest and item
  limit records find their own bytes ("already") and are no-ops; the `mpAtLevel` and `fasterMagicLevel` hooks are recognised by the routine they jump to, so `qol_levelup_mp_restore` and
  `qol_magic_level_rate` are left alone there (the behaviour exists) and `qol_dead_exp` composes with the hook. `qol_message_speed` x2 on a randomizer ROM finds the 0x30 of `fasterMessages` (counted as already applied); x4 reports a conflict instead of overwriting it.
  The tool reports each skip; nothing is silently overwritten.
* Magno (Spanish): the combat block is relocated; `qol_dead_exp` and `qol_unready_damage` find their sites by instruction signature; `qol_shop_fill` is found on Magno too, `qol_message_speed` is not (the translation rewrote the message builder) and is reported as a conflict.
