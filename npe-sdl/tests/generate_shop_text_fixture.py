import json
from pathlib import Path
rows=json.loads(Path('tmp/native-shop-text-settled-service.json').read_text(encoding='utf-8'))
fields=['mode','event','context','ui','selected','quantity','choice','length','start','end','loaded','sell','first','last','row']
def state(s):return '{'+','.join(str(s[f]) for f in fields)+',{'+','.join(map(str,s['bytes']))+'}}'
lines=['/* Generated from settled unmodified original shop samples. */',
 'typedef struct {unsigned '+','.join(fields)+';uint8_t bytes[142];} ShopTextState;',
 'typedef struct {unsigned key,patch[4],count,index;ShopTextState before,after,frame;} ShopTextPhase;',
 'typedef struct {const char *name;unsigned count,commits;ShopTextState before;ShopTextPhase phases[4];} ShopTextCase;',
 'static const ShopTextCase shop_text_cases[]={']
for c in rows:
 phases=[]
 for p in c['phases']:
  assert len(p['commits'])<=1
  patch=[p.get('patch',{}).get(f,4294967295) for f in ['length','start','end','choice']]
  frame=p['commits'][0] if p['commits'] else {'index':0,'state':p['after']}
  phases.append('{'+','.join([str(p['key']),'{'+','.join(map(str,patch))+'}',str(len(p['commits'])),str(frame['index']),state(p['before']),state(p['after']),state(frame['state'])])+'}')
 lines.append('{"'+c['name']+'",'+str(len(phases))+','+str(c['commits'])+','+state(c['before'])+',{'+','.join(phases)+'}},')
lines+=['};','']
Path('npe-sdl/tests/shop_text_native.h').write_text('\n'.join(lines),encoding='utf-8')
print(len(rows),'shop text fixtures')
