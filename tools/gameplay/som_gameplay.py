#!/usr/bin/env python3
"""som_gameplay: battle gameplay options for Secret of Mana as data patches on a COPY of a ROM.

  som_gameplay.py list
  som_gameplay.py plan  --options opts.json [--rom ROM]       print every byte change (and conflicts against ROM)
  som_gameplay.py apply --rom ROM --options opts.json --out OUT [--on-conflict error|skip] [--checksum auto|always|never]
  som_gameplay.py ips   --options opts.json --out FILE.ips    export the (ROM independent) record list as IPS
  som_gameplay.py verify --rom ROM --patched OUT              check that OUT differs from ROM only in planned records

The option file is JSON (see gameplay_options.json).  Every record carries the bytes it expects to find ("before"), so a
ROM that does not contain them (other region, or a randomizer hack on the same code) is reported, never silently
overwritten.  The input ROM is never modified and nothing is written inside the repository.  Python 3 standard library only.
Design and addresses: docs/gameplay-options-20261018.md
"""
import argparse
import json
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]

FREE_BASE = 0x1C0540          # file offset of DC:0540, 0xFF padding in vanilla, Magno and all randomizer modes
FREE_LEN = 0x3C0
SNES_BASE = 0xC00000 + FREE_BASE   # SNES address of FREE_BASE (HiROM: offset | 0xC00000)

CATALOGUE = {
    'weapon_recharge': {
        'values': ['vanilla', 'fast', 'faster', 'instant'], 'default': 'vanilla',
        'doc': 'Heroes only. Weapon recharge gauge E1ED loaded after a swing / interrupted charge. '
               'vanilla (100-Agi)/2+50 frames; fast (100-Agi)/2; faster (100-Agi)/8; instant 0. Monsters unchanged.'},
    'run_recharge': {
        'values': ['vanilla', 'fast', 'none'], 'default': 'vanilla',
        'doc': 'Gauge loaded when a run (A press) ends. vanilla 50-100 frames; fast (100-Agi)/2; none 0.'},
    'monster_flinch': {
        'values': ['vanilla', 'strong', 'heavy', 'never'], 'default': 'vanilla',
        'doc': 'Ordinary monsters only (bosses and heroes unchanged, lethal hits unchanged). strong: flinch only when '
               'hit by magic or a critical; heavy: also when damage > maxHP/4; never: no hit reaction.'},
    'charge_speed': {
        'values': ['vanilla', 'fast', 'faster', 'instant'], 'default': 'vanilla',
        'doc': 'Weapon charge build-up per stage: vanilla 90 frames, fast 45, faster 22, instant 1 (all stages in a few frames).'},
    'charge_frames_per_stage': {
        'values': 'null or integer 1..90', 'default': None,
        'doc': 'Overrides charge_speed with an exact frames-per-stage value (1..90).'},
    'charge_speed_up_bonus': {
        'values': [False, True], 'default': False,
        'doc': 'While the hero has the Speed Up buff (E1B0 bits 04+10) a stage takes half the frames. '
               'Do not combine with the randomizer Speedup hack.'},
    'hero_crit_bonus_girl': {
        'values': 'null or integer 0..120', 'default': None,
        'doc': 'Added to the critical chance (/256) of hero slot 1 (vanilla 10). Chance is a byte: keep base+bonus < 256.'},
    'hero_crit_bonus_others': {
        'values': 'null or integer 0..120', 'default': None,
        'doc': 'Added to the critical chance (/256) of hero slots 0 and 2 (vanilla 5).'},
    'hero_flinch': {
        'values': ['vanilla', 'light', 'never'], 'default': 'vanilla',
        'doc': 'Hero hit reaction (knock-back, charge interrupted by the reaction). light: no reaction unless damage > maxHP/4; '
               'never: no reaction (lethal hits unchanged). Shares the C0:4EBC site with monster_flinch.'},
    'crit_multiplier': {
        'values': [2, 3, 4], 'default': 2,
        'doc': 'Damage multiplier of a critical hit (vanilla 2). Damage is still capped at 999 per hit.'},
    'walk_speed': {
        'values': [2, 3, 4], 'default': 2,
        'doc': 'Hero walking speed in pixels per frame (vanilla 2, run is 3). Conflicts with the randomizer funSpeedup hack.'},
    'charge_bonus_item_id': {
        'values': 'null or integer 0..254', 'default': None,
        'doc': 'Equipped head/body/accessory id (E1E0-E1E2) that halves the acts per stage like Speed Up. Item ids differ between ROMs.'},
    'weapon_level_rate': {
        'values': [1, 2, 3], 'default': 1,
        'doc': 'Multiplier of the weapon-level progress per kill. Conflicts with the randomizer fasterWeaponLevel hook (default on there).'},
    'qol_chest_fast': {
        'values': [False, True], 'default': False,
        'doc': 'Treasure chests open without the lock-breaking shake: the three animation scripts end at once (idea: Zhade, Moppleton).'},
    'qol_unready_damage': {
        'values': [False, True], 'default': False,
        'doc': 'A swing made before the weapon gauge is full no longer loses half its damage on top of the gauge ratio (vanilla halves it twice); a ready swing is unchanged (idea: the SoM editor community fix, Moppleton).'},
    'qol_fast_transitions': {
        'values': [False, True], 'default': False,
        'doc': 'Map fades jump to the target brightness at once (door and exit transitions are shorter, may show brief graphic glitches) (idea: Moppleton).'},
    'qol_fast_fades': {
        'values': [False, True], 'default': False,
        'doc': 'Every screen fade runs twice as fast (one brightness step per half the frames) and keeps the fade look. Redundant with qol_fast_transitions.'},
    'qol_item_limit': {
        'values': [4, 7], 'default': 4,
        'doc': 'Maximum stack of one consumable: 4 (vanilla) or 7, the largest the inventory byte can hold (idea: Masterflow item limit patch, Moppleton).'},
    'qol_levelup_mp_restore': {
        'values': [False, True], 'default': False,
        'doc': 'A level-up also refills MP (vanilla refills HP only) (idea: Moppleton). A randomizer ROM that already has the same hook is left alone.'},
    'qol_dead_exp': {
        'values': [False, True], 'default': False,
        'doc': 'Fallen party members still receive experience from kills; they level up but stay at 0 HP (idea: AdmiralStiff). Works on top of the randomizer level-up hook.'},
    'qol_magic_level_rate': {
        'values': [1, 2, 3], 'default': 1,
        'doc': 'Multiplier of the spell-level progress per cast (vanilla 1). A randomizer ROM with the fasterMagicLevel hook (x4) is left alone (idea: Kethinov spell grinding hack, Moppleton).'},
    'qol_ring_speed': {
        'values': [1, 2], 'default': 1,
        'doc': 'Ring menu speed: 2 turns the ring twice as fast and opens and closes it in fewer frames; the rest positions are the same as in vanilla (idea: the ring code mapped in this project).'},
    'qol_shop_fill': {
        'values': [False, True], 'default': False,
        'doc': 'Buying a consumable in a shop fills the stack in one go: it keeps buying the same item while the stack has room and the money covers it (idea: Moppleton BuyMultipleItems).'},
    'qol_message_speed': {
        'values': [1, 2, 4], 'default': 1,
        'doc': 'Messages at the top of the screen (level-ups, items found, weapon and spell levels) stay 1/2 or 1/4 as long (idea: Moppleton FasterTopOfScreenDialogue).'},
    'qol_scroll_deadzone': {
        'values': ['vanilla', 'tight', 'tighter'], 'default': 'vanilla',
        'doc': 'Size of the window in which the hero can walk before the screen scrolls: vanilla x 56-199 y 72-175, tight x 80-175 y 80-159, tighter x 96-159 y 96-143 (idea: Binarynova).'},
    'fix_checksum': {
        'values': ['auto', 'always', 'never'], 'default': 'auto',
        'doc': 'auto: recompute the SNES header checksum only if the input had a valid one.'},
}


class Conflict(Exception):
    pass


class Rec:
    def __init__(self, off, before, after, opt, note):
        self.off, self.before, self.after, self.opt, self.note = off, bytes(before), bytes(after), opt, note
        assert len(self.before) == len(self.after)

    def as_json(self):
        return {'offset': '0x%06X' % self.off, 'snes': '%02X:%04X' % (0xC0 + (self.off >> 16), self.off & 0xFFFF),
                'before': self.before.hex(), 'after': self.after.hex(), 'option': self.opt, 'note': self.note}


def snes_long(off):
    return bytes([off & 0xFF, (off >> 8) & 0xFF, 0xC0 + ((off >> 16) & 0x3F)])


