"""Device regression: after every equip tap on the panel the highlighted weapon cell must equal the equipped weapon (WRAM) of the viewed hero.
Usage: python3 device_weapon_highlight.py SEED COUNT  (adb serial 5d1ee37e, panel on display 4, advanced test state)."""
import subprocess, random, time, sys, re
from PIL import Image
import numpy as np
S=['adb','-s','5d1ee37e']
M='/home/cidwel/.local/state/som-decomp/companion-20261017/device/matrix/'
BOT='4630946482288158084'
X=[155,460,760,1065]*2; Y=[600]*4+[820]*4
HX=[200,615,1020]
def sh(*a): return subprocess.run(S+list(a),capture_output=True,text=True).stdout
def tap(x,y,ms=90,wait=1.2):
    sh('shell','input','-d','4','swipe',str(x),str(y),str(x),str(y),str(ms)); time.sleep(wait)
def state():
    sh('shell','am','broadcast','-a','org.somdecomp.companion.DEBUG','--es','state','1'); time.sleep(0.7)
    LOG=sh('logcat','-d','-s','somcompanion')
    l=[x for x in LOG.splitlines() if 'state mode' in x][-1]
    r={}
    r['w']=[int(v) for v in re.search(r' w=(\d+),(\d+),(\d+)',l).groups()]
    r['h']=[tuple(int(v) for v in m) for m in re.findall(r'h\d\[t=(\d+) r3=(\d+) r8=(\d+) held=(-?\d+)\]',l)]
    r['wm']=re.search(r'wm=([0-9a-f]+)',l).group(1)
    r['cmd']=[x[x.find('direct'):][:60] for x in LOG.splitlines() if 'direct start kind=1' in x or 'direct refused' in x or 'toast' in x]
    return r
def shot(name):
    open(M+name,'wb').write(subprocess.run(S+['exec-out','screencap','-d',BOT,'-p'],capture_output=True).stdout)
def viewed(name):
    a=np.asarray(Image.open(M+name).convert('RGB')).astype(int)
    best=None
    for k,x in enumerate([200,615,1020]):
        m=(a[95:270,x-190:x+190,0]>220)&(a[95:270,x-190:x+190,1]>140)&(a[95:270,x-190:x+190,1]<200)&(a[95:270,x-190:x+190,2]<110)
        v=int(m.sum())
        if best is None or v>best[0]: best=(v,k)
    return best[1]
def highlight(name):
    a=np.asarray(Image.open(M+name).convert('RGB')).astype(int)
    m=(a[:,:,0]>220)&(a[:,:,1]>140)&(a[:,:,1]<200)&(a[:,:,2]<110)
    r=[int(m[Y[k]-110:Y[k]+110,X[k]-150:X[k]+150].sum()) for k in range(8)]
    return [k for k in range(8) if r[k]>1500]
random.seed(int(sys.argv[1]) if len(sys.argv)>1 else 1)
bad=0; n=int(sys.argv[2]) if len(sys.argv)>2 else 30
for it in range(n):
    hv=random.randrange(3); t=random.randrange(8)
    tap(HX[hv],180,90,0.8); tap(155,445,90,0.8)
    sh('logcat','-c')
    tap(X[t],Y[t],90,2.6)
    st=state(); shot('ws_cur.png'); hl=highlight('ws_cur.png')
    vv=viewed('ws_cur.png')
    h=st['h'][vv]
    ok_hw = all(x[3]<0 or x[0]==x[3] for x in st['h'])
    ok_hl = (hl==([h[3]] if h[3]>=0 else []))
    flag='' if (ok_hw and ok_hl) else '  <== MISMATCH'
    if flag: bad+=1; shot('ws_bad_%d.png'%it)
    print(f'{it:02d} wantview={hv} view={vv} tap={t} {st["cmd"]} -> t/r3/r8/held per hero={st["h"]} hl={hl}{flag}',flush=True)
print('bad',bad)
