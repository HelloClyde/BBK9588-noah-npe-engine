import json
from pathlib import Path

def array(v):return '{'+','.join(map(str,v))+'}'
rows=json.loads(Path('tmp/native-exit-record-load.json').read_text())
assert len(rows)==16
out=['/* Full288-byte exit cache after original public UI loads. */','static const ExitLoadCase exit_load_cases[]={']
for r in rows:
    b,a=r['beforeBytes'],r['afterBytes'];assert len(b)==len(a)==288
    assert all(b[i*24+3]==a[i*24+3] for i in range(12))
    out.append('{'+json.dumps(r['entry'])+','+str(r['targetMap'])+','+array(b)+','+array(a)+'},')
out.append('};');Path('npe-sdl/tests/exit_record_load_native.h').write_text('\n'.join(out)+'\n')

def state(s):return array(s[k] for k in ('mode','event','map','progress','direction','screenX','screenY','viewX','viewY','exitChoice','exitSlot','encounterSteps','rng'))
rows=json.loads(Path('tmp/native-exit-resident.json').read_text())['rows'];assert len(rows)==13
out=['/* Original exit controller; complete RAM cache injection and end comparison. */','static const ExitCase exit_cases[]={']
for r in rows:
    e=r['before']['exit'];raw=e[:3]+e[4:12]+[e[13]]+e[14:22]+[e[23]]
    assert len(raw)==21 and len(r['beforeBytes'])==len(r['afterBytes'])==288
    assert all(r['beforeBytes'][i*24+3]==r['afterBytes'][i*24+3] for i in range(12))
    npc='{'+','.join(array(n[k] for k in ('id','graphic','first','last','direction','x','y','moving')) for n in r['before']['npc'])+'}'
    phases='{'+','.join('{'+str(p['key'])+','+str(p['commits'])+','+state(p['after'])+'}' for p in r['phases'])+'}'
    out.append('{'+json.dumps(r['name'])+','+str(len(r['commits']))+','+state(r['before'])+','+array(raw)+','+npc+','+
        str(len(r['phases']))+','+phases+','+','.join(array(r[k]) for k in ['beforeBytes','afterBytes','npcBytes','monsterBytes','chaseBytes'])+'},')
out.append('};');Path('npe-sdl/tests/exit_resident_native.h').write_text('\n'.join(out)+'\n')
wide=next(r for r in rows if r['name']=='wide-primary-event')
assert wide['after']['event']==0x1234,wide['after']['event']
print('16 exit loads/4608 bytes;',len(rows),'controller routes/',sum(len(r['commits']) for r in rows),'commits; wide event4660 retained')
