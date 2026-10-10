import json
from pathlib import Path
rows=json.loads(Path('tmp/native-shop-catalog.json').read_text(encoding='utf-8'))
fields=['mode','context','money','owned','selected','ui','quantity','choice','descStart','descEnd','first','last','row','descLength','inventoryPrefix','kind','sellFlag','scanFirst','scanLast','inventoryOnly']
def byte_array(values):return '{'+','.join(map(str,values))+'}'
def state(s,item):
    data=dict(s,owned=dict(s['stock'])[item],descStart=s['start'],descEnd=s['end'],descLength=s['length'])
    return '{'+','.join(str(data[k]) for k in fields)+','+byte_array(s['catalog'])+'}'
def cache(s):return '{'+str(s['loaded'])+','+byte_array(s['bytes'])+'}'
out=['/* Unmodified original shop with explicit resident catalog/scan changes. */',
    'typedef struct {unsigned '+','.join(fields)+';uint8_t catalog[255];} ShopUiState;',
    'typedef struct {unsigned id;uint8_t bytes[142];} ShopItemCache;',
    'typedef struct {unsigned key,end;ShopUiState after;ShopItemCache cache;unsigned forcedRow;} ShopUiPhase;',
    'typedef struct {ShopUiState state;ShopItemCache cache;} ShopItemFrame;',
    'typedef struct {const char *name;unsigned selling,item,money,owned,count,commits,stock_count,initial_first,initial_last;unsigned stock[16][2];ShopUiPhase phase[48];ShopItemCache seed;ShopItemFrame frame[48];unsigned afterFlag,afterKind,patch_count,patch[16][2],afterContext,scanFirst,scanLast,inventoryOnly;uint8_t seed_catalog[255];} ShopUiCase;',
    'static const ShopUiCase shop_ui_cases[]={']
total=0
for r in rows:
    item=r['stocks'][0][0];seed=r['seed'];frames=list(r['initial']);phases=[]
    for p in r['phases']:
        frames.extend(p['commits']);phases.append('{%d,%d,%s,%s,256}'%(p['key'],len(frames),state(p['after'],item),cache(p['after'])))
    total+=len(frames)
    out.append('{"'+r['name']+'",'+','.join(map(str,[int(r.get('sell',False)),item,1000,dict(seed['stock'])[item],len(phases),len(frames),len(r['stocks']),seed['first'],seed['last']]))+
        ',{'+','.join('{%d,%d}'%tuple(s) for s in r['stocks'])+'},{'+','.join(phases)+'},'+cache(seed)+
        ',{'+','.join('{'+state(c['state'],item)+','+cache(c['state'])+'}' for c in frames)+'},256,256,'+str(len(r.get('catalog',[])))+
        ',{'+(','.join('{%d,%d}'%tuple(s) for s in r.get('catalog',[])) or '{0}')+'},'+','.join(str(r.get(k,65536 if k=='afterContext' else 256)) for k in ['afterContext','scanFirst','scanLast','inventoryOnly'])+','+byte_array(seed['catalog'])+'},')
assert len(rows)==8
out.append('};')
Path('npe-sdl/tests/shop_catalog_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
print(json.dumps(dict(original_paths=len(rows),lcd_frames=total,catalog_bytes_per_state=255)))
