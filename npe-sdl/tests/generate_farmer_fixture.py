import json
from pathlib import Path
data=json.loads(Path('tmp/native-farmer-route.json').read_text(encoding='utf-8'))
def state(s):
    return ','.join(str(s[k]) for k in ['mode','map','progress','sx','sy','vx','vy','direction'])
out=['/* Original exit-loaded map81 viewport and subsequent actual key frames. */',
     'static const unsigned farmer_entry[8]={'+state(data['entry'])+'};',
     'static const unsigned farmer_steps[][10]={']
for s in data['steps']:
    out.append('{%d,%d,%s},'%(s['mask'],len(s['commits']),state(s['after'])))
out.append('};')
Path('npe-sdl/tests/farmer_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
