# Companion bottom-screen UX (AYN Thor and 3DS)

Date: 2026-10-17.  Owner: UX track.  Code: `tools/companion/ux/`.  Manifest of the graphics kit: `tools/companion/assets/README.md`.
Everything generated (pack, WRAM captures, PNG mocks, goldens, evidence) lives in
`/home/cidwel/.local/state/som-decomp/companion-20261017/` and is not committed.

## 1. Design principles

1. The top screen is the game.  The bottom screen never repeats it: it shows what the game hides (all three heroes' HP/MP,
   charge, status, enemy HP, radar) and replaces what is slow in the game (the ring menu).
2. One screen, one job per mode.  The layout is chosen from WRAM (section 3); the player never switches modes by hand.
3. One tap does the thing.  Weapon and one-target-free spells run at once (direct control, `docs/companion-direct-20261017.md`);
   only spells and items that need a target open the target panel (cards for the party, rows for enemies, "all allies",
   "all enemies", Cancel).  The ring is a fallback only (Magic Rope, Flammie Drum: `NEEDS_RING`).
4. Never lie about what the game will accept.  Anything not usable right now is greyed and its hit area is disabled; a locked
   state (event, fade, ring open) says why in words ("COMMANDS LOCKED WHILE THE EVENT RUNS").
5. One layout builder feeds both drawing and touch (`build_layout`), so what is drawn is exactly what is tappable.
6. Read-only on WRAM and ROM.  The renderer only reads; writes happen in the macro/direct layers, driven by the returned action.
7. Same physical size on both devices.  Layout is in virtual pixels (vp); the scale is the integer
   `s = max(1, (min(w,h)+125)/250)`: 3DS 320x240 is 1x (320x240 vp), Thor 1240x1080 is 4x (310x270 vp), Thor portrait 1080x1240
   is 4x (270x310 vp).
8. Secret of Mana look: blue gradient windows with the light bevel and checker background, the game's own outlined font,
   round portraits, orange charge bar, yellow gold.  Art from the optional pack; built-in procedural art without it.
9. Legibility first (section 3a): one face, integer scaling only, vitals in large numbers with long thick bars, danger shown
   by shape and pattern as well as colour.

## 2. Touch sizes (mm)

Panels: 3DS bottom 3.0 in, 320x240 (about 5.25 px/mm, 1 vp = 0.19 mm); Thor secondary 1080 px wide (about 16.5 px/mm,
1 vp = 4 px = 0.24 mm).  Measured with `som_ux_min_target_vp()` on the real layouts (index: `<state>/ux/mocks/index.json`):

| Screen | Smallest target (vp), 3DS / Thor | mm, 3DS / Thor | Device rule | Result |
| --- | --- | --- | --- | --- |
| Header gear (OPT) in every mode | 20 / 29 | 3.8 / 7.0 | Thor finger >= 7 mm; 3DS stylus >= 3.5 mm | pass |
| Quick slots (field, ring, editor row) | 28 / 30 | 5.3 / 7.2 | same | pass |
| Field: tabs | 22 / 30 | 4.2 / 7.2 | same | pass |
| Field: weapon, item, magic cells | 38 (items) 56 (weapons) / 29 (items, landscape) 39 (items, portrait) 43-56 (weapons) | 7.2 / 7.0-9.4 | same | pass (the 1 vp gap between cells is part of the hit area) |
| Ring: other-hero avatars | 30 / 34 | 5.7 / 8.2 | same | pass |
| Ring: BIND ENTRY, BIND MENU | 24 / 30 | 4.6 / 7.2 | same | pass |
| Target panel: allies, buttons | 22-36 / 30-40 | 4.2-6.8 / 7.2-9.6 | same | pass |
| Target panel: enemy rows | 19 / 29 | 3.6 / 7.0 | same | pass (3DS rows are stylus work) |
| Field: hero cards (selectors) | 48 / 31-56 | 9.1 / 7.4-13 | same | pass |
| Options: option cells and BINDINGS / QUICK SLOTS / BACK | 42 / 46-54 | 8.0 / 11-13 | same | pass |
| Options: bindings list rows and BACK | 25-28 / 31-34 | 4.8-5.3 / 7.4-8.2 | same | pass |
| Quick-slot editor: tabs, pager, picker cells, bottom buttons | 22-51 / 30-56 | 4.2-9.7 / 7.2-13 | same | pass |
| Bind screen buttons | 30 / 44 | 5.7 / 10.6 | same | pass |
| Event NEXT, Flammie START | 24-30 / 38-46 | 4.6-5.7 / 9.1-11 | finger | pass |
| Name entry keys | 32-33 / 27-32 | 6.1-6.3 / 6.5-7.7 | stylus (3DS), finger (Thor) | Thor portrait keys are 6.5 mm (open) |
| Boot and menu pads | 44..72 / 58..83 | 8..17 | finger | pass |

Numbers come from `som_ux_target_min_vp()` / `som_ux_target_min_vp(hit)` on the real layouts, 0.19 mm per vp on the 3DS and 0.24 mm on
the Thor.  The Thor gear is the smallest permanent target (29 vp = 7.0 mm); the header is 29 vp tall on the Thor (20 on the 3DS) for it.

The 3DS rule is "stylus first": the resistive screen takes a finger, but the 4 mm cells are meant for the stylus.  The
densest control, the 4x3 grid, stays 22 vp tall (4.2 mm) because the hero cards and tabs must stay visible.

## 3. Modes and what they show

Mode detection reads WRAM only (`som_ux_classify`).  All rules were found by experiment on vanilla US (section 8).

| Mode | WRAM rule | Bottom screen | Touch |
| --- | --- | --- | --- |
| BOOT | `$E000 != 1` | title card, B / START / A pad | pad taps |
| FRONTEND | hero 0 active, max HP `$E184` = 0, `$E4` = 0 | game select pad; name entry shows an A-Z keyboard and the cursor | letters (stepped with LEFT/RIGHT, then B), START |
| FIELD | none of the others | READY/LOCKED chip and the OPT gear, 3 hero cards (large HP/MP), eight quick slots, tabs WEAPON / ITEM / MAGIC / MAP, button grid with original icons and hotkey badges, boss bar; radar and enemy list on the MAP tab | gear, tabs, cells, cards, quick slots, target panel, long press = bind / edit |
| TARGET (inside FIELD) | UI state after tapping a targeted item or spell | title, ally cards (only valid ones lit), enemy rows, ALL ALLIES / ALL ENEMIES / CANCEL | produces `target_kind` + slot |
| EVENT | `$D0 != 0` | locked: portraits dimmed, dialogue log from `$A1A4`, red notice, big NEXT (B) | NEXT only |
| TRANSITION | `$E6 != 15` or `$E4 != 1` | party and a WAIT chip | none |
| RING | `$7D = 1` and `$1847 = 0` | what the ring highlights (section 3f): owner, ring name, the entry in large type with its facts; the two other heroes as avatars; BIND ENTRY / BIND MENU; eight quick slots.  No ring buttons: the game ring is driven from the game | avatar tap / long press, BIND, quick slots, gear |
| SHOP | `$7D = 1` and `$1847` in 1..3 | buy/sell/upgrade in large type, gold, the party cards (inert) | gear only |
| OPTIONS (overlay) | the gear in the header, any mode | option cells, BINDINGS, QUICK SLOTS, BACK (section 3h) | cells, nav |
| FLAMMIE | `$E4 = 4`, `$E6 = 15`, `$D0 = 0` | world map from the ROM with the arrow, trail, heading, X/Y; START opens the globe | START |

Wireframes.  Three layouts share one builder: 3DS 320x240 (hero cards in a row), Thor landscape 1240x1080 (heroes and quick slots
stacked on the left with big HP numbers, tabs and the button grid on the right), Thor portrait 1080x1240 (wide hero rows, quick slots,
tabs, 4-column grid).  The header (20 vp on the 3DS, 29 on the Thor) always carries the mode chip and the OPT gear.

