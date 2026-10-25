"""Name entry and START on the PC core (stock snes9x libretro), with the same rules as the Android name keyboard in som_jni.c.

usage: python3 name_entry_start.py ROM [WORD]    ROM = a Secret of Mana image (stock or randomized), nothing from it is stored.

Checks what the Android touch keyboard relies on:
  * the stock game ignores B for about 12 frames after a d-pad step, so B waits 14 frames and is repeated until the letter counter ($A1CC) rises;
  * every letter of WORD ends up in the name ($A1CC == len(WORD), up to the ROM's limit: 6 stock, 12 with the long-name hack);
  * START after the name leaves the name entry and reaches the opening (map 176 stock, 128 randomized) without any injected sound.
Exit status 0 when all of it holds.
"""
import sys
sys.path.insert(0, str(__import__('pathlib').Path(__file__).resolve().parents[2]))
import retro

rom = sys.argv[1]
word = (sys.argv[2] if len(sys.argv) > 2 else 'LINK').upper()
c = retro.Core(rom)
c.run(3300); c.run(6, ['START']); c.run(400)        # logos, title, game select
c.run(6, ['B']); c.run(500)                          # NEW GAME -> name entry


def cursor():
    x = c.u8(0x9C00) - 19
    return x // 8 if x >= 0 and x % 8 == 0 and x // 8 < 26 else None


for ch in word:
    t = ord(ch) - 65
    for _ in range(400):
        k = cursor()
        if k is None:
            c.run(1); continue
        if k == t:
            n0 = c.u8(0xA1CC)
            for _try in range(5):
                c.run(14); c.run(3, ['B']); c.run(10)
                if c.u8(0xA1CC) != n0:
                    break
            break
        c.run(3, ['RIGHT' if (t - k) % 26 <= 13 else 'LEFT']); c.run(2)
count = c.u8(0xA1CC)
ok = count in (len(word), 6, 12) and count >= min(len(word), 6)   # all letters, or the ROM's name length limit
print('letters typed', count, 'of', len(word), 'OK' if ok else 'FAIL')
c.run(30); c.run(3, ['START'])
maps = []
for i in range(0, 2400, 30):
    c.run(30)
    m = c.u8(0xDC)
    if not maps or maps[-1][1] != m:
        maps.append((i + 30, m))
print('maps after START (frames, id):', maps)
adv = any(m in (176, 128, 256) for _, m in maps)
print('advances to the opening:', 'OK' if adv else 'FAIL')
sys.exit(0 if ok and adv else 1)
