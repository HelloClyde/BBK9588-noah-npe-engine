import json
from pathlib import Path

rows=json.loads(Path('tmp/native-money-item-resident-load.json').read_text())
assert len(rows)==16
def array(v):return '{'+','.join(map(str,v))+'}'
out=['/* Original UI loads: item40 + money40, including retained money padding. */',
     'static const MoneyItemResidentLoadCase money_item_load_cases[]={']
for row in rows:
 b,a=row['beforeBytes'],row['afterBytes'];assert len(b)==len(a)==80
 assert all(b[40+i*8+j]==a[40+i*8+j] for i in range(5) for j in (6,7))
 out.append('{'+json.dumps(row['entry'])+','+str(row['targetMap'])+','+array(b)+','+array(a)+'},')
out.append('};')
Path('npe-sdl/tests/money_item_resident_load_native.h').write_text('\n'.join(out)+'\n')
print('16 original loads:1280 complete table bytes,all money padding retained')
