"""Reference reader/validator of the start menu configuration (board item T9).

The C start menu (tools/companion/launcher/som_launcher.c) writes this JSON with sl_config_json(); this module re-derives every field
from tools/companion/randomizer/presets.json, so a configuration is accepted only when its option names and values are the generator's own.
Python 3 standard library only.
"""
import hashlib
import json
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
CATALOGUE = HERE.parent / 'randomizer' / 'presets.json'
SCHEMA = 1


def catalogue():
    return json.loads(CATALOGUE.read_text())


def canonical(config):
    """Compact JSON with sorted keys: byte-identical to sl_config_json()."""
    return json.dumps(config, sort_keys=True, separators=(',', ':'), ensure_ascii=True)


def _options(cat, mode):
    m = cat['modes'][mode]
    return {o['key']: o for o in m['options'] + cat['common_options']}


def _visible(o, cat_opts, values):
    for k, allowed in (o.get('requires') or {}).items():
        cur = values.get(k, cat_opts[k]['default'])
        if cur not in allowed:
            return False
    return True


def _legal(o, v):
    t = o['type']
    if t == 'bool':
        return v in ('yes', 'no')
    if t == 'enum':
        return v in [x['v'] for x in o['values']]
    if t == 'multi':
        names = [x['v'] for x in o['values']]
        parts = v.split(',')
        return bool(v) and all(p in names for p in parts) and parts == [n for n in names if n in parts]
    if t == 'num':
        try:
            f = float(v)
        except ValueError:
            return False
        if f < o['min'] or f > o['max'] or v != fmt_num(f):
            return False
        return abs(((f - o['min']) / o['step']) - round((f - o['min']) / o['step'])) < 1e-9
    return False


def fmt_num(f):
    return str(int(f)) if f == int(f) else repr(f)


def validate(config, cat=None):
    """Return a list of problems (empty when the configuration is acceptable)."""
    cat = cat or catalogue()
    errs = []
    if config.get('schema') != SCHEMA:
        errs.append('schema must be %d' % SCHEMA)
    if config.get('generator') != cat['generator']['version']:
        errs.append('generator version %r, this build supports %s' % (config.get('generator'), cat['generator']['version']))
    mode = config.get('mode')
    if mode not in cat['modes']:
        return errs + ['unknown mode %r' % mode]
    seed = config.get('seed', '')
    if not (isinstance(seed, str) and seed.isdigit() and len(seed) <= cat['seed']['max_len'] and str(int(seed)) == seed):
        errs.append('seed must be 1 to %d digits without leading zeros' % cat['seed']['max_len'])
    d = cat['modes'][mode]['difficulty']
    diff = config.get('difficulty')
    if d is None and diff is not None:
        errs.append('mode %s has no difficulty' % mode)
    if d is not None and diff not in d['values']:
        errs.append('difficulty %r not in %s' % (diff, d['values']))
    opts = _options(cat, mode)
    given = config.get('options', {})
    for k, v in given.items():
        o = opts.get(k)
        if o is None:
            errs.append('option %s does not exist in mode %s' % (k, mode))
        elif not isinstance(v, str) or not _legal(o, v):
            errs.append('option %s: illegal value %r' % (k, v))
        elif v == o['default']:
            errs.append('option %s repeats its default' % k)
        elif not _visible(o, opts, given):
            errs.append('option %s given while its requirement is not met' % k)
    if list(given) != sorted(given):
        errs.append('options are not sorted by key')
    for c in cat['modes'][mode]['constraints']:
        if 'any' in c and not any(given.get(k, opts[k]['default']) == 'yes' for k in c['any']):
            errs.append('at least one of %s must be yes' % c['any'])
        if 'le' in c:
            a, b = (int(given.get(k, opts[k]['default'])) for k in c['le'])
            if a > b:
                errs.append('%s must not exceed %s' % tuple(c['le']))
    if config.get('key') != key(config):
        errs.append('key does not match the configuration')
    return errs


def keystring(config):
    return 'somrando/%s|%s|%s|%s|%s' % (config['generator'], config['mode'], config['seed'], config.get('difficulty') or '-',
                                         ';'.join('%s=%s' % (k, v) for k, v in sorted(config.get('options', {}).items())))


def key(config):
    return hashlib.sha256(keystring(config).encode()).hexdigest()[:16]


def cli_args(config):
    a = ['--mode', config['mode'], '--seed', config['seed']]
    if config.get('difficulty'):
        a += ['--difficulty', config['difficulty']]
    for k, v in sorted(config.get('options', {}).items()):
        a += ['--option', '%s=%s' % (k, v)]
    return a


def from_preset(preset_id, seed, overrides=None):
    """Build a configuration the way the menu does: preset values over the generator defaults, only the differences kept."""
    cat = catalogue()
    p = next(x for x in cat['presets'] if x['id'] == preset_id)
    opts = _options(cat, p['mode'])
    values = dict(p['set'])
    values.update(overrides or {})
    given = {k: v for k, v in values.items() if v != opts[k]['default']}
    given = {k: v for k, v in given.items() if _visible(opts[k], opts, given)}
    d = cat['modes'][p['mode']]['difficulty']
    cfg = {'schema': SCHEMA, 'generator': cat['generator']['version'], 'preset': preset_id, 'mode': p['mode'], 'seed': str(int(seed)),
           'difficulty': (p.get('difficulty') or d['default']) if d else None, 'options': dict(sorted(given.items()))}
    cfg['key'] = key(cfg)
    return cfg
