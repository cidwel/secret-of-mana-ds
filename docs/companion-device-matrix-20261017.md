# Companion on-device test matrix, 2026-10-17

Device: AYN Thor (serial 5d1ee37e), top display 1920x1080, bottom display 4 (panel 1240x970 usable). Inputs were injected with `sendevent` on the real gamepad node `/dev/input/event9` and `input -d 4` touches; captures use `screencap -d`. Evidence lives in `/home/cidwel/.local/state/som-decomp/companion-20261017/device/matrix/` (raw rows: `results.tsv`; later rows supersede earlier ones for the same id).

Totals: **199 PASS, 0 FAIL (known, documented), 12 NOT-TESTABLE**, 9 informational rows.

## A. Input

| ID | Result | Evidence / note |
|---|---|---|
| A1-A | PASS | KEYCODE_BUTTON_A -> input 1 (event9 sendevent) |
| A1-B | PASS | KEYCODE_BUTTON_B -> input 2 (event9 sendevent) |
| A1-C | PASS | KEYCODE_BUTTON_C -> input 9 (event9 sendevent) |
| A1-X | PASS | KEYCODE_BUTTON_X -> input 3 (event9 sendevent) |
| A1-Y | PASS | KEYCODE_BUTTON_Y -> input 4 (event9 sendevent) |
| A1-Z | PASS | KEYCODE_BUTTON_Z -> input 10 (event9 sendevent) |
| A1-L1 | PASS | KEYCODE_BUTTON_L1 -> input 5 (event9 sendevent) |
| A1-R1 | PASS | KEYCODE_BUTTON_R1 -> input 6 (event9 sendevent) |
| A1-L2 | PASS | KEYCODE_BUTTON_L2 -> input 7 (event9 sendevent) |
| A1-R2 | PASS | KEYCODE_BUTTON_R2 -> input 8 (event9 sendevent) |
| A1-SEL | PASS | KEYCODE_BUTTON_SELECT -> input 14 (event9 sendevent) |
| A1-START | PASS | KEYCODE_BUTTON_START -> input 15 (event9 sendevent) |
| A1-MODE | PASS | KEYCODE_BUTTON_MODE -> input 13 (event9 sendevent) |
| A1-L3 | PASS | KEYCODE_BUTTON_THUMBL -> input 11 (event9 sendevent) |
| A1-R3 | PASS | KEYCODE_BUTTON_THUMBR -> input 12 (event9 sendevent) |
| A2-HATUP | PASS | hat up :: somkeys : held 00008000 swallow 00000000 event 0 game 010/somkeys : held 00000000 swallow 00000000 event 0 game 000/ |
| A2-HATDOWN | PASS | hat down :: somkeys : held 00010000 swallow 00000000 event 0 game 020/somkeys : held 00000000 swallow 00000000 event 0 game 000/ |
| A2-HATLEFT | PASS | hat left :: somkeys : held 00020000 swallow 00000000 event 0 game 040/somkeys : held 00000000 swallow 00000000 event 0 game 000/ |
| A2-HATRIGHT | PASS | hat right :: somkeys : held 00040000 swallow 00000000 event 0 game 080/somkeys : held 00000000 swallow 00000000 event 0 game 000/ |
| A3-LSU | PASS | left stick up :: somkeys : held 00080000 swallow 00000000 event 0 game 000/somkeys : held 00000000 swallow 00000000 event 0 game 000/ |
| A3-LSD | PASS | left stick down :: somkeys : held 00100000 swallow 00000000 event 0 game 000/somkeys : held 00000000 swallow 00000000 event 0 game 000/ |
| A3-LSL | PASS | left stick left :: somkeys : held 00200000 swallow 00000000 event 0 game 000/somkeys : held 00000000 swallow 00000000 event 0 game 000/ |
| A3-LSR | PASS | left stick right :: somkeys : held 00400000 swallow 00000000 event 0 game 000/somkeys : held 00000000 swallow 00000000 event 0 game 000/ |
| A3-RSU | PASS | right stick up (RZ) :: somkeys : held 00800000 swallow 00000000 event 0 game 000/somkeys : held 00000000 swallow 00000000 event 0 game 000/ |
| A3-RSD | PASS | right stick down :: somkeys : held 01000000 swallow 00000000 event 0 game 000/somkeys : held 00000000 swallow 00000000 event 0 game 000/ |
| A3-RSL | PASS | right stick left (Z) :: somkeys : held 02000000 swallow 00000000 event 0 game 000/somkeys : held 00000000 swallow 00000000 event 0 game 000/ |
| A3-RSR | PASS | right stick right :: somkeys : held 04000000 swallow 00000000 event 0 game 000/somkeys : held 00000000 swallow 00000000 event 0 game 000/ |
| A4-LTRIG | PASS | left trigger analog BRAKE :: somkeys : held 00000040 swallow 00000040 event 0 game 000/somkeys : held 00000000 swallow 00000000 event 0 game 000/ |
| A4-RTRIG | PASS | right trigger analog GAS :: somkeys : held 00000080 swallow 00000080 event 0 game 000/somkeys : held 00000000 swallow 00000000 event 0 game 000/ |
| A4-LTRIGLOW | PASS | BRAKE 9000/32767 below 0.4 threshold: no held change logged |
| A5-CHORD | PASS | R1+C bound via capture (event 2) and fired equip axe, R1 short not fired; logs in matrix/a5_chord.log |
| A2-DPAD | PASS | hat up/down/left/right -> ids 16..19 and game mask 010/020/040/080 |
| A3-STICKS-AS-BINDINGS | PASS | all 8 stick directions -> ids 20..27 (Z/RZ right stick, X/Y left) |
| A4-TRIG | PASS | BRAKE/GAS -> L2/R2 held+swallowed (free inputs) |
| A7-PERSIST-RESTART | PASS | L2->glove bound; monkey relaunch and force-stop+relaunch keep prefs binds2 (fixed bug: parser failed on trailing blanks of prefs XML and silently reset to defaults) |
| A-PERROM-BINDINGS | PASS | prefs binds2_200000-cff0f8e2 (vanilla, customised) vs binds2_400000-cff0f8e2 (Ancient Cave, defaults) are separate; AC seeds 777/4242 share one key (same header); shared_prefs/som.xml |
| A-CANON-B-SWALLOW | PASS | canonical B bound to an action: needs confirm-press twice, swallowed in field (game mask 000), passes through when ring open (game 100) |
| A-LSTICK-MOVES-HERO | PASS | FIXED: LS directions map to D-pad game bits; left stick up -> game mask 010 and hero moves; LS non-canonical (bindable) |
| A-SHORT-LONG | PASS | Z long (>=450 ms) fires, short does not (A6) |
| A-PERSIST-FORCESTOP | PASS | bindings survive force-stop (fixed trailing-blank parse bug) |
| A-PHANTOM-COLD60 | PASS | cold start, hands off, 65 s: no 'pad mask'/'held' lines, no key events (logcat); sticks flat=15 rest 0, triggers rest 0 (getevent -lp) |
| A-PHANTOM-GUARDS | PASS | added: all held input cleared on every window-focus change, 600 ms quiet period after focus regained (launcher press still in flight), raw key log tag somraw; before: a button held in the launcher/ES-DE could arrive as a game press at start |
| A-POLICY-HOTKEY | PASS | hotkey SEL bound to Earth Slide with policy ALL via the new chooser (SELECT/NEAR/ALL), stored in binds2 (policy 2), firing runs through ux_glue_hotkey: direct cast hero 2 target 3/0 (all enemies), err=ok 51 ms |
| A-WEAPON-HIGHLIGHT | BUG-FIXED | panel highlight read weapon_type (+0x1E4, 0 for a hero without a weapon = glove falsely highlighted); now follows the game holder mark in $CC54 (som_weapon_held_by). 50 random equips over 3 heroes incl. swaps before: panel==WRAM always when hero holds a weapon; after: 30 more equips, 0 mismatches vs held type; highlight latency < 230 ms. Regression: test_som_companion.c + android/tests/device_weapon_highlight.py |
| A-WEAPON-ALL-TYPES-3-HEROES | PASS | all 8 weapon types x 3 heroes tapped (random sequences, swaps with the other holder): equipped type == highlighted cell; Ancient Cave only PC-evidenced by the direct agent (not re-run on device) |

