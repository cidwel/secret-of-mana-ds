# Companion button bindings (Android on the AYN Thor, Nintendo 3DS)

Date: 2026-10-17, version 2.  The binding logic is one C module shared by both apps:

| File | Role |
| --- | --- |
| `tools/companion/som_bind.[ch]` | engine: input ids, per-platform tables, mode table, binding store and text format, press/chord/short/long handling, capture, analog helpers |
| `tools/companion/som_hotkey.[ch]` | executor: resolves an action against the live game state and runs it through the direct layer (`somd_*`) |
| `tools/companion/som_bind_ux.h` | glue to the bottom-screen UX layer: bind entries, badges, capture events (header only) |
| `tools/companion/android/.../som_jni.c`, `MainActivity.java`, `Bindings.java`, `BindPanel.java`, `OptionsPanel.java` | Android host: key/axis input, persistence in the app prefs, editor |
| `tools/companion/n3ds/source/main.c` | 3DS host: libctru keys, persistence in `sdmc:/som/binds_<variant>.txt`, bottom-screen touch |
| `tools/companion/test_som_bind.c`, `test_som_bind_ux.c`, `run_bind_tests.sh` | tests: PC with ASan/UBSan and the same code as an ARM binary under `qemu-arm` |

## 1. Which SNES buttons the game reads (per mode)

How it was derived:

* **Field and ring: measured on the device.**  `Native.probe` (debug broadcast `--ei probe 90 --ei hold 12`) saves the emulator state, runs 90 frames
  with no input, then replays the same state 12 times holding one SNES button for 12 frames each, and compares the whole 128 KiB WRAM (ignoring the
  joypad words `$00-$02 $42-$47 $CC-$CE $E9-$EB` and the stack page).  The core is deterministic, so a replay with no input is byte identical
  (noise 0 in every run); any other difference means the game consumed the button.
* **Menu pages, name entry, controller layout: read from `src/rom`.**  The pressed-edge consumers of the native menu framework read the byte at
  `$A204,X` with masks `$0F` (D-pad), `$10` (Start), `$20` (Select), `$40` (Y), `$80` (B); the controller layout page reads `$A203,X & $C0` (A, X).  No
  consumer tests L or R.  The hotkey `$3030` (L+R+Select+Start on any pad) is consumed by the poll routine `$C0:93B9` and cold-boots the game.
* **Event, shop, transition, boot: not reached on the device**, so every button but L and R is treated as used there (conservative).

| Mode | B | Y | Select | Start | Up | Down | Left | Right | A | X | L | R | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Field (free play) | used | used | used | used | used | used | used | used | used | used | **unused** | **unused** | measured |
| Ring (weapon, item, magic rings) | unused | used | unused | unused | used | used | used | used | unused | used | **unused** | **unused** | measured |
| Native menu pages, name entry, layout page | used | used | used | used | used | used | used | used | layout page only | layout page only | **unused** | **unused** | `src/rom` |
| Event, shop, transition, boot | assumed used | assumed used | assumed used | assumed used | assumed used | assumed used | assumed used | assumed used | assumed used | assumed used | **unused** | **unused** | not reached |

(Measured detail, field: B 150-390 differing bytes, Y/X about 1700-2700 (they open rings), Select 80-420, Start 11-78, D-pad 66-445, A 75-340, L 0, R 0 apart from
stack bytes.  Ring: Y 336-447, X 100-447, D-pad 348-805, B/A/Select/Start/L/R 0.  Raw lines: logcat tag `somcompanion`, "probe ...".)

Consequences, as implemented (`som_bind_used_mask`, `som_bind_canonical`):

* **L and R are free in every mode.**  All other SNES buttons are used in free field play, which is where bindings act, so a physical input that drives
  one of them is a *game button*: binding it asks for a second press and it is only hidden from the game in free field play (`som_bind_swallow_ok`).
  In the ring, menus, events and shops it reaches the game, so those stay usable.
* The Thor's **L1/R1 drive SNES L/R**, so they are free.  **Everything the game never sees is free**: L2/R2 (buttons and trigger axes), C, Z, L3, R3, MODE,
  and the four directions of both sticks as buttons.
* On the 3DS: L, R, ZL, ZR, circle pad and C-stick directions are free; A B X Y D-pad Select Start are game buttons.

## 2. Inputs (data driven)

`enum som_in` in `som_bind.h` is the shared id space; `som_bind_platform_inputs`, `som_bind_label`, `som_bind_name` and `som_bind_game_bit` are the
tables.  The editor builds its list from them (Android: `Native.bindList(1)`, shown in the Binds panel with FREE / GAME BUTTON state and how many
bindings use each; 3DS: the BINDS page).

AYN Thor, from `getevent -lp` and `dumpsys input` on the device (`Odin Controller`, key layout `Vendor_2020_Product_0111.kl`): BTN_GAMEPAD (A), BTN_EAST (B),
BTN_C, BTN_NORTH (X), BTN_WEST (Y), BTN_Z, BTN_TL, BTN_TR, BTN_TL2, BTN_TR2, BTN_SELECT, BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR, the D-pad (hat and keys),
axes X/Y (left stick), Z/RZ (right stick), GAS/BRAKE (triggers).  The Thor exposes no paddles.  HOME, BACK, volume and app-switch belong to the system and are not offered.

