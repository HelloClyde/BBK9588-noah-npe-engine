from pathlib import Path
import json

report = json.loads(Path('tmp/native-npc-periodic.json').read_text(encoding='utf-8'))

def npc(record):
    fields = ['id', 'graphic', 'first', 'last', 'direction', 'x', 'y', 'moving']
    return '{' + ','.join(str(record[key]) for key in fields) + '}'

def positions(rows):
    return '{' + ','.join('{' + ','.join(map(str, row)) + '}' for row in rows) + '}'

lines = ['/* Original firmware periodic fixtures, generated from native-npc-periodic.json. */',
         'static const NpcCase npc_cases[]={']
for case in report['rows']:
    lines.append('{"' + case['name'] + '",UINT32_C(' + str(case['seed']) +
                 '),UINT32_C(' + str(case['rngAfter']) + '),' +
                 '{' + ','.join(map(npc, case['before']['npc'])) + '},' +
                 '{' + ','.join(map(npc, case['after']['npc'])) + '},' +
                 positions(case['before']['objects']) + ',' +
                 positions(case['before']['exits']) + '},')
lines.append('};')
Path('npe-sdl/tests/npc_periodic_native.h').write_text('\n'.join(lines) + '\n',
                                                       encoding='utf-8', newline='\n')
print('Generated', len(report['rows']), 'native periodic cases')
