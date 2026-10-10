import json
from pathlib import Path
rows=json.loads(Path('tmp/native-encounter-control.json').read_text())['rows'];assert len(rows)==12
def array(v):return '{'+','.join(map(str,v))+'}'
out=['/* Original public walk controller:independent control words. */','static const EntryCase entry_cases[]={']
for r in rows:
 b,a=r['before'],r['after'];vals=[len(r['commits']),b['hp'],b['encounters'],b['encounterSteps'],a['hp'],a['mode'],a['encounterSteps'],a['rng'],a['chaseSlot'],b['rng'],b['partyInput']]
 s='{'+json.dumps(r['name'])+','+','.join(map(str,vals))+','+array(b['hpMap'])+','+array(b['encounterIds'])+','
 s+='{'+','.join(array([n[k] for k in ('id','graphic','first','last','direction','x','y','moving')]) for n in b['npc'])+'},'
 for n in (b['chase'],a['chase']):s+='{'+','.join(array([c[k] for k in ('graphic','first','last','enemy','x','y','cooldown')]) for c in n)+'},'
 s+=array(a['selectedIds'])+','+'{'+','.join('{'+','.join(str(n[k]) for k in ('hp','maxHp','qi','maxQi','attack','defense','speed'))+','+array(n['statuses'])+'}' for n in a['battleRecords'])+'}},'
 out.append(s)
out.append('};');Path('npe-sdl/tests/encounter_control_native.h').write_text('\n'.join(out)+'\n')
loads=json.loads(Path('tmp/native-encounter-control-load.json').read_text());assert len(loads)==16
out=['/* Original constructors rederive nonzero/signed first enemy enable. */','static const EncounterControlLoadCase control_load_cases[]={']
for r in loads:
 assert len(r['beforeBytes'])==len(r['afterBytes'])==2
 out.append('{'+json.dumps(r['entry'])+','+str(r['targetMap'])+','+array(r['beforeBytes'])+','+array(r['afterBytes'])+'},')
out.append('};');Path('npe-sdl/tests/encounter_control_load_native.h').write_text('\n'.join(out)+'\n')
print('16 original control loads/32 bytes,12 original walks/'+str(sum(len(r['commits']) for r in rows))+' LCD commits')
