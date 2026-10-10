import json
from pathlib import Path

data = json.loads(Path('tmp/native-walk-trigger-resident.json').read_text())
native = json.loads(Path('tmp/walk-trigger/native/report.json').read_text())
assert native['passed'] and len(native['cases']) == len(data['services']) == 10
assert len(data['loads']) == 8 and len(data['walks']) == 6
def array(values): return '{'+','.join(map(str,values))+'}'
def state(q):
    keys = ['mode','event','context','map','progress','init','sx','sy','vx','vy','direction','enabled','slot','menuPartyCount','menuStage','menuRow','menuMember']
    return '{'+','.join(str(q[k]) for k in keys)+','+','.join(array(q[k]) for k in ['cache','npc','monster','chase','viewCache'])+'}'
lines = ['/* Generated from unchanged original UI and numeric services. */',
         'static const TriggerLoad trigger_loads[]={']
for q in data['loads']:
    lines.append('{"%s",%d,%s,%s},' % (q['entry'],q['map'],array(q['before']),array(q['after'])))
lines += ['};','static const TriggerCase trigger_services[]={']
for q,n in zip(data['services'],native['cases']):
    assert q['name'] == n['name'] and len(q['commits']) == 1
    lines.append('{"%s",%d,%d,%d,%s,%s},' % (q['name'],n['mode'],n['event'],len(q['commits']),state(q['before']),state(q['after'])))
lines += ['};','static const TriggerCase trigger_walks[]={']
for q in data['walks']:
    lines.append('{"%s",0,0,%d,%s,%s},' % (q['name'],len(q['commits']),state(q['before']),state(q['after'])))
lines += ['};','']
Path('npe-sdl/tests/walk_trigger_native.h').write_text('\n'.join(lines))
print('Generated 8 original loads,10 original numeric/UI modes and6 walking paths')
