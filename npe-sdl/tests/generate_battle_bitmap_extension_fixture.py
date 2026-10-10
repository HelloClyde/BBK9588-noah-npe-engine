import json
from pathlib import Path

rows = json.loads(Path('tmp/native-battle-bitmap-extension.json').read_text(encoding='utf8'))
assert len(rows) == 4
# The fifth requested path (31) overwrites native resident RAM and has
# no victory result. Keep that unimplemented path separate, not a passing
# fixture or a replacement expectation for C's ordinary world fields.
ram = json.loads(Path('tmp/native-battle-bitmap-extension-extended-31-ram-overwrite.json').read_text(encoding='utf8'))
assert ram['after']['mode']==3 and len(ram['frames'])==31
assert {r['cfg']['name'] for r in rows} == {'resident-29','extended-30','short-header-34','displacement-wrap-10952'}
assert {r['cfg']['gapByte'] for r in rows} == {4}
assert sum(len(r['frames']) for r in rows) == 80
def arr(v): return '{' + ','.join(map(str, v)) + '}'
def records(v):
    return '{' + ','.join(arr([r[k] for k in
        ['hp','maxHp','qi','maxQi','attack','defense','level']] +
        r['status'] + [r['experience'],r['next']]) for r in v) + '}'
out = ['/* Original WASM advance-batch states; LCDs retain every commit. */',
    'typedef struct {const char *name;unsigned cached,second,reverse,enemy_hp,learn,frames,event,after_id,skill_after_id,skill_before_id,drop_id,learn_id;uint8_t skills[18],drops[3][2],before[142],after[142],skill_before[144],skill_after[144],drop_record[142],learn_record[144];unsigned stock[4],records[6][13];unsigned active_byte;uint8_t map_flags[3],battle_flags[3];unsigned group_byte;uint8_t map_groups[3],battle_groups[3];unsigned gap_byte,pose0,pose1,defer_pose;uint8_t tail_before[2],map_bytes[3][7],battle_bytes[3][7];} PostwinRecordCase;',
    'static const PostwinRecordCase postwin_record_cases[]={']
for r in rows:
    c = r['cfg']; before = r['before']; after = r['after']
    assert before['event'] == 1 and after['mode'] == 5 and after['event'] == 4
    assert before['battleFlags'] == after['battleFlags'] == after['mapFlags'] == [c['activeByte'],1,1]
    panel = next((f['batchState'] for f in r['frames'] if f['batchState']['skillCache']['id']==2),r['frames'][-2]['batchState'])
    assert after['mapGroups'] == after['battleGroups'] == [c['groupByte'],0,0]
    assert before['gapBytes'] == after['gapBytes'] == [c['gapByte'],0,0]
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
        records(after['records']),str(c['activeByte']),arr(after['mapFlags']),arr(after['battleFlags']),str(c['groupByte']),arr(after['mapGroups']),arr(after['battleGroups']),str(c['gapByte']),str(c.get('pose0',before['battleRecords'][0][46]*256+before['battleRecords'][0][47])),str(c.get('pose1',before['battleRecords'][0][48]*256+before['battleRecords'][0][49])),str(int(c.get('deferPose',False))),arr(before['battleRecords'][0][146:148]),'{'+','.join(arr([m[o] for o in [45,46,47,48,49,146,147]]) for m in after['mapRecords'])+'}','{'+','.join(arr([m[o] for o in [45,46,47,48,49,146,147]]) for m in after['battleRecords'][:3])+'}'])+'},')
out.append('};')
Path('npe-sdl/tests/battle_bitmap_extension_native.h').write_text('\n'.join(out)+'\n',encoding='utf8')
print(json.dumps({'paths':len(rows),'lcds':sum(len(r['frames']) for r in rows),'mapFlags':[r['after']['mapFlags'] for r in rows]}))
