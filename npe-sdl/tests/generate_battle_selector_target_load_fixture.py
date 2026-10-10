import json
from pathlib import Path
rows=json.loads(Path('tmp/native-battle-selector-target-load.json').read_text(encoding='utf8'))
assert [r['entry'] for r in rows]==['system','title','new','empty']
assert all(r['beforeBytes']==r['afterBytes']==[129,166] for r in rows)
out=['/* Four original warm constructors retain7AC0 and7AC8. */',
     'static const MenuRowsLoad menu_rows_load[]={']
out+=['{'+json.dumps(r['entry'])+',{129,166},{129,166}},' for r in rows]
out.append('};')
Path('npe-sdl/tests/battle_selector_target_load_native.h').write_text('\n'.join(out)+'\n',encoding='utf8')
print('Four original warm constructors validated for7AC0/7AC8')