class Asm:
    """Tiny label-aware byte emitter for the few 65816 routines placed in the free area."""
    def __init__(self, base_off):
        self.base = base_off
        self.b = bytearray()
        self.labels = {}
        self.fix = []
        self.fix16 = []

    def here(self):
        return self.base + len(self.b)

    def label(self, n):
        self.labels[n] = self.here()

    def raw(self, *bs):
        self.b += bytes(bs)

    def br(self, op, name):
        self.b.append(op)
        self.fix.append((len(self.b), name))
        self.b.append(0)

    def jmp(self, name):
        self.b.append(0x4C)
        self.fix16.append((len(self.b), name))
        self.b += b'\0\0'

    def jml(self, target_off):
        self.b += b'\x5c' + snes_long(target_off)

    def jsl(self, target_off):
        self.b += b'\x22' + snes_long(target_off)

    def jsl_abs(self, snes_addr):
        self.b += bytes([0x22, snes_addr & 0xFF, (snes_addr >> 8) & 0xFF, snes_addr >> 16])

    def done(self):
        for pos, name in self.fix:
            rel = self.labels[name] - (self.base + pos + 1)
            if not -128 <= rel <= 127:
                raise ValueError('branch out of range to ' + name)
            self.b[pos] = rel & 0xFF
        for pos, name in self.fix16:       # JMP abs inside the bank: the HiROM address of an offset is its low 16 bits
            self.b[pos:pos + 2] = (self.labels[name] & 0xFFFF).to_bytes(2, 'little')
        return bytes(self.b)


# ---- vanilla bytes at the sites (verified on US v1.0, Magno and every randomizer mode unless noted) ----
SITES = {
    'recharge_attack': [0xF942, 0x1CADE, 0x2C248],     # 69 32 9D ED E1: ADC #$32 ; STA $E1ED,X  (CLC precedes)
    'recharge_run': [0xB788],
    'charge_cmp': 0xB3C6,                              # 1A C9 2D 90: INC A ; CMP #$2D ; BCC
}
RECHARGE_V = bytes.fromhex('6932 9DEDE1'.replace(' ', ''))
FLINCH_V = bytes.fromhex('C220BD90E1')
CHARGE_V = bytes.fromhex('1AC92D90')
PAT_FLINCH = 'C220BD90E189 78C5 E220 D055 A901 85AC A90D 85A4 BDFBE1 8981'.replace(' ', '')
PAT_CRIT = '22????C0 A685 A483 C220 0689 E220 60'.replace(' ', '')
PAT_CRITBONUS = 'B996E1 85A4 BD90E1 8910 F002 06A4 B9FBE1 1016 A582 C901 D009 A5A4 18 690A 85A4 8007 A5A4 18 6905 85A4'.replace(' ', '')
PAT_ZERO = 'E220 22????C0 BDFBE1 1001 60'.replace(' ', '')
PAT_WLEVEL = 'A90938F99CE185A6'
PAT_HALVE = 'C220 4689 64CC 64D8 E220 64CE'.replace(' ', '')
PAT_LEVELUP = 'BD84E1 9D82E1 E220 A580 0940 8580'.replace(' ', '')
PAT_EXPGATE = 'A683 BD00E0 C901 D007 BC82E1 F002 8007'.replace(' ', '')
PAT_LVTAIL = 'E220 A580 0940 8580'.replace(' ', '')                       # level-up loop: SEP #$20 ; LDA $80 ; ORA #$40 ; STA $80
PAT_LV_RANDO = '22?????? EAEA E220 A580 0940 8580'.replace(' ', '')       # the same place after the randomizer mpAtLevel hook
PAT_SHOP = '20????D0??20????20????A2????4C'                                      # buy shop: JSR insert ; BNE fail ; JSR pay ; JSR refresh ; LDX #xxxx
PAT_MSG = 'E220 A960 9900FE C8 E69F E6A1'.replace(' ', '')                # message record builder: LDA #$60 ; STA $FE00,Y
PAT_RING_PHASE = ('C91000 902A 18 69F6FF 801F C92400 B022 18 690A00 8014 C9FE00 B014 18 690A00 8009 C92400 900C 18 69F6FF').replace(' ', '')
PAT_RING_STEP = 'A91200 8D2418 A94808 8D3218'.replace(' ', '')            # LDA #$0012 ; STA $1824 (angle step of a ring turn)
PAT_RING_SUB = 'B508 38 E91200 2900FC F021'.replace(' ', '')
PAT_RING_ADD = 'B508 18 691200 C90004 9010'.replace(' ', '')
RANDO_MP_ROUTINE = bytes.fromhex('BD84E19D82E1E220BD87E19D86E1C2206B')    # randomizer mpAtLevel: HP and MP refill
RANDO_MAGIC_ROUTINE = bytes.fromhex('A90938E59D0A0A85AC6B')               # randomizer fasterMagicLevel: spell progress x4
WALK_SITES = [(0xB71A, 0x02), (0xB720, 0x82), (0xB728, 0x02), (0xB72E, 0x82)]
RET_CHARGE_STORE = 0xB3DF
RET_CHARGE_UP = 0xB3CB


def locate(body, pat):
    """Offsets of a hex pattern with '??' wildcards inside bank C0 (first 64 KiB)."""
    toks = [pat[i:i + 2] for i in range(0, len(pat), 2)]
    first = next(i for i, t in enumerate(toks) if t != '??')
    lead = bytes.fromhex(toks[first])
    out = []
    i = body.find(lead, first, 0x10000)
    while i != -1:
        o = i - first
        if o + len(toks) <= 0x10000 and all(t == '??' or body[o + k] == int(t, 16) for k, t in enumerate(toks)):
            out.append(o)
        i = body.find(lead, i + 1, 0x10000)
    return out


class Sites(dict):
    """Resolved site offsets; reading a site that could not be located raises Conflict only when an option needs it."""
    def __getitem__(self, k):
        v = dict.__getitem__(self, k)
        if isinstance(v, Exception):
            raise v
        return v


def resolve(body):
    """Site offsets for this ROM. Vanilla, every randomizer mode and Magno differ only by a pure relocation of the
    combat block (Magno: +0x17 from C0:4EBC on, +2 before), so the relocated sites are found by instruction signature."""
    if body is None:
        return Sites(flinch=0x4EBC, crit=0x5372, crit_target=0x5990, crit_girl=0x5355, crit_other=0x535F,
                     zero=0x5176, zero_target=0x4E6B, wlevel=0x4358,
                     levelup=0x44D3, expgate=0x446B, halve=0x5383, lvtail=0x44DB, msg=0x5CF4,
                     shop=0x7AF2, shop_old=bytes.fromhex('D01E200D7D'), shop_fail=0x7B12,
                     ring_phase=0x6C1D, ring_step=0x6C7B, ring_sub=0x6CEA, ring_add=0x6CFB,
                     rando_mp=False, rando_magic=False)
    r = Sites()

    def one(pat, what):
        f = locate(body, pat)
        if len(f) != 1:
            return Conflict('%s not found exactly once in this ROM (%d matches): a hack rewrote it or the ROM is not SoM US' % (what, len(f)))
        return f[0]
    f = one(PAT_FLINCH, 'hit-reaction routine (C0:4EBC signature)')
    if not isinstance(f, Exception) and body[f + 0x61] != 0x60:
        f = Conflict('hit-reaction routine has an unexpected length')
    r['flinch'] = f
    c = one(PAT_CRIT, 'critical notice site')
    r['crit'] = c
    r['crit_target'] = c if isinstance(c, Exception) else body[c + 1] | body[c + 2] << 8
    b = one(PAT_CRITBONUS, 'critical chance routine')
    r['crit_girl'] = b if isinstance(b, Exception) else b + 29
    r['crit_other'] = b if isinstance(b, Exception) else b + 38
    z = one(PAT_ZERO, 'zero-damage reaction site')
    r['zero'] = z if isinstance(z, Exception) else z + 2
    r['zero_target'] = z if isinstance(z, Exception) else body[z + 3] | body[z + 4] << 8
    hv = locate(body, PAT_HALVE)
    r['halve'] = hv[0] + 2 if len(hv) == 1 else 0x5383
    lv = locate(body, PAT_LEVELUP)
    r['levelup'] = lv[0] if len(lv) == 1 else 0x44D3     # a hack rewrote it: the vanilla offset then reports the conflict
    eg = locate(body, PAT_EXPGATE)
    r['expgate'] = eg[0] + 12 if len(eg) == 1 else 0x446B
    w = locate(body, PAT_WLEVEL)
    r['wlevel'] = w[0] if len(w) == 1 else None      # None when the randomizer rewrote it: conflict reported at apply
    lt = locate(body, PAT_LVTAIL)
    r['lvtail'] = lt[0] + 2 if len(lt) == 1 else 0x44DB
    mg = locate(body, PAT_MSG)
    r['msg'] = mg[0] + 3 if len(mg) == 1 else 0x5CF4
    sh = locate(body, PAT_SHOP)
    if len(sh) == 1:
        r['shop'] = sh[0] + 3
        r['shop_old'] = bytes(body[sh[0] + 3:sh[0] + 8])
        r['shop_fail'] = sh[0] + 5 + body[sh[0] + 4] - (256 if body[sh[0] + 4] > 127 else 0)
    else:
        r['shop'] = r['shop_old'] = r['shop_fail'] = Conflict('buy-shop purchase site not found exactly once (%d matches)' % len(sh))
    for key, pat, van in (('ring_phase', PAT_RING_PHASE, 0x6C1D), ('ring_step', PAT_RING_STEP, 0x6C7B),
                          ('ring_sub', PAT_RING_SUB, 0x6CEA), ('ring_add', PAT_RING_ADD, 0x6CFB)):
        f = locate(body, pat)
        r[key] = f[0] if len(f) == 1 else van       # start of the signature; the plan adds the operand offsets
    # a randomizer ROM may already carry hooks with the same behaviour: recognised by the routine they jump to
    r['rando_mp'] = False
    lr = locate(body, PAT_LV_RANDO)
    if len(lr) == 1:
        t = body[lr[0] + 1] | body[lr[0] + 2] << 8 | (body[lr[0] + 3] & 0x3F) << 16
        r['rando_mp'] = bytes(body[t:t + len(RANDO_MP_ROUTINE)]) == RANDO_MP_ROUTINE
    r['rando_magic'] = False
    if bytes(body[0x104DEC:0x104DEC + 1]) == b'\x22' and bytes(body[0x104DF0:0x104DF3]) == b'\xEA\xEA\xEA':
        t = body[0x104DED] | body[0x104DEE] << 8 | (body[0x104DEF] & 0x3F) << 16
        r['rando_magic'] = bytes(body[t:t + len(RANDO_MAGIC_ROUTINE)]) == RANDO_MAGIC_ROUTINE
    return r


