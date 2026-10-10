"""Keep original commits and RAM checks paired; map-close LCDs belong to outer loop."""
import json
from pathlib import Path

rows=json.loads(Path('tmp/native-portrait-item.json').read_text(encoding='utf8'))
fields=['mode','selected','row','column','length','start','end','loaded']
def arr(values):return '{'+(','.join(map(str,values)) or '0')+'}'
def party_values(p):
    return [p[k] for k in ['hp','maxHp','qi','maxQi','attack','defense','level','experience','nextExperience']]+p['other']+p['status']

def state(s):
    p=s['party']
    return '{'+','.join(str(s[k]) for k in fields)+','+arr(s['bytes'])+','+arr(
        party_values(p)+party_values(s['party2']))+','+arr([n for _,n in s['stock']])+','+arr(s['portraits'])+'}'
out=['/* Original UI, diagnostic inventory/cache/HP; no original service replacement. */',
     'typedef struct {unsigned '+','.join(fields)+';uint8_t bytes[142];unsigned party[32],stock[14],portraits[3];} MapItemState;',
     'typedef struct {unsigned key,end;MapItemState after;} MapItemPhase;',
     'typedef struct {unsigned index;MapItemState state;} MapItemFrame;',
     'typedef struct {const char *name;unsigned members,stock_count,stock[14][2],phases,commits;MapItemState seed;MapItemPhase phase[8];MapItemFrame frame[12];} MapItemCase;',
     'static const MapItemCase map_item_cases[]={']
total=0
for r in rows:
    assert r['after']['mode']==1
    assert r['seed']['portraits']==r['portraits']
    assert all(c['state']['portraits']==r['portraits'] for p in r['phases'] for c in p['commits'])
    frames=[c for c in r['initial'] if c['state']['mode']==4];phases=[]
    for p in r['phases']:
        frames += [c for c in p['commits'] if c['state']['mode']==4]
        phases.append('{%d,%d,%s}'%(p['key'],len(frames),state(p['after'])))
    total+=len(frames)
    out.append('{"'+r['name']+'",'+str(r['members'])+','+str(len(r['stocks']))+',{'+(','.join(arr(s) for s in r['stocks']) or '{0}')+'},'+str(len(phases))+','+str(len(frames))+','+state(r['seed'])+',{'+','.join(phases)+'},{'+','.join('{%d,%s}'%(c['index'],state(c['state'])) for c in frames)+'}},')
out.append('};')
assert len(rows)==5 and total==35
Path('npe-sdl/tests/portrait_item_native.h').write_text('\n'.join(out)+'\n',encoding='utf8')
print(json.dumps({'paths':len(rows),'modal_lcd_frames':total,'outer_frames_not_compared':sum(c['state']['mode']!=4 for r in rows for p in r['phases'] for c in p['commits'])}))
