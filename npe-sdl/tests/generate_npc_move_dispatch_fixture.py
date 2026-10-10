import json
from pathlib import Path
from npc_move_reference_window import through_map_initialization
rows=json.loads(Path('tmp/native-npc-move-dispatch.json').read_text(encoding='utf-8'))
rows=[through_map_initialization(row) for row in rows]
out=['/* Original dispatch/geometry results, diagnostic resident slots. */','static const NpcScriptCase npc_script_cases[]={']
for r in rows:
    a=r['after']
    values=[r[k] for k in ['x','y','targetX','targetY']]+[len(r['commits']),11,a['mode'],a['event'],a['progress']]
    values += [r[k] for k in ['inputEvent','nextMode','nextId']]+[11]
    npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in a['npc'])
    out.append('{"'+r['name']+'",'+','.join(map(str,values))+',{'+npc+'}},')
out.append('};')
Path('npe-sdl/tests/npc_move_dispatch_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
print(len(rows),'original dispatch cases',sum(len(r['commits']) for r in rows),'commits')
