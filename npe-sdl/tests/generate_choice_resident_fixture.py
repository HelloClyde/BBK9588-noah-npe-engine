import json
from pathlib import Path
def array(v):return '{'+','.join(map(str,v))+'}'
loads=json.loads(Path('tmp/native-choice-resident-load.json').read_text());assert len(loads)==16
out=['/* Original common loader, complete five choice slots. */','static const ChoiceLoadCase choice_load_cases[]={']
for r in loads:
 assert len(r['beforeBytes'])==len(r['afterBytes'])==30
 out.append('{'+json.dumps(r['entry'])+','+str(r['targetMap'])+','+array(r['beforeBytes'])+','+array(r['afterBytes'])+'},')
out.append('};');Path('npe-sdl/tests/choice_resident_load_native.h').write_text('\n'.join(out)+'\n')
rows=json.loads(Path('tmp/native-choice-resident-public.json').read_text());assert len(rows)==10
def state(s):return array([s[k] for k in ['mode','event','context','map','progress','init']])
out=['/* Original public choice UI, resident state and bounded input phases. */','static const ChoicePublicCase choice_public_cases[]={']
for r in rows:
 phases='{'+','.join('{'+str(p['key'])+','+str(p['commits'])+','+state(p['after'])+'}' for p in r['phases'])+'}'
 out.append('{'+json.dumps(r['name'])+','+state(r['before'])+','+array(r['beforeBytes'])+','+str(r['address'])+','+array(r['alias'])+','+str(len(r['phases']))+','+phases+'},')
out.append('};');Path('npe-sdl/tests/choice_resident_public_native.h').write_text('\n'.join(out)+'\n')
print('16 original loads/480 bytes;10 choice paths/'+str(sum(len(r['commits']) for r in rows))+' LCD submissions')
