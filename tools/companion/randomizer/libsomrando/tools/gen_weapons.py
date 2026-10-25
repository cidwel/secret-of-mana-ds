import re,sys
src=open("/home/cidwel/.local/state/som-decomp/companion-cli-20261017/repo-moppu/SecretOfManaRandomizer/SoMRandomizer/processing/hacks/common/enhancement/WeaponRandomizer.cs",encoding="utf-8-sig").read()
lines=src.split("\n")
pals=re.findall(r'WeaponPaletteSet (\w+) = new WeaponPaletteSet\((0x[0-9A-Fa-f]+), (\d+)\);',"\n".join(lines[40:95]))
out=[]
out.append("/* Generated from WeaponRandomizer.cs (Moppleton) by gen_weapons.py: palette and weapon name tables. */")
out.append("struct sr_w1_pal { uint8_t weapon_pal, menu_pal; };")
for n,w,m in pals:
    out.append("static const struct sr_w1_pal sr_w1_pal_%s = {%s, %s};"%(n,w,m))
conds=["none","stun","freeze","engulf","poison","confuse","stone","none_golden","none_red","none_holy","none_black","none_dragon","none_crit","none_hit","none_supercrit","none_agiPlus","none_conPlus","none_intPlus","none_strPlus","none_Garbage"]
out.append("enum { "+", ".join("SR_W1_C_"+c.upper() for c in conds)+", SR_W1_C_MANASTR, SR_W1_C_COUNT };")
out.append("struct sr_w1_spec { const char *name; const uint8_t *icons; int n_icons; int cond; const char *desc; const struct sr_w1_pal *pal; };")
out.append("struct sr_w1_cat { const char *name; const uint8_t *menu_icons; int n_menu_icons; const struct sr_w1_spec *specs; int n_specs; const uint8_t *proj_keys; const uint8_t *proj_vals; int n_proj; };")
body="\n".join(lines[440:870])
spec_re=re.compile(r'(\w+)\.addWeaponSpec\(new WeaponSpec\("((?:[^"\\]|\\.)*)", (null|new byte\[\] \{ ([\d, ]+) \}), (\w+)(?:, "((?:[^"\\]|\\.)*)")?(?:, (\w+))?\)\);')
cats={}
order=[]
for m in re.finditer(r'WeaponCategory (\w+) = new WeaponCategory\(\);',body):
    cats[m.group(1)]={"specs":[],"proj":[]}; order.append(m.group(1))
cats["sharedSpecialNames"]={"specs":[],"proj":[]}
nspec=0
for m in spec_re.finditer(body):
    v,name,ic,icl,cond,desc,pal=m.groups()
    assert v in cats,v
    assert "\\" not in name and (desc is None or "\\" not in desc)
    cats[v]["specs"].append((name,[int(x) for x in icl.split(",")] if icl else None,cond,desc or "",pal)); nspec+=1
assert nspec==len(re.findall(r'addWeaponSpec',body)),(nspec,len(re.findall(r'addWeaponSpec',body)))
for m in re.finditer(r'(\w+)\.allMenuIcons = new byte\[\] \{ ([\d, ]+) \};',body): cats[m.group(1)]["menu"]=[int(x) for x in m.group(2).split(",")]
for m in re.finditer(r'(\w+)\.projectileImages\.Add\((\d+), (\d+)\);',body): cats[m.group(1)]["proj"].append((int(m.group(2)),int(m.group(3))))
for m in re.finditer(r'(\w+)\.name = "(\w+)";',body): cats[m.group(1)]["name"]=m.group(2)
def cname(c): return "SR_W1_C_"+c.upper()
for v in order+["sharedSpecialNames"]:
    c=cats[v]
    for i,(name,ic,cond,desc,pal) in enumerate(c["specs"]):
        if ic: out.append("static const uint8_t sr_w1_ic_%s_%d[] = {%s};"%(v,i,", ".join(map(str,ic))))
    out.append("static const struct sr_w1_spec sr_w1_specs_%s[] = {"%v)
    for i,(name,ic,cond,desc,pal) in enumerate(c["specs"]):
        out.append('  {"%s", %s, %d, %s, "%s", %s},'%(name,"sr_w1_ic_%s_%d"%(v,i) if ic else "NULL",len(ic) if ic else 0,cname(cond),desc,"&sr_w1_pal_"+pal if pal else "NULL"))
    out.append("};")
    if v!="sharedSpecialNames":
        out.append("static const uint8_t sr_w1_menu_%s[] = {%s};"%(v,", ".join(map(str,c["menu"]))))
        out.append("static const uint8_t sr_w1_pk_%s[] = {%s};"%(v,", ".join(str(k) for k,_ in c["proj"])))
        out.append("static const uint8_t sr_w1_pv_%s[] = {%s};"%(v,", ".join(str(x) for _,x in c["proj"])))
out.append("#define SR_W1_N(a) ((int)(sizeof(a) / sizeof((a)[0])))")
out.append("static const struct sr_w1_cat sr_w1_cats[8] = {")
for v in order:
    c=cats[v]
    out.append('  {"%s", sr_w1_menu_%s, SR_W1_N(sr_w1_menu_%s), sr_w1_specs_%s, SR_W1_N(sr_w1_specs_%s), sr_w1_pk_%s, sr_w1_pv_%s, SR_W1_N(sr_w1_pk_%s)},'%((c["name"],)+(v,)*7))
out.append("};")
out.append("static const struct sr_w1_spec *const sr_w1_shared_specs = sr_w1_specs_sharedSpecialNames;")
out.append("#define SR_W1_N_SHARED SR_W1_N(sr_w1_specs_sharedSpecialNames)")
print(order, file=sys.stderr)
open(sys.argv[1],"w").write("\n".join(out)+"\n")
