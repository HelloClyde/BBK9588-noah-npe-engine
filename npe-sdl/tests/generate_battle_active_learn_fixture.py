import json
from pathlib import Path

rows = json.loads(Path('tmp/native-battle-active-learn.json').read_text(encoding='utf8'))
assert len(rows) == 4
assert {r['cfg']['activeByte'] for r in rows} == {0,2,4,6}
assert sum(len(r['frames']) for r in rows) == 73
def arr(v): return '{' + ','.join(map(str, v)) + '}'
def records(v):
    return '{' + ','.join(arr([r[k] for k in
        ['hp','maxHp','qi','maxQi','attack','defense','level']] +
        r['status'] + [r['experience'],r['next']]) for r in v) + '}'
out = ['/* Original WASM advance-batch states; LCDs retain every commit. */',
    'typedef struct {const char *name;unsigned cached,second,reverse,enemy_hp,learn,frames,event,after_id,skill_after_id,skill_before_id,drop_id,learn_id;uint8_t skills[18],drops[3][2],before[142],after[142],skill_before[144],skill_after[144],drop_record[142],learn_record[144];unsigned stock[4],records[6][13];unsigned active_byte;uint8_t map_flags[3],battle_flags[3];} PostwinRecordCase;',
    'static const PostwinRecordCase postwin_record_cases[]={']
for r in rows:
    c = r['cfg']; before = r['before']; after = r['after']
    assert before['event'] == 1 and after['mode'] == 5 and after['event'] == 4
    assert before['battleFlags'] == after['battleFlags'] == after['mapFlags'] == [c['activeByte'],1,1]
    panel = r['frames'][-2]['batchState']
    nums = [c['cached'],c.get('second',0),int(c.get('reverse',False)),
        c.get('enemyHp',1),int(c.get('learn',False)),len(r['frames']),
        after['event'],after['cache']['id'],after['skillCache']['id'],
        before['skillCache']['id'],panel['cache']['id'],panel['skillCache']['id']]
    skills = (c.get('skills',[]) + [0]*18)[:18]
    out.append('{"'+c['name']+'",'+','.join(map(str,nums))+','+
        ','.join([arr(skills),'{'+','.join(arr(v) for v in c.get('drops',[[0,0]]*3))+'}',arr(before['cache']['bytes']),
        arr(after['cache']['bytes']),arr(before['skillCache']['bytes']),
        arr(after['skillCache']['bytes']),arr(panel['cache']['bytes']),
        arr(panel['skillCache']['bytes']),arr([v for _,v in after['stock']]),
        records(after['records']),str(c['activeByte']),arr(after['mapFlags']),arr(after['battleFlags'])])+'},')
out.append('};')
Path('npe-sdl/tests/battle_active_learn_native.h').write_text('\n'.join(out)+'\n',encoding='utf8')
print(json.dumps({'paths':len(rows),'lcds':sum(len(r['frames']) for r in rows),'mapFlags':[r['after']['mapFlags'] for r in rows]}))
