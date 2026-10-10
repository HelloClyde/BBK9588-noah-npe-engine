"""Complete native NPC records from original load and periodic services."""
import json
from pathlib import Path

rows = json.loads(Path('tmp/native-npc-record-load.json').read_text())
assert len(rows) == 12
out = ['/* Original UI loads after seeding every resident byte. */',
       'static const NpcLoadCase load_cases[]={']
for r in rows:
    assert len(r['beforeBytes']) == len(r['afterBytes']) == 280
    assert all(r['beforeBytes'][i] == r['afterBytes'][i] for i in range(13,280,14))
    out.append('{'+str(r['targetMap'])+',{'+','.join(map(str,r['beforeBytes']))+'},{'+','.join(map(str,r['afterBytes']))+'}},')
out.append('};')
Path('npe-sdl/tests/npc_resident_load_native.h').write_text('\n'.join(out)+'\n')

rows = json.loads(Path('tmp/native-npc-resident.json').read_text())['rows']
assert len(rows) == 26
fields = ('id','graphic','first','last','direction','x','y','moving')
out = ['/* Original public timer; injected RAM, unchanged NPE. */',
       'static const CompositorCase compositor_cases[]={']
for r in rows:
    out.append('{'+json.dumps(r['name'])+','+str(r['before']['heroY'])+', {')
    for which in ('before','after'):
        for n in r[which]['npc']:
            out.append('{'+','.join(str(n[f]) for f in fields)+'},')
        out.append('},{' if which=='before' else '},')
    for which in ('before','after'):
        assert len(r[which]['bytes']) == 280
        out.append('{'+','.join(map(str,r[which]['bytes']))+'}'+(',' if which=='before' else '},'))
out.append('};')
Path('npe-sdl/tests/npc_resident_native.h').write_text('\n'.join(out)+'\n')
print('12 original load records and26 original periodic cases generated')
