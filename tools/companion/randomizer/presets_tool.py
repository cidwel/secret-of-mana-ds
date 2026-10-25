#!/usr/bin/env python3
"""Build and check tools/companion/randomizer/presets.json, the preset and option catalogue of the launcher (board item T9).

  presets_tool.py build            regenerate presets.json from the hand written tables below and the upstream C# sources
  presets_tool.py check            verify presets.json against the upstream sources and (when built) the generator itself

Every option key, enumeration value, default and numeric range comes from the upstream sources of Moppu/SecretOfManaRandomizer v1.49
(LGPL-2.1): config/settings/*Settings.cs.  Nothing is invented: `check` fails when a key is missing from the C# or from the output of
`somrando --list-options`, when a default differs, or when an enumeration value is not declared by the C# `setInitial` call.
Only labels, grouping, which options are tweakable and the preset values (taken from the GUI presets of the upstream
forms/MainForm_OpenWorldPresets.cs where they exist) are decisions of this repository.
Python 3 standard library only.
"""
import json
import os
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
STATE = pathlib.Path(os.environ.get('SOM_CLI_STATE', '/home/cidwel/.local/state/som-decomp/companion-cli-20261017'))
SRC = pathlib.Path(os.environ.get('SOM_RANDO_SRC', str(STATE / 'repo-moppu' / 'SecretOfManaRandomizer' / 'SoMRandomizer')))
OUT = HERE / 'presets.json'
DLL = STATE / 'build' / 'headless' / 'somrando.dll'

