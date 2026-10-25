import json, sys
import harness
c = harness.core()
survey = json.load(open(harness.SURVEY))
res = {}
for m in sorted(int(k) for k, v in survey.items() if v.get('ok')):
    try:
        harness.load_map(c, m)
        c.run(30)
        a = harness.actors(c)
        res[m] = [{k: v for k, v in x.items() if k != 'raw'} for x in a.values() if x['id'] < 0x80 and x['hpmax'] > 0]
    except Exception as e:
        res[m] = 'ERR %s' % e
    print(m, [(x['slot'], x['id'], x['sx'], x['sy']) for x in res[m]] if isinstance(res[m], list) else res[m], flush=True)
    json.dump(res, open(harness.STATE / 'survey_enemies.json', 'w'))
