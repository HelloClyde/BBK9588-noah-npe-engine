import json
import sys
from pathlib import Path

exit_full='--exit-full' in sys.argv[1:]
viewport='--viewport' in sys.argv[1:]
actors='--actors' in sys.argv[1:]
continuation='--continuation' in sys.argv[1:]
stem='native-walk-continuation' if continuation else ('native-walk-actors' if actors else ('native-walk-viewport' if viewport else ('native-walk-exit' if exit_full else 'native-walk')))
stems=[stem,'native-walk-edge'] if viewport else [stem]
rows=[(r,s,i) for s in stems for i,r in enumerate(json.loads(Path('tmp/'+s+'.json').read_text(encoding='utf-8'))['rows'])]
out=['/* Original retained LCD walking attempts, including all commits. */',
     'static const WalkCase walk_cases[] = {']
for row,source,index in rows:
    b=row['before'];a=row['after']
    values=[row['mask'],len(row['commits']),b['map'],b['progress'],
            b['screenX'],b['screenY'],b['viewX'],b['viewY'],a['direction'],
            a['map'],a['progress'],a['mode'],a['screenX'],a['screenY'],a['viewX'],a['viewY']]
    out.append('    {"'+row['name']+'","tmp/'+source+'-'+str(index)+'",'+','.join(map(str,values))+', {')
    for n in b['npc']:
        out.append('        {'+','.join(str(n[k]) for k in
          ('id','graphic','first','last','direction','x','y','moving'))+'},')
    if actors:
        out.append('    }, {')
        for n in b['monsters']:
            out.append('        {'+','.join(str(n[k]) for k in
              ('enemy','npc','graphic','first','last','x','y','left','right'))+'},')
    if continuation:
        out.append('    },'+','.join(str(v) for v in (b['encounters'],b['encounterSteps'],a['encounterSteps'],b['rng'],a['rng']))+', {')
        for n in b['chase']:
            out.append('        {'+','.join(str(n[k]) for k in ('graphic','first','last','enemy','x','y','cooldown'))+'},')
        out.append('    }, {')
        for n in a['chase']:
            out.append('        {'+','.join(str(n[k]) for k in ('graphic','first','last','enemy','x','y','cooldown'))+'},')
    out.append('    }},')
out.append('};')
name='walk_continuation_native.h' if continuation else ('walk_actor_native.h' if actors else ('walk_viewport_native.h' if viewport else ('walk_exit_native.h' if exit_full else 'walk_native.h')))
Path('npe-sdl/tests/'+name).write_text('\n'.join(out)+'\n',encoding='utf-8',newline='\n')