# ---------------------------------------------------------------- hand written tables (labels, grouping, presets)
MODES = {
    'ancientcave': dict(name='Ancient Cave', cls='AncientCaveSettings', difficulty=['casual', 'hard', 'reallyhard'],
                        desc='Fight through generated floors, one weapon at a time, to the final boss.'),
    'open': dict(name='Open World', cls='OpenWorldSettings', difficulty=None,
                 desc='The whole world is open; items, enemies and bosses are shuffled and the goal is to finish the game.'),
    'rando': dict(name='Vanilla Randomizer', cls='VanillaRandoSettings', difficulty=None,
                  desc='The original game in its original order with enemies, bosses, orbs, elements and weapons shuffled.'),
    'bossrush': dict(name='Boss Rush', cls='BossRushSettings', difficulty=['casual', 'hard', 'reallyhard'],
                     desc='Back to back boss fights.'),
    'chaos': dict(name='Chaos', cls='ChaosSettings', difficulty=['casual', 'hard', 'reallyhard'],
                  desc='Random floors with random enemies and a few bosses.'),
}
# key -> (label, group, advanced).  Anything of a mode not listed here is a bug of this table and `build` stops.
LABELS = {
    'acBoy': ('Boy', 'Party', False), 'acGirl': ('Girl', 'Party', False), 'acSprite': ('Sprite', 'Party', False),
    'acFilter': ('Profanity filter', 'Dialogue', False), 'acMusic': ('Random music', 'Audio', False),
    'acLength': ('Length', 'Run', False), 'BossFrequency': ('Boss frequency', 'Run', False),
    'acDialogue': ('Dialogue source', 'Dialogue', True), 'acBiomeTypes': ('Biomes', 'Run', True),
    'opWeapons': ('Randomize weapons', 'Shuffle', False), 'opMusic': ('Randomize music', 'Audio', False),
    'opShop': ('Randomize shops', 'Shuffle', False), 'opMapColors': ('Random map colours', 'Cosmetic', True),
    'opAutosave': ('Autosave slot', 'Quality of life', True), 'opChestsDontRun': ('Chests do not run', 'Quality of life', True),
    'opShowLevels': ('Show enemy levels', 'Quality of life', True), 'opMagicRopeRefactor': ('Magic rope everywhere', 'Quality of life', True),
    'opPauseInMenu': ('Timer pauses in menus', 'Quality of life', True), 'opXmasMaps': ('Christmas maps', 'Cosmetic', True),
    'opXmasItems': ('Christmas drops', 'Cosmetic', True), 'opGPEleRando': ('Randomize Grand Palace elements', 'Shuffle', True),
    'opPlatinumMode': ('Blacked out maps', 'Challenge', True), 'opMinLevel': ('Minimum enemy level by area', 'Enemies', True),
    'opMaxLevel': ('Maximum enemy level by area', 'Enemies', True), 'opAllowLockedItems': ('Allow unreachable items', 'Logic', True),
    'opDisableHints': ('Disable hints', 'Challenge', True), 'opAllWeaponOrbs': ('Start with all weapon orbs', 'Start', True),
    'opFastWhipPosts': ('Fast whip posts', 'Quality of life', True), 'opFlammieDrumInLogic': ('Flammie drum in logic', 'Logic', True),
    'opEnemies': ('Enemies', 'Enemies', False), 'opBosses': ('Bosses', 'Enemies', False),
    'opStatusAilments': ('Status ailments', 'Enemies', True), 'opStatGrowth': ('Enemy stat growth', 'Enemies', False),
    'opDifficulty': ('Difficulty', 'Enemies', False), 'opCharacters': ('Girl and Sprite', 'Party', False),
    'opStartChar': ('Starting character', 'Party', False), 'opManaBeastScale': ('Mana Beast stats', 'Enemies', True),
    'opComplexity': ('Complexity', 'Logic', True), 'opLogic': ('Logic', 'Logic', False), 'opGoal': ('Goal', 'Run', False),
    'opTraps': ('Chest traps', 'Chests', True), 'opChestFreq': ('Chest frequency', 'Chests', True),
    'opAiRando': ('Enemy AI', 'Enemies', True), 'opExp': ('Experience multiplier', 'Rewards', False),
    'opGold': ('Gold multiplier', 'Rewards', False), 'opGoldCheckMul': ('Gold check multiplier', 'Rewards', True),
    'opGoldDropMul': ('Gold chest multiplier', 'Rewards', True), 'opStartGold': ('Starting gold', 'Start', True),
    'opXmasGifts': ('Christmas gifts', 'Run', True), 'opStartLevel': ('Starting level', 'Start', True),
    'oopsAllThis': ('Oops all: enemy', 'Enemies', True), 'opStartWeapon': ('Starting weapon', 'Start', True),
    'opNumSeeds': ('Mana seeds required', 'Run', True), 'opMinSeeds': ('Mana seeds, minimum', 'Run', True),
    'opMaxSeeds': ('Mana seeds, maximum', 'Run', True), 'opNoFutureLevel': ('No Future enemy level', 'Enemies', True),
    'opBoyRole': ('Boy plays as', 'Party', True), 'opGirlRole': ('Girl plays as', 'Party', True),
    'opSpriteRole': ('Sprite plays as', 'Party', True),
    'rEnemies': ('Randomize enemies', 'Shuffle', False), 'rBosses': ('Randomize bosses', 'Shuffle', False),
    'rOrbs': ('Randomize weapon orbs', 'Shuffle', False), 'rElements': ('Randomize elements', 'Shuffle', False),
    'rWeapons': ('Randomize weapons', 'Shuffle', False), 'rMusic': ('Randomize music', 'Audio', False),
    'rAutosave': ('Autosave slot', 'Quality of life', True), 'rDialogue': ('Shorter dialogue', 'Quality of life', False),
    'rPreserve': ('Keep the first bosses', 'Shuffle', True), 'rExp': ('Experience', 'Rewards', False),
    'rGold': ('Gold', 'Rewards', False), 'rHoliday': ('Holiday theme', 'Cosmetic', True),
    'rDifficulty': ('Enemy strength %', 'Enemies', False), 'rStatusAilments': ('Status ailments', 'Enemies', True),
    'brBoy': ('Boy', 'Party', False), 'brGirl': ('Girl', 'Party', False), 'brSprite': ('Sprite', 'Party', False),
    'brLimitMpAbsorb': ('Limit MP stolen by Luna', 'Rules', False),
    'chBoy': ('Boy', 'Party', False), 'chGirl': ('Girl', 'Party', False), 'chSprite': ('Sprite', 'Party', False),
    'chHealSpellsFirst': ('Healing spirit first', 'Rules', False), 'chEasyEarlyFloors': ('Safer early floors', 'Rules', False),
    'chMapColors': ('Map colours', 'Cosmetic', True), 'chFloors': ('Floors', 'Run', False), 'chBosses': ('Bosses', 'Run', False),
}
# Shorter display text for long enumeration values (the generator's own text is kept in `full`).
SHORT_VALUES = {
    'opCharacters': {'startboth': 'Both at start', 'findbothL1': 'Find both at L1', 'findbothCL': 'Find both at cur. L',
                     'start1find1': 'Start 1, find other', 'start1only': 'Start 1, no other', 'find1L1': 'Find 1 at L1',
                     'find1CL': 'Find 1 at cur. L', 'none': 'No other chars'},
    'acLength': {'short': 'Short (8)', 'medium': 'Medium (16)', 'long': 'Long (24)'},
    'opStatGrowth': {'bosses': 'After bosses'},
}
# Mode options that must not be offered: driven by the difficulty selector instead.
DIFFICULTY_KEYS = {'acDifficulty', 'brDifficulty', 'chDifficulty'}
# Options never offered for a reason (no meaning without a GUI side channel).
SKIP = {'plandoSettings'}
NO_RANDOMIZE = {  # kept at their default by "randomize all": they would make a run unplayable or are not choices
    'opStartGold', 'opXmasGifts', 'opXmasMaps', 'opXmasItems', 'rHoliday', 'oopsAllThis', 'opNoFutureLevel', 'opStartLevel',
    'opMinSeeds', 'opMaxSeeds', 'opPlatinumMode', 'opDisableHints', 'opGoldCheckMul', 'opGoldDropMul',
    'opBoyRole', 'opGirlRole', 'opSpriteRole', 'opStartWeapon', 'opAllowLockedItems', 'acBiomeTypes', 'acFilter', 'acDialogue',
    'opMinLevel', 'opMaxLevel', 'opAllWeaponOrbs', 'opFlammieDrumInLogic', 'opComplexity',
}
# "Randomize all" draws from these pools where the upstream GUI "Mystery seed" presets define one (forms/MainForm_OpenWorldPresets.cs),
# otherwise uniformly from every value of the option.
RAND_POOLS = {
    'opStatGrowth': ['player', 'bosses'], 'opDifficulty': ['sortaeasy', 'normal', 'kindahard'], 'opExp': ['1', '2', '3'],
    'opGold': ['1', '2', '3'], 'opCharacters': ['findbothL1', 'findbothCL', 'start1find1', 'start1only', 'find1CL', 'none'],
    'opStatusAilments': ['location', 'type', 'easy'], 'opGoal': ['vshort', 'vlong', 'mtr'], 'opEnemies': ['swap', 'random'],
    'opBosses': ['swap', 'random'], 'opLogic': ['basic', 'restrictive'], 'opStartChar': ['boy', 'girl', 'sprite'],
    'opTraps': ['normal', 'none', 'many'], 'opChestFreq': ['low', 'normal', 'high', 'highest'], 'opNumSeeds': ['3', '4', '5', '6', '7'],
    'rDifficulty': ['60', '80', '100', '125', '150'],
}
REQUIRES = {'oopsAllThis': {'opEnemies': ['oops']}, 'opNoFutureLevel': {'opStatGrowth': ['nofuture']},
            'opNumSeeds': {'opGoal': ['mtr']}, 'opMinSeeds': {'opGoal': ['mtr'], 'opNumSeeds': ['random']},
            'opMaxSeeds': {'opGoal': ['mtr'], 'opNumSeeds': ['random']}, 'opXmasGifts': {'opGoal': ['gift']}}
