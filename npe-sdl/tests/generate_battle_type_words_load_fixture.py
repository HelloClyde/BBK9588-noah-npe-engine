import json
from pathlib import Path

rows=json.loads(Path('tmp/native-battle-type-words-load.json').read_text(encoding='utf8'))
assert [r['entry'] for r in rows]==['system','title','new','empty']
pattern=[129,166,203,240,21,58]
assert all(r['beforeBytes']==r['afterBytes']==pattern for r in rows)
array='{'+','.join(map(str,pattern))+'}'
out=['/* Original four warm constructors preserve all six type-word bytes. */',
     'static const MenuRowsLoad menu_rows_load[]={']
out+=['{'+json.dumps(r['entry'])+','+array+','+array+'},' for r in rows]
out.append('};')
Path('npe-sdl/tests/battle_type_words_load_native.h').write_text('\n'.join(out)+'\n',encoding='utf8')
print('Four original warm constructors retain7AA0/7AA6/7AAA including high bytes')
