import json
from pathlib import Path

rows=json.loads(Path('tmp/native-character-metadata.json').read_text(encoding='utf8'))
assert len(rows)==3
def arr(v):return '{'+','.join(map(str,v))+'}'
def meta(m):return '{'+str(m['sex'])+','+arr(m['poses'])+','+arr(m['animations'])+'}'
zero={'sex':0,'poses':[0]*8,'animations':[0]*3}
def metas(v):return '{'+','.join(meta(m) for m in v+[zero]*(3-len(v)))+'}'
out=['/* Original firmware RAM: map/entry/mutated battle/flee/swap/reentry. */',
 'typedef struct {const char *name;unsigned count,removed,reentry_count;uint8_t ids[3],flags[3],gear[3][6];NpeCharacterMetadata defaults[3],before[3],entry[3],mutated[3],exit[3],swapped[3],reentry[3];} MetadataCase;',
 'static const MetadataCase metadata_cases[]={']
for r in rows:
 n=len(r['metadataEntry'])
 for i in range(n):
  for k in ['sex','poses','animations']:assert r['metadataEntry'][i][k]==r['metadataBefore'][i][k]
 for i in range(3):
  expected=r['metadataMutated'][i] if i<n else r['metadataBefore'][i]
  for k in ['sex','poses','animations']:assert r['metadataExit'][i][k]==expected[k]
 swapped=r['metadataRemoved'] or r['metadataExit']
 if r['metadataRemoved']:
  for i,j in enumerate([0,2,1]):
   for k in ['sex','poses','animations']:assert swapped[i][k]==r['metadataExit'][j][k]
 for i,m in enumerate(r['metadataReentry']):
  for k in ['sex','poses','animations']:assert m[k]==swapped[i][k]
 out.append('{"'+r['config']['name']+'",'+','.join(map(str,[n,int(bool(r['metadataRemoved'])),len(r['metadataReentry'])]))+','+
  arr(r['after']['ids'])+','+arr(r['config']['flags'])+','+'{'+','.join(arr(m['gear']) for m in r['defaults'])+'},'+
  ','.join(metas(r[k] or r['metadataExit']) for k in ['defaults','metadataBefore','metadataEntry','metadataMutated','metadataExit','metadataRemoved','metadataReentry'])+'},')
out+=['};','']
Path('npe-sdl/tests/character_metadata_native.h').write_text('\n'.join(out),encoding='utf8')
print('3 original metadata lifetimes;12 entry LCD commits;1 full-record party swap')
