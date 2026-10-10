from pathlib import Path
import json
import sys
invalid='--invalid-followup' in sys.argv
prefix='native-dialogue-invalid' if invalid else 'native-dialogue-return'
rows=json.loads(Path(f'tmp/{prefix}.json').read_text())
out=['/* Original ordinary NPC and full/window story returns, music off/on. */','static const ReturnCase return_cases[]={']
for i,r in enumerate(rows):
    b,a=r['before'],r['after']; plays=[e for e in r['audio'] if e['command']=='play']
    assert all(e['track_id']==22 and e['looping'] for e in plays)
    bg=Path(f'tmp/{prefix}-{i}-before.idx').read_bytes()[0]
    fields=[r[k] for k in ['npc','map','progress','music']]+[len(r['commits']),a['mode'],a['event'],a['context'],len(plays),bg,b['vx'],b['vy'],b['sx'],b['sy'],b['direction']]
    npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in b['npc'])
    extra=','+','.join(str(r[k]) for k in ['patchedMode','patchedEvent','targetMode']) if invalid else ''
    out.append('{"'+r['name']+'",'+','.join(map(str,fields))+',{'+npc+'},{'+','.join(map(str,b['npcState']))+'},{'+','.join(map(str,a['npcState']))+'}'+extra+'},')
out.append('};')
Path('npe-sdl/tests/'+('dialogue_invalid_native.h' if invalid else 'dialogue_return_native.h')).write_text('\n'.join(out)+'\n')
print(len(rows),sum(len(r['commits']) for r in rows),'original dialogue return commits')
