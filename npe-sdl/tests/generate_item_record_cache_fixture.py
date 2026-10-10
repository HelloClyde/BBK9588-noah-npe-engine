import json
from pathlib import Path
rows=json.loads(Path('tmp/native-item-record-cache.json').read_text(encoding='utf8'))
def state(s):
    fields=['mode','event','map','progress','init','sx','sy','vx','vy','direction','loaded','quantity']
    return '{'+','.join(str(s[k]) for k in fields)+',{'+','.join(map(str,s['bytes']))+'},{'+','.join('{%d,%d,%d,%d}'%(p['id'],p['x'],p['y'],p['direction']) for p in s['npc'])+'}}'
out='/* Original mode12 UI paths with diagnostic resident cache. */\nstatic const ItemRecordCase item_record_cases[]={\n'
for r in rows:out+='{"%s",%d,%d,%s,%s},\n'%(r['name'],r['id'],len(r['commits']),state(r['before']),state(r['after']))
out+='};\n'
lifetimes=json.loads(Path('tmp/native-item-record-lifetime.json').read_text(encoding='utf8'))
assert [r['route'] for r in lifetimes]==['new','empty','system','title','teleport','equipment']
def resident(s):
    return '{'+','.join(str(s[k]) for k in ['selected','loaded','length','start','end'])+',{'+','.join(map(str,s['bytes']))+'}}'
out+='static const ItemLifetimeCase item_lifetime_cases[]={\n'
for r in lifetimes:
    stages=','.join(resident(s) for s in r['stages']) or '{0}'
    out+='{"%s",%s,%s,{%s}},\n'%(r['route'],resident(r['before']),resident(r['after']),stages)
out+='};\n'
assert all(s['commits']==1 for s in lifetimes[-1]['stages'][3:])
Path('npe-sdl/tests/item_record_cache_native.h').write_text(out,encoding='utf8')
assert len(rows)==4 and sum(len(r['commits']) for r in rows)==16
print(json.dumps({'paths':4,'original_lcd_commits':16,'lifetime_paths':6,'equipment_lcd_commits':3}))
