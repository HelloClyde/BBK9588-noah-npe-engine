import json
from pathlib import Path

rows = json.loads(Path('tmp/native-scripted-entry.json').read_text(encoding='utf-8'))
out = ['/* Original map-mode to scripted entry, actual LCD commits. */',
       'static const ScriptEntryCase script_cases[] = {']
for row in rows:
    c, a = row['config'], row['after']
    out.append('{"' + c['name'] + '",' + str(len(c['hps'])) +
               ',{' + ','.join(map(str, c['hps'])) + '},{' +
               ','.join(map(str, a['ids'])) + '},{')
    for r in a['records']:
        out.append('{' + ','.join(str(r[k]) for k in
                   ('hp', 'maxHp', 'qi', 'maxQi', 'attack', 'defense', 'speed')) +
                   ',{' + ','.join(map(str, r['status'])) + '}},')
    out.append('}},')
out.append('};')
Path('npe-sdl/tests/scripted_battle_entry_native.h').write_text(
    '\n'.join(out) + '\n', encoding='utf-8', newline='\n')
