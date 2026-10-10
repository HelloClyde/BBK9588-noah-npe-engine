import json
from pathlib import Path
def arr(v):return '{'+','.join(map(str,v))+'}'
rows=json.loads(Path('tmp/battle-selection-native/report.json').read_text())['cases'];assert len(rows)==32
out=['/* Unchanged original selector:independent words,NUM and flag. */','static const SelectionCase selection_cases[]={']
for r in rows:
 c=r['input'];enemies=c['enemies']+[0]*(8-len(c['enemies']))
 out.append('{'+json.dumps(c['name'])+','+','.join(str(c[k]) for k in ['flag','count','context','chase','seed','key'])+','+str(c.get('slot',0))+','+arr(c['ids'])+','+arr(enemies)+','+arr(c['monster'] or [0,0,0])+','+arr(r['ids'])+','+str(r['count'])+','+str(r['rng'])+'},')
out.append('};');Path('npe-sdl/tests/battle_selection_native.h').write_text('\n'.join(out)+'\n')
rows=json.loads(Path('tmp/native-battle-selection-entry.json').read_text())['rows'];assert len(rows)==4
out=['/* Original public walking into empty/one/two-enemy formation. */','static const EntryCase entry_cases[]={']
for r in rows:
 b,a=r['before'],r['after'];vals=[len(r['commits']),b['hp'],b['encounters'],b['encounterSteps'],a['hp'],a['mode'],a['encounterSteps'],a['rng'],a['chaseSlot'],b['rng'],b['partyInput']]
 s='{'+json.dumps(r['name'])+','+','.join(map(str,vals))+','+arr(b['hpMap'])+','+arr(b['encounterIds'])+','
 s+='{'+','.join(arr([n[k] for k in ('id','graphic','first','last','direction','x','y','moving')]) for n in b['npc'])+'},'
 for n in (b['chase'],a['chase']):s+='{'+','.join(arr([c[k] for k in ('graphic','first','last','enemy','x','y','cooldown')]) for c in n)+'},'
 s+=arr(a['selectedIds'])+','+'{'+','.join('{'+','.join(str(n[k]) for k in ('hp','maxHp','qi','maxQi','attack','defense','speed'))+','+arr(n['statuses'])+'}' for n in a['battleRecords'])+'},'
 s+=arr(b['selectedIds'])+','+str(b['num'])+','+str(a['num'])+','+str(a['flag'])+'},';out.append(s)
out.append('};');Path('npe-sdl/tests/battle_selection_entry_native.h').write_text('\n'.join(out)+'\n')
assert all(r['after']['mode']==3 for r in rows)
assert all(not any(rows[i]['before']['encounterIds']) for i in [0,1])
print('32 original selectors;4 original walking paths/'+str(sum(len(r['commits']) for r in rows))+' LCD frames')

loads=json.loads(Path('tmp/native-battle-selection-load.json').read_text());assert len(loads)==16
out=['/* Original public loads preserve selector words,NUM and flag. */','static const SelectionLoadCase selection_load_cases[]={']
for r in loads:
 assert len(r['beforeBytes'])==len(r['afterBytes'])==8
 out.append('{'+json.dumps(r['entry'])+','+str(r['targetMap'])+','+arr(r['beforeBytes'])+','+arr(r['afterBytes'])+'},')
out.append('};');Path('npe-sdl/tests/battle_selection_load_native.h').write_text('\n'.join(out)+'\n')
print('16 original loads/128 selector bytes')
