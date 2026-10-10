import json
from pathlib import Path
def array(v):return '{'+','.join(map(str,v))+'}'
loads=json.loads(Path('tmp/native-battle-event-resident-load.json').read_text());assert len(loads)==16
out=['/* Original group8 loader:32 complete bytes. */','static const BattleEventLoadCase battle_event_load_cases[]={']
for r in loads:
 assert len(r['beforeBytes'])==len(r['afterBytes'])==32
 assert all(r['beforeBytes'][i*8+3]==r['afterBytes'][i*8+3] for i in range(4))
 out.append('{'+json.dumps(r['entry'])+','+str(r['targetMap'])+','+array(r['beforeBytes'])+','+array(r['afterBytes'])+'},')
out.append('};');Path('npe-sdl/tests/battle_event_resident_load_native.h').write_text('\n'.join(out)+'\n')
rows=json.loads(Path('tmp/native-battle-event-resident-entry.json').read_text());assert len(rows)==7
out=['/* Original scripted battle entry, full word addressing and live records. */','static const ScriptEntryCase script_cases[]={']
for r in rows:
 c,a,t=r['config'],r['after'],r['beforeTables'];assert len(r['commits'])==4
 records='{'+','.join('{'+','.join(str(s[k]) for k in ['hp','maxHp','qi','maxQi','attack','defense','speed'])+','+array(s['status'])+'}' for s in a['records'])+'}'
 out.append('{'+json.dumps(c['name'])+','+str(len(c['hps']))+','+array(c['hps'])+','+array(a['ids'])+','+records+','+','.join(str(c[k]) for k in ['event','winMode','winEvent','loseMode','loseEvent'])+','+array(t['battle'])+','+array(t['monster'])+','+str(r['address'])+','+array(t['alias'])+'},')
out.append('};');Path('npe-sdl/tests/battle_event_resident_entry_native.h').write_text('\n'.join(out)+'\n')
print('16 loads/512 bytes,7 original entry paths/28 LCD commits')
rows=json.loads(Path('tmp/native-battle-event-resident-exit.json').read_text());assert len(rows)==6
out=['/* Original real-command victory/defeat after live branch mutation. */','static const BattleEventExitCase battle_event_exit_cases[]={']
for r in rows:
 c,t,changed,a=r['config'],r['beforeTables'],r['changedTables'],r['after']
 assert a['mode']==17 and a['event']==(0x4567 if c['outcome']=='win' else 0x7654)
 out.append('{'+json.dumps(c['name'])+','+str(int(c['outcome']=='win'))+','+str(c['event'])+','+str(r['address'])+','+array(t['battle'])+','+array(t['monster'])+','+array(t['alias'])+','+array(changed['battle'])+','+array(changed['alias'])+','+str(a['mode'])+','+str(a['event'])+'},')
out.append('};');Path('npe-sdl/tests/battle_event_resident_exit_native.h').write_text('\n'.join(out)+'\n')
print('6 original win/loss final branches reread after resident mutation')
