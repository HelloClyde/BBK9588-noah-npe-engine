import json
from pathlib import Path

rows=json.loads(Path('tmp/native-character-attack-group.json').read_text(encoding='utf8'))
assert len(rows)==5
def arr(values):return '{'+','.join(map(str,values))+'}'
out=['/* Original ordinary sword action with resident647C group bytes. */',
     'typedef struct {const char *name;unsigned group,surround,frames;unsigned records[6][11];} CharacterGroupCase;',
     'static const CharacterGroupCase character_group_cases[]={']
for r in rows:
    assert r['before']['commands'][0]['kind']==0 and r['before']['commands'][0]['group']==r['group']
    assert r['after']['commands'][0]['group']==(0 if r.get('surround') else r['group'])
    out.append('{"'+r['name']+'",'+str(r['group'])+','+str(int(r.get('surround',False)))+','+str(len(r['frames']))+',{'+
               ','.join(arr([p[k] for k in ['hp','maxHp','qi','maxQi','attack','defense','level']]+p['status'])
                        for p in r['after']['records'])+'}},')
out+=['};','']
Path('npe-sdl/tests/character_attack_group_native.h').write_text('\n'.join(out),encoding='utf8',newline='\n')
print(str(len(rows))+' original ordinary-attack group paths,'+str(sum(len(r['frames']) for r in rows)))