```
3DS FIELD (320 x 240 vp)               THOR LANDSCAPE FIELD (310 x 270 vp)
+---------------------------------+   +----------------------------------------------------+
|[READY] MAP 215   65535 G [* OPT]|   |[READY] MAP 215                  65535 G   [* OPT]  |
|[Boy L50 ][Girl L50 ][Sprite    ]|   |+---------------+ [ WEAPON ][ ITEM ]                 |
|[HP310/524][!88/417][HP250/412  ]|   ||[face] BOY     | [ MAGIC  ][  MAP  ]                 |
|[=====   ][//===   ][=====      ]|   ||  H 310 /524   | +----------+----------+            |
|[slot][slot][slot]...8 in a row  |   ||  =========    | |[ZL]   L8 |          |            |
|[WEAPON][ITEM][MAGIC][MAP]       |   |+---------------+ |  icon    |  icon    |            |
|[ZL L8][ L8 ][ L8 ][R   L8]      |   ||[face] GIRL   ! | |  NAME    |  NAME    |            |
|[icon ][icon][icon][icon ]       |   ||  H 88 /417     | +----------+----------+            |
|[name ][name][name][name ]       |   ||  M9/47 ====    |  (2 columns, 6 rows for items)     |
+---------------------------------+   |+---------------+                                    |
                                      ||[face] SPRITE  >|                                    |
THOR PORTRAIT FIELD (270 x 310)       |+---------------+                                    |
+---------------------------------+   ||[s][s][s][s]    |   quick slots: 4 x 2 in this       |
|[READY]              65535 [*OPT]|   ||[s][s][s][s]    |   column, one row of 8 elsewhere   |
|[face] BOY L50  HP 310/524 =====|   |+---------------+------------------------------------+
|[face] GIRL L50 HP 88/417  //// |
|[face] SPRITE   HP 250/412 =====|   RING (any size; the same blocks, 3DS 320x240 shown)
|[s][s][s][s][s][s][s][s]         |   +---------------------------------+
|[WEAPON][ITEM][MAGIC][MAP]       |   |[RING] MAP 131   100 G   [* OPT] |
|[ZL L8][ L8 ][ L8 ][ L8 ]        |   |[face] BOY                       |
|[icon ][icon][icon][icon ]       |   |       WEAPON RING               |
+---------------------------------+   |---------------------------------|
                                      |LODE JAVELIN        (large type) |
TARGET (field)                        |TYPE JAVELIN                     |
+---------------------------------+   |LEVEL 0                          |
|CAST ON WHOM?                    |   |NOT EQUIPPED                     |
|[Boy][Girl][Sprite] (valid lit)  |   |[face GIRL  45 ===][face SPRITE ]|  <- avatars: tap = their ring,
|[Water Thug  ======    145]      |   |[ BIND ENTRY ][ BIND MENU ]      |     long press = their STATUS
|[ALL ALLIES][ALL ENEMIES][X]     |   |[+][+][+][+][+][+][+][+]         |  <- quick slots ("+" = empty)
+---------------------------------+   +---------------------------------+

OPTIONS (overlay, 2 columns)          QUICK-SLOT EDITOR                     BIND SCREEN (long press)
+------------------------------+   +--------------------------------+   +------------------------------+
|[OPTIONS]                [*OPT]|   |[OPTIONS]                 [*OPT]|   |[icon] DAEDALUS LANCE         |
|[WIDE ON  [x]][FALLBACK OFF[ ]]|   |[s*][s][s][s][s][s][+][+] target|   |PRESS A BUTTON                |
|[HUD HIDE OFF ][CORE SNES9X >] |   |[MENU][WEAPON][ITEM][SPIRIT][SPELL]|  |press the console button ...  |
|[UI SOUNDS ON ][HAPTICS ON [x]]|   |[cell][cell][cell][cell]         |   |NOW: R                        |
|[LANGUAGE ENGLISH >][BINDINGS >]|  |[cell][cell][cell][cell]         |   |[CLEAR KEY]      [CANCEL]     |
|[QUICK SLOTS >][     BACK     ]|   |[<  prev ][  next  >]            |   +------------------------------+
+------------------------------+   |[CLEAR SLOT][KEY ZL][BACK]       |
                                   +--------------------------------+
EVENT (locked): portraits dimmed, dialogue log, red notice, big NEXT (B).   FLAMMIE: world map, arrow, heading, START.
```

PNG mocks for every mode on both resolutions (`3ds`, `thor` 1240x1080, `thorp` 1080x1240) are rendered from real WRAM
captures: `<state>/ux/mocks/*.png` (boot, frontend_select, frontend_name, field_weapon/item/magic/spells/map/enemies/boss,
target_ally, target_enemy, event, event_danger, transition, shop_buy/sell, flammie, flammie_otherworld, the second-round scenes
field_danger, field_bound, field_spells_bound (spell icons and badges) and bind_wait, and the third-round scenes ring_weapon,
ring_weapon_filled, ring_item, ring_item_armed, ring_spirit, ring_spell, ring_config, field_quick, field_quick_danger,
options_main, options_bindings and quick_editor).

### 3a. Typography (sizes in mm)

* One face: the game's own outlined 8x11 font (the Godot project stores it pixel-doubled at 16x32; the pack keeps the exact
  8x11 cell, so no resampling).  It is drawn only at integer scales: 1 device pixel per source pixel on the 3DS, 4 on the Thor
  (`s = 4`), and large numbers at 2x that (`mul = 2`, 8 device pixels per source pixel).  The 6x9 "small" face of the first
  prototype was dropped: it was tall and thin and hard to read.
* Sizes: body text 11 vp tall = 2.7 mm on the Thor (cap height 1.7 mm) and 2.1 mm on the 3DS; large numbers (current HP and MP on
  the Thor landscape cards) 22 vp = 5.3 mm (cap height 3.4 mm); 3DS compact cards print the value at body size.
* Text always has the game's dark outline around a flat fill, on every background, so contrast does not depend on the window.
  Dim text was raised from 8C94C0 to B0B8E8.
* Portraits are painted art, so they are scaled bilinearly (never nearest); the font, icons and window frames are never filtered.

### 3b. Vitals: bars and numbers

| | 3DS (compact card) | Thor landscape (tall card) | Thor portrait (wide card) |
| --- | --- | --- | --- |
| HP/MP number | value at body size, 2.1 mm | current value large (5.3 mm), maximum body size | body size, 2.7 mm |
| Bar height | 4 vp = 0.8 mm | 6 vp = 1.4 mm | 7 vp = 1.7 mm |
| Bar length | about 70 vp = 13 mm | about 92 vp = 22 mm | about 140 vp = 34 mm |
| Weapon charge | 3 vp bar under the portrait | 5 vp | 3 vp |

Before this change the bars were 3-4 vp tall and the text 9 vp.  HP colour follows the thresholds below; MP is blue.

### 3c. Danger (HP) states

| State | Threshold | Colour cue | Shape / pattern cue (colour-blind safe) | Motion |
| --- | --- | --- | --- | --- |
| Fine | 25 percent or more | green bar (yellow below 50 percent) | none | none |
| Low | below 25 percent | red bar, red dashed card frame, portrait tint | diagonal hatch on the bar fill, dashed frame, yellow warning triangle | slow pulse (frame and tint every 16 frames) |
| Critical | below 10 percent | red bar, solid double-width frame, stronger tint | dense hatch, solid frame, triangle that blinks red | fast pulse (every 8 frames) |
| KO | HP 0 / status 0x8000 | card and portrait greyed | white boxed "KO" text and a cross icon | none |
| Poison | status 0x2000 | green | drop icon | none |
| Other ailment | any other status bit | purple | diamond icon (the bit meanings are not decoded) | none |

In cutscenes and transitions the card is dimmed and the pulse is off: a low hero keeps a thin static dashed frame and the
triangle, a critical one the triangle, nothing flashes.  The pulse uses the renderer's frame counter, so every golden is
reproducible (`som_ux_update` advances it).

### 3d. Spell, spirit, weapon and item icons

* Weapons and items: the 16x16 icons of the Godot project's `spr_iconsGeneral` sheet, which are the game's ring icons.  Compared
  by eye with the item ring of the real game (candy, jam, walnut, herb, goblet, rope and others are identical).
