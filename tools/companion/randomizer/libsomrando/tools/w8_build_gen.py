import re, subprocess, sys
sys.path.insert(0, '.')
import mkgen

HEADER = '''/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Boss rush mode (group w8): the machine-code generating hacks.  Derived from the C# sources named at each function (Author of every
 * upstream file: Moppleton), translated to C 2026-10-18.  Comments are those of the C#.
 */
#include <math.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p2_bossrush.h"
#include "sr_p2_bossrush_int.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define BYTES(...) (const uint8_t[]){__VA_ARGS__}, (int)sizeof((const uint8_t[]){__VA_ARGS__})

'''

CLASSES = [
 ("StartingDoor-skip",),
 ("hacks/common/procgen/DoorExpansion.cs", "sr_w8_hack_door_expansion", "door"),
 ("hacks/common/other/AnimatedPaletteSimplification.cs", "sr_w8_hack_animated_palette", "anim"),
 ("hacks/common/procgen/Layer3Changes.cs", "sr_w8_hack_layer3_changes", "l3"),
 ("hacks/common/procgen/TimerDialogue.cs", "sr_w8_hack_timer_dialogue", "td"),
 ("hacks/common/procgen/Statistics.cs", "sr_w8_hack_statistics", "st"),
 ("hacks/bossrush/InstantWeaponLevels.cs", "sr_w8_hack_instant_weapon_levels", "iwl"),
 ("hacks/bossrush/MpAbsorbLimiter.cs", "sr_w8_hack_mp_absorb_limiter", "mpa"),
 ("hacks/bossrush/StartingLevel.cs", "sr_w8_hack_starting_level", "sl"),
 ("hacks/common/procgen/MagicRope.cs", "sr_w8_hack_magic_rope", "mr"),
]
out = [HEADER]
for cl in CLASSES:
    if len(cl) == 1: continue
    path, cf, pre = cl
    out.append("/* upstream %s */\n" % path)
    out.append(mkgen.gen(path, cf, pre))
    out.append("\n")
open('gen_raw.c','w').write("\n".join(out))
