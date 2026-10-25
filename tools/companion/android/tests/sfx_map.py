"""Measures the game menu sound ids on the PC: resets $1E01 every frame and reports the id each ring action writes.
Needs devstate.sav (the advanced test state) in the working directory."""
import sys,ctypes as C
sys.path.insert(0, str(__import__('pathlib').Path(__file__).resolve().parents[2]))
import retro
c=retro.Core('/home/cidwel/projects/rommancer/roms/snes/Secret of Mana (USA).smc')
st=open('devstate.sav','rb').read(); c.load_state(st); c.run(30)
log=[]
def act(name,btn,hold=4,after=40):
    """press btn for `hold` frames and run `after` more; report every sound id written to $1E01 (reset each frame)"""
    got=[]
    for f in range(hold+after):
        c.write(0x1E01,b'\x00')
        c.run(1,btn if f<hold else None)
        v=c.u8(0x1E01)
        if v: got.append((f,hex(v)))
    r=(c.u8(0x7D),c.u8(0x1808),c.u8(0x1809),c.u8(0x180A),c.u8(0x180B),c.u8(0x1803))
    print(f'{name:28s} snd={got} ring open={r[0]} owner={r[1]} cat={r[2]} cur={r[3]} cnt={r[4]} 1803={r[5]:02x}')
c.run(60)
for n,b in [('Y (open ring)','Y'),('RIGHT cursor','RIGHT'),('LEFT cursor','LEFT'),('UP category','UP'),('DOWN category','DOWN'),
            ('X owner','X'),('UP category','UP'),('UP category','UP'),('A on spirit','A'),('A (spell, ok)','A'),('B','B'),('B','B'),('B','B'),('Y','Y'),('Y','Y')]:
    act(n,b); c.run(25)
