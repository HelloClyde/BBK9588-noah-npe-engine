import json
from pathlib import Path

rows = json.loads(Path('tmp/native-battle-party-mapping.json').read_text(encoding='utf8'))
assert len(rows) == 4 and all(len(r['commits']) == 4 for r in rows)
out = ['/* Original battle prefix, unchanged map flags and actual entry LCDs. */',
       'static const ScriptEntryCase script_cases[] = {']
for row in rows:
    c, a = row['config'], row['after']
    out.append('{"%s",%u,{80,90,100},{%s},{' % (c['name'], a['party'], ','.join(map(str, a['ids']))))
    for r in a['records']:
        out.append('{' + ','.join(str(r[k]) for k in
                   ('hp', 'maxHp', 'qi', 'maxQi', 'attack', 'defense', 'speed')) +
                   ',{' + ','.join(map(str, r['status'])) + '}},')
    out.append('},{%s},{%s},{%s}},' % (
        ','.join(map(str, c['flags'])), ','.join(map(str, c['order'])),
        ','.join(str(r['target']) for r in a['records'][:a['party']])) )
out += ['};', '']
Path('npe-sdl/tests/battle_party_mapping_native.h').write_text('\n'.join(out), encoding='utf8', newline='\n')
print('4 original party layouts,16 entry LCDs')