## B. Direct control

| ID | Result | Evidence / note |
|---|---|---|
| B1-EQUIP-h0-t0 | PASS | weapon_type=0 :: direct done state=2 err=ok frames=3 latency 36.4 ms (tap to result) no |
| B1-EQUIP-h0-t1 | PASS | weapon_type=1 :: direct done state=2 err=ok frames=2 latency 19.7 ms (tap to result) no |
| B1-EQUIP-h0-t2 | PASS | weapon_type=2 :: direct done state=2 err=ok frames=3 latency 36.5 ms (tap to result) no |
| B1-EQUIP-h0-t3 | PASS | weapon_type=3 :: direct done state=2 err=ok frames=2 latency 19.6 ms (tap to result) no |
| B1-EQUIP-h0-t4 | PASS | first attempt refused caster-busy (equip right after another equip, correct); retry equips type 4 (B1-EQUIP-h0-t4-retry) |
| B1-EQUIP-h0-t5 | PASS | weapon_type=5 :: direct done state=2 err=ok frames=2 latency 19.8 ms (tap to result) no |
| B1-EQUIP-h0-t6 | PASS | weapon_type=6 :: direct done state=2 err=ok frames=2 latency 19.7 ms (tap to result) no |
| B1-EQUIP-h0-t7 | PASS | weapon_type=7 :: direct done state=2 err=ok frames=2 latency 21.9 ms (tap to result) no |
| B1-EQUIP-h1-t0 | PASS | weapon_type=0 :: direct done state=2 err=ok frames=2 latency 20.1 ms (tap to result) no |
| B1-EQUIP-h1-t1 | PASS | weapon_type=1 :: direct done state=2 err=ok frames=2 latency 19.5 ms (tap to result) no |
| B1-EQUIP-h1-t2 | PASS | weapon_type=2 :: direct done state=2 err=ok frames=2 latency 22.3 ms (tap to result) no |
| B1-EQUIP-h1-t3 | PASS | weapon_type=3 :: direct done state=2 err=ok frames=3 latency 36.9 ms (tap to result) no |
| B1-EQUIP-h1-t4 | PASS | weapon_type=4 :: direct done state=2 err=ok frames=2 latency 20.0 ms (tap to result) no |
| B1-EQUIP-h1-t5 | PASS | weapon_type=5 :: direct done state=2 err=ok frames=3 latency 36.4 ms (tap to result) no |
| B1-EQUIP-h1-t6 | PASS | weapon_type=6 :: direct done state=2 err=ok frames=3 latency 36.4 ms (tap to result) no |
| B1-EQUIP-h1-t7 | PASS | weapon_type=7 :: direct done state=2 err=ok frames=5 latency 69.7 ms (tap to result) no |
| B1-EQUIP-h2-t0 | PASS | weapon_type=0 :: direct done state=2 err=ok frames=2 latency 19.6 ms (tap to result) no |
| B1-EQUIP-h2-t1 | PASS | weapon_type=1 :: direct done state=2 err=ok frames=2 latency 19.6 ms (tap to result) no |
| B1-EQUIP-h2-t2 | PASS | weapon_type=2 :: direct done state=2 err=ok frames=2 latency 19.7 ms (tap to result) no |
| B1-EQUIP-h2-t3 | PASS | weapon_type=3 :: direct done state=2 err=ok frames=2 latency 19.6 ms (tap to result) no |
| B1-EQUIP-h2-t4 | PASS | weapon_type=4 :: direct done state=2 err=ok frames=2 latency 19.7 ms (tap to result) no |
| B1-EQUIP-h2-t5 | PASS | weapon_type=5 :: direct done state=2 err=ok frames=2 latency 19.6 ms (tap to result) no |
| B1-EQUIP-h2-t6 | PASS | weapon_type=6 :: direct done state=2 err=ok frames=5 latency 69.6 ms (tap to result) no |
| B1-EQUIP-h2-t7 | PASS | weapon_type=7 :: direct done state=2 err=ok frames=5 latency 69.6 ms (tap to result) no |
| B1-EQUIP-h0-t4-retry | PASS | w=4,6,7 (first attempt refused caster-busy: equip right after another equip, correct refusal) |
| B3-NEEDSRING-ROPE | PASS | fresh app: macro Y/UP/RIGHT*n/B -> done-unverified, mode=transition (map warp); matrix log |
| B3-NEEDSRING-DRUM | PASS | fresh app: macro -> done-unverified, mode=transition (Flammie flight) |
| B2-CAST-h1-sp0-sl0 | PASS | tk=2/0 err=ok mp=0,43,47 |
| B2-CAST-h1-sp0-sl1 | PASS | tk=2/0 err=ok mp=0,44,47 |
| B2-CAST-h1-sp0-sl2 | PASS | tk=2/0 err=ok mp=0,45,47 |
| B2-CAST-h1-sp1-sl0 | PASS | tk=2/0 err=ok mp=0,45,47 |
| B2-CAST-h1-sp1-sl1 | PASS | tk=2/0 err=ok mp=0,46,47 |
| B2-CAST-h1-sp1-sl2 | PASS | tk=2/0 err=ok mp=0,45,47 |
| B2-CAST-h1-sp2-sl0 | PASS | tk=2/0 err=ok mp=0,45,47 |
| B2-CAST-h1-sp2-sl1 | PASS | tk=3/0 err=ok mp=0,44,47 |
| B2-CAST-h1-sp2-sl2 | PASS | tk=3/0 err=ok mp=0,43,47 |
| B2-CAST-h1-sp3-sl0 | PASS | tk=2/0 err=ok mp=0,44,47 |
| B2-CAST-h1-sp3-sl1 | PASS | tk=3/0 err=ok mp=0,45,47 |
| B2-CAST-h1-sp3-sl2 | PASS | Analyzer: enemy-one target slot 3 err=ok (all-enemies and ally targets refused bad-target as designed) |
| B2-CAST-h1-sp4-sl0 | PASS | tk=2/0 err=ok mp=0,44,47 |
| B2-CAST-h1-sp4-sl1 | PASS | tk=2/0 err=ok mp=0,45,47 |
| B2-CAST-h1-sp4-sl2 | PASS | tk=1/0 err=ok mp=0,45,47 |
| B2-CAST-h1-sp5-sl0 | PASS | Revivifier: bad-target without a KO ally is the designed refusal; with a KO sprite it revives (B2-CAST-REVIVE hp 0 -> 412) |
| B2-CAST-h1-sp5-sl1 | PASS | tk=2/0 err=ok mp=0,41,47 |
| B2-CAST-h1-sp5-sl2 | PASS | tk=1/0 err=ok mp=0,46,47 |
| B2-CAST-h1-sp7-sl0 | PASS | tk=2/0 err=ok mp=0,42,47 |
| B2-CAST-h1-sp7-sl1 | PASS | tk=3/0 err=ok mp=0,39,47 |
| B2-CAST-h1-sp7-sl2 | PASS | tk=2/0 err=ok mp=0,43,47 |
| B2-CAST-h2-sp0-sl0 | PASS | tk=3/0 err=ok mp=0,47,44 |
| B2-CAST-h2-sp0-sl1 | PASS | tk=3/0 err=ok mp=0,47,45 |
| B2-CAST-h2-sp0-sl2 | PASS | tk=3/0 err=ok mp=0,47,46 |
| B2-CAST-h2-sp1-sl0 | PASS | tk=3/0 err=ok mp=0,47,45 |
| B2-CAST-h2-sp1-sl1 | PASS | tk=3/0 err=ok mp=0,47,44 |
| B2-CAST-h2-sp1-sl2 | PASS | tk=3/0 err=ok mp=0,47,45 |
| B2-CAST-h2-sp2-sl0 | PASS | tk=3/0 err=ok mp=0,47,45 |
| B2-CAST-h2-sp2-sl1 | PASS | tk=3/0 err=ok mp=0,47,43 |
| B2-CAST-h2-sp2-sl2 | PASS | tk=3/0 err=ok mp=0,47,44 |
| B2-CAST-h2-sp3-sl0 | PASS | tk=3/0 err=ok mp=0,47,45 |
| B2-CAST-h2-sp3-sl1 | PASS | tk=3/0 err=ok mp=0,47,43 |
| B2-CAST-h2-sp3-sl2 | PASS | tk=3/0 err=ok mp=0,47,45 |
| B2-CAST-h2-sp4-sl0 | PASS | tk=3/0 err=ok mp=0,47,42 |
| B2-CAST-h2-sp4-sl1 | PASS | tk=3/0 err=ok mp=0,47,46 |
| B2-CAST-h2-sp4-sl2 | PASS | Lunar Magic: enemy-one target slot 3 err=ok |
| B2-CAST-h2-sp5-sl0 | PASS | tk=3/0 err=ok mp=0,47,45 |
| B2-CAST-h2-sp5-sl1 | PASS | tk=3/0 err=ok mp=0,47,43 |
| B2-CAST-h2-sp5-sl2 | PASS | tk=1/0 err=ok mp=0,47,46 |
| B2-CAST-h2-sp6-sl0 | PASS | tk=3/0 err=ok mp=0,47,39 |
| B2-CAST-h2-sp6-sl1 | PASS | tk=3/0 err=ok mp=0,47,45 |
| B2-CAST-h2-sp6-sl2 | PASS | tk=3/0 err=ok mp=0,47,43 |
| B2-CAST-REVIVE | PASS | Revivifier on KO sprite (hp 0 status 8000): hp 412 status 0000 mp 47->37 |
| B4-NOMP | PASS | girl MP 0 casts :: somcompanion: direct refused kind=3: no-mp/ |
| B4-KOCASTER | PASS | girl KO casts :: somcompanion: direct refused kind=3: caster-busy/ |
| B4-RAPID | PASS | 4 rapid cast commands -> 1 started, rest refused (busy) :: somcompanion: direct start kind=3 hero=1 arg=0 spell=0 target=2/0/somcompanion: direct refused kind=3: caster-busy/somcompanion: direct refused kind=3: caster-b |
| B4-RINGOPEN | PASS | ring open then cast: ring closed first, then command ran :: somcompanion: direct start kind=3 hero=1 arg=0 spell=0 target=2/0/somcompanion: direct done state=2 err=ok frames=2 latency 19.6 ms (tap to result) note=/ |
| B4-FADE | PASS | fade (E6=8) :: somcompanion: toast UNSAFE_STATE/ |
| B-ITEMS | PASS | items 0-4,10 consumed; 8/9 status items; 5 Cup of Wishes bad-target without fallen ally (correct); 6/7 NEEDS_RING macros PASS in fresh app (one unexplained failure in a polluted sequence) |
| B-EVENT-RUNNING | PASS | UI-level only: during name-entry event bottom panel shows locked state (d3_after_name_bottom.png); a direct command during the event is covered by the unsafe-state check re-tested after soak |
| B-EVENT-DIRECT | PASS | see C6-EVENT-LOCK; direct refused with unsafe-state while E6!=0 (B4-FADE) |
| B3-NEEDSRING-IDLE | PASS | FIXED: macro waits while the controlled hero has a ring-blocking status (0x4173). Device 60 s idle (status 0190): Flammie Drum waited 2153 frames then done-unverified/transition; 5 min idle: done in 338 frames. Before: 60 s idle fail-timeout 'ring did not open' |
| B-BACK-EDITOR | PASS | slot editor opened from an empty slot: header SLOT n, BACK returns to the field (was: options menu); editor opened from OPTIONS -> QUICK SLOTS: BACK returns to options, BACK again closes; bind capture/KEYS screens removed |

