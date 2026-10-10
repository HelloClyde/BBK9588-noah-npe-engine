"""Export original mode22 and unchanged-NPE inn choice references; never use C output."""
import json
from pathlib import Path

fields = ['mode', 'event', 'context', 'map', 'progress', 'money', 'init',
          'sx', 'sy', 'vx', 'vy', 'direction', 'enabled', 'slot']


def array(values):
    return '{' + ','.join(str(v) + 'u' for v in values) + '}'


def state(s):
    npc = ','.join(array(n[k] for k in ['id', 'x', 'y', 'direction']) for n in s['npc'])
    return ('{' + ','.join(str(s[k]) + 'u' for k in fields) + ',' +
            ','.join(array(s[k]) for k in ['maxHp', 'hp', 'maxQi', 'qi']) + ',{' + npc + '}}')


def service(r):
    return ('{"' + r['name'] + '",' + str(r['fee']) + 'u,' + str(len(r['commits'])) +
            'u,' + state(r['before']) + ',' + state(r['after']) + '}')


rows = json.loads(Path('tmp/native-healing-service.json').read_text(encoding='utf-8'))
choices = json.loads(Path('tmp/native-healing-choice-short.json').read_text(encoding='utf-8'))
assert len(rows) == 9 and len(choices) == 3
lines = ['/* Original mode22: isolated numeric boundaries and actual NPE inn choices. */',
         'static const HealingCase healing_cases[]={']
lines += [service(r) + ',' for r in rows]
lines += ['};', 'static const HealingChoiceCase healing_choices[]={']
for r in choices:
    phases = ','.join('{' + str(p['key']) + 'u,' + str(p['commits']) + 'u,' +
                      state(p['after']) + '}' for p in r['phases'])
    lines.append('{' + service(r) + ',' + str(len(r['phases'])) + 'u,{' + phases + '},' +
                 array(r['choiceBytes']) + '},')
lines += ['};', '']
Path('npe-sdl/tests/healing_service_native.h').write_bytes('\r\n'.join(lines).encode('utf-8'))
print('9 direct healing paths,3 unchanged-NPE choice chains,46 original commits')
