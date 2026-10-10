import json
from pathlib import Path
rows=json.loads(Path('tmp/native-magic-description-retain.json').read_text(encoding='utf8'))
fields=['mode','page','member','loaded','length','start','end']
def arr(v):return '{'+(','.join(map(str,v)) or '0')+'}'
def state(s):
    p=s['party'];party=[p[k] for k in ['hp','maxHp','qi','maxQi','attack','defense','level','experience','nextExperience']]+p['other']+p['status']
    if 'party2' in s:
        p=s['party2'];party += [p[k] for k in ['hp','maxHp','qi','maxQi','attack','defense','level','experience','nextExperience']]+p['other']+p['status']
    return '{'+','.join(str(s[k]) for k in fields)+','+arr(s['bytes'])+','+arr(party)+','+arr(s['portraits'])+'}'
out=['/* Original map skill modal commits; outer map close frames retained separately. */',
 'typedef struct {unsigned '+','.join(fields)+';uint8_t bytes[144];unsigned party[32],portraits[3];} MapSkillState;',
 'typedef struct {unsigned key,end,patch,patch_length,patch_start,patch_end;MapSkillState after;} MapSkillPhase;',
 'typedef struct {unsigned index;MapSkillState state;} MapSkillFrame;',
 'typedef struct {const char *name;uint8_t skills[18];unsigned members,phases,commits;MapSkillState before;MapSkillPhase phase[8];MapSkillFrame frame[14];} MapSkillCase;',
 'static const MapSkillCase map_skill_cases[]={']
total=closed=0
for r in rows:
    assert r['after']['mode']==1
    assert r['seed']['portraits']==r['portraits']
    assert all(c['state']['portraits']==r['portraits'] for p in r['phases'] for c in p['commits'])
    frames=[{'index':65535,'state':r['before']}];phases=[]
    for p in r['phases']:
        frames += [c for c in p['commits'] if c['state']['mode']==6]
        closed+=sum(c['state']['mode']!=6 for c in p['commits'])
        patch=p.get('patch',{});before=p['before']
        phases.append('{%d,%d,%d,%d,%d,%d,%s}'%(p['key'],len(frames),bool(patch),before['length'],before['start'],before['end'],state(p['after'])))
    total+=len(frames)
    out.append('{"'+r['name']+'",'+arr(r['skills'])+','+str(r.get('members',1))+','+str(len(phases))+','+str(len(frames))+','+state(r['before'])+',{'+','.join(phases)+'},{'+','.join('{%d,%s}'%(c['index'],state(c['state'])) for c in frames)+'}},')
out.append('};')
assert len(rows)==4
Path('npe-sdl/tests/magic_description_retain_native.h').write_text('\n'.join(out)+'\n',encoding='utf8')
print(json.dumps({'paths':len(rows),'modal_frames':total,'outer_frames_not_compared':closed}))
