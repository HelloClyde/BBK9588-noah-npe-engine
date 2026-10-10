"""Export unchanged original mode23/24 state fixtures; C is never the oracle."""
import json
from pathlib import Path

rows = json.loads(Path('tmp/native-simple-events.json').read_text(encoding='utf-8'))
native = json.loads(Path('tmp/simple-events/native/report.json').read_text(encoding='utf-8'))
assert len(rows) == 18 and native['passed'] and len(native['cases']) == 18
fields = ['mode', 'event', 'context', 'map', 'progress', 'money', 'init', 'sx', 'sy', 'vx', 'vy',
          'direction', 'enabled', 'slot', 'level', 'experience', 'threshold', 'attack', 'defense']


def array(values):
    return '{' + ','.join(str(v) + 'u' for v in values) + '}'


def state(s):
    npc = ','.join(array(n[k] for k in ['id', 'x', 'y', 'direction']) for n in s['npc'])
    return ('{' + ','.join(str(s[k]) + 'u' for k in fields) + ',' + array(s['other']) + ',' +
            array(s['npcState']) + ',{' + npc + '}}')


lines = ['/* Original numeric CPU and original UI agree on these18 diagnostic services. */',
         'static const SimpleCase simple_cases[]={']
for row in rows:
    assert len(row['commits']) == 1
    lines.append('{"' + row['name'] + '",' + str(row['mode']) + 'u,' + str(row['event']) + 'u,' +
                 state(row['before']) + ',' + state(row['after']) + '},')
choices = json.loads(Path('tmp/native-simple-events-choice.json').read_text(encoding='utf-8'))
assert len(choices) == 3
lines += ['};', 'static const SimpleCase simple_choices[]={']
for row in choices:
    assert len(row['commits']) == 2 and row['choiceBytes'] == [11,24,0,1,0,60]
    lines.append('{"' + row['name'] + '",' + str(row['mode']) + 'u,' + str(row['event']) + 'u,' +
                 state(row['before']) + ',' + state(row['after']) + '},')
lines += ['};', '']
Path('npe-sdl/tests/simple_events_native.h').write_bytes('\r\n'.join(lines).encode('utf-8'))
print('18 original numeric/UI fixtures,3 actual-NPE cancel choices/6 frames')
