import re,sys
# Generates src/sr_p4_shared_boss_all.h: allSupportedBosses of every VanillaBossMap (BossSwaps open world 'random' mode).  Usage: gen_p4_shared_boss.py OUT
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
out.append('/* Generated from VanillaBossMaps.cs (Author: Moppleton) by tools/gen_p4_shared_boss.py: allSupportedBosses per map, indexed by BM_*. */')
for n in order:
    out.append('static const uint8_t sr_p4_all_%s[] = {%s};'%(n,','.join(map(str,bytelist(maps[n][4])))))
out.append('static const struct { int n; const uint8_t *p; } sr_p4_all_bosses[] = {')
for n in order:
    out.append('  {%d, sr_p4_all_%s},'%(len(bytelist(maps[n][4])),n))
out.append('};')
open(sys.argv[1],'w').write('\n'.join(out)+'\n')