def recharge_routine(a, mode):
    """A = (100-Agi)>>1 on entry; X = actor offset; heroes (X<0x600) by mode, everything else vanilla."""
    a.raw(0xE0, 0x00, 0x06)        # CPX #$0600
    a.br(0xB0, 'van')              # BCS vanilla
    if mode == 'instant' or mode == 'none':
        a.raw(0xA9, 0x00)          # LDA #$00
    elif mode == 'faster':
        a.raw(0x4A, 0x4A)          # LSR LSR
    elif mode == 'fast':
        pass
    else:
        raise ValueError(mode)
    a.raw(0x9D, 0xED, 0xE1, 0x6B)  # STA $E1ED,X ; RTL
    a.label('van')
    a.raw(0x18, 0x69, 0x32, 0x9D, 0xED, 0xE1, 0x6B)   # CLC (CPX set it) ; ADC #$32 ; STA $E1ED,X ; RTL


def flinch_routine(a, mon, hero, done, cont):
    """Reaction gate in front of C0:4EBC. mon: vanilla/strong/heavy/never for ordinary monsters, hero: vanilla/light/never."""
    a.raw(0xE2, 0x20)              # SEP #$20
    a.raw(0xBD, 0xFB, 0xE1)        # LDA $E1FB,X
    a.br(0x30, 'hero')             # BMI hero
    a.raw(0x29, 0xC1)              # AND #$C1   boss / other
    a.br(0xD0, 'pass')
    if mon == 'vanilla':
        a.br(0x80, 'pass')
    else:
        a.raw(0xC2, 0x20)          # REP #$20
        a.raw(0xBD, 0x82, 0xE1)    # LDA $E182,X   lethal hit keeps the vanilla path
        a.br(0xF0, 'pass')
        a.raw(0xE2, 0x20)          # SEP #$20
        if mon != 'never':
            a.raw(0xBD, 0x6B, 0xE1)    # LDA $E16B,X  critical mark
            a.br(0xD0, 'strong')
            a.raw(0xBD, 0xF0, 0xE1)    # LDA $E1F0,X  bit 7: last damage was not a hero weapon hit (magic)
            a.br(0x30, 'strong')
            if mon == 'heavy':
                a.raw(0xC2, 0x20)      # REP #$20
                a.raw(0xBD, 0x84, 0xE1, 0x4A, 0x4A)   # LDA $E184,X ; LSR ; LSR
                a.raw(0xC5, 0x89)      # CMP $89
                a.br(0x90, 'strong16')  # BCC  maxHP/4 < damage
                a.raw(0xE2, 0x20)
        a.raw(0x9E, 0x6B, 0xE1)    # STZ $E16B,X
        a.jml(done)                # weak hit: no reaction, no stun
        a.label('strong16')
        a.raw(0xE2, 0x20)
        a.label('strong')
        a.raw(0x9E, 0x6B, 0xE1)    # STZ $E16B,X
    a.label('pass')
    a.raw(0xC2, 0x20, 0xBD, 0x90, 0xE1)   # REP #$20 ; LDA $E190,X  (displaced)
    a.jml(cont)
    a.label('hero')
    if hero == 'vanilla':
        a.br(0x80, 'pass')
    else:
        a.raw(0xC2, 0x20)          # REP #$20
        a.raw(0xBD, 0x82, 0xE1)    # LDA $E182,X
        a.br(0xF0, 'pass')         # lethal: vanilla
        if hero == 'light':
            a.raw(0xBD, 0x84, 0xE1, 0x4A, 0x4A)   # LDA $E184,X ; LSR ; LSR
            a.raw(0xC5, 0x89)      # CMP $89
            a.br(0x90, 'pass')     # BCC: big hit keeps the reaction
        a.raw(0xE2, 0x20)
        a.jml(done)


def zero_routine(a, target):
    """Zero damage after defence: drop the critical mark of a monster defender, then the displaced JSL."""
    a.raw(0x48)                    # PHA
    a.raw(0xBD, 0xFB, 0xE1)        # LDA $E1FB,X
    a.raw(0x30, 0x05)              # BMI +5
    a.raw(0xA9, 0x00)              # LDA #$00
    a.raw(0x9D, 0x6B, 0xE1)        # STA $E16B,X
    a.raw(0x68)                    # PLA
    a.jsl(target)
    a.raw(0x6B)


def critmul_routine(a, mult):
    a.raw(0xC2, 0x20)              # REP #$20
    if mult == 3:
        a.raw(0xA5, 0x89, 0x0A, 0x18, 0x65, 0x89, 0x85, 0x89)   # LDA $89 ; ASL ; CLC ; ADC $89 ; STA $89
    elif mult == 4:
        a.raw(0x06, 0x89, 0x06, 0x89)
    else:
        raise ValueError(mult)
    a.raw(0x6B)


def wlevel_routine(a, mult):
    a.raw(0xA9, 0x09, 0x38, 0xF9, 0x9C, 0xE1)   # LDA #$09 ; SEC ; SBC $E19C,Y
    a.raw(0x85, 0xA6)              # STA $A6
    if mult == 2:
        a.raw(0x0A)                # ASL
    elif mult == 3:
        a.raw(0x0A, 0x18, 0x65, 0xA6)   # ASL ; CLC ; ADC $A6
    a.raw(0x85, 0xA6, 0x6B)        # STA $A6 ; RTL


def crit_routine(a, jsl_target):
    a.raw(0x48)                    # PHA
    a.raw(0xBD, 0xFB, 0xE1)        # LDA $E1FB,X
    a.raw(0x30, 0x09)              # BMI +9  (hero defender: leave the record alone)
    a.raw(0x29, 0x40)              # AND #$40
    a.raw(0xD0, 0x05)              # BNE +5  (boss defender: leave the record alone, bosses are exempt anyway)
    a.raw(0xA9, 0x01)              # LDA #$01
    a.raw(0x9D, 0x6B, 0xE1)        # STA $E16B,X
    a.raw(0x68)                    # PLA
    a.jsl(jsl_target)              # displaced JSL $C05990
    a.raw(0x6B)


