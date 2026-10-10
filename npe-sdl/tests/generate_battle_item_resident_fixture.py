import json
from pathlib import Path
rows=json.loads(Path('tmp/native-battle-item-resident.json').read_text())
assert len(rows)==7
def arr(v):return '{'+','.join(map(str,v))+'}'
def commands(s):return '{'+','.join(arr([c[k] for k in ['kind','item','type','copy','group','side','skill','target']]) for c in s['commands'])+'}'
def stage(s):return '{'+arr(s['stock'])+','+commands(s)+'}'
def records(v):return '{'+','.join(arr([r[k] for k in ['hp','maxHp','qi','maxQi','attack','defense','level']]+r['status']) for r in v)+'}'
out=['/* Original controller phases and advance-batch round-end records. */',
 'typedef struct {unsigned stock[2],commands[3][8];} ResidentStage;',
 'typedef struct {const char *name;unsigned frames,phase_count,after_mode,after_id,before_id;ResidentStage initial,phases[5],before,after;uint8_t before_cache[142],cache[142];unsigned records[6][11];} ResidentCase;',
 'static const ResidentCase resident_cases[]={']
for r in rows:
    out.append('{"'+r['name']+'",'+','.join(map(str,[len(r['frames']),len(r['phases']),r['after']['mode'],r['after']['cache']['id'],r['before']['cache']['id']]))+','+
        stage(r['initial'])+',{'+','.join(stage(p['state']) for p in r['phases'])+'},'+stage(r['before'])+','+stage(r['after'])+','+
        arr(r['before']['cache']['bytes'])+','+arr(r['after']['cache']['bytes'])+','+records(r['after']['records'])+'},')
out.append('};')
Path('npe-sdl/tests/battle_item_resident_native.h').write_text('\n'.join(out)+'\n')
print(json.dumps({'paths':len(rows),'phases':sum(len(r['phases']) for r in rows),'lcds':sum(len(r['frames']) for r in rows)}))
