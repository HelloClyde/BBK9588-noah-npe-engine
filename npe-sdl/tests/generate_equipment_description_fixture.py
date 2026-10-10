import json
from pathlib import Path
rows=json.loads(Path('tmp/native-equipment-description-service.json').read_text(encoding='utf-8'))
fields=['mode','event','map','progress','sx','sy','vx','vy','direction','money','enabled','slot','context','menu','member','equipmentSlot','gearSelected','descLength','descStart','descEnd','itemLoaded','skillLoaded']
def arr(values):return '{'+','.join(map(str,values))+'}'
def state(s):
    npc=arr(arr(n[k] for k in ['id','x','y','direction']) for n in s['npc'])
    return '{'+','.join(str(s[k]) for k in fields)+','+npc+','+arr(s['itemBytes'])+','+arr(s['skillBytes'])+'}'
out=['/* Original equipment entry and live description-page input; no production engine instrumentation. */',
     'typedef struct {unsigned id,x,y,direction;} MenuNpc;',
     'typedef struct {unsigned '+','.join(fields)+';MenuNpc npc[20];uint8_t itemBytes[142],skillBytes[144];} MenuState;',
     'typedef struct {unsigned mask,commits,patch[3];MenuState before,after;} MenuPhase;',
     'typedef struct {const char *name;unsigned party_count;uint8_t gear[3][6];MenuState before;uint32_t rng_before,rng_after;unsigned phases;MenuPhase phase[12];} MenuCase;',
     'static const MenuCase menu_cases[]={']
for r in rows:
    phases=[]
    for p in r['phases']:
        assert len(p['commits'])<=1
        if p['commits']:assert p['commits'][0]['state']==p['after']
        patch=arr(p.get('patch',{}).get(k,4294967295) for k in ['length','start','end'])
        phases.append('{'+str(p['mask'])+','+str(len(p['commits']))+','+patch+','+state(p['before'])+','+state(p['after'])+'}')
    out.append('{"'+r['name']+'",1,'+arr(arr(g) for g in r['gear'])+','+state(r['before'])+',11u,11u,'+str(len(phases))+','+arr(phases)+'},')
out+=['};',''];Path('npe-sdl/tests/equipment_description_native.h').write_text('\n'.join(out),encoding='utf-8')
print(len(rows),'cases,',sum(len(p['commits']) for r in rows for p in r['phases']),'commits,',sum(not p['commits'] for r in rows for p in r['phases']),'stops')
