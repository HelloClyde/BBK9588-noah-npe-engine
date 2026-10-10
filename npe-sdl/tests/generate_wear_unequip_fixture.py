"""Serialize original-interpreter wear-page observations, without C expectations."""
import json
from pathlib import Path

rows = json.loads(Path('tmp/native-wear-unequip.json').read_text(encoding='utf-8'))
fields = ['mode','event','map','progress','sx','sy','vx','vy','direction','money',
          'enabled','slot','context','menu','member','equipmentSlot','gearSelected',
          'descLength','descStart','descEnd','itemLoaded','skillLoaded']
def arr(values): return '{' + ','.join(map(str, values)) + '}'
def state(s):
    return arr([*(s[k] for k in fields),
                arr(arr(n[k] for k in ['id','x','y','direction']) for n in s['npc']),
                arr(s['itemBytes']),arr(s['skillBytes']),arr(s['roles']),arr(s['inventory'])])
out = ['/* Actual original wear-page U/u input, diagnostic resident initial states. */',
       'typedef struct {unsigned id,x,y,direction;} MenuNpc;',
       'typedef struct {unsigned '+','.join(fields)+';MenuNpc npc[20];uint8_t itemBytes[142],skillBytes[144],roles[444];uint16_t inventory[255];} MenuState;',
       'typedef struct {unsigned mask,commits,selected;MenuState after;} MenuPhase;',
       'typedef struct {const char *name;unsigned party_count;MenuState before;unsigned phases;MenuPhase phase[32];} MenuCase;',
       'static const MenuCase menu_cases[]={']
for r in rows:
    phases=[]
    for p in r['phases']:
        assert len(p['commits']) <= 1, (r['name'],p['mask'])
        if p['commits']: assert p['commits'][0]['state'] == p['after']
        phases.append(arr([p['mask'],len(p['commits']),p.get('selected','UINT_MAX'),state(p['after'])]))
    out.append(arr(['"'+r['name']+'"',r.get('partyCount',1),state(r['before']),len(phases),arr(phases)])+',')
out += ['};','']
Path('npe-sdl/tests/wear_unequip_native.h').write_text('\n'.join(out),encoding='utf-8')
print(len(rows),'cases,',sum(len(p['commits']) for r in rows for p in r['phases']),
      'commits,',sum(not p['commits'] for r in rows for p in r['phases']),'zero-commit inputs')
