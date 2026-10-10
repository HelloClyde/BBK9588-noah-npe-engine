import json
from pathlib import Path

rows=json.loads(Path('tmp/native-map-group.json').read_text(encoding='utf8'))['rows']
assert len(rows)==6
fields=('id','graphic','first','last','direction','x','y','moving')
out=['/* Original public periodic LCD with diagnostic resident NPC base. */',
     'static const CompositorCase compositor_cases[]={']
for row in rows:
    assert row['before']['npc_base']==row['after']['npc_base']
    assert row['before']['npc']==row['after']['npc']
    out.append('{'+json.dumps(row['name'])+','+str(row['before']['heroY'])+', {')
    for which in ('before','after'):
        for n in row[which]['npc']:out.append('{'+','.join(str(n[f]) for f in fields)+'},')
        out.append('},{' if which=='before' else '},'+str(row['before']['npc_base'])+'},')
out.append('};')
Path('npe-sdl/tests/map_group_native.h').write_text('\n'.join(out)+'\n',encoding='utf8')
print('Validated6 original public live-NPC-base cases')