# Constraints applied after any edit and by "randomize all": at least one true / non-empty, or a <= b.
CONSTRAINTS = {
    'ancientcave': [{'any': ['acBoy', 'acGirl', 'acSprite']}],
    'bossrush': [{'any': ['brBoy', 'brGirl', 'brSprite']}],
    'chaos': [{'any': ['chBoy', 'chGirl', 'chSprite']}],
    'open': [{'le': ['opMinSeeds', 'opMaxSeeds']}],
}
# Common (all modes) options: label, group.  Only the ones a player can reasonably choose; the cheat/debug ones are advanced and
# excluded from "randomize all".
COMMON = {
    'ovrchrgFix': ('Overcharge glitch fix', 'Fixes'), 'leavePlayersBehind': ('Scroll players off screen', 'Fixes'),
    'seReduction': ('Mute a few sound effects', 'Audio'), 'aggBosses': ('Aggressive bosses', 'Challenge'),
    'funSpeedup': ('Speedup spell is haste', 'Rules'), 'rainbowRabites': ('Rainbow Rabites', 'Cosmetic'),
    'longStatus': ('Longer status effects', 'Rules'), 'fasterChests': ('Faster chests', 'Quality of life'),
    'magicRebalance': ('Magic rebalance', 'Rules'), 'mpAtLevel': ('Level ups restore MP', 'Rules'),
    'fasterMagicLevel': ('Faster spell levels', 'Rules'), 'fasterWeaponLevel': ('Faster weapon levels', 'Rules'),
    'fasterMessages': ('Faster status messages', 'Quality of life'), 'moreItems': ('Carry up to 7 consumables', 'Rules'),
    'starterGear': ('Start with gear', 'Start'), 'noBsVampires': ('Hittable vampires', 'Fixes'),
    'orbFix': ('Orb reward softlock fix', 'Fixes'), 'bossDeathFix': ('Boss death softlock fix', 'Fixes'),
    'speciesDamage': ('Species damage bonus', 'Rules'), 'elemDamage': ('Elemental damage', 'Rules'),
    'fastTransition': ('Fast transitions', 'Quality of life'), 'accuratePercent': ('Accurate weapon percent', 'Rules'),
    'noStaminaRun': ('Running costs no stamina', 'Rules'), 'statusGlow': ('Status glow', 'Cosmetic'),
    'mode7Fix': ('Flammie edge fix', 'Fixes'), 'noGigasSplit': ('Gigas do not split', 'Rules'),
    'whipFix': ('Whip post fix', 'Fixes'), 'offscreenTarget': ('Target off screen', 'Quality of life'),
    'buyMultiple': ('Buy several consumables', 'Quality of life'), 'manaMagicFix': ('Fixed Mana magic charge', 'Rules'),
    'ohko': ('One hit KO', 'Challenge'), 'noSummonDespawn': ('Summons keep NPCs', 'Fixes'), 'noclip': ('Walk through walls', 'Challenge'),
    'confusionFix': ('Confusion fix', 'Fixes'), 'hittableBeaks': ('Disable bird beak shield', 'Challenge'),
    'mechRiderDeathFix': ('Mech Rider death fix', 'Fixes'), 'adjustEnemyPos': ('Move enemies from doorways', 'Fixes'),
    'attacksForever': ('Attacks cost no stamina', 'Challenge'), 'deathByPoison': ('Permanent poison', 'Challenge'),
    'percentPoison': ('Poison is a percentage', 'Challenge'), 'aggEnemies': ('Aggressive enemies', 'Challenge'),
    'obscureDamage': ('Hide damage numbers', 'Challenge'), 'obscureHp': ('Hide own HP', 'Challenge'),
    'obscureGold': ('Hide gold', 'Challenge'), 'defRefactor': ('Experimental defense formula', 'Challenge'),
    'muteMusic': ('Mute music', 'Audio'), 'showEvades': ('Show evades as zero', 'Quality of life'),
    'bossFixedZ': ('Bosses at Z 0 (debug)', 'Fixes'), 'bossElementRando': ('Randomize boss elements', 'Challenge'),
    'nameEntryFix': ('Longer name entry', 'Quality of life'), 'enemyInfiniteMp': ('Enemies have infinite MP', 'Challenge'),
    'trashWeapons': ('Include useless weapons', 'Rules'), 'showMinimap': ('Minimap on Flammie', 'Quality of life'),
    'cupAtZeroHp': ('Cup of wishes at 0 HP', 'Rules'), 'candyHealouts': ('Heal at 0 HP with consumables', 'Rules'),
    'magicRopeDeathFix': ('Magic rope invulnerability fix', 'Fixes'), 'raceMode': ('Race mode', 'Challenge'),
    'characterColors': ('Character colours', 'Cosmetic'), 'dmgCancelAllow': ('Damage cancels actions', 'Rules'),
}
COMMON_NO_RANDOMIZE_GROUPS = {'Challenge', 'Fixes', 'Start', 'Audio'}