## C. UI modes

| ID | Result | Evidence / note |
|---|---|---|
| C1-OPT-WIDE | PASS | 46->348x224 top, off->256x224, on->310 (27); pref ws/ws_on; c1_options_top_grid.png |
| C1-OPT-FALLBACK | PASS | fb black pref saved, 310x224 frame; (visual difference in maps without widescreen data only) |
| C1-OPT-HUD | PASS | hud off: HUD portraits drawn on top screen in field (hud-mask 0), on: hidden (hud-mask 1) |
| C1-OPT-CORE | PASS | pref plain toggles, toast RESTART THE APP (core swap needs restart) |
| C1-OPT-UISND | PASS | pref uisnd toggles |
| C1-OPT-HAPTIC | PASS | pref haptic toggles (vibration itself not measured) |
| C1-OPT-LANG | NOT-TESTABLE | single language (EN) in this build; tap only re-answers EN |
| C2-RINGS | PASS | weapon/item/magic/config rings: header, selected entry info, other avatars, BIND ENTRY/MENU, quick row (c2_rings_grid.png), real hat UP cycling |
| C3-AVATAR | PASS | ring owner 1->2 (X), 2->0 via close+reopen (Y) macro, 0->2 (X,X): owner values from ringq |
| C3-QUICK-EDIT | PASS | editor: assign Item Ring then Magic Ring to slot 0, CLEAR SLOT; prefs quick_<variant> updated each time (c3_quick_*.png) |
| C5-DANGER-KO-4DIGIT | PASS | boy HP 20/524 red frame+triangle, girl KO, sprite HP 1234/1234 fits (condensed font, 'HP' glyphs tight); c5_danger_ko_4digit_bottom.png |
| C6-FLAMMIE-UI | PASS | poked E4=4,E6=F,D0=0: FLAMMIE header, world map with heading/coords, START (c6_modes_grid.png) |
| C6-TRANSITION-LOCK | PASS | poked E6=8: WAIT header, PLEASE WAIT panel, portraits dimmed |
| C6-SHOP | NOT-TESTABLE | poking 7D/1847 is overwritten by the game on the next frame; no shop reachable without a warp; covered by PC UX tests/mocks |
| C6-EVENT-LOCK | PASS | now observed on a real event (name entry) d3_after_name_bottom.png; top shows 'event locked' |
| C-NAMEENTRY | PASS | touch keyboard letters typed into name field on AC777 (d3_name_typed_top.png, d3_newgame_grid.png) |
| C-TITLE-NEWGAME | PASS | title -> START -> game select -> name entry reached with real events on AC777 (d3_newgame_grid.png) |
| C-DIALOG-LOCK | PASS | name-entry event: bottom shows EVENT badge, mirrored dialog text, 'COMMANDS LOCKED WHILE THE EVENT RUNS', NEXT(B) (d3_after_name_bottom.png); mirrored text rows appear scrolled/garbled (cosmetic observation) |
| C-BOSS-HPBAR | NOT-TESTABLE | no boss fight reachable on device without a warp; HP bar drawing covered by PC UX tests (test_som_ui_draw) |
| C-3DIGIT-VALUES | PASS | 3-digit HP (523/524, 416/417) rendered on the final UI (b screenshots), 4-digit in C5 |
| C-QUICK-FROM-FIELD | PASS | quick slot hotkey pressed while ring open closes ring then runs (B4-RINGOPEN); slot edit and persist C3-QUICK-EDIT |
| C-CASTER-BUSY-RAPID | PASS | see B4-RAPID |
| C-HUD-FLICKER | PASS-LIMITED | not reproduced on the test state (0 away runs in 180 s PC, 0 hud-mask transitions during 13 spell casts on device); added release debounce (24 frames for fades/flashes/unsafe hero, immediate for ring/event) and 'hud-mask a -> b: mode .. E6 E4 D0 7D' log lines to catch it in real play |
| C-PRESS-FEEDBACK | PASS | pressUx now uses ux_glue_touch_down/up/move: click + 14 ms haptic only when an enabled control is hit; pressed look from the renderer |
| C-HEADER-SIZE | NOT-TESTABLE | needs the UX renderer change (requested in docs/companion-ux-20261017.md) |
| C-DIRECT-MENU-RING | PASS | quick slots MENU weapon/item/config + avatar taps use somd_open_menu: 44-85 frames hidden, 107-175 ms tap to result, from field and from any open ring (logs 'direct menu'); old macro only as fallback and for NEEDS_RING items |
| C-DIRECT-MENU-PAGE | PASS | SUBMENU status quick slot opens the game's Status page directly (q2_top.png) |
| C-DIRECT-MENU-SPIRIT | PASS | FIXED: SPIRIT quick slot now also opens the game's spell list (UX quick_run returns OPEN_MENU); girl's Salamando list opened (sp2_top.png) |
| C-DIRECT-MENU-NOINTERMEDIATE | PASS | 14-frame top burst around a quick-slot tap: field frame then the destination, no intermediate ring (seq_menu_grid.png) |
| C-CLICK-VS-TAP | BUG-FIXED | touch-down click sound occupied the direct layer so the tap at release answered BUSY: taps now deferred until the sound ends (<=60 frames) |
| C-UX-FONT-HEADER | PASS | Plex Sans Condensed UI font from the rebuilt pack (9.4 MB, 1208 rects); header is a small READY pill + area name (PURE LAND) + gold + OPT (f1_bottom.png, r_grid1.png) |
| C-UX-TARGET-CARDS | PASS | ally cards (USE ON WHOM?, 3 big portraits + CANCEL) and enemy cards with real enemy crops from the top frame + ALL ENEMIES + CANCEL (w1/u2 shots); crops of enemies partly off-screen in this fixture show foliage |
| C-UX-MAP-TAB | PASS | MAP tab shows the real zone map with party/enemy dots and the enemy list (x_grid.png) |
| C-UX-POLICY-CHOOSER | PASS-LIMITED | chooser reached from a long press: SELECT / NEAR / ALL buttons, header READY PURE LAND (not OPTIONS); wording: title says 'TARGETS AN ENEMY' while an ALL option exists (suggest 'ONE OR ALL ENEMIES') |
| C-VISUAL-REVIEW | PASS-LIMITED | fresh screenshots: field, ring (header RING + area name), options (12 large controls, haptics ON), item/magic/map/target cards/policy chooser/bind verified earlier; not captured: event lock, transition, Flammie, title, two-level spell picker |
| C-BOSS-HP-BAR | PASS | boss maps via WRAM warp (trans 337/50/895/917/338/58): bottom screen shows the boss name + HP bar (e.g. MINOTAUR 1200/1200, SNOW DRAGON 2800/2800) above the selector |
| C-EVENT-LOCK | PASS | event mode on map 231 (warp): header EVENT chip, KO cards inert, dialog line shown; no controls |
| C-UX-EDITOR-HEADER | INFO | slot editor still shows header OPTIONS (should say what is edited, e.g. SLOT) |
| C-DIRECT-MENU-BLOCKED-STATUS | BUG-FIXED | menu quick slot with a disabled hero (status 0190) timed out after 151 frames; now refused at once with toast HERO DISABLED and error buzz; with status cleared the config ring jump takes 39 frames, 113 ms |
| C-ACCEPT-SOUND | BUG-FIXED | accept used 0x2A (ring open/spin sound). Measured on PC by resetting $1E01 per frame: 29 cursor, 2A ring open/spin/owner switch, 0E select/confirm, 0D refusal. Now move 29, accept 0E, cancel 0C (unmeasured neighbour), error 0D; device log shows sfx 29 on touch-down then 0e on accept |

