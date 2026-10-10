import json
import sys
from pathlib import Path
type_words='--type-words' in sys.argv
selector_target='--selector-target' in sys.argv or type_words
resources='--resources' in sys.argv or selector_target
suffix='type_words' if type_words else 'selector_target' if selector_target else 'resources' if resources else 'record'
rows=json.loads(Path('tmp/native-battle-item-'+suffix.replace('_','-')+'.json').read_text(encoding='utf8'))
def arr(v):return '{'+(','.join(map(str,v)) or '0')+'}'
def state(s):
    fields=[s[k] for k in ['mode','menu','actor','loaded','length','start','end','selected','row','column','alias']]
    if resources:fields.append(s['resourceFlags'])
    if selector_target:fields.append(s['selectorTarget'])
    if type_words:fields+=s['typeWords']
    cmd=s['commands'][0];command=[cmd[k] for k in ['kind','target','id','type','group','side']]
    return '{'+','.join(map(str,fields))+','+arr(s['bytes'])+','+arr(command)+','+arr([p[1] for p in s['stock']])+'}'
out=['/* Native item selection/target cache and inventory phases. */',
 'typedef struct {unsigned mode,menu,actor,loaded,length,start,end,selected,row,column,alias'+(',resourceFlags' if resources else '')+(',selectorTarget' if selector_target else '')+(',itemType,magicType,typeCopy' if type_words else '')+';uint8_t bytes[142];unsigned command[6],stock[32];} BattleItemState;',
 'typedef struct {unsigned key,end;BattleItemState after;} BattleItemPhase;',
 'typedef struct {unsigned index;BattleItemState state;} BattleItemFrame;',
 'typedef struct {const char *name;unsigned throwing,count,ids[32],phases,commits;BattleItemState seed;BattleItemPhase phase[8];BattleItemFrame frame[24];'+('uint8_t nextTypes[3];' if type_words else '')+'} BattleItemCase;',
 'static const BattleItemCase battle_item_cases[]={']
outer=total=0
for r in rows:
    frames=[{'index':65535,'state':r['before']}];phases=[]
    for p in r['phases']:
        matched=[c for c in p['commits'] if c['state']['menu'] in (33,37) and c['state']['actor']==0]
        frames+=matched;outer+=len(p['commits'])-len(matched)
        phases.append('{%d,%d,%s}'%(p['key'],len(frames),state(p['after'])))
    assert len(frames)<=24
    total+=len(frames)
    out.append('{"'+r['name']+'",'+str(int(r.get('throwing',False)))+','+str(len(r['items']))+','+arr(r['items'])+','+str(len(phases))+','+str(len(frames))+','+state(r['seed'])+',{'+','.join(phases)+'},{'+','.join('{%d,%s}'%(f['index'],state(f['state'])) for f in frames)+'}'+(','+arr(r.get('nextTypes',[0,0,0])) if type_words else '')+'},')
out.append('};');Path('npe-sdl/tests/battle_item_'+suffix+'_native.h').write_text('\n'.join(out)+'\n',encoding='utf8')
print(json.dumps({'paths':len(rows),'lcds':total,'outerExcluded':outer}))
