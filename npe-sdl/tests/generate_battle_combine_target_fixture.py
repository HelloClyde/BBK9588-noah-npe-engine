import json
import sys
from pathlib import Path
type_words='--type-words' in sys.argv
lines=['/* Original public combine inputs, including two uninterrupted rounds. */',
       'static const CombineTarget combine_targets[]={']
count=0
for stem in ('combine-animation','combine-repeat'):
    rows=json.loads(Path('tmp/native-'+stem+('-type-words' if type_words else '-selector-target')+'.json').read_text(encoding='utf8'))
    assert len(rows)==(6 if stem=='combine-animation' else 2)
    for row in rows:
        name=row['config']['name'].removesuffix('-type-words').removesuffix('-selector-target').removesuffix('-animation')
        events=row['events'];assert len(events) in (3,5)
        extra=',{'+','.join('{'+','.join(map(str,e['typeWords']))+'}' for e in events)+'}' if type_words else ''
        lines.append('{"%s",%u,{%s}%s},'%(name,len(events),','.join(str(e['displayTarget']) for e in events),extra))
        count+=1
lines.append('};')
assert count==8
Path('npe-sdl/tests/battle_combine_'+('type_words' if type_words else 'targets')+'_native.h').write_text('\n'.join(lines)+'\n',encoding='utf8')
print('Eight original combine paths, ten rounds,28 global-target states')
