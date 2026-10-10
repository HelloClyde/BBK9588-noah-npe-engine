import json
from pathlib import Path

rows=json.loads(Path('tmp/native-other-npe-skill.json').read_text(encoding='utf-8'))
def arr(v):return '{'+','.join(map(str,v))+'}'
def records(v):return '{'+','.join(arr([r[k] for k in ['hp','maxHp','qi','maxQi','attack','defense','level']]+r['status']) for r in v)+'}'
out=['/* Actual other-NPE records exercised through the original shared cache. */',
 'typedef struct {const char *name,*source;unsigned source_skill,reject,fallen,frames;uint8_t cache[144];unsigned records[6][11];} OtherSkillCase;',
 'static const OtherSkillCase other_skill_cases[]={']
for r in sorted(rows,key=lambda r:r['cfg']['name']):
    c=r['cfg']
    out.append('{'+json.dumps(c['name'])+','+json.dumps(c['source'])+','+','.join(map(str,[c['sourceSkill'],int(c.get('reject',False)),int(c.get('fallBeforeConfirm',False)),len(r['frames'])]))+','+arr(c['cache'])+','+records(r['after']['records'])+'},')
out.append('};')
Path('npe-sdl/tests/other_npe_skill_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
print(json.dumps({'paths':len(rows),'rounds':sum(not r['cfg'].get('reject',False) for r in rows),'lcds':sum(len(r['frames']) for r in rows)}))
