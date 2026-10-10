import json
import sys
from pathlib import Path

resources='--resources' in sys.argv
rows = json.loads(Path('tmp/native-battle-status-resources.json' if resources else 'tmp/native-battle-status-view.json').read_text())['rows']
assert len(rows) == 4
def array(value):
    if isinstance(value, list):
        return '{' + ','.join(array(v) for v in value) + '}'
    return str(value)

out = ['/* Generated from original public controller input; do not edit. */',
       'static const StatusCase status_cases[] = {']
for row in rows:
    for step in row['steps']:
        assert step['after']['map'] == row['before']['map']
        assert step['after']['battle'] == row['before']['battle']
        assert step['after']['actor'] == row['config']['actor']
    steps = [[s['key'], len(s['frames']), s['after']['phase'],
              s['after']['member'], s['after']['row']] for s in row['steps']]
    if resources:
        for step,sample in zip(steps,row['steps']):step.append(sample['after']['resourceFlags'])
    c = row['config']
    out.append('{' + json.dumps(c['name']) + ',' +
               ','.join(str(c[k]) for k in ['party', 'actor', 'member']) + ',' +
               str(row['before']['money']) + ',' +
               (str(row['before']['resourceFlags'])+',' if resources else '') +
               array(row['entry']['map'] + row['entry']['battle']) + ',' +
               array(row['before']['map'] + row['before']['battle']) + ',' +
               array(steps) + '},')
out.append('};')
Path('npe-sdl/tests/battle_status_resources_native.h' if resources else 'npe-sdl/tests/battle_status_view_native.h').write_text('\n'.join(out)+'\n')
print('four original status paths,', sum(len(s['frames']) for r in rows for s in r['steps']),
      'LCD commits and 48 input steps')

load_path=Path('tmp/native-battle-status-member-load.json')
if load_path.exists():
    loads=json.loads(load_path.read_text())
    assert [r['entry'] for r in loads]==['system','title','new','empty']
    assert all(r['beforeBytes']==r['afterBytes']==[129,166] for r in loads)
    header=['/* Four original warm constructors; full retained bytes. */',
            'static const MenuRowsLoad menu_rows_load[]={']
    header += ['{'+json.dumps(r['entry'])+','+array(r['beforeBytes'])+','+
               array(r['afterBytes'])+'},' for r in loads]
    header.append('};')
    Path('npe-sdl/tests/battle_status_member_load_native.h').write_text('\n'.join(header)+'\n')
