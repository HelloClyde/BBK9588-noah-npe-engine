from pathlib import Path
import json
from npc_move_reference_window import through_map_initialization

rows=json.loads(Path('tmp/native-npc-script-move.json').read_text(encoding='utf-8'))
rows=[through_map_initialization(row) for row in rows]
lines=['/* Original mode18 runtime samples; generated, no reference pixels in C. */',
       'static const NpcScriptCase npc_script_cases[]={']
for row in rows:
    before,after=row['before'],row['after']
    values=[row[k] for k in ['x','y','targetX','targetY']]
    values += [len(row['commits']),11,after['mode'],after['event'],after['progress'],11]
    npcs=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in after['npc'])
    lines.append('{"'+row['name']+'",'+','.join(map(str,values))+',{'+npcs+'}},')
lines+=['};']
Path('npe-sdl/tests/npc_script_move_native.h').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('Generated',len(rows),'NPC script cases')
