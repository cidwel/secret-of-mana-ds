"""WRAM-only routine call: a stub in WRAM, entered through the game's own RAM NMI trampoline at $00:0100.

The game keeps its NMI vector in WRAM (`JML $C0:C0A8` at $0100).  This module builds a small 65816 stub, writes it
to a free WRAM area and points the trampoline at it.  On the next NMI the stub checks that the interrupted main thread
is parked in its frame-wait spin (so no game logic is half done), saves the registers, performs the calls, writes the
register results to a mailbox, puts the trampoline back and continues into the real NMI handler.  The ROM is never
modified and no CPU register of the core is accessed, so it works with any libretro core that exposes WRAM.
"""
import struct

MBOX = 0x1FF90          # 7F:FF90 mailbox (4 bytes), free in every state checked: vanilla, Magno, Ancient Cave
STUB = 0x1FF94          # 7F:FF94 stub (108 bytes)
NMI_VEC = 0x100


def L(off):
    return 0x7E0000 + off


class Asm:
    def __init__(self, base):
        self.base = base
        self.b = bytearray()
        self.labels = {}
        self.fix = []

    def pc(self):
        return self.base + len(self.b)

    def raw(self, *bs):
        self.b += bytes(bs)

    def label(self, n):
        self.labels[n] = len(self.b)

    def branch(self, op, n):
        self.b += bytes([op, 0])
        self.fix.append((len(self.b) - 1, n))

    def resolve(self):
        for pos, n in self.fix:
            rel = self.labels[n] - (pos + 1)
            assert -128 <= rel < 128, 'branch out of range'
            self.b[pos] = rel & 255
        return bytes(self.b)

    # helpers
    def rep(self, v): self.raw(0xC2, v)
    def sep(self, v): self.raw(0xE2, v)
    def lda16(self, v): self.raw(0xA9, v & 255, v >> 8 & 255)
    def ldx16(self, v): self.raw(0xA2, v & 255, v >> 8 & 255)
    def ldy16(self, v): self.raw(0xA0, v & 255, v >> 8 & 255)
    def lda8(self, v): self.raw(0xA9, v & 255)
    def cmp16(self, v): self.raw(0xC9, v & 255, v >> 8 & 255)
    def lda_long(self, a): self.raw(0xAF, a & 255, a >> 8 & 255, a >> 16 & 255)
    def sta_long(self, a): self.raw(0x8F, a & 255, a >> 8 & 255, a >> 16 & 255)
    def jsl(self, a): self.raw(0x22, a & 255, a >> 8 & 255, a >> 16 & 255)
    def jml(self, a): self.raw(0x5C, a & 255, a >> 8 & 255, a >> 16 & 255)
    def pea(self, v): self.raw(0xF4, v & 255, v >> 8 & 255)


def find_rtl(rom, bank):
    """Offset (inside the bank) of a 0x6B byte in the upper half of a HiROM bank (C0..FF)."""
    base = (bank - 0xC0) * 0x10000
    for off in range(0x8000, 0x10000):
        if rom[base + off] == 0x6B:
            return off
    raise ValueError('no RTL in bank %02X' % bank)


def build(calls, rom, spin=0xB078, orig=(0x5C, 0xA8, 0xC0, 0x00), base=STUB):
    """calls: list of dict(addr=0xBBAAAA, a=, x=, y=, rts=bool).  Returns the stub bytes.

    spin: interrupted PCs whose (PC & 0xFFFC) equals this value count as parked in the frame-wait spin.
    Mailbox: +0 status (0 idle, 2 done), +1/+2 A and +3 P after the last call.
    Calls run with DB = $7E and D = 0; X/Y/A are loaded from the call."""
    a = Asm(base)
    a.rep(0x30)
    a.raw(0x48)                                     # PHA
    a.raw(0xA3, 0x04)                               # LDA $04,S  (interrupted PC: A=2 bytes, P=1 byte pushed)
    a.raw(0x29, 0xFC, 0xFF)                         # AND #$FFFC
    a.cmp16(spin)
    a.branch(0xF0, 'idle')                          # BEQ idle
    a.raw(0x68)                                     # PLA: not parked, run the NMI unchanged, hook stays armed
    a.jml((orig[3] << 16) | (orig[2] << 8) | orig[1])
    a.label('idle')
    a.raw(0xDA, 0x5A, 0x0B, 0x8B)                   # PHX PHY PHD PHB
    a.sep(0x20)
    a.lda8(0x7E)
    a.raw(0x48, 0xAB)                               # PHA PLB  (DB = $7E)
    a.rep(0x30)
    a.pea(0x0000)
    a.raw(0x2B)                                     # PLD
    for c in calls:
        addr = c['addr']
        if c.get('rts'):
            off = find_rtl(rom, addr >> 16)
            # RTS of the routine lands on an RTL in its own bank, which returns to the instruction after the JML
            a.sep(0x20)
            a.lda8(0x7F)
            a.raw(0x48)                             # PHA (return bank)
            a.rep(0x20)
            nld = sum(1 for k in 'axy' if k in c)
            after = a.pc() + 3 + 3 + 3 * nld + 4
            a.pea(after - 1)
            a.pea(off - 1)
            if 'a' in c: a.lda16(c['a'])
            if 'x' in c: a.ldx16(c['x'])
            if 'y' in c: a.ldy16(c['y'])
            a.jml(addr)
        else:
            if 'a' in c: a.lda16(c['a'])
            if 'x' in c: a.ldx16(c['x'])
            if 'y' in c: a.ldy16(c['y'])
            a.jsl(addr)
    a.raw(0x08)                                     # PHP (flags of the last call)
    a.rep(0x30)
    a.sta_long(L(MBOX + 1))
    a.sep(0x20)
    a.raw(0x68)                                     # PLA = flags
    a.sta_long(L(MBOX + 3))
    a.lda8(2)
    a.sta_long(L(MBOX))
    a.rep(0x30)
    a.lda16(orig[0] | orig[1] << 8)                 # put the trampoline back
    a.sta_long(L(NMI_VEC))
    a.lda16(orig[2] | orig[3] << 8)
    a.sta_long(L(NMI_VEC + 2))
    a.raw(0xAB, 0x2B, 0x7A, 0xFA, 0x68)             # PLB PLD PLY PLX PLA
    a.jml((orig[3] << 16) | (orig[2] << 8) | orig[1])
    out = a.resolve()
    assert STUB + len(out) <= 0x20000, 'stub does not fit: %d bytes' % len(out)
    return out


def arm(core, calls, rom):
    """Writes the stub and the hook.  Returns the original trampoline bytes."""
    orig = bytes(core.read(NMI_VEC, 4))
    stub = build(calls, rom, orig=tuple(orig))
    core.write(MBOX, bytes(4))
    core.write(STUB, stub)
    core.write(NMI_VEC, bytes([0x5C, STUB & 255, STUB >> 8 & 255, 0x7F]))
    return orig, len(stub)


def done(core):
    return core.u8(MBOX) == 2


def result(core):
    m = bytes(core.read(MBOX, 4))
    return dict(status=m[0], A=m[1] | m[2] << 8, P=m[3])


def run_job(core, call, rom, max_frames=6):
    """Arms one call, steps frames until the stub reports done.  Returns (result dict, frames used) or (None, n)."""
    arm(core, [call], rom)
    for n in range(1, max_frames + 1):
        core.step()
        if done(core):
            return result(core), n
    core.write(NMI_VEC, bytes(core.read(NMI_VEC, 4)))  # leave the hook as it is; caller decides
    return None, max_frames
