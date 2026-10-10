import json
import sys
from pathlib import Path
selection_matrix='--selection' in sys.argv[1:]
prefix='native-shop-selection' if selection_matrix else 'native-shop-item-cache'
rows=json.loads(Path('tmp/'+prefix+'.json').read_text(encoding='utf8'))
fields=['mode','context','money','owned','selected','ui','quantity','choice','descStart','descEnd','first','last','row','descLength','inventoryPrefix']
def state(s,item):
    data=dict(s,owned=dict(s['stock'])[item],descStart=s['start'],descEnd=s['end'],descLength=s['length'],inventoryPrefix=s.get('inventoryPrefix',0))
    return '{'+','.join(str(data[k]) for k in fields)+'}'
def cache(s):return '{'+str(s['loaded'])+',{'+','.join(map(str,s['bytes']))+'}}'
out=[
 '/* Unmodified original shop services with diagnostic142-byte resident inputs. */',
 'typedef struct {unsigned '+','.join(fields)+';} ShopUiState;',
 'typedef struct {unsigned id;uint8_t bytes[142];} ShopItemCache;',
 'typedef struct {unsigned key,end;ShopUiState after;ShopItemCache cache;unsigned forcedRow;} ShopUiPhase;',
 'typedef struct {ShopUiState state;ShopItemCache cache;} ShopItemFrame;',
 'typedef struct {const char *name;unsigned selling,item,money,owned,count,commits,stock_count,initial_first,initial_last;unsigned stock[16][2];ShopUiPhase phase[48];ShopItemCache seed;ShopItemFrame frame[48];} ShopUiCase;',
 'static const ShopUiCase shop_ui_cases[]={',
]
total=0
for r in rows:
    item=r['stocks'][0][0];frames=list(r['initial']);phases=[]
    for p in r['phases']:
        frames.extend(p['commits']);phases.append('{%d,%d,%s,%s,%d}'%(p['key'],len(frames),state(p['after'],item),cache(p['after']),p.get('forcedRow',256)))
    total+=len(frames);seed=r['seed']
    out.append('{"'+r['name']+'",'+','.join(map(str,[int(r.get('sell',False)),item,1000,dict(seed['stock'])[item],len(phases),len(frames),len(r['stocks']),seed['first'],seed['last']]))+
               ',{'+','.join('{%d,%d}'%tuple(s) for s in r['stocks'])+'},{'+(','.join(phases) or '{0}')+'},'+cache(seed)+
               ',{'+','.join('{'+state(c['state'],item)+','+cache(c['state'])+'}' for c in frames)+'}},')
out.append('};')
assert len(rows)==(8 if selection_matrix else 6)
assert total==(41 if selection_matrix else 37)
Path('npe-sdl/tests/'+('shop_selection_native.h' if selection_matrix else 'shop_item_cache_native.h')).write_text('\n'.join(out)+'\n',encoding='utf8')
print(json.dumps({'original_paths':len(rows),'lcd_frames':total,'bytes_per_cache':142}))
