import json
from pathlib import Path

rows = json.loads(Path('tmp/npc-condition-restart-native/report.json').read_text(encoding='utf-8'))['rows']
for row in json.loads(Path('tmp/npc-condition-title-boot-native/report.json').read_text(encoding='utf-8')):
    rows.append(dict(row, route='title-load'))
keys = ['guardIndex', 'guardWordIndex', 'guardA', 'guardB', 'failureMode', 'failureEvent', 'directory']
lines = ['/* Original runtime observations; no C output supplies expected values. */',
         'static const NpcLifetimeCase npc_lifetime_cases[]={']
for row in rows:
    states = []
    for phase in ('before', 'after'):
        state = row[phase]
        values = [str(state[k]) for k in keys]
        values += ['{' + ','.join(map(str, state[k])) + '}' for k in ('npc', 'conditions')]
        states.append('{' + ','.join(values) + '}')
    lines.append('{"%s",%d,%s,%s},' % (row['route'], row['salt'], *states))
lines.append('};')
Path('npe-sdl/tests/npc_condition_lifetime_native.h').write_bytes(('\r\n'.join(lines)+'\r\n').encode())
print('Original NPC condition lifetime cases:', len(rows))
