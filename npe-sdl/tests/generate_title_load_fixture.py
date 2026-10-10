import json
from pathlib import Path
j=json.loads(Path('tmp/native-title-reload.json').read_text(encoding='utf-8'))
s=next(r for r in j['stages'] if r['label']=='load')['commits'][0]['state']
assert (s['sx'],s['sy'],s['vx'],s['vy'])==(j['saved']['sx'],j['saved']['sy'],j['saved']['vx'],j['saved']['vy'])
out=['/* Real title boot and persistent original save. Host NPC diagnostic seed; uses observed later periodic positions; only first load display compared. */',
 'static const unsigned title_load_position[]={'+','.join(str(s[x])for x in ['mode','event','map','progress','sx','sy','vx','vy','direction','money'])+'};']
for name,rows in [('saved',j['after']['npc']),('loaded',s['npc'])]:
 out.append('static const unsigned title_load_'+name+'_npc[20][4]={'+','.join('{'+','.join(str(r[x])for x in ['id','x','y','direction'])+'}'for r in rows)+'};')
Path('npe-sdl/tests/title_load_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
