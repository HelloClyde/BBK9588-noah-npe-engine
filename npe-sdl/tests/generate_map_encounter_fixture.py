import json
from pathlib import Path
rows=json.loads(Path('tmp/native-map-encounter-load.json').read_text())
assert len(rows)==14
out=['/* Original UI map loads,56 monster and28 chase bytes. */',
     'static const MapEncounterLoadCase encounter_load_cases[]={']
for r in rows:
    assert len(r['beforeBytes'])==len(r['afterBytes'])==84
    assert all(r['beforeBytes'][i]==r['afterBytes'][i] for i in (8,22,36,50,69,83))
    out.append('{'+str(r['targetMap'])+',{'+','.join(map(str,r['beforeBytes']))+'},{'+','.join(map(str,r['afterBytes']))+'}},')
out.append('};')
Path('npe-sdl/tests/map_encounter_load_native.h').write_text('\n'.join(out)+'\n')
rows=json.loads(Path('tmp/native-chase-resident.json').read_text())['rows'];assert len(rows)==24
out=['/* Original public chase timer with complete resident state. */',
     'static const ChaseCase chase_cases[]={']
for r in rows:
    b,a=r['before'],r['after']
    out.append('{'+json.dumps(r['name'])+','+','.join(map(str,(b['heroY'],a['mode'],b['rng'],a['rng'])))+',{')
    for which,key,fields in [('before','npc',('id','graphic','first','last','direction','x','y','moving')),
      ('after','npc',('id','graphic','first','last','direction','x','y','moving')),
      ('before','chase',('graphic','first','last','enemy','x','y','cooldown')),
      ('after','chase',('graphic','first','last','enemy','x','y','cooldown'))]:
        for n in r[which][key]:out.append('{'+','.join(str(n[f]) for f in fields)+'},')
        out.append('},{')
    out.append(','.join(map(str,b['cacheTail']))+'},')
    for which,key in [('before','npcBytes'),('after','npcBytes'),('before','chaseBytes'),('after','chaseBytes')]:
        out.append('{'+','.join(map(str,r[which][key]))+'}'+('},' if (which,key)==('after','chaseBytes') else ','))
out.append('};')
Path('npe-sdl/tests/chase_resident_native.h').write_text('\n'.join(out)+'\n')
print('14 original map loads and24 resident chase cases generated')