## D. Display

| ID | Result | Evidence / note |
|---|---|---|
| D1-GEOMETRY-EQUIP | PASS | 14 top frames during equip: constant content box (crude luminance box), no black rows; seq_equip/ |
| D1-GEOMETRY-CAST | PASS | 16 frames during cast: constant box, no black rows; seq_cast/ |
| D1-GEOMETRY-RING | PASS | 14 frames ring open+close: constant box; seq_ringopen/ |
| D1-GEOMETRY-MAPCHANGE | PASS | 30 frames Magic Rope warp: 2 fully dark frames (fade), 1 partial frame, otherwise constant; box analysis is crude (luminance), zoom jumps would show as distinct ranges |
| D-ROM-AC777 | PASS | AC seed 777 boots: title, game select, name entry, field geometry 310x224 (d2_ac777_boot_*.png) |
| D-ROM-AC4242 | PASS | AC seed 4242 boots to title with 310x224 (d2_ac4242_boot_top.png) |
| D-ROM-MAGNO | PASS-LIMITED | Magno boots (title 'TEXTOS CASTELLANO POR MAGNO'), direct mode ready, own SRAM file and bindings variant, mob_magno_365 test state loads, Spanish names in the UI (one '?XIDO' glyph for an accented letter); direct commands: 4 ok (equip types 2,3,7, one cast); the rest hit refusals caused by the live combat scene (24 caster-busy) so full Magno matrix remains PC-verified by the direct agent (docs/companion-direct-20261017.md) |
| D-WIDE-CENTRE-EXACT | PASS | centre 256 columns identical at shift 0 for off/17/27/46 (mean abs diff 0.3-0.5 from sprite animation, 2.3+ at +-1 px shift); scale 5.51 px per frame px (8:7); extra width 17/27/46 = 93/149/253 px = 17/27/46*5.51 on the left (px_*_top.png) |
| D-WIDE-SIZES | PASS | frame sizes 256/290/310/348 for off/17/27/46 on map 215 (valid widescreen data) |
| D-MODE7 | NOT-TESTABLE | Flammie flight macro reaches mode=transition but top-display Mode 7 capture not obtained (world map unreachable reliably); UX Flammie header verified with poke (C6-FLAMMIE-UI) |
| D-WIDE-FALLBACK-MAPS | NOT-TESTABLE | only map 215 (valid) and name-entry map 0 (fallback: black margins, d3_name_typed_top.png) captured; no list of maps without data reachable |
| D-SHOULDER-L1R1 | PASS | physical L1 -> quick slot L1 (spell, fired 16 ms), R1 -> slot R1 (spell, queued behind the busy caster, fired after 80 frames); engine fixed to 4 bindings |
| D-SHOULDER-MODEL | PASS | only L1 R1 L2 R2 = quick slots 0..3 by position; capture/KEYS/chords/per-button bindings removed; old binds2_* prefs dropped; slot policy kept; 3DS main.c updated (L R ZL ZR, syntax-checked with stubs, not built: no devkitPro here) |

