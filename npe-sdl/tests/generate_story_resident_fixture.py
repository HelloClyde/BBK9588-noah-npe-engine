import json
from pathlib import Path

def array(v):return '{'+','.join(map(str,v))+'}'
rows=json.loads(Path('tmp/native-story-resident-load.json').read_text());assert len(rows)==16
out=['/* Complete native story cache including retained padding. */','static const StoryResidentLoadCase story_resident_load_cases[]={']
for row in rows:
    b,a=row['beforeBytes'],row['afterBytes'];assert len(b)==len(a)==90
    assert all(b[i*6+5]==a[i*6+5] for i in range(15))
    out.append('{'+json.dumps(row['entry'])+','+str(row['targetMap'])+','+array(b)+','+array(a)+'},')
out.append('};');Path('npe-sdl/tests/story_resident_load_native.h').write_text('\n'.join(out)+'\n')
print('16 original story cache loads,1440 bytes including padding')
bulk=json.loads(Path('tmp/object-bulk-native/report.json').read_text());assert bulk['passed'] and len(bulk['cases'])==5
out=['/* Original bulk service threshold and exact byte equality. */','static const unsigned object_bulk_cases[][3]={']
for row in bulk['cases']:out.append('{'+','.join(str(row[k]) for k in ('id','flag','after'))+'},')
out.append('};');Path('npe-sdl/tests/object_bulk_native.h').write_text('\n'.join(out)+'\n')
