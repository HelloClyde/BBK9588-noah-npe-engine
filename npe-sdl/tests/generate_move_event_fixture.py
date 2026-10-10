import json
from pathlib import Path
from npc_move_reference_window import through_map_initialization

def array(values):
    return '{'+','.join(map(str,values))+'}'

rows=json.loads(Path('tmp/native-move-event-load.json').read_text())
assert len(rows)==16
out=['/* Original UI loaders: five NPC and four hero ten-byte slots. */',
     'static const MoveLoadCase move_load_cases[]={']
for r in rows:
    b,a=r['beforeBytes'],r['afterBytes']
    assert len(b)==len(a)==90
    for p in [3+i*10 for i in range(5)]+[50+i*10+p for i in range(4) for p in (1,3)]:
        assert b[p]==a[p]
    out.append('{'+json.dumps(r['entry'])+','+str(r['targetMap'])+','+array(b)+','+array(a)+'},')
out.append('};')
Path('npe-sdl/tests/move_event_load_native.h').write_text('\n'.join(out)+'\n')

rows=[through_map_initialization(r) for r in json.loads(Path('tmp/native-npc-move-resident.json').read_text())]
assert len(rows)==14
out=['/* Original mode18 public path, canonical resident bytes included. */',
     'static const NpcScriptCase npc_script_cases[]={']
for r in rows:
    a=r['after']
    fields=[r[k] for k in ('x','y','targetX','targetY')]+[len(r['commits']),11,a['mode'],a['event'],a['progress'],11]
    npc='{'+','.join(array(n[k] for k in ('id','x','y','direction')) for n in a['npc'])+'}'
    out.append('{'+json.dumps(r['name'])+','+','.join(map(str,fields))+','+npc+','+str(r['inputEvent'])+','+
               array(r['before']['cacheBytes'])+','+array(a['cacheBytes'])+'},')
out.append('};')
Path('npe-sdl/tests/npc_move_resident_native.h').write_text('\n'.join(out)+'\n')
print('NPC',len(rows),'paths',sum(len(r['commits']) for r in rows),'commits')

rows=json.loads(Path('tmp/native-script-walk-resident.json').read_text())
assert len(rows)==9
out=['/* Original mode19 public path and resident bytes. */','static const ScriptCase script_cases[]={']
for i,r in enumerate(rows):
    a=r['after'];frame=Path(f'tmp/native-script-walk-resident-{i}-0.idx').read_bytes();bg=frame[0]
    assert len(frame)==28800 and all(v==bg for v in frame[:960]+frame[-960:])
    fields=[r[k] for k in ('inputEvent','x','y','targetX','targetY','blockedNpc')]+[
        len(r['commits']),a['mode'],a['event'],a['progress'],a['vx'],a['vy'],a['sx'],a['sy'],a['direction'],bg]
    npc='{'+','.join(array(n[k] for k in ('id','x','y','direction')) for n in a['npc'])+'}'
    out.append('{'+json.dumps(r['name'])+','+','.join(map(str,fields))+','+npc+','+
               array(r['before']['cacheBytes'])+','+array(a['cacheBytes'])+'},')
out.append('};')
Path('npe-sdl/tests/script_walk_resident_native.h').write_text('\n'.join(out)+'\n')
print('Hero',len(rows),'paths',sum(len(r['commits']) for r in rows),'commits')
