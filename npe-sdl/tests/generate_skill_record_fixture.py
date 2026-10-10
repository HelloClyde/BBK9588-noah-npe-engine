import json
from pathlib import Path
rows=json.loads(Path('tmp/native-skill-record-cache.json').read_text(encoding='utf8'))
fields=['mode','event','map','progress','init','sx','sy','vx','vy','direction','loaded']
def arr(v):return '{'+','.join(map(str,v))+'}'
def state(s):return '{'+','.join(str(s[k]) for k in fields)+','+arr(s['bytes'])+','+arr(s['skills'])+',{'+','.join('{%d,%d,%d,%d}'%(p['id'],p['x'],p['y'],p['direction']) for p in s['npc'])+'}}'
out=['/* Original shared skill loader and mode9; diagnostic cache, unchanged native services. */',
 'typedef struct {unsigned id,x,y,direction;} RecordNpc;',
 'typedef struct {unsigned '+','.join(fields)+';uint8_t bytes[144],skills[18];RecordNpc npc[20];} SkillRecordState;',
 'typedef struct {const char *name;unsigned id,commits;SkillRecordState before,after;} SkillRecordCase;',
 'static const SkillRecordCase skill_record_cases[]={']
service=[r for r in rows if 'route' not in r]
life=[r for r in rows if 'route' in r]
assert len(service)==6 and len(life)==5
for r in service:out.append('{"%s",%d,%d,%s,%s},'%(r['name'],r['id'],len(r['commits']),state(r['before']),state(r['after'])))
out+=['};','static const SkillRecordCase skill_record_lifetimes[]={']
for r in life:out.append('{"%s",0,0,%s,%s},'%(r['name'],state(r['before']),state(r['after'])))
out.append('};')
Path('npe-sdl/tests/skill_record_native.h').write_text('\n'.join(out)+'\n',encoding='utf8')
print(json.dumps({'paths':len(service),'commits':sum(len(r['commits']) for r in service),'lifetime_paths':len(life)}))