PRESETS = [
    # Ancient Cave first.
    dict(id='ancientcave', name='Ancient Cave', mode='ancientcave', difficulty='casual', set={},
         desc='The default Ancient Cave: 16 floors, casual, a boss every few floors, random music.'),
    dict(id='ancientcave-short', name='Ancient Cave: short', mode='ancientcave', difficulty='casual',
         set={'acLength': 'short'}, desc='8 floors for a quick run.'),
    dict(id='ancientcave-marathon', name='Ancient Cave: marathon', mode='ancientcave', difficulty='hard',
         set={'acLength': 'long', 'BossFrequency': 'final'}, desc='24 floors, hard, only the final boss.'),
    dict(id='ancientcave-gauntlet', name='Ancient Cave: gauntlet', mode='ancientcave', difficulty='reallyhard',
         set={'acLength': 'medium', 'BossFrequency': 'every'}, desc='16 floors, really hard, a boss on every floor.'),
    dict(id='ancientcave-hero', name='Ancient Cave: boy only', mode='ancientcave', difficulty='casual',
         set={'acGirl': 'no', 'acSprite': 'no'}, desc='A solo run with the boy.'),
    # Open World.
    dict(id='open', name='Open World', mode='open', set={}, desc='The default Open World: short goal, swapped enemies, random bosses.'),
    dict(id='open-mtr', name='Open World: Mana Tree Revival', mode='open', set={'opGoal': 'mtr', 'opNumSeeds': '5'},
         desc='Collect 5 Mana seeds to revive the Mana tree.'),
    dict(id='open-long', name='Open World: vanilla long', mode='open', set={'opGoal': 'vlong', 'opDifficulty': 'kindahard'},
         desc='The full game goal with tougher enemies.'),
    dict(id='open-casual', name='Open World: casual', mode='open',
         set={'opDifficulty': 'easy', 'opExp': '3', 'opGold': '3', 'opStartChar': 'boy', 'opCharacters': 'startboth'},
         desc='Easier enemies, triple experience and gold, the whole party from the start.'),
    dict(id='open-owl', name='Open World: owls only', mode='open',
         set={'opGoal': 'vlong', 'opDifficulty': 'hard', 'opEnemies': 'oops', 'oopsAllThis': '21', 'opLogic': 'restrictive', 'confusionFix': 'no'},
         desc='The upstream "owl%" preset: every enemy is a Nemesis Owl.'),
    # Vanilla randomizer.
    dict(id='rando', name='Vanilla Randomizer', mode='rando', set={}, desc='The original game order, everything shuffled, double experience and gold.'),
    dict(id='rando-plus', name='Vanilla Randomizer: classic plus', mode='rando',
         set={'rBosses': 'no', 'rEnemies': 'no', 'rExp': 'normal', 'rGold': 'normal'},
         desc='Only orbs, elements and weapons are shuffled; enemies, bosses and rewards stay vanilla.'),
    dict(id='rando-hard', name='Vanilla Randomizer: hard', mode='rando',
         set={'rDifficulty': '150', 'rExp': 'normal', 'rGold': 'normal', 'rPreserve': 'no'}, desc='Stronger enemies and normal rewards.'),
    # Chaos and Boss Rush.
    dict(id='chaos', name='Chaos', mode='chaos', difficulty='casual', set={}, desc='The default Chaos mode.'),
    dict(id='chaos-short', name='Chaos: short', mode='chaos', difficulty='casual', set={'chFloors': 'few', 'chBosses': 'few'},
         desc='Fewer floors and fewer bosses.'),
    dict(id='bossrush', name='Boss Rush', mode='bossrush', difficulty='casual', set={}, desc='The default Boss Rush.'),
    dict(id='bossrush-hard', name='Boss Rush: hard', mode='bossrush', difficulty='hard', set={}, desc='Boss Rush on the hard preset.'),
]