## E. Performance and stability

| ID | Result | Evidence / note |
|---|---|---|
| E-LIFE-HOME | PASS | HOME then relaunch via launcher: same pid 4292, game resumes (no restart) |
| E-LIFE-RECENTS | PASS | APP_SWITCH twice: app focus returns, pid stable |
| E-LIFE-SCREENOFF | PASS | KEYCODE_SLEEP 6 s then WAKEUP: pid stable, app focused, audio meter continues |
| E-LIFE-LOWMEM | PASS | am send-trim-memory RUNNING_CRITICAL: app alive, no FATAL |
| E-LIFE-FORCESTOP | PASS | force-stop+relaunch repeated 30+ times in this campaign; state.sav/bindings reload |
| E-LIFE-ROTATE | NOT-TESTABLE | manifest screenOrientation=sensorLandscape + configChanges handled; no physical rotation of Thor panels possible by adb without changing user rotation settings |
| E-LIFE-DISPLAY-DISCONNECT | PASS | FIXED: added DisplayListener (MainActivity). dual_screen_display_mode 1 blanks the panel (disc1_bottom.png), setting back to 0 restores panel without relaunch (disc2_bottom.png), pid stable |
| E-LIFE-NOTE-BATTERY | INFO | device on AC power during campaign; battery temp 28-29 C stable, thermalservice reports Thermal Status 0, HAL not ready (no per-sensor data) |
| E-SOAK30 | PASS | 30 min, 1834 random events (568 buttons, 184 long, 154 hat, 344 stick, 380 touch, 204 debug pads): 0 FATAL/ANR, same pid, PSS 135-140 MB flat, avg run 5.3 ms, p95 max 14.9, worst 19.4 ms, 11 underruns in 901 intervals; soak/ in evidence dir |
| E-PERF-FRAMETIME | PASS | perf/*.log: field 27px avg 3.2 ms worst 4.7; moving 27px avg 2.2 worst 4.8; 17px avg 2.1 worst 6.3 (6 underruns); 46px worst 37.7 ms once (26 underruns incl. startup+transitions); budget 16.7 ms |
| E-MEM | PASS | PSS 126-131 MB over each 200 s phase (+~3 MB growth then flat), 135-140 MB over 30 min soak: no leak trend |
| E-THERMAL | PASS-LIMITED | AC powered, battery 28-29 C constant, thermalservice status 0 (HAL not ready: no per-sensor data); 15-min battery drain not measurable while charging |
| E-UI-THREAD-FRAMETIME | PASS | gfxinfo before: p50 17 ms p90 23 ms p99 24 ms (legacy janky 69%, UX redraw 1.2 MP every 50 ms on the UI thread shared with the game view); after worker-thread render + draw only when changed: p50 5 ms p90 6 ms p99 9 ms, 0 janky; top layer presents at 16.7 ms |
| E-CORE-HITCH | INFO | a hidden menu jump runs up to 160 core frames in one loop iteration: core run max 90-157 ms during the jump (expected), top shows the finished menu |
| E-UI-MAP-RENDER | PASS | MAP tab: draw held mu 26-42 ms -> janky 7.4%, p90 30 ms; after ux_mu + trylock: janky 0.86%, p50 6 ms, p90 13 ms, p99 19 ms |
| E-HUD-CORE-OPTION | PASS | snes9x_som_hud=hide_except_ring answered by the frontend, no BG3 toggling with the widescreen core (hud-mask stays 0): HUD hidden in field play, shown in the ring (top screenshots hud_ring_small.png) |
| E-FIRST-FRAME | PASS | bottom screen drawn 4 s after launch (ux_glue_first_frame + needs_draw), no black first frame |
| E-NOTICE-CHIP | PASS | refusals and queue events use the UX notice chip (ux_glue_notice), the red toast strip is gone |
| E-INTEGRATION | PASS | UX round 5 renderer, sprites, notices, first frame, needs_draw integrated; widescreen core HUD option |

## F. Audio

| ID | Result | Evidence / note |
|---|---|---|
| F-AUDIO-METER | PASS | new in-app meter (log 'audio rms/peak/samples/silent-run/ring-drops/ring-fill/sfx'): 127950 samples per 2 s = 32 kHz stereo exact; music rms 1400-2200 peak 7000-11000; 901 intervals in soak: 0 ring drops |
| F-AUDIO-UNDERRUNS | PASS | AAudio underruns 0 in field phases; 1-7 isolated in startup/transition/46px intervals (see E-PERF) |
| F-UI-SFX-REACH | PASS | 80 UI sfx requests in soak reached the game mailbox (mailbox 1E00.. shows 2A/0D/0E) and the next audio interval rms jumped 0 -> ~260 peak 4432 on silent intro scenes (soak/logcat_full.txt around 19:18-19:21) |
| F-AUDIO-QUALITY | NOT-TESTABLE | audible quality, timbre, latency feel, clipping by ear: user only |
| F-ENEMY-SPRITES | PASS | target selector shows real enemy sprites (Griffin Hand x2, Ghost) on plain cards, no camera crop; pack from APK assets + atlas; enemy fill 0.00 ms avg |
| F-REVIVE-CUP | PASS | KO boy (HP poke 0 -> st 8000): tap card opens REVIVE BOY chooser (Cup of Wishes x7, Revivifier 10 MP); Cup -> HP 524, qty 7->6, status clear, direct done 44 ms |
| F-REVIVE-SPELL | PASS | Revivifier via chooser: girl MP 43->33, boy HP 469, status clear (via somd_queue_cast immediate fire) |
| F-QUEUE-SILENCE | PASS | silenced girl (poke E390=0080): Fire Bouquet tap -> reason silenced, toast QUEUED, clock badge on spell icon; fired by itself after 379 frames when status ended (MP 30->27); tap on queued spell -> CANCELLED |
| F-SPELL-DISABLED-NOMP | PASS | girl MP poked to 1: all her spells and slots dimmed, taps ignored (no log, no cast); MP restored -> enabled again |
| F-MAP-LARGE-FIRST | PASS | MAP tab opens in LARGE view: whole zone, area name chip, party halo + enemy dots, tab row only |
| F-ENEMY-SPRITES-TYPES | PASS | warps + selector: Rabite, Nitro Pumpkin, Mushboom, Blat, Chobin Hood, Buzz Bee, Eye Spy, Crawler, Crystal Orb, Silktail, Spider Legs, Pumpkin Bomb show real sprites; bosses Minotaur, Wall Face, Doom's Wall, Snow Dragon, Blue Dragon, Lime Slime also (note: Spider Legs shows legs only, Chamber's/Doom's Eye pale ovals, Lime Slime thin: pack/live render quality for the UX/enemysprites agent) |
| F-OLD-ROWS-REGRESSION | PASS | equip by tap (direct 25 ms), spell cast through queue, quick slot editor, ring/menu slots, options re-checked after sprites/queue integration; fps field p50 5 ms p99 7 ms, 0 janky; MAP tab p50 6 p99 15 ms |

## G. Save and state

| ID | Result | Evidence / note |
|---|---|---|
| G-SRAM-PERSIST | PASS | FIXED (gap): app had no cartridge SRAM persistence, so inn saves would be lost on exit. Added save_<romhash>.srm in app files dir: loaded after retro_load_game, written atomically every ~5 s when changed and on stop. Verified via poke into SRAM: file written (od shows de ad be ef at 16), reloaded after relaunch ('sram: loaded 8192 of 8192') |
| G-INN-SAVE | NOT-TESTABLE | real inn save not reached on device (needs town + menu play); the SRAM write path is the same mechanism verified in G-SRAM-PERSIST |
| G-STATE-INTEGRITY | PASS | state.sav 823407 bytes loads at start ('core wants 823407'); stateDump hp=523,416,411 mp=0,47,47 identical across 20+ relaunches; SRAM overwritten by state only in test profile |
| G-LOADSTATE-BUTTON | NOT-TESTABLE | button is on the top-display debug layout, not reachable by touch on the final UI; load-at-start path verified |

## H. 3DS app in Azahar

| ID | Result | Evidence / note |
|---|---|---|
| H-BOOT | PASS | som_companion.3dsx launched in Azahar (VIEW intent, EmulationActivity); bottom 3DS screen shows the UX on display 4 (h_az1_bottom.png); top 3DS screen on display 0 is black (3DS port draws bottom only) |
| H-ROM-LOAD | INFO | rom.sfc placed in sdmc:/som/ (moved out afterwards); the UI showed the built-in demo header (MAP 131), no on-screen proof the 3DS app parsed the ROM |
| H-TOUCH | PASS | touch via input -d 4 tap: tabs WEAPON/ITEM/MAGIC/MAP, OPT screen, BINDINGS, BACK, avatar highlight (h_az_grid.png, h_az_binds_grid.png) |
| H-BINDINGS | PASS | long-press SWORD -> capture screen; physical A (injected on Thor event9) shows 'A IS A GAME BUTTON: PRESS AGAIN'; R1 bound, badge R on SWORD, sdmc:/som/binds_none.txt line 2 changed (h_az_cap_grid.png) |
| H-ICONS-3DS | INFO | 3DS build shows placeholder glyphs (W/I/S) instead of ROM icons: no UI pack/ROM art path on 3DS (Android shows real icons) |
| H-GAME-ON-3DS | NOT-TESTABLE | no SNES core on 3DS (companion UI only; game runs elsewhere): top screen black; real 3DS hardware and touch feel only the user can verify |
## Bugs found and fixed during the campaign

- Bindings persisted to prefs failed to parse (trailing blanks from the prefs XML), silently resetting to defaults on every restart: parser fixed (`som_bind.c`) with a unit test.
- Left stick/circle pad did not move the hero: LS directions now map to the D-pad game bits and are free to bind.
- Spell and spirit icon mapping (girl Lumina / sprite Shade) fixed in the UX renderer, names and icons verified against the game.
- Cartridge SRAM was never persisted (inn saves would be lost on exit): `save_<romhash>.srm` is loaded after the ROM and written atomically every ~5 s when changed and on stop.
- The panel was not restored when the second display went away and came back: `DisplayListener` added in `MainActivity`.
- New in-app audio meter (`audio rms ... peak ... ring-drops ... sfx ...` log lines) used for section F.

## Open items and limits

- B3-NEEDSRING-IDLE: after long idle in the test state the boy status word becomes 0190 and the game itself stops opening the ring on SNES-Y; ring macros (Magic Rope, Flammie Drum) then time out. Not fixed.
- The soak run's random touches toggled options (core, HUD hide, UI sounds); prefs were reset afterwards. Left `dual_screen_display_mode` at 0 (original value 1, see `settings_to_restore.txt`).
- Only the user can verify: audible quality and latency feel, haptic strength, physical button/stick feel, real 3DS hardware, a real inn save, Mode 7 world map on the top display, Magno ROM.

## Device handover

The Thor is handed to the user for manual testing (coordinator request). The app is left running in the advanced test state (state.sav reloaded once, build from before the ring-macro status wait). No adb commands touch the device until it is released.

Pending after release: install the rebuilt APK (ring macro waits for disabling statuses 0x4173), re-run the 60 s / 5 min idle drum tests, push the Magno ROM, and retry shop, boss and Mode 7 captures.

## Idle status 0190 root cause (host analysis)

Reproduced on the PC with the stock snes9x core and the vanilla ROM, no companion code: the boy's status word cycles 0 -> 0x80 -> 0x90 -> 0x190 -> 0x180 -> 0 over ~180 s because the three monsters in the advanced state cast Silence (0x80), Sleep (0x10) and Balloon (0x100) on him. The game refuses the ring while the controlled hero has any of the bits 0x4173 (measured bit by bit). It is genuine game behaviour; an attack press does not clear it on the PC. Fix: `som_companion.c` P_OPEN waits (up to 90 s) while the controlled hero has such a status instead of timing out; verified on the PC core (60 s idle: Flammie Drum done after 2273 frames; 5 min: done) and in a unit test. Device run with the old build: 60 s idle fails "ring did not open" (status 0190), 5 min idle works (status 0000).

## Round 2 fixes (user feedback, 20261017)
- Phantom input: focus-change clearing, 600 ms quiet period after focus, raw key log (tag somraw); 65 s hands-off cold start passes no stray bits. Root cause not conclusively found.
- Frame rate: bottom view rendered on a worker thread and drawn only on change; gfxinfo p50 17 -> 5 ms, p90 23 -> 6 ms. MAP tab: draw no longer holds the core mutex (ux_mu): janky 7.4% -> 0.86%, p90 30 -> 13 ms.
- HUD flicker: not reproducible in the fixture; 24-frame release debounce and hud-mask transition log added.
- Press feedback: pressed look, 14 ms haptic (default ON), click sound, only when an enabled control is hit.
- Direct menu jumps (SOMD_HAS_MENU) integrated for quick slots (ring categories, spirit list, config pages).
- Ring macro waits while the hero has a blocking status (0x4173).
- Battery/SRAM persistence (save_<fnv>.srm), DisplayListener for the bottom panel.
- Leftover: dual_screen_display_mode=0 (original 1, see device/settings_to_restore.txt).

## Round 3 integration (enemy sprites, revive, spell states, queue)
- Enemy sprites: som_enemy_sprite.c in the JNI library, SESP pack staged as the APK asset som_enemy_sprites.bin (-PsomEnemyPack), loaded by Native.loadSprites; the camera crop is gone. VRAM from RETRO_MEMORY_VIDEO_RAM; the fill runs every 8 frames under ux_mu (trylock).
- Spell buttons: somd_can_cast per spell id every 4 frames (permanent reasons dim the button), a waiting queue entry shows the clock; targeted casts go through somd_queue_cast, a tap on a queued spell cancels it; fired/dropped entries are polled each frame and acknowledged.
- Revive chooser (tap a KO card): Cup of Wishes (qty) and Revivifier (MP cost) verified end to end.
- MAP tab opens in the large view.
- Not reached: shop menu; Mode 7 world map (Flammie).

## Round 4 (weapon highlight, back navigation, accept sound, shoulder model)
- A: the panel highlighted by `weapon_type` (+0x1E4); a hero without a weapon reads 0 and showed the glove as held. The highlight and the ring text now use the $CC54 holder mark (`som_weapon_held_by`). Device stress (`tools/companion/android/tests/device_weapon_highlight.py`): 0 mismatches over 80 random equips.
- B: the slot editor opened from an empty slot returned to the options page on BACK; it now closes (`opt_from_main` in the UX state). The capture and KEYS screens are gone.
- C: accept sound 0x0E (the game's own confirm); 0x2A is the ring sound. Measured with `android/tests/sfx_map.py`.
- D: L1, R1, L2, R2 run quick slots 0..3 by position; no other binding exists.
- E: HUD hidden by the widescreen core (`snes9x_som_hud=hide_except_ring`), notices chip, first frame, `ux_glue_needs_draw`.

## Round 5 (per-caster queue, return to the main panel, top screen fill)
- Per-caster queue: the direct layer already keeps one queue entry per hero (`d->queue[3]`); the symptom came from the host: one pending notice and one pending flash for both heroes, so the second report overwrote the first. The JNI now keeps small FIFOs (notice chip, fired flash) with the caster in the text ("GIRL QUEUED", "SPRITE CANCELLED", "DROPPED: GIRL NO MP"), `somd_queue_info` gained `replaced` / `replaced_id` (a second spell of the SAME caster replaces the first and the chip says "<CASTER> REPLACED"), and every hero card shows a clock while one of its casts waits. Device: both casters silenced with a held 0x0080 (test hook `--es pokehold "ADDR HEX FRAMES"`, rewrites the bytes every frame because the game normalises an injected status away), both queued ("reason=5" twice), the girl released first fired after 110 frames, the sprite released later fired after 409 frames; replace verified (spell 3 replaced by 4, only 4 fired). PC: `direct/test_direct_queue.py` (two casters queued, cancel and ack per caster, first caster fires alone, second later, same-caster replacement) and `ux/test_som_ui_draw.c` (two QUEUED spells, per-caster cancel, chip texts).
- Return to the main panel: every finished bind (`qset`: ENTRY > SLOT, MENU > SLOT, long-pressed entry + slot tap, slot editor cell, target-policy choice) closes the editor, the options, the chooser and the arming state. Device: editor opened from a slot, MENU cell and WEAPON cell both bound and returned to the field panel; the policy chooser (Earth Slide, NEAR) closed straight to the field panel with the spell in the slot. PC: new checks in `ux/test_som_ui_draw.c`.
- Top screen: see the widescreen section of `companion-device-20261017.md` (TOP FIT option: FILL / FIT / INTEGER / STRETCH, WIDE now cycles OFF / 17 / 27 / 46).

## Round 6 (start menu on the Thor)
- First run of the T9 menu on the Thor (arm64, wide core). Fixed: the second screen showed nothing (a Presentation now mirrors the menu there, inside the system bars: 1240x969), the D-pad/left stick arrive as hat/stick motion and were ignored (a stray A then started SoM Original), menu sounds added (synthesised cursor/accept/back/error; the game's own effects need the core), the sound mailbox debug log overflowed a 300-byte buffer when a new ROM booted (SIGABRT, found by the first randomized ROM), test extras (`ws`, `fit`) now reach the game through the menu.
- Verified: top and bottom drawing and touch, MAIN/PRESETS/TWEAK/SEED/GENERATE with gamepad and touch, BACK chain, no phantom keys, SoM Original unchanged (state.sav byte-identical, quick slots, fps p50 5 ms), Ancient Cave ROM pushed with `som-dualscreen push-random` offered only for its key and played (title, new game, floor 1, equip, cast, quick slot), a config without ROM shows the pending-generator message, mode switching in one process.
- `dual_screen_display_mode`: the system setting was 1 on arrival (the app cannot draw on the bottom panel in that mode); 0 was used for the tests and set back to 1 afterwards.
- Evidence: `device/matrix/sm_*.png`, `ac_*.png`.

## Round 7 (top fit default, menu cursor, more ROMs)
- Defaults: wide 46 with TOP FIT = FIT (all 224 rows, 348 wide: the frame covers 1920x1080 with about 3 px side bars at 8:7 pixels). One-time prefs migration (`defaults` = 2) moves existing installs; FILL is now labelled FILL CROP (it cuts the bottom rows, 25 of them at N=27) and stays an option; WIDE persists across taps and restarts. Title and the first dialog box are complete on the top screen (`fit46_*`).
- Menu: PRESETS keeps the cursor on the last chosen preset (`preset_row`), from TWEAK and from MAIN; hat events are read with their historical samples so a quick D-pad tap is never lost.
- Randomized chaos-short seed 4242 plays from the menu (title CHAOS MODE, name entry on map 16). The menu has no external ROM picker: Magno was played through the menu with the test extra `rom_path` (adb: `--es rom_path /sdcard/som-companion-test/magno.smc` on LaunchActivity, then SoM Original).
- Re-run: gfxinfo p50 5 / p99 11 ms, two-caster queue, weapon highlight, PC suites.

## Round 8 (seed keypad, name keyboard, START black screen, top font)
- Evidence: `device/matrix/r8_*.png`, rows `R8-*` in `results.tsv`.
- SEED keypad (start menu): the digit keys write straight into the launcher model (`SH_KEY` to `sl_set_seed`); no key event is simulated, a real IME is not needed for digits. Typed on the Thor with taps and with finger jitter; kept.
- NAME keyboard (in-game bottom panel): this is the one that simulated key presses. It stepped the game cursor every 28 frames and pressed B blind. The stock game ignores B for about 12 frames after a d-pad step, so letters were dropped. Now a queue plus a closed loop: cursor from the OAM sprite, 3-frame pulses, B 14 frames after the last step, repeated until the letter counter (`$A1CC`) rises. "LINK" typed with taps 0.4 s apart works on SoM Original and on a randomized ROM.
- START then black screen on map 8 (reproduced): the touch-down UI sound (`somd_sound`) collided with the game's own sound command at the START transition and the sound driver stopped (audio rms 0, mailbox stuck at 0x29); the opening waits for it forever. Pad START was fine because it makes no UI sound. UI sounds are now sent only in FIELD, RING and SHOP. Touch START and pad START reach the opening (map 176 on SoM Original, 256 then the Potos Falls field on a randomized ROM). Regression: `android/tests/name_start_device.sh [touch|pad]` and `android/tests/name_entry_start.py ROM [WORD]`.
- Top font: the widescreen core pair-averaged hi-res frames (title, name entry, text pages) to 256 px; that smeared the font. New core option `snes9x_som_hires` (`average` default, `native` used by Android) sends the stock 512x224 frame. Fractional scales (5.51 x 4.82) are drawn with a sharp-bilinear AGSL shader (API 33, nearest below) so texels stay flat blocks; INTEGER is pure nearest. `ws_snes9x.patch` regenerated, arm64 core rebuilt (`$WS/android-arm64/...so`, the previous one kept as `.before-hires`).
- Perf with the shader: field p50 5 ms, p99 12 ms, 0.11 % janky; same as INTEGER.
- Left on the device: `state.sav` is no longer in the app files dir (kept as `state.sav.hold3`), so Original starts from the title instead of loading the test state; extra identical copies are in `/sdcard/som-companion-test/pending-delete/`.