| Input | Label (Thor) | SNES button it drives | Free? |
| --- | --- | --- | --- |
| A south, B east, X west, Y north | A B X Y | SNES B, A, Y, X | game button |
| Select, Start, D-pad | SEL STRT UP DN LT RT | same | game button |
| L1, R1 | L1 R1 | L, R | free |
| L2, R2 (button or trigger axis, 0.6 on / 0.4 off) | L2 R2 | none | free |
| C, Z, L3 (left stick click), R3, MODE | C Z L3 R3 MODE | none | free |
| Left stick up/down/left/right (0.55 on / 0.35 off) | LSU LSD LSL LSR | none | free |
| Right stick up/down/left/right | RSU RSD RSL RSR | none | free |

3DS: A B X Y keep their SNES positions, L R ZL ZR (labels L, R, ZL, ZR), circle pad `CPU CPD CPL CPR`, C-stick `CSU CSD CSL CSR` (New 3DS only: ZL, ZR and the
C-stick are absent on the old models and not offered there; `APT_CheckNew3DS`).  Touch gestures are not bindable.

## 3. Bindings

A binding is `key` or the chord `mod + key`, as a **short** or a **long** press (450 ms), bound to an action:

| Action | Fields | What it does |
| --- | --- | --- |
| CAST | hero (girl/sprite), spirit, spell 1-3, target policy | casts that spell with `somd_cast` |
| EQUIP | weapon type | equips that weapon on the pad hero |
| CYCLE_WEAPON | +1 / -1 | next / previous owned weapon of the pad hero |
| ITEM | item id, target policy | uses the first record holding that item |
| SELECT_SPIRIT | hero, spirit | focuses the bottom-screen spell list on that spirit |
| CYCLE_SPIRIT | +1 / -1 | next / previous spirit that has spells (same focus) |

Target policies (CAST, ITEM): ASK (target cards on the bottom screen; the 3DS UX layer shows them too), NEAREST ENEMY, ALL ENEMIES, LOWEST ALLY, WHOLE PARTY, SELF.
A policy that does not fit the spell class degrades sensibly.  Refusals toast `NO_TARGET`, `NO_MP`, `UNSAFE_STATE`, `CASTER_BUSY`, `NOT_OWNED`, `NO_ITEM`, `BUSY`.

Timing rules of the engine (`som_bind_update`, covered by `test_som_bind.c`):

* An input with a single short binding fires on **press**.  If the same input also has a long binding, or is the modifier of a chord, the short binding is
  deferred to **release** (and does not fire after a long press or after a chord used it as modifier).  A long binding fires when the input has been held 450 ms.
* A chord fires when the key is pressed while the modifier is held (either order works when both arrive in the same 3DS frame).  A chord can be short or long too.
* A swallowed input is hidden from the game from press to release (`out.swallow`), including a game button whose binding fired.
* While a capture runs every held input is hidden from the game.
* Stored per ROM variant (size + SNES header checksum bytes): Android prefs key `binds2_<variant>` (text, one binding per line), 3DS `sdmc:/som/binds_<variant>.txt`.
  Version 1 (JSON, key `binds_<variant>`) bindings are not migrated; the defaults are used.

Defaults: L (L1) previous weapon, R (R1) next weapon, ZL (L2) previous spirit, ZR (R2) next spirit (2 of them on an old 3DS).

## 4. Editor

* Long-press (0.5 s) a weapon, item or spell button on the bottom screen: the UX layer's bind screen ("PRESS A BUTTON", NOW: R, CLEAR KEY, CANCEL).  Then press a
  key; hold it 0.45 s for a **long** binding; hold one key and press another for a **chord**.  A game button asks for the same press twice ("... IS A GAME BUTTON: PRESS
  AGAIN").  The entry shows a badge (`R`, `ZL`, `R+X`, a trailing `.` = long press).  An entry has one trigger; binding it again moves it.
* Android: gear button, Options, "Button bindings...": the current bindings (tap a row to cycle its target policy, X deletes it), Reset defaults, Clear all, then the
  data-driven list of every input with FREE / GAME BUTTON and its binding count, then the per-mode table above.
* 3DS: BINDS (bottom-right corner of the bottom screen) opens the list of inputs; a tap closes it.  The binding editing itself is the same long-press flow.

## 5. Tests

* `tools/companion/run_bind_tests.sh`: `test_som_bind.c` (tables, store, text format, every timing rule, capture, analog helpers, 400 randomised input streams with invariants,
  the executor's pure parts) and `test_som_bind_ux.c` (capture to badge, rebinding, CLEAR KEY, confirmation, timeout, mode mapping, ROM variant key), built for the PC with
  ASan/UBSan and for ARM (`arm-linux-gnueabihf-gcc -static`) under `qemu-arm`.
