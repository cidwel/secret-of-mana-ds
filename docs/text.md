# Dialog text: where it is and how it is encoded

Tags as in `docs/rom-combat.md`. Tool: `tools/text_decode.py` (decoder model; runs the real text engine on the text blocks of the events and compares the glyphs it emits with the model; `show` prints one event locally). No text is stored in this repository.

## 1. Where the text is [V]
- There is no message table: **dialog text is inline in the event scripts** (`docs/cutscene-engine.md` section 8). Events 0-0x3FF are in bank `$C9`, 0x400-0x7FF in bank `$CA`, each through a word pointer table at the start of the bank (`tools/event_vm.py event_range`). A script byte >= 0x50 starts a text block; the block runs to the first byte < 0x50 that is not inside a credits segment.
- The names of spells, elements, weapons, armor, consumables, menu words and monsters are a separate block: events 0x800-0x94E, pointers at file `0xA0000 + 2 * (id - 0x400)`, zero-terminated strings of glyph codes (`tools/names.py`).

## 2. Encoding of a text block [V for the glyph stream: see 4]
The text engine fetches one byte at a time from `$1D01-$1D03` (`$C0:16DF-17C6`) and writes glyph codes to the line buffer `$7E:A1A4` (`$C0:17CE`):
| byte | meaning |
|---|---|
| 0x80-0xD2 | one glyph, the code itself |
| 0x60-0x7E | two glyphs: the word at `$C7:7299 + 2 * (byte - 0x60)`, low byte first (the pair table has 61 entries, 0x00-0x3C; bytes 0x60-0x7E use entries 0x00-0x1E, bytes 0xD3-0xFF use entries 0x10-0x3C; none of these bytes occurs in the US dialog, see below) |
| 0xD3-0xFF | two glyphs from the same table at index `byte - 0xC3` (0x10-0x3C) |
| 0x7F | end of the line: the fetch loop stops for this pass |
| 0x53-0x5C | commands (table `$C0:1A22`): 53 none; 54 / 55 / 56 read an index byte, take the word at `$CA:(base + 2 * index)` with base `$099E` / `$0864` / `$0800` (ROM pointer lists in bank `$CA`, handlers `$C0:1804` / `1811` / `181E` via `$C0:190A`) and continue the script there (checked 2026-10-24 against the ROM; they do not occur in normal dialog, only in the credits event 0x4FD); 57 n insert entry n of the 12-byte name list at `$A22F`; 58 clear the number list `$A1D4`; 59 n and 5C n set the cursor `$A1CE` (and `$A173`); 5A n does the same and also appends n to the number list `$A1D4` (handlers `$C0:1883` / `1893` / `189D`); 5B ends the window. 57, 59, 5A and 5C take one argument byte |
| 0x50-0x52, 0x5D-0x5F | window commands, consumed by the VM when it hands the block to the engine (`$C1:F27A`); they emit no glyph |
| other < 0x60 | not text: the end of the block, the byte is the next VM command |

Glyph codes (the same in the name block): 0x80 space, 0x81-0x9A a-z (lowercase), 0x9B-0xB4 A-Z (uppercase; the earlier text of this line had the two ranges swapped, fixed 2026-10-18 against `tools/godot/somedit/text.py` and the game screenshots), 0xB5-0xBE 0-9, 0xBF `.`, 0xC0 `,`, 0xC1 `/`, 0xC2 `'`, 0xC3 `[`, 0xC4 `]`, 0xC5 `:`, 0xC6 `-`, 0xC8 `!`, 0xCA `?`; 0xC7, 0xC9, 0xCB-0xD2 are other symbols (0xCB after a line break, 0xCF-0xD2 framing marks of place names) whose pictures were not looked at.

## 3. Not determined
- The glyph pictures of 0xC7, 0xC9, 0xCB-0xD2; the exact meaning of the window commands 0x50-0x52, 0x5D-0x5F and of the number formats of 5A.
- The text speed and the window opening timing.

## 4. Validation
`tools/text_decode.py ROM STATE 80` starts the first 80 events of bank `$CA` whose first command is text, runs the whole frame with the text engine (the A button is pressed whenever the VM waits for it) and compares, for every block, the glyphs the engine wrote at `$C0:17CE` from that block with the model: 44 events ran, the engine showed 95 text blocks, **95 of 95 match the model** (one event, 0x482, stops in the harness at a menu wait `$C0:6945`).