def charge_routine(a, n, nh, buff, item):
    """n = acts per stage (vanilla 45); nh = acts per stage when hasted (nh < n) or None.
    Hasted = Speed Up buff (buff) and/or an equipped id (item) in E1E0-E1E2."""
    a.raw(0x1A)                    # INC A
    if nh is not None:
        a.raw(0xC9, nh)            # CMP #nh
        a.br(0x90, 'store')        # BCC store
        a.raw(0xC9, n)             # CMP #n
        a.br(0xB0, 'up')           # BCS up
        a.raw(0x48)                # PHA
        if buff:
            a.raw(0xBF, 0xB0, 0xE1, 0x7E)   # LDA $7EE1B0,X
            a.raw(0x29, 0x14, 0xC9, 0x14)   # AND #$14 ; CMP #$14  (Speed Up gives both)
            a.br(0xF0, 'yes')
        if item is not None:
            for off in (0xE0, 0xE1, 0xE2):
                a.raw(0xBF, off, 0xE1, 0x7E)   # LDA $7EE1E0..E2,X
                a.raw(0xC9, item)
                a.br(0xF0, 'yes')
        a.br(0x80, 'no')
        a.label('yes')
        a.raw(0x68)                # PLA
        a.br(0x80, 'up')
        a.label('no')
        a.raw(0x68)                # PLA
        a.label('store')
        a.jml(RET_CHARGE_STORE)
        a.label('up')
        a.jml(RET_CHARGE_UP)
    else:
        a.raw(0xC9, n)             # CMP #n
        a.br(0x90, 'store')
        a.jml(RET_CHARGE_UP)
        a.label('store')
        a.jml(RET_CHARGE_STORE)


# ---- quality-of-life group (T11) ----
QOL_CHEST = [(0x115FB8, 0x83), (0x11ED01, 0x8F), (0x11EC84, 0x8F)]    # animation script bytes -> FF (end of animation)
QOL_FADE_STEP = (0x8B71, bytes.fromhex('A5E585E4'))                    # LDA $E5 ; STA $E4 (fade step timer reload)
QOL_FADE_TARGET = 0x8B76                                               # LDA $E6 operand -> $E7 (jump to the target brightness)
QOL_ITEM_CMP = (0x6559, bytes.fromhex('C99F'))                         # CMP #$9F before BCS: the ADC carry alone limits to 7
QOL_MAGIC = (0x104DEC, bytes.fromhex('A90938E59D85AC'))                # LDA #9 ; SEC ; SBC $9D ; STA $AC
QOL_LEVELUP = (0x44D3, bytes.fromhex('BD84E19D82E1E220'))              # HP refill at level-up
QOL_LV_TAIL = bytes.fromhex('A5800940')                                  # LDA $80 ; ORA #$40 at the end of a level-up step
QOL_EXP_GATE = (0x446B, bytes.fromhex('F002'))                         # BEQ over a hero with 0 HP in the experience share loop
QOL_DEADZONE = {'tight': (0x50, 0xB0, 0x50, 0xA0), 'tighter': (0x60, 0xA0, 0x60, 0x90)}   # x lo, x hi, y lo, y hi
QOL_DEADZONE_SITES = ((0xD903, 0x38), (0xD90A, 0xC8), (0xD952, 0x48), (0xD959, 0xB0))


def shopfill_routine(a, fail_off):
    """Hook at C0:7AF2 (`BNE fail ; JSR $7D0D`) of the buy shop, entered with Z = result of the first inventory insert.
    Pays for the first item (the replaced JSR $7D0D), then, when the item is a consumable (ids $BA..$C5), keeps buying the
    same item while the stack has room and the money covers the price at $1822."""
    a.raw(0xC2, 0x30)                  # REP #$30
    a.br(0xF0, 'ok')                   # BEQ ok                  insert succeeded
    a.raw(0x68)                        # PLA                     the insert failed: drop the JSL return address ...
    a.raw(0xE2, 0x20, 0x68, 0xC2, 0x30)   # SEP #$20 ; PLA ; REP #$30
    a.jml(fail_off)                    # ... and take the original BNE target
    a.label('ok')
    a.raw(0xF4, 0x08, 0x00)            # PEA #$0008              safety counter
    a.label('pay')
    a.raw(0xC2, 0x30)                  # REP #$30
    a.raw(0xAF, 0x6A, 0xCC, 0x7E)      # LDA $7ECC6A             money, low word
    a.raw(0x38, 0xED, 0x22, 0x18)      # SEC ; SBC $1822         price
    a.raw(0x8F, 0x6A, 0xCC, 0x7E)      # STA $7ECC6A
    a.br(0xB0, 'paid')                 # BCS paid
    a.raw(0xE2, 0x20)                  # SEP #$20
    a.raw(0xAF, 0x6C, 0xCC, 0x7E)      # LDA $7ECC6C             borrow from the high byte
    a.raw(0x3A)                        # DEC A
    a.raw(0x8F, 0x6C, 0xCC, 0x7E)      # STA $7ECC6C
    a.raw(0xC2, 0x30)                  # REP #$30
    a.label('paid')
    a.raw(0xA3, 0x01, 0x3A, 0x83, 0x01)   # LDA $01,S ; DEC A ; STA $01,S
    a.br(0xD0, 'more')                 # BNE more
    a.jmp('done')
    a.label('more')
    a.raw(0xAD, 0x0A, 0x18)            # LDA $180A               cursor row
    a.raw(0x29, 0xFF, 0x00)            # AND #$00FF
    a.raw(0x18, 0x6D, 0x3C, 0x18)      # CLC ; ADC $183C         + list offset
    a.raw(0xAA)                        # TAX
    a.raw(0xBF, 0x00, 0x00, 0xD8)      # LDA $D80000,X           item id of the selected row
    a.raw(0x29, 0xFF, 0x00)            # AND #$00FF
    a.raw(0xC9, 0xBA, 0x00)            # CMP #$00BA              consumables are $BA..$C5
    a.br(0xB0, 'lo')                   # BCS lo
    a.jmp('done')
    a.label('lo')
    a.raw(0xC9, 0xC6, 0x00)            # CMP #$00C6
    a.br(0x90, 'hi')                   # BCC hi
    a.jmp('done')
    a.label('hi')
    a.raw(0x38, 0xE9, 0x7A, 0x00)      # SEC ; SBC #$007A        inventory id for the insert routine ($40 + k)
    a.raw(0x48)                        # PHA
    a.raw(0xAF, 0x6A, 0xCC, 0x7E)      # LDA $7ECC6A
    a.raw(0x38, 0xED, 0x22, 0x18)      # SEC ; SBC $1822
    a.br(0xB0, 'afford')               # BCS afford
    a.raw(0xE2, 0x20)                  # SEP #$20
    a.raw(0xAF, 0x6C, 0xCC, 0x7E)      # LDA $7ECC6C
    a.raw(0x3A, 0xC9, 0xFF)            # DEC A ; CMP #$FF        high byte was 0: not enough money
    a.raw(0xC2, 0x20)                  # REP #$20
    a.br(0xD0, 'afford')               # BNE afford
    a.raw(0x68)                        # PLA
    a.jmp('done')
    a.label('afford')
    a.raw(0x68)                        # PLA
    a.jsl_abs(0xC00051)                # JSL $C00051             one more into the inventory
    a.raw(0xE2, 0x20)                  # SEP #$20
    a.raw(0xAF, 0x0F, 0xCF, 0x7E)      # LDA $7ECF0F             result, 0 = added
    a.raw(0x48, 0xA9, 0x00, 0x8F, 0x0F, 0xCF, 0x7E, 0x68)   # PHA ; LDA #0 ; STA $7ECF0F ; PLA
    a.raw(0xC2, 0x20)                  # REP #$20
    a.raw(0x29, 0xFF, 0x00)            # AND #$00FF
    a.br(0xD0, 'done')                 # BNE done                the stack is full
    a.jmp('pay')
    a.label('done')
    a.raw(0xC2, 0x30, 0x68)            # REP #$30 ; PLA         drop the counter
    a.raw(0x6B)                        # RTL


def levelup_routine(a, mp):
    """Level-up refill (displaced LDA $E184,X ; STA $E182,X ; SEP #$20 at C0:44D3, 16-bit A on entry, returns 8-bit A)."""
    a.raw(0xBD, 0x84, 0xE1)        # LDA $E184,X
    a.raw(0x9D, 0x82, 0xE1)        # STA $E182,X
    a.raw(0xE2, 0x20)              # SEP #$20
    if mp:
        a.raw(0xBD, 0x87, 0xE1)    # LDA $E187,X
        a.raw(0x9D, 0x86, 0xE1)    # STA $E186,X
    a.raw(0x6B)


def deadguard_routine(a):
    """End of one level-up step (displaced LDA $80 ; ORA #$40, 8-bit A): a hero with the dead flag ($E191 bit 7) goes back to 0 HP."""
    a.raw(0xA5, 0x80, 0x09, 0x40)  # LDA $80 ; ORA #$40
    a.raw(0x48)                    # PHA
    a.raw(0xBD, 0x91, 0xE1)        # LDA $E191,X
    a.br(0x10, 'alive')            # BPL alive
    a.raw(0xC2, 0x20)              # REP #$20
    a.raw(0x9E, 0x82, 0xE1)        # STZ $E182,X
    a.raw(0xE2, 0x20)              # SEP #$20
    a.label('alive')
    a.raw(0x68, 0x6B)              # PLA ; RTL