BASIC_KEEP = 8   # at most this many non-advanced options per mode are shown before "More options" (documentation only)


# ---------------------------------------------------------------- C# parsing
def read(p):
    return (SRC / p).read_text(encoding='utf-8-sig')


def parse_consts(text):
    return dict(re.findall(r'public const string (PROPERTYNAME_\w+)\s*=\s*"([^"]*)"', text))


def split_args(s):
    out, depth, cur, q = [], 0, '', False
    for ch in s:
        if ch == '"':
            q = not q
        if not q and ch in '({[':
            depth += 1
        if not q and ch in ')}]':
            depth -= 1
        if not q and depth == 0 and ch == ',':
            out.append(cur.strip())
            cur = ''
        else:
            cur += ch
    out.append(cur.strip())
    return out


def elems(arg, vals):
    """Tokens of a `new string[] { ... }` initialiser: quoted strings or PROPERTYVALUE constants, in order."""
    body = arg[arg.index('{') + 1:arg.rindex('}')]
    out = []
    for tok in split_args(body):
        tok = tok.strip()
        if not tok:
            continue
        out.append(tok[1:-1] if tok.startswith('"') else vals.get(tok, tok))
    return out


def parse_settings(path):
    """Return ({key: spec}, consts, vals) for one settings class: bool, enum, num and text specs in declaration order; anything
    built in a loop is reported as type raw and handled by the caller."""
    text = read(path)
    consts = parse_consts(text)
    consts.update(COMMON_CONSTS)
    vals = dict(re.findall(r'public const string (PROPERTYVALUE_\w+)\s*=\s*"([^"]*)"', text))
    specs = {}
    for m in re.finditer(r'setInitial\((PROPERTYNAME_\w+),(.*?)\);[ \t]*(?://[ \t]*([^\n]*))?', text, re.S):
        key = consts.get(m.group(1))
        rest, comment = m.group(2).strip(), (m.group(3) or '').strip()
        if key is None:
            continue
        args = split_args(rest)
        if rest in ('true', 'false'):
            specs[key] = dict(type='bool', default='yes' if rest == 'true' else 'no', help=comment)
        elif len(args) == 4 and re.fullmatch(r'-?[\d.]+', args[0]):
            specs[key] = dict(type='num', default=fmt_num(float(args[0])), min=float(args[1]), max=float(args[2]),
                              decimals=int(args[3]), help=comment)
        elif len(args) == 1 and args[0].startswith('"'):
            specs[key] = dict(type='text', default=args[0][1:-1], help=comment)
        elif len(args) == 3 and args[0].startswith('new string[]') and args[1].startswith('new string[]'):
            v, d = elems(args[0], vals), elems(args[1], vals)
            if args[2].startswith('new string[]'):
                specs[key] = dict(type='raw', args=args, help=comment)
            else:
                dflt = args[2][1:-1] if args[2].startswith('"') else vals.get(args[2], args[2])
                specs[key] = dict(type='enum', values=[dict(v=a, l=b) for a, b in zip(v, d)], default=dflt, help=comment)
        else:
            specs[key] = dict(type='raw', args=args, help=comment)
    return specs, consts, vals


