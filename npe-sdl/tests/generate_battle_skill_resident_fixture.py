import json
from pathlib import Path
rows=json.loads(Path('tmp/native-battle-skill-resident.json').read_text())
assert len(rows)==4
def arr(v):return '{'+','.join(map(str,v))+'}'
def stage(s):return '{'+arr(s['stock'])+',{'+','.join(arr([c[k] for k in ['kind','item','skill','magicType','type','copy','group','side','target']]) for c in s['commands'])+'}}'
def records(s):return '{'+','.join(arr([r[k] for k in ['hp','maxHp','qi','maxQi','attack','defense','level']]+r['status']) for r in s['records'])+'}'
out=['/* Original public input; RAM state at advance boundaries. */',
 'typedef struct {unsigned stock[2],commands[3][9];} ResidentStage;',
 'typedef struct {ResidentStage before,after;unsigned frames,cache_id;uint8_t cache[144];unsigned records[6][11];} RetainedRound;',
 'typedef struct {const char *name;unsigned frames,phase_count;ResidentStage initial,phases[9];RetainedRound rounds[2];} ResidentCase;',
 'static const ResidentCase resident_cases[]={']
for r in rows:
    out.append('{"'+r['name']+'",'+str(len(r['frames']))+','+str(len(r['phases']))+','+stage(r['initial'])+',{'+','.join(stage(p['state']) for p in r['phases'])+'},{'+','.join('{'+stage(t['before'])+','+stage(t['after'])+','+str(t['count'])+','+str(t['after']['skillCache']['id'])+','+arr(t['after']['skillCache']['bytes'])+','+records(t['after'])+'}' for t in r['rounds'])+'}},')
out.append('};')
Path('npe-sdl/tests/battle_skill_resident_native.h').write_text('\n'.join(out)+'\n')
print(json.dumps({'paths':len(rows),'rounds':len(rows)*2,'phases':sum(len(r['phases']) for r in rows),'lcds':sum(len(r['frames']) for r in rows)}))
