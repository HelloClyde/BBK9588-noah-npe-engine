import json
from pathlib import Path

rows = json.loads(Path('tmp/native-system-slot-return.json').read_text(encoding='utf-8'))
fields = ['mode', 'event', 'map', 'progress', 'sx', 'sy', 'vx', 'vy', 'direction', 'money']

def state(s):
    npc = ','.join('{' + ','.join(str(n[k]) for k in ['id', 'x', 'y', 'direction']) + '}' for n in s['npc'])
    return '{' + ','.join(str(s[k]) for k in fields) + ',{' + npc + '}}'

out = ['/* Real boot, original persistent slot, natural NPC period, system slot UI. */',
       'static const SlotCase slot_cases[]={']
for r in rows:
    phases = ','.join('{' + str(p['mask']) + ',' + str(len(p['commits'])) + ',' + state(p['after']) + '}' for p in r['stages'])
    out.append('{"' + r['name'] + '",' + state(r['before']) + ',' + str(len(r['stages'])) + ',{' + phases + '}},')
out.append('};')
Path('npe-sdl/tests/system_slot_return_native.h').write_text('\n'.join(out) + '\n', encoding='utf-8')
print('System slot fixture:', len(rows), 'cases,', sum(len(p['commits']) for r in rows for p in r['stages']), 'commits')
