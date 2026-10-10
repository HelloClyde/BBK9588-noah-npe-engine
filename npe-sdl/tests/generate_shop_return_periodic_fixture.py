import json
from pathlib import Path

rows=json.loads(Path('tmp/native-shop-return-periodic.json').read_text(encoding='utf-8'))
fields=['mode','event','context','map','progress','sx','sy','vx','vy','direction','money','enabled','slot','ui','selected','first','last','row','quantity','choice','descStart','descEnd','rng']
def state(s):
    npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in s['npc'])
    return '{'+','.join(str(s[k])+('u' if k=='rng' else '') for k in fields)+',{'+','.join(map(str,s['stocks'][:255]))+'},{'+npc+'}}'
out=['/* Original shop service with controlled mode/context entry; original NPC/color/RNG retained. */','static const ShopReturnCase shop_return_cases[]={']
for r in rows:
    phases=','.join('{'+str(s['mask'])+','+str(int(s.get('period',False)))+','+str(len(s['commits']))+','+state(s['after'])+'}' for s in r['stages'])
    out.append('{"'+r['name']+'",'+str(int(r['sell']))+','+state(r['before'])+','+str(len(r['stages']))+',{'+phases+'}},')
out.append('};')
Path('npe-sdl/tests/shop_return_periodic_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
print(len(rows),'cases,',sum(len(s['commits']) for r in rows for s in r['stages']),'commits')
