import json
import argparse
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('--kind',choices=['chain','return'],default='chain')
kind=parser.parse_args().kind
rows=json.loads(Path(f'tmp/native-npc-move-{kind}.json').read_text(encoding='utf-8'))
out=['/* Original mode18 return/chain commits and retained music slot. */','static const ChainCase chain_cases[]={']
for r in rows:
    a=r['after'];npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in a['npc'])
    frame=Path(f"tmp/native-npc-move-{kind}-{r['music']}-0.idx").read_bytes();background=frame[0]
    assert all(v==background for v in frame[:240*4]+frame[-240*4:])
    plays=[e for e in r['audio'] if e['command']=='play']
    assert all(e['track_id']==22 and e['looping'] for e in plays)
    out.append('{'+','.join(map(str,[r['music'],len(r['commits']),a['mode'],a['event'],a['progress'],len(plays),background]))+',{'+npc+'}},')
out.append('};')
Path(f'npe-sdl/tests/npc_move_{kind}_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
print(len(rows),'original NPE chains',sum(len(r['commits']) for r in rows),'commits')
