import json
from pathlib import Path
rows=json.loads(Path('tmp/native-exit-state-load.json').read_text(encoding='utf-8'))
assert len(rows)==8
out=['/* Original UI save/load: resident flags only; not LCD/position proof. */',
     'static const struct {unsigned title,choice,slot,mode,event,map,progress;} exit_load_cases[]={']
for r in rows:
    s=r['after']
    assert s['choice']==r['choice'] and s['exitSlot']==r['slot']
    out.append('{'+','.join(map(str,[int(r['entry']=='title'),s['choice'],s['exitSlot'],s['mode'],s['event'],s['map'],s['progress']]))+'},')
out.append('};')
Path('npe-sdl/tests/exit_state_load_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