def magic_rate_routine(a, mult):
    a.raw(0xA9, 0x09, 0x38, 0xE5, 0x9D)   # LDA #$09 ; SEC ; SBC $9D
    if mult == 2:
        a.raw(0x0A)                # ASL
    elif mult == 3:
        a.raw(0x85, 0xAC, 0x0A, 0x18, 0x65, 0xAC)   # STA $AC ; ASL ; CLC ; ADC $AC
    a.raw(0x85, 0xAC, 0x6B)        # STA $AC ; RTL


def fade_routine(a):
    a.raw(0xA5, 0xE5, 0x4A)        # LDA $E5 ; LSR
    a.br(0xD0, 'ok')
    a.raw(0x1A)                    # INC A (never 0: DEC $E4 would wrap to 255)
    a.label('ok')
    a.raw(0x85, 0xE4, 0x6B)        # STA $E4 ; RTL


def qol_plan(opts, R, emit):
    recs = []
    for k in ('qol_chest_fast', 'qol_fast_transitions', 'qol_fast_fades',
              'qol_levelup_mp_restore', 'qol_dead_exp', 'qol_unready_damage'):
        if not isinstance(opts.get(k, False), bool):
            raise ValueError(k + ' must be true or false')
    if opts.get('qol_chest_fast'):
        for off, van in QOL_CHEST:
            recs.append(Rec(off, bytes([van]), b'\xFF', 'qol_chest_fast', 'chest animation script byte %02X -> FF (end)' % van))
    if opts.get('qol_unready_damage'):
        recs.append(Rec(R['halve'], b'\x46\x89', b'\xEA\xEA', 'qol_unready_damage', 'LSR $89 (second halving of an unready swing) -> NOP NOP'))
    if opts.get('qol_fast_transitions'):
        recs.append(Rec(QOL_FADE_TARGET, b'\xE6', b'\xE7', 'qol_fast_transitions', 'fade LDA $E6 -> LDA $E7 (current = target)'))
    if opts.get('qol_fast_fades'):
        fb = emit('qol_fast_fades', 'fade_step', fade_routine)
        recs.append(Rec(QOL_FADE_STEP[0], QOL_FADE_STEP[1], b'\x22' + snes_long(fb), 'qol_fast_fades', 'LDA $E5 ; STA $E4 -> JSL fade_step (half)'))
    lim = opts.get('qol_item_limit', 4)
    if lim not in (4, 7):
        raise ValueError('qol_item_limit must be 4 or 7')
    if lim == 7:
        recs.append(Rec(QOL_ITEM_CMP[0], QOL_ITEM_CMP[1], b'\xEA\xEA', 'qol_item_limit', 'CMP #$9F -> NOP NOP (stack limit 4 -> 7, BCS keeps the overflow carry)'))
    mp, dead = bool(opts.get('qol_levelup_mp_restore')), bool(opts.get('qol_dead_exp'))
    if mp and not R['rando_mp']:
        lb = emit('qol_levelup_mp_restore', 'levelup', levelup_routine, True)
        recs.append(Rec(R['levelup'], QOL_LEVELUP[1], b'\x22' + snes_long(lb) + b'\xEA' * 4, 'qol_levelup_mp_restore',
                        'level-up refill -> JSL levelup (HP and MP)'))
    if dead:
        recs.append(Rec(R['expgate'], QOL_EXP_GATE[1], b'\xEA\xEA', 'qol_dead_exp', 'experience share: BEQ over 0-HP hero -> NOP NOP'))
        gb = emit('qol_dead_exp', 'deadguard', deadguard_routine)
        recs.append(Rec(R['lvtail'], QOL_LV_TAIL, b'\x22' + snes_long(gb), 'qol_dead_exp',
                        'end of a level-up step -> JSL deadguard (a hero with the dead flag goes back to 0 HP)'))
    ml = opts.get('qol_magic_level_rate', 1)
    if ml not in (1, 2, 3):
        raise ValueError('qol_magic_level_rate must be 1, 2 or 3')
    if ml != 1 and not R['rando_magic']:
        mb = emit('qol_magic_level_rate', 'magic_rate', magic_rate_routine, ml)
        recs.append(Rec(QOL_MAGIC[0], QOL_MAGIC[1], b'\x22' + snes_long(mb) + b'\xEA' * 3, 'qol_magic_level_rate',
                        'spell level step 9 - level -> JSL magic_rate (x%d)' % ml))
    rs = opts.get('qol_ring_speed', 1)
    if rs not in (1, 2):
        raise ValueError('qol_ring_speed must be 1 or 2')
    if rs == 2:
        for site, off in ((R['ring_step'], 1), (R['ring_sub'], 4), (R['ring_add'], 4)):
            recs.append(Rec(site + off, b'\x12', b'\x24', 'qol_ring_speed', 'ring turn step per frame 0x12 -> 0x24'))
        for off, van, new in ((0x07, 0xF6, 0xEC), (0x12, 0x0A, 0x14), (0x1D, 0x0A, 0x14), (0x28, 0xF6, 0xEC)):
            recs.append(Rec(R['ring_phase'] + off, bytes([van]), bytes([new]), 'qol_ring_speed', 'ring open/close step %+d -> %+d' % (van - (256 if van > 127 else 0), new - (256 if new > 127 else 0))))
    ms = opts.get('qol_message_speed', 1)
    if ms not in (1, 2, 4):
        raise ValueError('qol_message_speed must be 1, 2 or 4')
    if ms != 1:
        recs.append(Rec(R['msg'], b'\x60', bytes([0x60 // ms]), 'qol_message_speed', 'top-of-screen message lifetime 0x60 -> 0x%02X' % (0x60 // ms)))
    if not isinstance(opts.get('qol_shop_fill', False), bool):
        raise ValueError('qol_shop_fill must be true or false')
    if opts.get('qol_shop_fill'):
        sb = emit('qol_shop_fill', 'shopfill', shopfill_routine, R['shop_fail'])
        old = R['shop_old']
        recs.append(Rec(R['shop'], old, b'\x22' + snes_long(sb) + b'\xEA', 'qol_shop_fill',
                        'buy shop: BNE fail ; JSR pay -> JSL shopfill (pay, then buy the stack full)'))
    dz = opts.get('qol_scroll_deadzone', 'vanilla')
    if dz not in ('vanilla', 'tight', 'tighter'):
        raise ValueError('qol_scroll_deadzone must be vanilla, tight or tighter')
    if dz != 'vanilla':
        for (off, van), new in zip(QOL_DEADZONE_SITES, QOL_DEADZONE[dz]):
            recs.append(Rec(off, bytes([van]), bytes([new]), 'qol_scroll_deadzone', 'camera window bound %02X -> %02X' % (van, new)))
    return recs


def charge_params(opts):
    """returns (reload_byte_patch, acts, acts_haste) or None for vanilla"""
    f = opts.get('charge_frames_per_stage')
    if f is None:
        f = {'vanilla': 90, 'fast': 45, 'faster': 22, 'instant': 1}[opts.get('charge_speed', 'vanilla')]
    if not 1 <= f <= 90:
        raise ValueError('charge_frames_per_stage must be 1..90')
    reload_ = 1 if f > 45 else 0
    acts = f if reload_ == 0 else (f + 1) // 2
    cadence = 2 if reload_ else 1
    haste = None
    if opts.get('charge_speed_up_bonus') or opts.get('charge_bonus_item_id') is not None:
        haste = max(1, acts // 2)
        if haste >= acts:
            haste = None
    return reload_, acts, haste


def plan(opts, body=None):
    """Returns the list of Rec (ROM independent)."""
    for k in opts:
        if k not in CATALOGUE and not k.startswith('_'):
            raise ValueError('unknown option ' + k)
    recs = []
    code = []     # (name, bytes) in order

    def emit(owner, name, fn, *args):
        a = Asm(FREE_BASE + sum(len(b) for _, _, b in code))
        fn(a, *args)
        code.append((owner, name, a.done()))
        return a.base

    R = resolve(body)
    wr = opts.get('weapon_recharge', 'vanilla')
    if wr not in CATALOGUE['weapon_recharge']['values']:
        raise ValueError('weapon_recharge: ' + str(wr))
    if wr != 'vanilla':
        base = emit('weapon_recharge', 'recharge_attack', recharge_routine, wr)
        for s in SITES['recharge_attack']:
            recs.append(Rec(s, RECHARGE_V, b'\x22' + snes_long(base) + b'\xEA', 'weapon_recharge',
                            'ADC #$32 ; STA $E1ED,X  ->  JSL recharge_attack (%s, heroes only)' % wr))
    rr = opts.get('run_recharge', 'vanilla')
    if rr not in CATALOGUE['run_recharge']['values']:
        raise ValueError('run_recharge: ' + str(rr))
    if rr != 'vanilla':
        base = emit('run_recharge', 'recharge_run', recharge_routine, 'instant' if rr == 'none' else 'fast')
        recs.append(Rec(SITES['recharge_run'][0], RECHARGE_V, b'\x22' + snes_long(base) + b'\xEA', 'run_recharge',
                        'run end gauge ADC #$32 ; STA $E1ED,X  ->  JSL recharge_run (%s)' % rr))
    mf = opts.get('monster_flinch', 'vanilla')
    if mf not in CATALOGUE['monster_flinch']['values']:
        raise ValueError('monster_flinch: ' + str(mf))
    hf = opts.get('hero_flinch', 'vanilla')
    if hf not in CATALOGUE['hero_flinch']['values']:
        raise ValueError('hero_flinch: ' + str(hf))
    if mf != 'vanilla' or hf != 'vanilla':
        owner = 'monster_flinch' if mf != 'vanilla' else 'hero_flinch'
        base = emit(owner, 'reaction_gate', flinch_routine, mf, hf, R['flinch'] + 0x61, R['flinch'] + 5)
        recs.append(Rec(R['flinch'], FLINCH_V, b'\x5c' + snes_long(base) + b'\xEA', owner,
                        '4EBC entry REP #$20 ; LDA $E190,X  ->  JML reaction_gate (monsters %s, heroes %s)' % (mf, hf)))
        if mf in ('strong', 'heavy'):
            cb = emit('monster_flinch', 'crit_mark', crit_routine, R['crit_target'])
            recs.append(Rec(R['crit'], bytes([0x22, R['crit_target'] & 255, R['crit_target'] >> 8, 0xC0]), b'\x22' + snes_long(cb), 'monster_flinch',
                            'critical notice JSL $C05990 -> JSL crit_mark (sets $E16B on a monster defender)'))
            zb = emit('monster_flinch', 'zero_clear', zero_routine, R['zero_target'])
            recs.append(Rec(R['zero'], bytes([0x22, R['zero_target'] & 255, R['zero_target'] >> 8, 0xC0]), b'\x22' + snes_long(zb), 'monster_flinch',
                            'zero-damage JSL $C04E6B -> JSL zero_clear (drops the critical mark)'))
    cm = opts.get('crit_multiplier', 2)
    if cm not in (2, 3, 4):
        raise ValueError('crit_multiplier must be 2, 3 or 4')
    if cm != 2:
        cbase = emit('crit_multiplier', 'crit_mul', critmul_routine, cm)
        recs.append(Rec(R['crit'] + 8, bytes.fromhex('C2200689'), b'\x22' + snes_long(cbase), 'crit_multiplier',
                        'REP #$20 ; ASL $89 (x2)  ->  JSL crit_mul (x%d)' % cm))
    wl = opts.get('weapon_level_rate', 1)
    if wl not in (1, 2, 3):
        raise ValueError('weapon_level_rate must be 1, 2 or 3')
    if wl != 1:
        off = R['wlevel'] if R['wlevel'] is not None else 0x4358
        wb = emit('weapon_level_rate', 'wlevel', wlevel_routine, wl)
        recs.append(Rec(off, bytes.fromhex(PAT_WLEVEL), b'\x22' + snes_long(wb) + b'\xEA\xEA\xEA\xEA', 'weapon_level_rate',
                        'a6 = 9 - level  ->  JSL wlevel (x%d)' % wl))
    ws = opts.get('walk_speed', 2)
    if ws not in (2, 3, 4):
        raise ValueError('walk_speed must be 2, 3 or 4')
    if ws != 2:
        for off, van in WALK_SITES:
            recs.append(Rec(off, bytes([van]), bytes([(van & 0x80) | ws]), 'walk_speed', 'walk immediate %02X -> %02X' % (van, (van & 0x80) | ws)))
    cp = charge_params(opts)
    reload_, acts, haste = cp
    item = opts.get('charge_bonus_item_id')
    if item is not None and not 0 <= item <= 254:
        raise ValueError('charge_bonus_item_id must be 0..254')
    if (reload_, acts, haste) != (1, 45, None):
        base = emit('charge_speed', 'charge_gate', charge_routine, acts, haste, bool(opts.get('charge_speed_up_bonus')), item)
        recs.append(Rec(SITES['charge_cmp'], CHARGE_V, b'\x5c' + snes_long(base),
                        'charge_speed', 'INC A ; CMP #$2D ; BCC  ->  JML charge_gate (acts per stage %d%s)'
                        % (acts, '' if haste is None else ', Speed Up %d' % haste)))
        if reload_ == 0:
            recs.append(Rec(0xB3A7 + 1, b'\x01', b'\x00', 'charge_speed',
                            'LDA #$01 ; STA $E01F,X (act every 2nd frame) -> act every frame'))
    for key, off, van in (('hero_crit_bonus_girl', R['crit_girl'], 0x0A), ('hero_crit_bonus_others', R['crit_other'], 0x05)):
        v = opts.get(key)
        if v is not None:
            if not 0 <= v <= 120:
                raise ValueError(key + ' must be 0..120')
            if v != van:
                recs.append(Rec(off, bytes([van]), bytes([v]), key, 'ADC #$%02X -> ADC #$%02X' % (van, v)))
    recs.extend(qol_plan(opts, R, emit))
    total = sum(len(b) for _, _, b in code)
    if total > FREE_LEN:
        raise ValueError('free area overflow')
    pos = FREE_BASE
    for owner, name, blob in code:
        recs.append(Rec(pos, b'\xFF' * len(blob), blob, owner, 'routine %s (%d bytes) in the free area' % (name, len(blob))))
        pos += len(blob)
    return recs


def split_header(rom):
    hdr = len(rom) % 0x8000 if len(rom) % 0x8000 in (0, 512) else None
    if hdr is None:
        raise ValueError('unexpected ROM size %d' % len(rom))
    return hdr


def checksum_ok(body):
    comp = body[0xFFDC] | body[0xFFDD] << 8
    chk = body[0xFFDE] | body[0xFFDF] << 8
    return (comp ^ chk) == 0xFFFF and chk == sum(body) & 0xFFFF


def set_checksum(body):
    body[0xFFDC:0xFFE0] = b'\xFF\xFF\x00\x00'
    s = sum(body) & 0xFFFF
    body[0xFFDE:0xFFE0] = bytes([s & 0xFF, s >> 8])
    c = s ^ 0xFFFF
    body[0xFFDC:0xFFDE] = bytes([c & 0xFF, c >> 8])


def check_rec(body, r):
    cur = bytes(body[r.off:r.off + len(r.before)])
    if cur == r.before:
        return 'ok'
    if cur == r.after:
        return 'already'
    return 'conflict'


KNOWN_HACKS = {
    0xF942: 'randomizer NoEnergyToAttack / FasterEnemies (aggressive enemies) on the swing-end gauge',
    0x1CADE: 'unexpected bytes at the interrupted-charge gauge',
    0x2C248: 'randomizer NoEnergyToAttack / FasterEnemies on the weapon-sprite gauge',
    0xB788: 'randomizer NoEnergyToRun (NOPs the gauge store)',
    0xB3C6: 'unexpected bytes in the charge gauge routine',
}


def apply_recs(body, recs, on_conflict='error'):
    """Checks every record against the ROM. An option is all-or-nothing: with on_conflict='skip' the options that
    conflict are dropped entirely (their routines too) and reported; with 'error' any conflict aborts."""
    conflicts = []
    states = []
    for r in recs:
        st = check_rec(body, r)
        states.append(st)
        if st == 'conflict':
            conflicts.append((r, bytes(body[r.off:r.off + len(r.before)])))
    if conflicts and on_conflict == 'error':
        msg = ['conflicts (bytes differ from vanilla and from the patch):']
        for r, cur in conflicts:
            msg.append('  %s @0x%06X expected %s found %s: %s' % (r.opt, r.off, r.before.hex(), cur.hex(), KNOWN_HACKS.get(r.off, r.note)))
        raise Conflict('\n'.join(msg))
    bad = {r.opt for r, _ in conflicts}
    applied = [(r, st) for r, st in zip(recs, states) if r.opt not in bad]
    for r, st in applied:
        body[r.off:r.off + len(r.after)] = r.after
    return applied, conflicts, sorted(bad)


def inside_repo(path):
    p = pathlib.Path(path).resolve()
    return p == ROOT or ROOT in p.parents


def load_opts(path):
    d = json.loads(pathlib.Path(path).read_text())
    return d.get('options', d)


# ---- conflict detector for the free monster byte +0x16B (the critical-hit mark used by monster_flinch strong/heavy) ----
# Looks for instructions whose operand is $016B (absolute / absolute indexed / immediate index) or $7E:E16B (long).
# Vanilla, Magno and every randomizer mode contain exactly the two benign hits below, both inside compressed data
# (D2:A283 and DD:9906), so any other hit is a hack that may use the byte.  Heuristic, not a full disassembly.
MARK_ABS_OPS = {0x0D, 0x0E, 0x1D, 0x1E, 0x2C, 0x2D, 0x2E, 0x3C, 0x3D, 0x3E, 0x4D, 0x4E, 0x5D, 0x6D, 0x6E, 0x7D, 0x7E,
                0x8C, 0x8D, 0x8E, 0x9C, 0x9D, 0x9E, 0xAC, 0xAD, 0xAE, 0xBC, 0xBD, 0xBE, 0xCC, 0xCD, 0xCE, 0xDD, 0xDE,
                0xEC, 0xED, 0xEE, 0xFD, 0xFE, 0x19, 0x39, 0x59, 0x79, 0x99, 0xB9, 0xD9, 0xF9, 0x1C, 0x0C, 0xA0, 0xA2, 0xC0, 0xE0}
MARK_LONG_OPS = {0x0F, 0x1F, 0x2F, 0x3F, 0x4F, 0x5F, 0x6F, 0x7F, 0x8F, 0x9F, 0xAF, 0xBF, 0xCF, 0xDF, 0xEF, 0xFF}
MARK_BENIGN = {0x12A283, 0x1D9906}


def scan_mark_use(body, skip=((FREE_BASE, FREE_BASE + FREE_LEN),)):
    hits = []
    for pat, ops in ((b'\x6b\x01', MARK_ABS_OPS), (b'\x6b\xe1\x7e', MARK_LONG_OPS)):
        i = body.find(pat)
        while i != -1:
            o = i - 1
            if o >= 0 and body[o] in ops and o not in MARK_BENIGN and not any(a <= o < b for a, b in skip):
                hits.append(o)
            i = body.find(pat, i + 1)
    return sorted(hits)


# ---- launcher menu export (schema style of tools/companion/randomizer/presets.json) ----
MENU_META = {
    'weapon_recharge': ('Weapon recharge', 'Weapon', False, {'vanilla': 'Vanilla', 'fast': 'Fast', 'faster': 'Faster', 'instant': 'Instant'}),
    'run_recharge': ('Run recharge', 'Weapon', False, {'vanilla': 'Vanilla', 'fast': 'Fast', 'none': 'None'}),
    'charge_speed': ('Charge build-up', 'Weapon', False, {'vanilla': 'Vanilla (90 frames/stage)', 'fast': 'Fast (45)', 'faster': 'Faster (22)', 'instant': 'Instant'}),
    'charge_frames_per_stage': ('Charge frames per stage', 'Weapon', True, None),
    'charge_speed_up_bonus': ('Speed Up speeds charge', 'Weapon', True, None),
    'charge_bonus_item_id': ('Charge-boost equipment id', 'Weapon', True, None),
    'weapon_level_rate': ('Weapon level progress', 'Weapon', False, {'1': 'x1 (vanilla)', '2': 'x2', '3': 'x3'}),
    'monster_flinch': ('Monster flinch', 'Enemies', False, {'vanilla': 'Vanilla', 'strong': 'Only magic and criticals', 'heavy': 'Also big hits', 'never': 'Never'}),
    'hero_flinch': ('Hero flinch', 'Heroes', False, {'vanilla': 'Vanilla', 'light': 'Only big hits', 'never': 'Never'}),
    'hero_crit_bonus_girl': ('Girl critical bonus', 'Heroes', True, None),
    'hero_crit_bonus_others': ('Boy and Sprite critical bonus', 'Heroes', True, None),
    'crit_multiplier': ('Critical multiplier', 'Heroes', False, {'2': 'x2 (vanilla)', '3': 'x3', '4': 'x4'}),
    'walk_speed': ('Walk speed', 'Heroes', False, {'2': '2 px/frame (vanilla)', '3': '3 px/frame', '4': '4 px/frame'}),
    'qol_chest_fast': ('Chests open instantly', 'Quality of life', False, None),
    'qol_unready_damage': ('Full damage when swinging early', 'Quality of life', False, None),
    'qol_fast_transitions': ('No fade on map transitions', 'Quality of life', False, None),
    'qol_fast_fades': ('Faster screen fades', 'Quality of life', False, None),
    'qol_item_limit': ('Item stack limit', 'Quality of life', False, {'4': '4 (vanilla)', '7': '7'}),
    'qol_levelup_mp_restore': ('Level-up refills MP', 'Quality of life', False, None),
    'qol_dead_exp': ('Fallen heroes gain experience', 'Quality of life', False, None),
    'qol_magic_level_rate': ('Spell level progress', 'Quality of life', False, {'1': 'x1 (vanilla)', '2': 'x2', '3': 'x3'}),
    'qol_scroll_deadzone': ('Screen scroll window', 'Quality of life', False, {'vanilla': 'Vanilla', 'tight': 'Tight', 'tighter': 'Tighter'}),
    'qol_ring_speed': ('Ring menu speed', 'Quality of life', False, {'1': 'x1 (vanilla)', '2': 'x2'}),
    'qol_shop_fill': ('Shops fill the stack', 'Quality of life', False, None),
    'qol_message_speed': ('Top-of-screen message time', 'Quality of life', False, {'1': 'Vanilla', '2': 'Half', '4': 'Quarter'}),
}
NUM_RANGE = {'charge_frames_per_stage': (1, 90), 'hero_crit_bonus_girl': (0, 120), 'hero_crit_bonus_others': (0, 120),
             'charge_bonus_item_id': (0, 254)}
RANDOMIZER_CONFLICTS = {   # randomizer setting (default) -> options whose bytes it also writes
    'attacksForever': ('weapon_recharge',), 'aggEnemies': ('weapon_recharge',), 'noStaminaRun': ('run_recharge',),
    'funSpeedup': ('walk_speed',), 'fasterWeaponLevel': ('weapon_level_rate',), 'fasterMagicLevel': ('qol_magic_level_rate',)}
RANDOMIZER_ALREADY = {   # randomizer setting (default) that already applies the same behaviour: the option is a no-op there
    'fasterChests': ('qol_chest_fast',), 'fastTransition': ('qol_fast_transitions',),
    'moreItems': ('qol_item_limit',), 'mpAtLevel': ('qol_levelup_mp_restore',)}
OPTION_CONFLICTS = {     # option -> {other option: relation}. 'redundant': allowed together, the second adds nothing; 'excludes': do not enable both
    'qol_fast_fades': {'qol_fast_transitions': 'redundant'},
    'qol_fast_transitions': {'qol_fast_fades': 'redundant'},
    'qol_dead_exp': {'qol_levelup_mp_restore': 'independent'},
    'qol_levelup_mp_restore': {'qol_dead_exp': 'independent'},
    'qol_magic_level_rate': {'weapon_level_rate': 'independent'},
}
GAMEPLAY_EQUIVALENTS = {  # community patch idea -> existing option of this tool
    'Secret of Mana - Stamina Removed (Faydgamer)': {'weapon_recharge': 'instant', 'run_recharge': 'none'},
    'Faster Weapon Grinding (Kethinov)': {'weapon_level_rate': 3},
}
MENU_PRESETS = [
    ('vanilla', 'Vanilla', 'No change: the ROM is left byte-identical.', {}),
    ('quick-combat', 'Quick Combat', 'Short waits between swings, quick charge, ordinary enemies only flinch from magic and critical hits.',
     {'weapon_recharge': 'faster', 'run_recharge': 'fast', 'charge_speed': 'faster', 'monster_flinch': 'strong'}),
    ('arcade', 'Arcade', 'No weapon wait, instant-ish charge, heroes shrug off small hits, triple criticals, brisk walking.',
     {'weapon_recharge': 'instant', 'run_recharge': 'none', 'charge_speed': 'fast', 'monster_flinch': 'strong',
      'hero_flinch': 'light', 'crit_multiplier': 3, 'hero_crit_bonus_girl': 30, 'hero_crit_bonus_others': 30, 'walk_speed': 3}),
    ('qol', 'Quality of life', 'Shorter animations and fades, 7 items per stack, MP on level-up, fallen heroes still gain experience.',
     {'qol_chest_fast': True, 'qol_fast_fades': True, 'qol_item_limit': 7,
      'qol_levelup_mp_restore': True, 'qol_dead_exp': True, 'qol_unready_damage': True, 'qol_scroll_deadzone': 'tight',
      'qol_ring_speed': 2, 'qol_message_speed': 2, 'qol_shop_fill': True}),
]


def menu_json():
    opts = []
    for key, (label, group, adv, labels) in MENU_META.items():
        c = CATALOGUE[key]
        o = {'key': key, 'label': label, 'group': group, 'advanced': adv, 'help': c['doc']}
        if labels:
            o['type'] = 'enum'
            o['default'] = str(c['default'])
            o['values'] = [{'v': str(v), 'l': labels[str(v)]} for v in c['values']]
        elif key in NUM_RANGE:
            lo, hi = NUM_RANGE[key]
            o.update(type='num', default='', optional=True, min=lo, max=hi, decimals=0, step=1)
        else:
            o.update(type='bool', default='yes' if c['default'] else 'no')
        cf = sorted(k for k, v in RANDOMIZER_CONFLICTS.items() if key in v)
        if cf:
            o['randomizer_conflicts'] = cf
        al = sorted(k for k, v in RANDOMIZER_ALREADY.items() if key in v)
        if al:
            o['randomizer_already'] = al
        if key in OPTION_CONFLICTS:
            o['conflicts'] = dict(OPTION_CONFLICTS[key])
        opts.append(o)
    return {
        'schema': 1,
        'generator': {'name': 'som_gameplay', 'path': 'tools/gameplay/som_gameplay.py', 'licence': 'project'},
        'note': 'Generated by "som_gameplay.py menu". Option keys are the keys of the option file passed to "som_gameplay.py apply --options". '
                'Convert types when writing the file: enum -> string (numeric-looking values as integers), bool yes/no -> true/false, '
                'num -> integer, an empty optional num is left out. Applied AFTER the ROM is generated; not a generator setting.',
        'config_schema': 'docs/gameplay-options-20261018.md',
        'apply': {'command': 'python3 tools/gameplay/som_gameplay.py apply --rom IN.sfc --options OPTIONS.json --out OUT.sfc --on-conflict skip',
                  'integer_enums': ['crit_multiplier', 'walk_speed', 'weapon_level_rate', 'qol_item_limit', 'qol_magic_level_rate', 'qol_ring_speed',
                                    'qol_message_speed'],
                  'on_conflict': 'skip drops only the conflicting option and reports it on stderr'},
        'supported_roms': ['US v1.0 vanilla', 'Magno', 'randomizer ROMs (Ancient Cave, Open World, Randomizer, Boss Rush, Chaos)'],
        'groups': ['Weapon', 'Enemies', 'Heroes', 'Quality of life'],
        'equivalents': GAMEPLAY_EQUIVALENTS,
        'options': opts,
        'presets': [{'id': i, 'name': n, 'description': d, 'set': dict(v)} for i, n, d, v in MENU_PRESETS],
    }


def cmd_list(a):
    for k, v in CATALOGUE.items():
        print('%-26s default=%-8s values=%s\n    %s' % (k, v['default'], v['values'], v['doc']))


def cmd_menu(a):
    text = json.dumps(menu_json(), indent=1) + '\n'
    if a.out:
        if inside_repo(a.out) and pathlib.Path(a.out).resolve() != HERE / 'gameplay_menu.json':
            sys.exit('error: only tools/gameplay/gameplay_menu.json may be written inside the repository')
        pathlib.Path(a.out).write_text(text)
    else:
        sys.stdout.write(text)


def cmd_plan(a):
    opts = load_opts(a.options)
    body = None
    if a.rom:
        raw = pathlib.Path(a.rom).read_bytes()
        body = raw[split_header(raw):]
    recs = plan(opts, body)
    out = [r.as_json() for r in recs]
    if a.rom:
        for o, r in zip(out, recs):
            o['rom'] = check_rec(body, r)
    print(json.dumps(out, indent=1))


def cmd_apply(a):
    if inside_repo(a.out):
        sys.exit('error: refusing to write a ROM inside the repository')
    opts = load_opts(a.options)
    raw = pathlib.Path(a.rom).read_bytes()
    h = split_header(raw)
    header, body = raw[:h], bytearray(raw[h:])
    try:
        recs = plan(opts, bytes(body))
    except Conflict as e:
        sys.exit('error: ' + str(e))
    if len(body) <= FREE_BASE + FREE_LEN:
        sys.exit('error: ROM too small')
    was_valid = checksum_ok(body)
    if opts.get('monster_flinch', 'vanilla') in ('strong', 'heavy'):
        hits = scan_mark_use(bytes(body))
        if hits:
            print('warning: the ROM has code that may use monster byte +0x16B (critical mark): ' +
                  ', '.join('0x%06X' % x for x in hits), file=sys.stderr)
    try:
        applied, conflicts, skipped = apply_recs(body, recs, a.on_conflict)
    except Conflict as e:
        sys.exit('error: ' + str(e))
    for r, cur in conflicts:
        print('skipped option %s: %s @0x%06X (%s)' % (r.opt, r.note, r.off, KNOWN_HACKS.get(r.off, 'bytes differ from vanilla')), file=sys.stderr)
    mode = opts.get('fix_checksum', 'auto')
    changed = any(st == 'ok' for _, st in applied)
    if changed and (mode == 'always' or (mode == 'auto' and was_valid)):
        set_checksum(body)
    pathlib.Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    pathlib.Path(a.out).write_bytes(header + bytes(body))
    print('wrote %s: %d records (%d bytes changed), checksum %s' % (
        a.out, len(applied), sum(len(r.after) for r, st in applied if st == 'ok'), 'updated' if changed and (mode == 'always' or (mode == 'auto' and was_valid)) else 'untouched'))


def cmd_ips(a):
    recs = plan(load_opts(a.options))
    out = bytearray(b'PATCH')
    for r in sorted(recs, key=lambda r: r.off):
        out += r.off.to_bytes(3, 'big') + len(r.after).to_bytes(2, 'big') + r.after
    out += b'EOF'
    if inside_repo(a.out):
        sys.exit('error: refusing to write inside the repository')
    pathlib.Path(a.out).write_bytes(bytes(out))
    print('wrote %s (%d records; headerless offsets; the header checksum is not part of the IPS)' % (a.out, len(recs)))


def cmd_verify(a):
    v = pathlib.Path(a.rom).read_bytes()
    p = pathlib.Path(a.patched).read_bytes()
    hv, hp = split_header(v), split_header(p)
    v, p = v[hv:], p[hp:]
    recs = plan(load_opts(a.options), v)
    if len(v) != len(p):
        sys.exit('size differs')
    covered = set()
    for r in recs:
        covered.update(range(r.off, r.off + len(r.after)))
    covered.update(range(0xFFDC, 0xFFE0))
    diff = [i for i in range(len(v)) if v[i] != p[i]]
    stray = [i for i in diff if i not in covered]
    print('differing bytes %d, outside planned records %d' % (len(diff), len(stray)))
    hits = scan_mark_use(v)
    if hits:
        print('WARNING: the input ROM has code that may use the monster byte +0x16B (critical mark of monster_flinch '
              'strong/heavy) at: ' + ', '.join('0x%06X' % h for h in hits))
    else:
        print('+0x16B check: no foreign use of the monster byte $E16B found in the input ROM')
    marked = load_opts(a.options).get('monster_flinch', 'vanilla') in ('strong', 'heavy')
    sys.exit(1 if stray or (hits and marked) else 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest='cmd', required=True)
    sp.add_parser('list').set_defaults(f=cmd_list)
    p = sp.add_parser('menu'); p.add_argument('--out'); p.set_defaults(f=cmd_menu)
    p = sp.add_parser('plan'); p.add_argument('--options', required=True); p.add_argument('--rom'); p.set_defaults(f=cmd_plan)
    p = sp.add_parser('apply'); p.add_argument('--rom', required=True); p.add_argument('--options', required=True); p.add_argument('--out', required=True)
    p.add_argument('--on-conflict', choices=['error', 'skip'], default='error'); p.set_defaults(f=cmd_apply)
    p = sp.add_parser('ips'); p.add_argument('--options', required=True); p.add_argument('--out', required=True); p.set_defaults(f=cmd_ips)
    p = sp.add_parser('verify'); p.add_argument('--rom', required=True); p.add_argument('--patched', required=True); p.add_argument('--options', required=True); p.set_defaults(f=cmd_verify)
    a = ap.parse_args()
    a.f(a)


if __name__ == '__main__':
    main()