* Spirits: the same sheet (the eight spirit sprites).  The magic ring of the real game shows the same artwork.
* Spells: the 42 spell icons of `spr_iconsSkills`.  Which cell belongs to which spell was checked against the real game with
  `extract_icons.py`, which opens the magic ring of the girl and the sprite on the vanilla ROM and saves the frame crop of every
  selected icon.  Matched in the game: Earth Slide, Gem Missile, Speed Down, Stone Saber, Speed Up, Defender (gnome), Freeze,
  Acid Storm, Energy Absorb, Ice Saber, Remedy, Cure Water (undine), the sylphid set (Air Blast, Thunderbolt, Silence, Thunder
  Saber, Balloon, Analyzer), the girl's three Luna spells and the sprite's Shade spells (Evil Gate, Dark Force, Dispel Magic).
  The rest (Salamando, the sprite's Luna spells, Dryad, Lumina) are assigned by the spell's meaning from the same sheet and are
  marked as such in `build_assets.py`.  The party state used for the capture only owns four spirits, so the game's ring repeats
  art for the others; that is why those icons could not be read from it.
* The icons are not decoded from the ROM: they are compressed there and drawn as sprites with colour math.  An attempt to read
  their tiles back from VRAM and the frame failed for the translucent ones (see section 12).
* Pack ids: `0x1000+n` general icon, `0x1100+spell id` spell icon.

### 3e. Shoulder slots (replaces the round 1-4 hotkey bindings)

Round 5 removed the "press a button to bind" flow, the OPT > BINDINGS list, the KEY button of the slot editor and the BIND ENTRY / BIND MENU
wording.  See section 3l for the model that replaces it: four fixed shoulder slots bound to L1, R1, L2, R2 by position.

### 3f. Ring mode: what is selected (no ring buttons)

The touch buttons that moved the ring (arrows, BACK, HERO, OK) are gone: the ring is driven from the game, so the freed space shows
what the ring highlights, the other heroes, and the quick slots.  The "ENTRY n / m" counter was removed in every mode (ring and shop).

* **Info panel** (`som_ux_selection()`, `struct som_ux_selinfo`): owner portrait and name, the ring name in cyan, the highlighted
  entry's name in yellow at 2x on the Thor (1x on the 3DS), then up to five fact lines, 2x when they fit, else 1x, one or two columns.

| Ring (`$1809`) | Entry (`$180A`) | Facts shown | Source |
| --- | --- | --- | --- |
| Weapon (3) | owned types in the order 1..7 then 0 (`som_weapon_ring_index`); title is the tier name | TYPE, LEVEL (0..8 of the ring owner), EQUIPPED BY or NOT EQUIPPED | WRAM `$CC54+type`, hero records, ROM names |
| Item (0) | non-empty item records in order | QUANTITY, effect line, TARGET class | WRAM `$CC48`, ROM names; effect text authored |
| Magic (1), spirits | mask bytes of the owner, bytes 6 (girl) / 7 (sprite) skipped | LEVEL, ELEMENT, YOUR MP | WRAM; element names authored |
| Magic (1), spell sub-ring (`$1803` bit 7) | spell id = `$1839` | MP COST, TARGET class, ELEMENT, YOUR MP or NOT ENOUGH MP | ROM spell table `0x102AD0 + 64*id` (+15 cost, +8 class), WRAM |
| Config (2) | entry 0..6 = EQUIP ARMOR, TARGETS (the game's Targeting), STATUS, WEAPON LEVELS, ACTION GRID, CONTROLLER, WINDOW | one-line description | authored |

  While the ring is opening or moving (`$1812 != 0`) the panel shows "..." and no entry is bound.
* **Avatars**: the two heroes that do not own the open ring (portrait, name, HP, bar with the danger hatch, KO box).  A tap returns
  `ACT_AVATAR` with the hero: the host makes that hero the ring owner (their weapons, items, magic).  A long press returns
  `ACT_OPEN_MENU` (`SUBMENU`, `CFG_STATUS`, hero): the host opens that hero's STATUS page.  Design reason: in Secret of Mana each
  hero has their own ring and status page, and the owner can only be changed by `X`, which cycles 1 <-> 2 and never returns to the
  boy; the avatar hides that quirk (the glue closes and reopens the ring when it has to reach the boy).
* **BIND ENTRY / BIND MENU**: BIND ENTRY arms the next slot tap to store the highlighted entry (weapon, item, spirit, spell or config
  screen); BIND MENU stores the open ring itself (kind `MENU`).  The armed button says PICK A SLOT and all slots turn yellow; a
  second tap on the button cancels.  A "+" slot tapped in the ring binds the highlighted entry at once (one tap, no arming).

### 3g. Quick slots (hotlinks)

Eight slots, one row of eight under the party cards on the 3DS and Thor portrait, a 4 x 2 block under the heroes on Thor landscape,
and the bottom row in ring mode.  The same component in every mode: icon, optional physical-button pill, "+" when empty.

* Kinds: weapon, item, spell, spirit (opens that spirit's spell list), menu (a ring), submenu (a config ring screen).  Menu slots
  carry the category art in a yellow frame (a drawn gear for the config ring); submenu slots show two letters (EQ, TG, ST, LV, AG, PD, WN).
* Tap on a filled slot: weapon, item and spell behave like the grid cell (`ACT_CMD`, or the target panel when a target is needed;
  in the ring a target-needing slot returns `ACT_CLOSE_RING` and keeps the pending target); menu and submenu return `ACT_OPEN_MENU`
  (kind, id, acting hero); a spirit slot only switches the tab (UI state, no action).  Unavailable entries (weapon not owned, no
  item, not enough MP) are greyed and inert.
* Tap on "+": in the ring it binds the highlighted entry; in the field it opens the quick-slot editor on that slot.
* Long press on any slot: opens the editor on it (`ACT_QUICK_EDIT`, informational: vibrate).
* Editor (OPT > QUICK SLOTS, or the two paths above): the slot row (tap to choose the target slot), five picker tabs MENU / WEAPON /
  ITEM / SPIRIT / SPELL, a 4 x 2 page of entries with PREV / NEXT, and CLEAR SLOT, KEY (physical-button capture for the slot's entry)
  and BACK.  Picking an entry stores it (`ACT_QUICK_SET`, slot, kind, id; kind 0 = cleared).
* Persistence is the host's: restore with `som_ux_quick_set()` at start, save on every `ACT_QUICK_SET`.

### 3h. Options panel

The old strip at the bottom (Wide, Fallback, HUD hide, Core, Binds ...) is gone from the main screen.  The gear button labelled OPT is
the right end of the header in every mode; it opens the overlay and toggles it closed.  Page `SOM_UX_OPT_MAIN` has ten cells in two
columns: WIDE, FALLBACK, HUD HIDE, CORE, UI SOUNDS, HAPTICS, LANGUAGE, BINDINGS (list page), QUICK SLOTS (editor page) and BACK.
Toggles show ON / OFF in large type with a checkbox (shape, not colour only); enumerations (CORE, LANGUAGE) show their value text and a
triangle.  The host owns the values: it pushes them with `som_ux_set_option(id, value, text)`; a tap returns `ACT_OPTION` (id and
the value asked for; toggles are flipped locally, enumerations ask for current + 1 and the host wraps).  UI sound and vibration are the
host's: the UX layer only reports taps.

### 3i. Names are never cut (label fitting and the layout audit)

* Every cell label goes through one planner: the game face on one line; else two lines (break at a space or after a hyphen);
  else the same two tries with the *tight* face (1 vp less per glyph); else the *squeezed* face (3/4 width).  Cells choose a vertical
  shape (icon on top, up to two label lines below, the icon's last rows may sit under the first line) or, when that does not fit, a
  horizontal one (icon and badge at the left, two label lines at the right).  Portrait uses wider cells where it matters: the spell
  list is 2 x 2 and the quick-slot editor 2 x 4.  The longest vanilla names (THUNDERBOLT, WERE-BUSTER, DAEDALUS LANCE, VALKYRIAN)
  all fit; the tight face is used only for a word wider than the cell (about 60 vp in the Thor portrait grid).
* Hero cards have no overlapping text rows at any size: 3DS compact cards, Thor landscape cards (big HP number, MP on one row)
  and the Thor portrait "slim" cards (31 vp: name and level in a left column, HP and MP rows 13 vp apart with long bars).  When
  the status icons would push into the name, the warning triangle and then the ailment icon are dropped (the bar hatch and the card
  frame still say danger).  The enemy rows of the target panel are two-row (name and number, then a full-width gauge) on the Thor.
* Ring avatars read "HP n" in front of a bar (7 vp tall), or a KO box.
* Audit: while drawing, the renderer counts every cut text, every label that fits neither cell shape in any face, every badge wider than
  its slot, every card or avatar text row that overlaps another and every title wider than its panel (`som_ux_audit()`).
  `test_som_ui_draw.c` requires zero after every rendered scene.  `tools/companion/ux/test_text_fit.py` puts every name the UI can
  show into every cell in all three layouts and with the ROM names of vanilla, Magno and both Ancient Cave seeds (666 draws per ROM:
  every weapon type at every tier for each hero, the item grid, every spirit and spell grid for the girl and the sprite, the ring
  panel for all 42 spell ids and every ring entry, quick strip and editor pages with badges, bindings list, options, every enemy
  name in the target panel, nine card states including four-digit HP, KO, poison, ailments and danger) and fails on any non-zero
  audit.  Result: `<state>/ux/evidence/text_fit.json`, 0 problems on all four ROM variants.

### Why this layout (alternatives considered)

* A copy of the ring menu on the touch screen: rejected, it keeps the slow ring and wastes the second screen.
* One giant grid of every action: rejected, three heroes x eight spirits x six spells does not fit a finger-sized grid.  The
  tab + spirit sub-grid keeps each tap decision under twelve choices.
* Hero cards as permanent targets: kept.  Tapping a card selects the hero for the grid; in the target panel the same cards
  become the choice of ally, so the player never reads a new layout.
* Radar: the game top screen is the map; the bottom radar is only a tap surface for enemies and a threat read-out, so in
  portrait it moves to the MAP tab to give the grid 30 vp rows.

## 4. Flammie world map mode

* Position is `$FA`/`$FC` (word, 8 units per world-map pixel, 4096 x 4096 units = 512 x 512 pixels) and the heading is the
  word `$F6` (1024 per turn; 0 north, 256 west, 512 south, 768 east).  The bearing shown is degrees clockwise from north.
* `som_ux_flammie_map_pos()` returns `(x/8, y/8)` and the bearing; `fl_world_ok` is false when the position is outside the
  main world (the other-world Flammie map shows "UNMAPPED WORLD").
* The map picture is decoded at runtime from the user's ROM: 512 row pointers at 0x68000 with data at 0x60000 (+0x10000 each
  time the pointer wraps), RLE rows, palette at 0x67D00.  No picture of the map is stored.  The palette alignment is approximate.
* Scaled with box averaging into the map square; the trail keeps the last 32 positions.

## 5. Locked events and dialogue

The event screen is the "locked" state: `$D0 != 0` means a script owns the game, so commands are refused by the direct layer
(`UNSAFE_STATE`) and the UI says so.  The dialogue line is read from the game's text buffer `$A1A4` (glyph 0x81..0x9A lowercase,
0x9B..0xB4 uppercase, 0x80 space, 0xB5..0xBE digits).  The buffer holds only the line being typed, so the log shows the
latest distinct lines polled each frame (six lines).  NEXT sends B.

Note: `docs/text.md` lists the lowercase/uppercase glyph order swapped; `tools/names.py` and the live game agree with the order
above.

## 6. Accessibility

* Contrast: white outlined text on dark blue; status colours are never the only signal (KO and PSN also print text,
  disabled cells are dim and the label stays readable).
* Text is 11 vp tall (2.7 mm on the Thor, 2.1 mm on the 3DS), vitals up to 5.3 mm.  The Thor integer scale keeps the pixel art sharp; text is never scaled by a fractional factor.
* No colour-only HP: the number is always printed next to the bar, and danger adds a hatch pattern, a dashed or solid frame and
  a warning triangle (section 3c).
* The pending-target title ("CAST ON ... CHOOSE A TARGET") and the toast text name the action in words.
* All targets grow with the integer scale, none below 6 mm on the Thor.
* One-handed use: the quick slots, the BIND buttons and the pad-like buttons (name entry, START) are in the lower third; in the ring
  the whole lower half is quick access.
* Left as future work: a large-text option (size 1 font for the small labels), haptic ticks, and a left-handed mirror.

## 7. Error and unsafe states

| Situation | How it shows | Source |
| --- | --- | --- |
| Not safe to inject (event, fade, ring open, no controllable hero) | header chip LOCKED / EVENT / WAIT / RING (red or yellow), grid greyed, nothing is sent | `som_state.safe_to_inject`, classifier |
| No MP for a spell | cell greyed, tap ignored | direct layer `NO_MP`, mirrored by the grid |
| Wrong target class | the cards that are not valid stay dark; ALL ALLIES / ALL ENEMIES light only when the class allows them | `target_ok_*` |
| Dead or absent hero | portrait greyed, KO in red | WRAM status `0x8000` |
| Modal item (Magic Rope, Flammie Drum) | falls back to the ring macro, header shows RING | `NEEDS_RING` |
| ROM names unavailable or a modified ROM moved the name block (Ancient Cave) | built-in English names | sample check in `decode_names` |
| Unknown ROM or no pack | built-in procedural art; everything still works | `som_ux_set_pack(NULL)` |
| Flammie outside the main world | "UNMAPPED WORLD" instead of an arrow | `fl_world_ok` |
| Short or odd canvas | clamped, never crashes (unit test with 64x64) | tests |

## 8. What is verified in the real game and what is a mock

Runs on the libretro snes9x harness (`tools/companion/retro.py`) with real WRAM dumps.  Evidence JSON:
`<state>/ux/evidence/verify_modes.json` (made by `tools/companion/ux/verify_modes.py`).

| Item | Vanilla US | Magno (Spanish) | Ancient Cave (seeds 1, 777) | Notes |
| --- | --- | --- | --- | --- |
| FIELD classified, 3 heroes present | yes | yes | yes | saved free-field states |
| RING (`$7D = 1`, `$1847 = 0`) by pressing Y | yes | yes | yes | |
| SHOP (`$7D = 1`, `$1847 = 1`) via event command 0x2E | yes | yes | yes | buy, sell, upgrade captured on vanilla |
| EVENT (`$D0 != 0`) | yes | yes | yes | |
| Dialogue line decoded from `$A1A4` | yes (natural text) | not read: crafted text did not display, buffer empty | yes ("...might happen.  Enjoy!") | game-originated text only |
| ROM names (weapon, item, enemy) | yes | yes (accents print as `?`) | no, the name block moved: fallback to built-in names | per-sample check |
| FLAMMIE | yes, natural flight from Potos (`$E4 = 4`), position and heading follow the pad | classifier only (`$E4` poked) | classifier only | the poke proves the rule, not a flight |
| FRONTEND, BOOT, name-entry cursor | yes (new-game script) | not run | not run | |
| Boss flag `+0x1FB` bit 6 and boss HP | yes, map 232 fixture | not run | not run | |
| World-map decode (512 rows, 0 bad rows) | yes | not run (same table assumed) | not run | |
| Enemy list and radar | yes, maps 215/222/232 | not run | not run | |

Second round (typography, vitals, danger, icons, bindings):

| Item | Status |
| --- | --- |
| Original ring icons: spells of gnome, undine, sylphid, the girl's Luna and the sprite's Shade, the spirits, items and weapons | verified against the running vanilla game (ring crops, `extract_icons.py`); other spells by meaning, not verified |
| Hero cards, bars, numbers, grid and target panel draw from real WRAM (names, levels, HP, MP, items, weapons, enemies) | real data on vanilla fixtures (maps 215, 222, 232) |
| Danger thresholds, KO, poison and other-ailment icons | mock: HP and status words edited on the captured WRAM (the draw logic runs on real struct fields; the pulse is checked by the unit test) |
| Which status bits mean what besides poison and KO | not decoded: one generic ailment icon |
| Hotkey badges, long press, bind screen, bindings list | mock: no binding store exists yet; the tests drive the API |
| Layout, text and touch sizes on Thor and 3DS panels | computed from the specs, not seen on hardware |

Third round (ring info, avatars, quick slots, options).  Evidence JSON: `<state>/ux/evidence/ring_selection.json`
(`tools/companion/ux/capture_ring.py`).  Method: open the ring with Y, cycle the owner with X and the category with UP, walk one full
lap of every ring with RIGHT, enter the spirit ring's first spirit with B (spell sub-ring) and leave with Y; nothing is confirmed.

| Item | Vanilla US | Magno (Spanish) | Ancient Cave (seeds 1, 777) | Notes |
| --- | --- | --- | --- | --- |
| Ring cursor `$180A`, category `$1809`, owner `$1808`, count `$180B`, busy `$1812` read live | yes | yes | yes | every lap had `count` distinct, decoded entries for the boy (weapon, item, config) and the girl and sprite (also magic): 0 problems |
| Weapon, item, spirit entries match the owned lists (titles and order) | yes | yes | yes | AC has all 8 weapons and 8 items, vanilla 4 weapons and 6 items |
| Spell sub-ring: `$1803` bit 7 set, `$1839` = highlighted spell id (follows the cursor), 3 spells | yes (girl: Stone Saber 3, Defender 5, Speed Up 4; sprite: Earth Slide 0, Gem Missile 1, Speed Down 2) | yes (same ids) | yes (same ids) | the id formula `6 * spirit + 3 * girl + slot` held in all four |
| Spell and item names | ROM names | ROM names, accents print as `?` (`CHOCO??`, `?XIDO`) | fallback names: the generator moved the name block (`SPELL 3`, `SWORD`, `CANDY`) | per-sample sanity check |
| Owner order of X: 0 -> 1 -> 2 -> 1 -> 2 (never back to the boy); Y closes, Y reopens at owner 0 | yes | yes | yes | the avatar macro in the glue relies on it |
| Item effects, config entry descriptions, spirit elements | authored text, not read from the game | | | the game has no such strings in the ring |
| Quick slots, BIND buttons, options panel, avatar taps, quick-slot editor | mock: the tests drive the API; no host persistence or sound exists yet | | | |
| Touch sizes in mm | computed from the panel specs, not measured on hardware | | | |

Mocks (not seen in a running game):

* The shop item list and prices (not located in WRAM): the shop screen shows the mode, the gold, the party cards and "price and name list: see the
  top screen".
* The "hurt" HP/MP/status values in the field mocks are edits of a few WRAM words on the captured fixtures, to show gauges
  and the PSN chip.  Names, levels, items and enemies in those scenes are real.
* The enemy list in `field_boss` is real; the boss HP value is from the captured fixture.
* Landing records and spirit slots 6/7: the spirit byte naming conflict (6 vs 7 for Luna/Lumina on the girl) is unresolved;
  the grid follows `som_companion.c`.
* Device behaviour (real Thor and 3DS panels): not tested, no hardware access in this run.  mm figures are computed from
  the panel specs.

Other findings to pass on:

* `som_read_state` classifies Flammie flight (`$E4 = 4`) as TRANSITION; the UX classifier distinguishes it.
* `docs/text.md` has the lowercase/uppercase glyph blocks swapped.

## 9. Licence of the art

The Godot project (GPL-3.0) states in its README that parts of `assets/` are Square Enix artwork kept as stand-ins and cannot be
distributed.  The pack is therefore generated into the state directory and never committed; the repository holds only code,
the pack manifest and hashes.  The renderer is fully usable without the pack.

## 10. Prototype renderer

* `tools/companion/ux/som_ui_draw.h/.c`, `som_ui_state.c`, `som_ui_priv.h`: pure C99, depends on `som_companion.h`.
  Public API: `som_ux_update(wram)`, `som_ux_draw(w, h)`, `som_ux_touch(x, y, w, h)`, `som_ux_classify`, `som_ux_flammie_map_pos`,
  `som_ux_min_target_vp`, `som_ux_name_next_pad`.
* `tools/companion/ux/som_ui_draw.h/.c`, `som_ui_state.c`, `som_ui_sel.c` (ring selection decoding and the name tables), `som_ui_priv.h`.
* Tests: `tools/companion/ux/test_text_fit.py` (names and rows in every cell, see 3i) and `tools/companion/ux/test_som_ui_draw.c` (289 checks, clean under AddressSanitizer and UBSan: all modes, tabs, weapon equip,
  target panel, cancel, locked event, Flammie mapping and heading, front end, bindings set/clear/long press/bind screen/bindings
  list, ring selection of every category and the spell sub-ring, no ring pad buttons, avatars, BIND ENTRY / BIND MENU, quick
  slots in ring and field (bind, run, clear, edit), the editor, the options panel, touch-size floors, danger pulse and calm in
  cutscenes, KO, small canvas, WRAM unchanged by drawing).  Build and run:

  ```
  cc -std=c99 -Wall -Wextra -O1 -o /tmp/test_ux tools/companion/ux/test_som_ui_draw.c tools/companion/ux/som_ui_draw.c \
     tools/companion/ux/som_ui_state.c tools/companion/ux/som_ui_sel.c tools/companion/som_companion.c -lm
  cd tools/companion/ux && /tmp/test_ux            # compares pixel hashes with golden_hashes.txt
  /tmp/test_ux --dump <state>/ux/golden            # raw ARGB goldens (state dir only)
  /tmp/test_ux --update-hashes                     # after an intentional layout change
  ```
* Pixel goldens: pack-less renders (built-in art) at 320x240, 1240x1080 and 1080x1240 (45 images); only FNV-1a hashes are committed
  (`tools/companion/ux/golden_hashes.txt`).
* Tools: `build_assets.py` (pack, now with the spell icons), `extract_icons.py` (ring icon crops from the real game), `capture_wram.py` (WRAM per mode), `capture_ring.py` (ring cursor, category, sub-ring, owner order on the four ROMs; writes the ring
  WRAM images used by the ring mocks), `render_mocks.py` (PNG mocks), `verify_modes.py` (cross-ROM mode check), `ux_lib.py` (ctypes wrapper).

## 11. Android integration proposal (no existing file is edited)

Glue: `tools/companion/ux/android_patch/som_ux_glue.h` (header-only, compiles and links on the host).  Steps for the Android owner:

1. Assets to bundle (generated, not in the repo): `<state>/assets/som_ui_pack.bin` (1.05 MB) copied to
   `app/src/main/assets/som_ui_pack.bin`.  Optional: the app falls back to procedural art when it is absent.
2. `CMakeLists.txt`: add `${COMPANION}/ux/som_ui_draw.c`, `${COMPANION}/ux/som_ui_state.c` and `${COMPANION}/ux/som_ui_sel.c` to `somjni`, and `${COMPANION}` is
   already an include directory (the glue includes `ux/som_ui_draw.h`).
3. `som_jni.c`: `#include "ux/android_patch/som_ux_glue.h"`; after `loadRom` call `ux_glue_init(pack, len, rom_copy, n)` (the ROM
   memory is already kept in `rom_copy`); after each `r_run()` call `ux_glue_frame(wram)`.  New JNI functions:
   `Native.renderUx(Bitmap bmp)` (draw at the bitmap size, then convert to the bitmap channel order exactly as
   `renderStatus` does) and `Native.touchUx(int x, int y, int w, int h)` which stores the tap; the loop thread calls
   `ux_glue_touch` and dispatches: `ACT_CMD` -> `run_direct(&cmd, target)` with `target_kind` 1/2/3 mapped to
   `SOMD_T_ONE/ALL_ALLIES/ALL_ENEMIES` and `target_slot` as the slot (same numbering as `struct somd_target`); `ACT_PAD` ->
   OR the mask into `pad_latch`; `ACT_NAME_LETTER` -> repeat `ux_glue_name_step(letter)` every 28 frames until the cursor is there.
   `draw_target`, `touch_target`, `grey_cells` and `pend` become unnecessary: the UX layer owns the target panel and the greying.
3b. Bindings: add `Native.longPressUx(x, y, w, h)` (same dispatch as a tap) and `Native.setBinding(kind, id, label)` for loading the store at
   start; the capture itself and the store live in the Android layer (`ux_glue_longpress`, `ux_glue_bind_store`).
3c. Third-round API for the glue (all in `som_ux_glue.h`; the contract is described at the top of that file):
   * Dispatch kinds new to the host: `OPTION` (opt_id, opt_val), `AVATAR` (hero), `OPEN_MENU` (bind_kind MENU or SUBMENU, bind_id,
     hero), `QUICK_SET` (slot, bind_kind, bind_id; kind 0 = cleared), `QUICK_EDIT` (haptic tick), `CLOSE_RING`.  The ring and the shop
     no longer produce `ACT_PAD`.
   * State the host pushes: `ux_glue_option(id, value, text)` for WIDE, FALLBACK, HUD_HIDE, CORE, UI_SOUNDS, HAPTICS, LANGUAGE;
     `ux_glue_quick(slot, kind, id)` per slot at start; `ux_glue_bind_store()` per physical binding (kinds now include SPIRIT, MENU,
     SUBMENU).  Remove the config strip (Wide, Fallback, HUD, Core, Binds) from the Java layer: the gear in the header replaces it.
   * Macros the host must provide, only pressing real buttons and reading `$7D $1808 $1809 $180A $1812 $1803`: avatar owner switch
     (X for 1 <-> 2; close with Y and reopen to reach the boy), open menu / submenu (open ring, UP to the category, RIGHT/LEFT to the
     config entry, B), close ring (A, then Y, at most 12 taps).  The sequences and the verified behaviour are in the glue header.
   * Sounds and haptics: play the confirm sound when a dispatch returns 1, the cursor sound for tab/cell taps (return 0) and the error
     buzz when a locked state swallows a tap; all gated by `ux_glue_option_value(SOM_UX_OPT_UI_SOUNDS)` and `..._HAPTICS`.
4. `Native.java`: add `renderUx`, `touchUx` and `loadPack(byte[])`.  `StatusView.java`: allocate the bitmap at the view size
   rounded down to an integer multiple of the vp grid (1240x1080 on the Thor landscape, 1080x1240 portrait) instead of a fixed
   320x240; send `Native.touchUx(x, y, bmp.getWidth(), bmp.getHeight())` using the view-to-bitmap scale.
5. Orientation: pick the Thor portrait or landscape bitmap from `getWidth() >= getHeight()`; the layout builder handles both.
6. Direct agent contract: the `som_ux_tclass` values equal `enum somd_class`; the action carries the macro command plus the target,
   so no mapping table is needed.
7. 3DS: the same C files build with the n3ds port (pure C99, no OS use); a 320x240 buffer in, a touch point out.

### 3j. Round 3c: device-path defects (glued labels, level over the face)

Cause. The Android view is not 1240x1080: the navigation bar is taken off, so the bitmap is 1240x970 (310x242 vp at 4x). That is
layout form 0 (the 3DS-style three cards in a row) at Thor scale, a size the earlier tests and mocks never rendered. The device capture
`device/final/field_bottom.png` is reproduced by `som_ux_draw` at 1240x970 (outside the cards the match is 99.8 percent, the rest is
RGB565 rounding). In that form the old compact card drew `H523/524` (short label, no gap) and `L50` on the portrait.

Fixes (`som_ui_draw.c`):
* Compact and wide cards, and the slim portrait rows, draw the label and the numbers through `value_row()`: the label is always `HP`/`MP`
  and is followed by a real gap (a space glyph, or 2 vp in the squeezed last resort); the face tightens before anything is cut.
  Tall and mid cards use `big_row()` (big current value, small maximum); with 3-digit/3-digit values the value face tightens, then drops
  to normal size when the card is too short.
* Narrow cards put the level under the portrait gauge (`L50`), or in the name row when there is no room; never over the face.
* Layout thresholds found by a height sweep (`test_text_fit.py --sweep`, landscape 932..1100, portrait 1100..1300): landscape heights up
  to 256 vp use form 0; the ITEM grid needs 38 vp per cell, so the quick strip yields on a short panel; the quick editor buttons shrink
  to 31 vp on a short portrait panel.
* Hypothetical 4-digit HP (the game caps at 999) in the compact card uses the squeezed face for the label too.

Tests added:
* `test_glue_path.c` + `test_device_path.py`: renders through `android_patch/som_ux_glue.h` (`ux_glue_init/frame/draw`, real pack and ROM)
  at 1240x1080, 1080x1240, 1240x970, 1080x1160 and 320x240 for map 215 with HP 523/416/411, 3-digit, 4-digit and low values. Checks:
  layout audit, label contract (every hero has a separate `HP` box, and `MP` when it has MP, followed by its numbers with a gap >= 2 vp),
  no text over a portrait, no overlapping text boxes (`som_ux_text_log`), bit-identical to the ctypes renderer used for the mocks, golden
  hashes (`glue_hashes.txt`), and the device capture outside the cards.
* `test_text_fit.py` now also draws at 1240x970, 1240x940, 1240x1000, 1240x1004, 1080x1160, 1080x1100 and has 3-digit, 4-digit and
  4-digit-with-ailment card states; 0 problems on vanilla, Magno, Ancient Cave seeds 1 and 777. `--sweep` covers every height.
* The audit now flags glued labels, text outside the card, text over the portrait and overlapping text boxes (mutation-checked).

New mocks: `field_device_*` (map 215, HP 523/416/411) and `field_hp4_*` at 3ds, thor, thor_nav (1240x970), thorp, thorp_nav (1080x1160).

### 3k. Round 4: hands-on Thor feedback (header, pressed look, target cards, MAP, pickers, bind, policies)

Mocks: `<state>/ux/mocks/` (`target_enemy_*`, `target_ally*`, `policy_*`, `picker_*`, `bind_wait_*`, `pressed_*`, `map_*`, `font_sheet_*`, header in every field mock).

1. **Font and header.** On the Thor (scale >= 2) all UI text uses IBM Plex Sans Condensed SemiBold (SIL OFL, `tools/companion/assets/fonts/`), rasterised per device scale into the pack; the 3DS keeps the game pixel font. The sheet `font_sheet_*.png` shows the full alphabet, digits and the W/M/N words in the three faces (normal, tight, condensed); text is sized in device pixels, so the cap height stays about 2.6 mm. The header is a single 15 vp pill with one text size (`READY`, `RING`, `OPTIONS`), never the old giant badge.
2. **Area name.** The header shows the area name and never the map number. The ROM holds no map-name table; `ux/area_names.json` is built from `docs/maps.md` and the device exploration, every range with its evidence and confidence (47 maps named: Potos, Path to Pandora, Pure Land). Unknown maps show nothing (`WORLD MAP` on the world map). `test_area_names.py` checks that the JSON and `som_ux_area_name()` agree on ids 0..511.
3. **Pressed look.** `som_ux_touch_down/move/up(ux, ...)`, `som_ux_press_hit(ux, hit, a, on)`, `som_ux_pressed()` and the JNI entry `som_ux_press(ux, x, y, w, h, down)`. The pressed control is drawn depressed (darker fill, inner shadow, rect shrunk 1-2 vp, text offset) in `win()`/`draw_hit`; moving the finger off un-presses it. A hotkey can flash its slot with `som_ux_press_hit(SOM_UX_HIT_QUICK, slot, 1)`. The host still plays the click and the haptic tick.
4. **Target selector.** A full-width modal. Enemies: a grid (1-3 columns, up to 2-3 rows, up to 9 cards) of cards with the enemy picture, name and HP bar. Allies: three big portrait cards with name, `HP n/m` and bar. `ALL ENEMIES` / `ALL ALLIES` is a wide highlighted button next to `CANCEL`. Every card and button is >= 58 vp tall and wide on the Thor (14 mm at 0.24 mm/vp); tests enforce it. Enemy sprites (revised after the first Thor test: the selector shows ONLY the sprite, never a camera crop): the frontend supplies one straight-alpha ARGB sprite per enemy id with `som_ux_set_enemy_sprite(ux, enemy_id, argb, w, h)` (w, h <= 96, transparent background, any trim; `som_ux_enemy_sprites_wanted()` lists the ids on screen that still lack one; `som_ux_clear_enemy_sprites`). The card centres the opaque part, scales it by the largest integer factor that fits (nearest neighbour), alpha-blends it over the plain card and adds a soft drop shadow; without a sprite a silhouette is drawn. The crop API (`som_ux_enemy_crop_rect`, `set_enemy_crop`, `ux_glue_crop_enemies`) is gone. The mocks use `UX.sprites()`, a flood-fill stand-in that cuts the enemy out of the frame, until the sprite atlas of `tools/companion/enemysprites` exists. Glue: `ux_glue_set_enemy_sprite`, `ux_glue_clear_enemy_sprites`, `ux_glue_sprites_wanted`.
5. **Quick-slot icons** are centred on the opaque bounding box of the icon art (`icon_bbox`) with equal padding for weapons, items, spells and spirits; the badge keeps its own band.
6. **Locked modes.** Cutscenes, dialogue and events no longer print any text: the whole bottom window is dimmed.
7. **MAP tab.** `som_ui_zone.c` builds the zone map from WRAM (tile layers, tile words) and the ROM tile graphics (party and enemies overlaid, world map image for the world map); `som_ux_set_zone_image()` lets the frontend pass its own. The tab opens in the LARGE view: only the tab row stays, the whole zone is fitted into the rest of the bottom screen, markers are bigger (active hero with a halo), and the area name sits on the map. A tap (`SOM_UX_HIT_MAPVIEW`) switches to the small view (close-up around the active hero plus the enemy list, party cards and slots back); the choice (`map_small`) is kept for the session. Mocks `map_215/222/232_*` (large) and `map_*_small` on three layouts.
8. **Two-level spell picker** (quick-slot editor, SPELL tab): the first level lists the spirits with their icon (`qe_spirit` = 0), a tap opens that spirit's six spells with a `< SPIRITS` back button (`SOM_UX_HIT_QE_BACK`). The SPIRIT tab still binds a spirit itself.
9. **Bind screen.** One icon, the entry name, `PRESS A BUTTON`, and two big buttons `CLEAR` / `CANCEL` (60 vp). The legend and the long help are gone (`som_ux_set_legend` is now optional).
10. **Target policy.** Items and spells that need a target ask for a policy when stored in a quick slot or bound to a hotkey: enemy targets `SELECT` / `NEAR` / `ALL` (ALL only when the spell allows a group), ally targets `SELECT` / `LESS HP` / `ALL`; self and no-target entries ask nothing, and an ally-only choice is skipped when the player is alone. Chooser buttons: `SOM_UX_HIT_POLICY` (a = `enum som_ux_policy`) and `SOM_UX_HIT_POL_CANCEL`. `ACT_QUICK_SET` carries `policy`; the slot shows an N/A/L tag. Hotkeys: store with `som_ux_set_binding_policy`, and when the button fires call `som_ux_run_entry(kind, id, som_ux_binding_policy(...))` (glue: `ux_glue_hotkey`): NEAR / LESS HP / ALL return the command at once, SELECT returns 0 with the selector already showing, and a lone hero skips it.

11. **Revive chooser.** Tapping a fallen hero's card (field) or avatar (ring) opens a modal listing the ways to revive: Cup of Wishes (item id 5, with its quantity from the inventory, greyed at 0), the Dryad spell Revivifier (spell id 33, cast by the girl: MP cost and caster shown; greyed with the reason `NOT ENOUGH MP`, `SPIRIT NOT OWNED` or `CASTER DOWN`), and any other item whose class is `ALLY_DEAD_ONE`. A choice runs at once on that hero: `ACT_CMD` USE_ITEM or CAST_MAGIC with target_kind 1 and target_slot = the fallen hero (the item user is the viewed hero or the first standing one). Rows and CANCEL are >= 58 vp on the roomy forms. API: `som_ux_revive_open/count/option/choose/close`, glue `ux_glue_revive_open`, `ux_glue_revive_choose`, hit kinds `SOM_UX_HIT_REVIVE` (a = option) and `REV_CANCEL`. The chooser closes by itself when the hero stands again.
12. **Spell availability states.** `enum som_ux_avail` ENABLED / DISABLED / QUEUED per spell id. The host (direct layer: `somd_can_cast` reasons and queue) passes the array with `som_ux_set_spell_avail()` (glue `ux_glue_spell_avail`); entries of 0, or no array, derive the state from WRAM (caster alive, MP, spirit owned). DISABLED spells are dimmed and ignore taps; QUEUED ones show a clock badge on the icon (spell lists, hotkey slots, revive row) and a tap or the hotkey dispatches `ACT_CANCEL_QUEUED` (bind_id = spell id, hero = caster).
13. Mocks of this round: `target_enemy_*` (sprites), `revive*`, `spells_states_*`, `slots_states_*`, `map_*`.

14. **Polish round.** (a) Headers name what is edited: `SLOT` (quick-slot editor), `KEYS` (bindings list), `OPTIONS` (main page), `BIND`, `TARGET` (target selector and policy chooser), `REVIVE`; gold and area are hidden on those screens. (b) The policy chooser states the spell's real target class: `ONE ENEMY`, `ONE OR ALL ENEMIES`, `ONE ALLY`, `ONE OR ALL ALLIES`, `ONE FALLEN ALLY`, `ONE ALLY WITH AILMENT`, `THE BOY ONLY`. (c) Queue notices: `som_ux_notice(kind, text)` (QUEUED, CANCELLED, DROPPED with a reason cut to 14 characters, INFO, ERROR) draws a small SoM-window chip in the header strip between the status pill and the gear (never over the content, never clipped: condensed faces and an audit), shape-coded icon (clock, cross, !), 120 frames with a 24 frame fade, no hit target. It replaces the host's red toast for queue events (glue `ux_glue_notice`). (d) First frame: `som_ux_draw` works on a fresh object (title block), so the host draws right after init and on every surface creation (`ux_glue_first_frame(px, w, h)`) and then only when `som_ux_needs_redraw(w, h)` / `ux_glue_needs_draw` says so (state hash, pulse, press, notice, size change). (e) Thin sprites: the card paints a two-level soft glow behind every opaque pixel, the sprite is centred on its opaque bounding box, enlarged by the largest integer factor, reduced by area averaging when larger than the card, and half-transparent pixels are strengthened.

Glue API changes (all additive): `struct ux_dispatch.policy`; `ux_glue_hotkey`, `ux_glue_quick_policy`, `ux_glue_bind_policy`, `ux_glue_touch_down/move/up`, `ux_glue_press_slot`, `ux_glue_set_enemy_sprite`, `ux_glue_spell_avail`, `ux_glue_revive_open/choose`; `som_ux_press` is implemented; `ACT_CANCEL_QUEUED` is a new action kind. The hit enum gained `POLICY`, `POL_CANCEL`, `QE_BACK`, `REVIVE`, `REV_CANCEL` at the end (ux_lib.py mirrors it).

Tests: `/tmp/test_ux` (939 checks, also under ASan/UBSan), `test_text_fit.py` (2907 draws per ROM, 0 problems; also revive chooser for each fallen hero and spell states; covers enemy grids of 1..9, ally cards, the chooser for every spell and item, both picker levels, bind screens, MAP tab; `--sweep` 28858 draws, 0 problems), `test_device_path.py` (glue path incl. the new API; the comparison with the old device capture is informational now, since the capture predates the font/header redesign: `SOM_UX_STRICT_CAPTURE=1` restores it once the Android agent takes a fresh `field_bottom.png`), `test_area_names.py`.

Known limits: the 3DS keeps the pixel font; at most 6 enemy cards fit on the roomy forms without scrolling (9 on the Thor portrait); enemy sprites come from the frontend (silhouette otherwise); the old device capture is stale; the area table names only three areas.

### 3l. Round 5: names, MP bars, menu icons, slot feedback, shoulder slots

1. **Hero names.**  They did NOT work before: the cards always showed the fixed BOY / GIRL / SPRITE.  The names live at WRAM `$CC00 + 12 * slot`,
   one game glyph per byte (0x80 space, 0x81..0x9A a-z, 0x9B..0xB4 A-Z, 0xB5..0xBE 0-9, punctuation above), ended by 00 or FF; found by typing "ABC"
   in the new-game name entry of the vanilla ROM (`9b 9c 9d 00` at `$CC00`).  `som_ux_update` decodes them (upper case, at most 9 characters); an
   empty slot or no readable glyph falls back to BOY / GIRL / SPRITE.  Read them with `som_ux_hero_name(ux, slot)` / `ux_glue_hero_name(slot)`.
   Limit: the accented glyphs of other ROMs (Magno) have no verified code, so they are dropped, not guessed.
2. **MP bars.**  Every hero with magic (girl, sprite) always draws an MP bar and the MP number as large as the HP row in all layouts (including
   1240x970 and the 3DS); MP below a quarter turns the bar orange, zero shows an empty bar and "0".  The boy shows a dim "NO MP" marker, never a gap.
   `som_ux_mp_bars(slot)` plus a draw audit ("hero with magic shows no MP bar") test it.  Mocks: `mp_full|mp_low|mp_zero_{3ds,thor_nav,thorp_nav}.png`.
3. **Menu icons.**  Taken from the Mana Magic sheet `spr_iconsGeneral` (all 49 cells are in the state-dir pack as ids 0x1000 + n; never committed):
   Equip Armor, Status, Levels, Action Grid, Controller and Window have their own cell; the ring categories use the weapon, candy and spirit art
   and a drawn gear for CONFIG; Targeting has no clear art and uses the nearest cell (a guess).  Entries with no icon of their own: the CONFIG
   category (drawn gear) and Targeting (borrowed).  Icons appear in the ring info panel, in quick slots bound to menus or submenus and in the
   editor pickers (`quick_editor_menu*_*.png`).
4. **Slot feedback.**  `som_ux_slot_flash(ux, slot, kind)` (0..7, 0..3 are the shoulder slots) plays an 18 frame (0.3 s at 60 fps) animation: a
   white flash that fades, a ring growing from the centre and a glyph (`SOM_UX_FLASH_OK` check, `CAST` star, `QUEUED` star with a clock, `REFUSED`
   cross).  `som_ux_flash_entry(ux, kind, entry_kind, entry_id)` flashes every slot holding the entry and, for a spell, its button in the spell list.
   Taps start it by themselves (slot taps, a spell cast from the list, a target chosen for a pending spell or item); `som_ux_shoulder_press` does
   it for hotkeys; the host calls `flash_entry` for a queued spell that fires later.  `som_ux_update` ticks it; `som_ux_needs_redraw` is 1 while
   it runs.  Casting from the spell list no longer returns to the spirit list, so the button flash is visible.  Mock strip (frames 0, 3, 6, 9, 12,
   15, 17; slot above, list button below): `slot_flash_{3ds,thor_nav,thorp_nav}.png`.
5. **Shoulder slots and the new binding model.**  Quick slots 0..3 are fixed shoulder slots L1, R1, L2, R2 (3DS: L, R, ZL, ZR through
   `som_ux_set_shoulder_labels`); slots 4..7 are the free touch-only slots in the middle.  Shoulder slots have an orange rim and print the button
   label permanently (also when empty); free slots look as before.  Order: an 8 x 1 row reads `L2 L1 [4 5 6 7] R1 R2` (compact 3DS / 1240x970 and
   portrait), the 4 x 2 landscape grid `L1 [4 5] R1` over `L2 [6 7] R2`.  Putting an entry in a shoulder slot binds it to that button by position.
   "Assigning" always means "put this action in this slot": (a) tap an empty or filled slot to open the slot editor on it and pick from the
   pickers; (b) in the ring, ENTRY > SLOT or MENU > SLOT, then the slot; (c) long press on a weapon, item, spirit or spell cell arms it ("TAP A
   SLOT" notice) and the next slot tap stores it, any other tap cancels.  The target policy chooser (SELECT / NEAR / ALL or SELECT / LESS HP / ALL)
   is still asked at assignment time.  The slot editor header reads SLOT L1 ... SLOT 4 and has CLEAR SLOT and BACK only.  The ITEM tab keeps the
   slot row on every layout: on a short panel the items use two rows of six cells (long names break with a hyphen).  Slot height is 34 vp on the
   Thor (29 on the 3DS).  Mocks: `slots_{empty,filled,armed}_{3ds,thor_nav,thorp_nav}.png`, `slots_3ds_labels_3ds.png`, `field_quick_*`,
   `ring_weapon_filled_*`, `quick_editor_*`.

Glue changes for the Android agent.  REMOVED: `ACT_BIND_BEGIN`, `ACT_BIND_CLEAR`, `ACT_BIND_CANCEL` (kept as reserved enum values, never returned),
`ux_glue_bind_store`, `ux_glue_bind_policy`, `ux_glue_hotkey`, `ux_glue_legend`; `som_ux_get_binding`, `som_ux_set_binding`, `som_ux_bind_begin`,
`som_ux_bind_end`, `som_ux_set_legend`, `som_ux_set_binding_policy`, `som_ux_binding_policy`; the options page `SOM_UX_OPT_BINDINGS` (opens the
main page) and the hit kinds KEYROW / KEYS_SCROLL / BIND_CANCEL / BIND_CLEAR / QE_KEY (enum values kept for ABI, never produced).  NEW:
`ux_glue_shoulder(which, &dispatch)` (L1 0, R1 1, L2 2, R2 3; 1 = dispatch to run), `ux_glue_shoulder_labels`, `ux_glue_slot_flash`,
`ux_glue_flash_entry`, `ux_glue_hero_name`, `som_ux_hero_name`, `som_ux_mp_bars`.  `QUICK_SET` is unchanged: persist all eight slots and their
policies; migrate old physical bindings by putting each bound entry in the slot of its button.  Long press now returns 0 after arming an entry.

Tests: `test_som_ui_draw.c` (1017 checks, ASan/UBSan clean; slot assignment, shoulder press, flash sequence, MP audit, name decoding),
`test_text_fit.py` (2925 draws per ROM, 0 problems; `--sweep` 30550), `test_device_path.py`, `test_area_names.py`.

## 12. Known defects and next steps

* Spells without a game-verified icon (Salamando, the sprite's Luna, Dryad, Lumina) use the closest cell of the same sheet.
  A later pass with a party that owns every spirit would confirm them.
* The ring's own icons are compressed in the ROM; the first extraction attempt (matching VRAM tiles to the frame) failed on
  translucent sprites and was replaced by the crop capture.
* Item effect text, config entry descriptions and element names are authored, not read from the game; healing amounts are not shown.
* Ring facts not shown because they are not decoded: weapon attack power and the armour entries (the EQUIP ARMOR page lists no item).
* Name entry keys on the Thor portrait are 27 vp (6.5 mm), below the 7 mm target; everything else on the Thor is at least 29 vp.
* Avatar and menu macros, sounds, haptics and the persistence of options and quick slots are host work (see the glue header);
  they are not implemented or tested here.
* Magno accented letters print as `?` in ROM names (only the digraph tiles `A ` = 0xE6 and `AN` = 0xF3 are decoded; the rest of its
  glyph table is unmapped).  They are never cut, only imperfect.
* Landing records, shop item list and prices, the other status bit meanings and the world palette alignment are open.
* Magno and Ancient Cave: natural Flammie flight, boss, world-map decode and the front end were not re-run.
* The PSN, ailment and warning icons are original 9x10 shapes made for this kit, not the game's.

## Requests from the device owner (2026-10-17, from the Android integration)

1. **Pressed state.** The Android glue now calls `som_ux_press(struct som_ux *ux, int x, int y, int w, int h, int down)` on touch-down (down = 1) and on release or cancel (down = 0; x, y are the original down position, or 0, 0 on cancel). It is declared `__attribute__((weak))` in `som_jni.c`, so it links today without an implementation. Please implement it in the renderer: while a control is held, draw it depressed and brightened in the SoM style (tabs, cards, entries, quick slots, OPT, buttons); the Android side already plays the click sound and a 14 ms haptic pulse at touch-down. The view re-renders within 33 ms and only uploads when the picture changed.
2. **Header text is too large** ('READY', 'RING', 'OPTIONS' badges): please shrink the header font and badge.
3. The bottom view is now rendered on a worker thread; `som_ux_*` functions are still called under the glue mutex, nothing changes for the renderer.

Note from the Android integration: `quick_run` for `SOM_UX_BK_SPIRIT` now also returns `SOM_UX_ACT_OPEN_MENU` (bind_kind SPIRIT, hero = the viewed caster) next to the UI-only change of tab/spirit, so the host opens the game's own spell list with `somd_open_menu(SOMD_MENU_SPIRIT)`. `test_som_ui_draw.c` line 390 was updated. Also `som_ui_zone.c` is now in the Android CMake list, `n3ds/Makefile` and `run_bind_tests.sh`. At the time of writing the UX tests show golden-hash and quick-editor page failures (lines 351-357) from the renderer work in progress, not from this change.