def fmt_num(d):
    return str(int(d)) if d == int(d) else repr(d)


COMMON_CONSTS = {}


def load_all():
    global COMMON_CONSTS
    COMMON_CONSTS = parse_consts(read('config/settings/CommonSettings.cs'))
    common, _, _ = parse_settings('config/settings/CommonSettings.cs')
    modes = {}
    for mode, m in MODES.items():
        modes[mode] = parse_settings('config/settings/%s.cs' % m['cls'])
    return common, modes


def build_open_extras(specs):
    text = read('config/settings/OpenWorldSettings.cs')
    enemies = re.findall(r'enemyTypeOptions\.Add\("([^"]*)"\)', text)
    weapons = re.findall(r'forceStartWeaponOptions\.Add\("([^"]*)"\)', text)
    n_enemy = int(re.search(r'for \(int i = 0; i <= (\d+); i\+\+\)\s*\{\s*oopsAllFlagsValues', text).group(1))
    assert len(enemies) == n_enemy + 2, (len(enemies), n_enemy)
    specs['oopsAllThis'] = dict(type='enum', default='21', help='enemy used by "Oops all"',
                                values=[dict(v='random', l=enemies[0])] + [dict(v=str(i), l=enemies[i + 1]) for i in range(n_enemy + 1)])
    specs['opStartWeapon'] = dict(type='enum', default='-1', help='weapon the first character starts with',
                                  values=[dict(v='-1', l=weapons[0])] + [dict(v=str(i), l=weapons[i + 1]) for i in range(8)])
    seeds = ['random'] + [str(i) for i in range(1, 9)]
    specs['opNumSeeds'] = dict(type='enum', default='8', help='Mana seeds needed for Mana Tree Revival',
                               values=[dict(v=v, l='Random' if v == 'random' else v) for v in seeds])
    for k, d in (('opMinSeeds', '1'), ('opMaxSeeds', '8')):
        specs[k] = dict(type='enum', default=d, help='range of random Mana seeds required', values=[dict(v=str(i), l=str(i)) for i in range(1, 9)])
    specs['opNoFutureLevel'] = dict(type='num', default='99', min=1.0, max=99.0, decimals=0, help='enemy level in No Future mode')
    roles = [('random', 'Random'), ('randomunique', 'Random (unique)'), ('OGboy', 'Boy'), ('OGgirl', 'Girl'), ('OGsprite', 'Sprite')]
    for k, d in (('opBoyRole', 'OGboy'), ('opGirlRole', 'OGgirl'), ('opSpriteRole', 'OGsprite')):
        specs[k] = dict(type='enum', default=d, help='class this character plays as', values=[dict(v=a, l=b) for a, b in roles])
    specs.pop('plandoSettings', None)


