import json
import sys
from pathlib import Path

triggers='--triggers' in sys.argv
name='walk_trigger_continuation' if triggers else 'walk_battle_entry'
native='native-walk-trigger-continuation' if triggers else 'native-walk-battle-entry'
rows=json.loads(Path('tmp/'+native+'.json').read_text(encoding='utf-8'))['rows']
out=['/* Original map attempts and complete mode3 entry display commits. */',
     'static const EntryCase entry_cases[] = {']
for row in rows:
    b=row['before'];a=row['after']
    vals=[len(row['commits']),b['hp'],b['encounters'],b['encounterSteps'],
          a['hp'],a['mode'],a['encounterSteps'],a['rng'],a['chaseSlot'],b['rng'],b['partyInput']]
    out.append('{"'+row['name']+'",'+','.join(map(str,vals))+', {'+','.join(map(str,b['hpMap']))+'}, {'+
               ','.join(map(str,b['encounterIds']))+'}, {')
    for n in b['npc']:
        out.append('{'+','.join(str(n[k]) for k in ('id','graphic','first','last','direction','x','y','moving'))+'},')
    for records in (b['chase'],a['chase']):
        out.append('}, {')
        for n in records:
            out.append('{'+','.join(str(n[k]) for k in ('graphic','first','last','enemy','x','y','cooldown'))+'},')
    out.append('}, {'+','.join(map(str,a['selectedIds']))+'}, {')
    for n in a['battleRecords']:
        out.append('{'+','.join(str(n[k]) for k in ('hp','maxHp','qi','maxQi','attack','defense','speed'))+
                   ',{'+','.join(map(str,n['statuses']))+'}},')
    if triggers:
        t=b['trigger'][0]
        out.append('},'+str(a['event'])+', {'+','.join(str(t[k]) for k in ('tile','mode','event','first','last'))+'},'+str(b['money'])+'},')
    else:
        out.append('}},')
out.append('};')
if triggers:
    navigation=rows[0]['menuNavigation']
    assert len(navigation)==8 and all(len(n['frames'])==1 for n in navigation)
    out.append('static const unsigned menu_keys[]={'+','.join(str(n['key']) for n in navigation)+'};')
    out.append('static const unsigned menu_selections[]={'+','.join(str(n['after']['menuSelection']-1) for n in navigation)+'};')
Path('npe-sdl/tests/'+name+'_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8',newline='\n')
