"""Builds a host variant with two equip fixes: ring order 1..7 then 0, and a cursor relative to the held weapon.

The repository's som_companion.c ranks owned weapon types in type order, which is wrong whenever the glove (type 0) is
owned (always the case on Ancient Cave, where every weapon type is owned).  This file does not touch the repository
source: it patches a copy under $SOM_COMPANION_STATE/build_ac and compiles it.  The proposed change is in
docs/companion-ancientcave-20261017.md.
"""
import json
import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
import common  # noqa: E402

OLD = """int som_weapon_ring_index(const struct som_state *s, int type) {
  if (type < 0 || type >= SOM_WEAPON_TYPES || s->weapon_slot[type] == 0xFF) return -1;
  int idx = 0;
  for (int t = 0; t < type; t++) if (s->weapon_slot[t] != 0xFF) idx++;
  return idx;
}"""
NEW = """int som_weapon_ring_index(const struct som_state *s, int type) {
  static const uint8_t order[SOM_WEAPON_TYPES] = {1, 2, 3, 4, 5, 6, 7, 0};   /* measured ring order: glove last */
  if (type < 0 || type >= SOM_WEAPON_TYPES || s->weapon_slot[type] == 0xFF) return -1;
  int idx = 0;
  for (int k = 0; k < SOM_WEAPON_TYPES; k++) {
    if (order[k] == type) return idx;
    if (s->weapon_slot[order[k]] != 0xFF) idx++;
  }
  return -1;
}"""
OLD_CURSOR = """    unsigned want = m->sub ? m->cmd.spell : m->want_index;
    if (!s.ring.ready) { if (m->timer > TIMEOUT_STEP) fail(m, SOM_FAIL_TIMEOUT, "cursor step"); break; }"""
NEW_CURSOR = """    unsigned want = m->sub ? m->cmd.spell : m->want_index;
    if (!s.ring.ready) { if (m->timer > TIMEOUT_STEP) fail(m, SOM_FAIL_TIMEOUT, "cursor step"); break; }
    if (m->cmd.kind == SOM_CMD_EQUIP_WEAPON) {
      /* the weapon ring re-centres on the held weapon at the first press: sync with one tap, then cursor + held rank = ring position */
      if (!m->sub) { m->sub = 1; m->taps++; m->timer = 0; m->wait = 22; m->pad = tap(m, SOM_PAD_LEFT); m->hold = TAP_FRAMES - 1; return m->pad; }
      int held = som_weapon_ring_index(&m->before, m->before.hero[m->cmd.hero].weapon_type);
      if (s.ring.count == 0 || held < 0) { fail(m, SOM_FAIL_BAD_ARGUMENT, "held weapon rank unknown"); break; }
      want = (m->want_index + s.ring.count - (unsigned)held) % s.ring.count;
    }"""
OLD_START = """    if (som_weapon_ring_index(&s, c->arg) < 0) { m->result = SOM_FAIL_BAD_ARGUMENT; strcpy(m->note, "weapon not owned"); return 0; }"""
NEW_START = OLD_START + """
    if (s.hero[c->hero].weapon_type == c->arg) { m->result = SOM_DONE; strcpy(m->note, "already held"); m->phase = P_FINISHED; return 1; }"""
DIR = common.STATE / 'build_ac'


def build():
    DIR.mkdir(parents=True, exist_ok=True)
    src = (common.HERE / 'som_companion.c').read_text()
    for old in (OLD, OLD_CURSOR, OLD_START):
        assert old in src, 'som_companion.c changed; update the patch'
    (DIR / 'som_companion_patched.c').write_text(src.replace(OLD, NEW).replace(OLD_CURSOR, NEW_CURSOR).replace(OLD_START, NEW_START))
    out = DIR / 'som_companion_host_patched'
    subprocess.run(['gcc', '-std=gnu99', '-O2', '-I', str(common.HERE), '-o', str(out), str(DIR / 'som_companion_patched.c'),
                    str(common.HERE / 'som_companion_host.c'), '-ldl'], check=True)
    return out


def run(script, rom, name, host=None, timeout=900):
    host = host or build()
    path = common.WORK / ('acp_' + name + '.txt')
    path.write_text(script)
    r = subprocess.run([str(host), str(common.CORE), str(rom), str(path)], capture_output=True, text=True, timeout=timeout)
    if r.returncode:
        raise RuntimeError('host failed (%d): %s' % (r.returncode, r.stderr[-400:]))
    return [json.loads(ln) for ln in r.stdout.splitlines() if ln.startswith('{')]