def option(key, spec, advanced_default=False):
    label, group, advanced = LABELS[key]
    o = dict(key=key, label=label, type=spec['type'], default=spec['default'], group=group, advanced=advanced, help=spec.get('help', ''))
    if spec['type'] in ('enum', 'multi'):
        o['values'] = [dict(v, l=SHORT_VALUES.get(key, {}).get(v['v'], v['l']), **({'full': v['l']} if v['v'] in SHORT_VALUES.get(key, {}) else {}))
                       for v in spec['values']]
    if spec['type'] == 'num':
        o.update(min=spec['min'], max=spec['max'], decimals=spec['decimals'])
        o['step'] = 1 if spec['decimals'] == 0 else 10 ** -spec['decimals']
        if key in ('opExp', 'opGold'):
            o['step'] = 0.5
        if key in ('opStartGold',):
            o['step'] = 100
        if key == 'opGoldCheckMul' or key == 'opGoldDropMul':
            o['step'] = 1
        for kk in ('min', 'max'):
            o[kk] = int(o[kk]) if o[kk] == int(o[kk]) else o[kk]
    o['randomize'] = key not in NO_RANDOMIZE
    if key in RAND_POOLS:
        o['rand_pool'] = RAND_POOLS[key]
    if key in REQUIRES:
        o['requires'] = REQUIRES[key]
    return o


def build():
    common, modes = load_all()
    cat = dict(schema=1, generator=dict(name='Moppu/SecretOfManaRandomizer', version='1.49', commit='1404b7b', licence='LGPL-2.1',
                                         url='https://github.com/Moppu/SecretOfManaRandomizer'),
               note='Generated by tools/companion/randomizer/presets_tool.py from the upstream config/settings/*.cs. Option keys are the '
                    'generator\'s own setting names (passed as --set key=value). Do not edit by hand.',
               seed=dict(kind='digits', max_len=8, note='the generator accepts any string; the launcher offers up to 8 digits, written without leading zeros'),
               config_schema='docs/companion-startmenu-20261018.md', modes={}, presets=[])
    cmn = []
    for key, (label, group) in COMMON.items():
        spec = common[key] if key in common else None
        if spec is None:
            raise SystemExit('common option %s not found in CommonSettings.cs' % key)
        if key == 'characterColors':
            spec = dict(type='enum', default='rando', help='colour palettes of the characters', values=[dict(v='none', l='No change'), dict(v='rando', l='Randomize')])
        if key == 'dmgCancelAllow':
            spec = dict(type='enum', default='all', help='what interrupts a charging action', values=[dict(v='all', l='All'), dict(v='consumables', l='Consumables'), dict(v='none', l='None')])
        o = dict(key=key, label=label, type=spec['type'], default=spec['default'], group=group, advanced=True, help=spec.get('help', ''))
        if spec['type'] == 'enum':
            o['values'] = spec['values']
        o['randomize'] = group not in COMMON_NO_RANDOMIZE_GROUPS and key not in ('raceMode', 'muteMusic', 'noclip')
        cmn.append(o)
    cat['common_options'] = cmn
    for mode, m in MODES.items():
        specs, consts, vals = modes[mode]
        if mode == 'open':
            build_open_extras(specs)
        opts = []
        for key, spec in specs.items():
            if key in DIFFICULTY_KEYS or key in SKIP:
                continue
            if key not in LABELS:
                raise SystemExit('no label for %s (%s)' % (key, mode))
            if spec['type'] == 'raw':
                spec = fix_raw(key, spec, vals)
            opts.append(option(key, spec))
        entry = dict(name=m['name'], description=m['desc'], options=opts, constraints=CONSTRAINTS.get(mode, []))
        if m['difficulty']:
            entry['difficulty'] = dict(values=m['difficulty'], default='casual', target='generator argument (--difficulty)',
                                       note='reads the upstream difficulty_*.properties preset; "custom" needs a per-field editor and is not offered')
        else:
            entry['difficulty'] = None
        cat['modes'][mode] = entry
    cat['presets'] = [dict(id=p['id'], name=p['name'], mode=p['mode'], description=p['desc'], difficulty=p.get('difficulty'), set=p['set'])
                      for p in PRESETS]
    OUT.write_text(json.dumps(cat, indent=1, ensure_ascii=False) + '\n')
    print('wrote %s: %d modes, %d presets, %d common options' % (OUT, len(cat['modes']), len(cat['presets']), len(cmn)))
    return cat


