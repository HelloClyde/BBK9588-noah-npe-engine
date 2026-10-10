import json
from pathlib import Path

out = ['/* Native public controller snapshots. Commit RAM is sampled at the end',
       '   of each advance; do not interpret tail transitions as LCD-time RAM. */',
       'typedef struct { const char *name; unsigned phases,frames;',
       ' uint8_t state[5][2],commit[56][2],lcd[56][2]; } ExecutionCase;',
       'static const ExecutionCase execution_cases[]={']
count = 0
for stem, expected in [('group-animation',6),('combine-animation',6),('combine-repeat',2),('enemy-animation',2)]:
    rows=json.loads(Path('tmp/native-'+stem+'-execution-state.json').read_text(encoding='utf8'))
    assert len(rows)==expected
    for row in rows:
        native_name=row['config']['name']
        name=native_name.removesuffix('-execution-state')
        if stem.startswith('combine'): name=name.removesuffix('-animation')
        events=row['events']
        # The group C fixture observes before/group/after. Its queued-defense
        # path also has two native intervening menus, kept in the JSON evidence.
        if stem=='group-animation': events=[events[0],events[-2],events[-1]]
        commits=json.loads(Path('tmp/native-'+native_name+'-commits.json').read_text(encoding='utf8'))
        assert len(events)<=5 and len(commits)<=56
        states=lambda rs:'{'+','.join('{%u,%u}'%(r['actionReady'],r['execution']) for r in rs)+'}'
        c=row['config'];party=c['party'];total=party+3;slots=[]
        if stem=='group-animation':
            speeds=[1,999,2] if c.get('secondFast') else [999,998,997]
            order=sorted([i for i in range(party) if c['hp'][i]>0 and i!=c.get('sleepSlot')],key=lambda i:-speeds[i])
            for i,actor in enumerate(order):
                if i:slots.append(order[i-1]+1) #33:C876, before the next39:5D1E
                slots.extend([actor+1]*8)
            slots.extend([order[-1]+1]*2+[total+1]*5)
        elif stem.startswith('combine'):
            for _ in range(c.get('rounds',1)):
                slots.extend([1]*14+[party]*9+[total+1]*5)
        else:
            order=[('enemy',c['enemyActor']),('party',0)] if c.get('enemyFast') else [('party',0),('enemy',c['enemyActor'])]
            if party==2:order.append(('party',1))
            previous=0
            for i,(side,actor) in enumerate(order):
                if i:slots.append(previous)
                previous=party+actor+1 if side=='enemy' else actor+1
                slots.extend([previous]*(7 if side=='enemy' else 1))
            slots.extend([previous]*2+[total+1]*5)
        assert len(slots)==len(commits),(name,len(slots),len(commits))
        # LCD-time sequence comes from the original service ordering. Native
        # commit RAM above is independently retained, even when an advance
        # already reached the next scheduler write before returning to JS.
        lcd=[{'actionReady':1,'execution':slot} for slot in slots]
        out.append('{%s,%u,%u,%s,%s,%s},'%(json.dumps(name),len(events),len(commits),states(events),states(commits),states(lcd)))
        count+=1
out.append('};')
assert count==16
Path('npe-sdl/tests/battle_execution_native.h').write_text('\n'.join(out)+'\n',encoding='utf8')

rows=json.loads(Path('tmp/native-battle-execution-load.json').read_text(encoding='utf8'))
assert [r['entry'] for r in rows]==['system','title','new','empty']
assert all(r['beforeBytes']==r['afterBytes']==[129,166] for r in rows)
out=['/* Original four warm constructors retain independent7AA8/7ACC. */',
     'static const MenuRowsLoad menu_rows_load[]={']
out+=['{'+json.dumps(r['entry'])+',{129,166},{129,166}},' for r in rows]
out.append('};')
Path('npe-sdl/tests/battle_execution_load_native.h').write_text('\n'.join(out)+'\n',encoding='utf8')
print('16 original public paths and four warm constructors; sampled RAM retained separately from LCD service sequence')
