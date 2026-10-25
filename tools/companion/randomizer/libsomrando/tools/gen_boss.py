import re,sys
base='/home/cidwel/.local/state/som-decomp/companion-cli-20261017/repo-moppu/SecretOfManaRandomizer/SoMRandomizer/processing/common/'
sv=open(base+'SomVanillaValues.cs').read()
consts={}
for m in re.finditer(r'public const (?:byte|int) (\w+) = (\d+);',sv): consts[m.group(1)]=int(m.group(2))
src=open(base+'VanillaBossMaps.cs').read()
def strip(s):
    s=re.sub(r'/\*.*?\*/','',s,flags=re.S)
    s=re.sub(r'//[^\n]*','',s)
    return s
blocks=re.split(r'public static VanillaBossMap (\w+) = new VanillaBossMap',src)
maps={}
for i in range(1,len(blocks),2):
    name=blocks[i]; body=blocks[i+1].split('// post-processing')[0]
    body=strip(body)
    body=body.replace('SomVanillaValues.','')
    # split top-level args
    s=body.strip()
    assert s.startswith('(')
    s=s[1:]
    args=[];depth=0;cur=''
    for ch in s:
        if ch in '({[': depth+=1
        if ch in ')}]': depth-=1
        if depth<0: break
        if ch==',' and depth==0: args.append(cur.strip());cur=''
        else: cur+=ch
    if cur.strip(): args.append(cur.strip())
    maps[name]=args
def ev(e):
    e=e.strip()
    for k,v in sorted(consts.items(),key=lambda x:-len(x[0])): e=re.sub(r'\b'+k+r'\b',str(v),e)
    return eval(e)
def bytelist(a):
    a=a.strip()
    if a=='null': return None
    inner=re.search(r'\{(.*)\}',a,re.S).group(1)
    return [ev(x) for x in inner.split(',') if x.strip()]
def xy(a):
    a=a.strip()
    if a=='null': return None
    m=re.match(r'new XyPos\((.*)\)$',a,re.S); x,y=m.group(1).split(',')
    return (ev(x),ev(y))
def xylist(a):
    a=a.strip()
    if a=='null': return None
    inner=re.search(r'\{(.*)\}\.ToList',a,re.S).group(1)
    return [xy(p) for p in re.findall(r'new XyPos\([^)]*\)',inner)]
order=['MANTISANT','WALLFACE','TROPICALLO','MINOTAUR','SPIKEY','JABBER','SPRINGBEAK','FROSTGIGAS','SNAPDRAGON','MECHRIDER1','DOOMSWALL','VAMPIRE','METALMANTIS','MECHRIDER2','KILROY','GORGON','BOREAL','GREATVIPER','BLUESPIKE','HYDRA','AEGRTOJNFSDGHOPALNIAPLEIAN','HEXAS','KETTLEKIN','TONPOLE','MECHRIDER3','SNOWDRAGON','FIREGIGAS','REDDRAGON','AXEBEAK','BLUEDRAGON','BUFFY','DARKLICH','DRAGONWORM','THUNDERGIGAS']
out=[]
out.append('/* Generated from VanillaBossMaps.cs (author Moppleton) by a throw-away script: the per-map data BossSwaps reads. */')
for i,n in enumerate(order): out.append('#define BM_%s %d'%(n,i))
out.append('/* Generated from VanillaBossMaps.cs by gen_boss.py: per-map data used by BossSwaps (swap mode). */')
out.append('struct sr_w6_bmap {\n  const char *name;\n  int map_num, obj_num;\n  int n_swap; const uint8_t *swap;\n  int sp_c[2], sp_l[2], sp_r[2], beak_c[2];\n  int n_jump; const int (*jump)[2];\n  int n_pod; const int (*pod)[2];\n  int n_trop; const int (*trop)[2];\n  int n_bram; const int (*bram)[2];\n  int wall_y;\n};')
for n in order:
    a=maps[n]
    assert len(a)==19, (n,len(a))
    sw=bytelist(a[3])
    out.append('static const uint8_t sw_%s[] = {%s};'%(n,','.join(map(str,sw))))
    for tag,idx in (('jump',9),('pod',10),('trop',11),('bram',12)):
        l=xylist(a[idx]) or [(0,0)]; out.append('static const int %s_%s[][2] = {%s};'%(tag,n,','.join('{%d,%d}'%p for p in l)))
out.append('static const struct sr_w6_bmap sr_w6_bmaps[] = {')
for n in order:
    a=maps[n]; nm=eval(a[0]); 
    sc,sl,sr_=[xy(a[i]) or (0,0) for i in (5,6,7)]; bc=xy(a[8]) or (0,0)
    sw=bytelist(a[3]); j=xylist(a[9-0+0]) if False else None
    jl=xylist(a[9]) if False else None
    out.append('  {"%s", %d, %d, %d, sw_%s, {%d,%d}, {%d,%d}, {%d,%d}, {%d,%d}, %d, jump_%s, %d, pod_%s, %d, trop_%s, %d, bram_%s, %d},'%(nm,ev(a[1]),ev(a[2]),len(sw),n,*sc,*sl,*sr_,*bc,len(xylist(a[9]) or []),n,len(xylist(a[10]) or []),n,len(xylist(a[11]) or []),n,len(xylist(a[12]) or []),n,ev(a[13])))
out.append('};')
open(sys.argv[1],'w').write('\n'.join(out)+'\n')
print([ (n,len(maps[n])) for n in order][:3], [ (i,x[:40]) for i,x in enumerate(maps['MANTISANT'])])