def fix_raw(key, spec, vals):
    if key == 'acBiomeTypes':
        text = read('config/settings/AncientCaveSettings.cs')
        m = re.search(r'setInitial\(PROPERTYNAME_BIOME_TYPES,(.*?)\);\s*\n\s*\}', text, re.S)
        arrays = re.findall(r'new string\[\]\s*\{(.*?)\}', m.group(1), re.S)
        v = [vals.get(x.strip(), x.strip()) for x in arrays[0].split(',') if x.strip()]
        d = re.findall(r'"([^"]*)"', arrays[1])
        return dict(type='multi', values=[dict(v=a, l=b) for a, b in zip(v, d)], default=','.join(v), help='terrain types used for floors')
    raise SystemExit('unhandled raw spec for %s: %r' % (key, spec['args']))


# ---------------------------------------------------------------- check
def check():
    cat = json.loads(OUT.read_text())
    errors = []
    if not SRC.is_dir():
        sys.exit('upstream sources not found at %s (set SOM_RANDO_SRC or run som-dualscreen setup-randomizer)' % SRC)
    fresh = None
    saved = OUT.read_text()
    build()
    fresh = OUT.read_text()
    OUT.write_text(saved)
    if fresh != saved:
        errors.append('presets.json differs from what the tables and the upstream sources produce: run presets_tool.py build')
    # presets refer to real keys and legal values
    for p in cat['presets']:
        mode = cat['modes'][p['mode']]
        keys = {o['key']: o for o in mode['options'] + cat['common_options']}
        for k, v in p['set'].items():
            o = keys.get(k)
            if not o:
                errors.append('preset %s: unknown option %s' % (p['id'], k))
            elif o['type'] in ('enum',) and v not in [x['v'] for x in o['values']]:
                errors.append('preset %s: %s=%s not among %s' % (p['id'], k, v, [x['v'] for x in o['values']]))
            elif o['type'] == 'bool' and v not in ('yes', 'no'):
                errors.append('preset %s: %s=%s is not yes/no' % (p['id'], k, v))
        d = p.get('difficulty')
        if d and (not mode['difficulty'] or d not in mode['difficulty']['values']):
            errors.append('preset %s: difficulty %s not offered by %s' % (p['id'], d, p['mode']))
    # the generator itself
    if DLL.exists():
        dn = os.environ.get('DOTNET') or 'dotnet'
        for mode, m in cat['modes'].items():
            r = subprocess.run([dn, str(DLL), '--mode', mode, '--list-options'], capture_output=True, text=True)
            listed = {}
            for ln in r.stdout.splitlines():
                scope, kv = ln.split(' ', 1)
                k, _, v = kv.partition('=')
                listed[k] = v
            for o in m['options'] + cat['common_options']:
                if o['key'] not in listed:
                    errors.append('%s: %s is not reported by --list-options' % (mode, o['key']))
                elif listed[o['key']] != o['default']:
                    errors.append('%s: %s default %r in the catalogue, %r in the generator' % (mode, o['key'], o['default'], listed[o['key']]))
            known = {o['key'] for o in m['options'] + cat['common_options']}
            missing = [k for k in listed if k not in known and k not in DIFFICULTY_KEYS | SKIP |
                       {'debugLoggingEnabled', 'testOnly', 'version', 'buildDate', 'mode', 'currentGenerateProgress', 'spoilerLog'}]
            if missing:
                errors.append('%s: generator options missing from the catalogue: %s' % (mode, missing))
        print('checked against the generator build %s' % DLL)
    else:
        print('generator build not found; checked against the C# sources only')
    for e in errors:
        print('FAIL ' + e)
    print('presets.json: %s' % ('OK' if not errors else '%d problem(s)' % len(errors)))
    return 1 if errors else 0


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'check'
    if cmd == 'build':
        build()
    elif cmd == 'check':
        sys.exit(check())
    else:
        sys.exit(__doc__)
