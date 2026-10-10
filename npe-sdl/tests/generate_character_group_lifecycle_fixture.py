import json
from pathlib import Path

rows=json.loads(Path('tmp/native-character-group-lifecycle.json').read_text(encoding='utf8'))
assert len(rows)==3
def arr(v): return '{'+','.join(map(str,v))+'}'
def state(s):
    n=len(s['battle'])
    return '{'+str(n)+','+','.join(arr(s[k]) for k in ['ids','flags','map'])+','+arr(s['battle']+[0]*(3-n))+'}'
out=['/* Original entry/flee/party-remove/reentry RAM batches. */',
     'typedef struct {unsigned count;uint8_t ids[3],flags[3],map[3],battle[3];} GroupState;',
     'typedef struct {const char *name;unsigned remove;GroupState entry,exit,removed,reentry;} GroupLifecycleCase;',
     'static const GroupLifecycleCase group_lifecycle_cases[]={']
for r in rows:
    assert r['after']['battle']==r['after']['map'][:len(r['after']['battle'])]
    assert r['exit']['map']==[1,255,2][:len(r['after']['battle'])]+r['after']['map'][len(r['after']['battle']):]
    assert r['reentry']['battle']==r['reentry']['map'][:len(r['reentry']['battle'])]
    out.append('{"'+r['config']['name']+'",'+str(int(r['removed'] is not None))+','+
               ','.join(state(r[k] or r['exit']) for k in ['after','exit','removed','reentry'])+'},')
out+=['};','']
Path('npe-sdl/tests/character_group_lifecycle_native.h').write_text('\n'.join(out),encoding='utf8')
print('3 original character-group entry/exit/reentry paths and1 party removal')
