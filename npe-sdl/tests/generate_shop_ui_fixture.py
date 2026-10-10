import json,sys
from pathlib import Path
window='--window' in sys.argv;prefix='shop-boundary' if '--boundary' in sys.argv else ('shop-window' if window else 'shop-ui')
j=json.loads(Path('tmp/native-'+prefix+'.json').read_text(encoding='utf-8-sig'))
f=['mode','context','money','owned','selected','ui','quantity','choice','descStart','descEnd','first','last','row']
out=['typedef struct {unsigned '+','.join(f)+';} ShopUiState;', 'typedef struct {unsigned key,end;ShopUiState after;} ShopUiPhase;', 'typedef struct {const char *name;unsigned selling,item,money,owned,count,commits,stock_count,initial_first,initial_last;unsigned stock[16][2];ShopUiPhase phase[48];} ShopUiCase;', 'static const ShopUiCase shop_ui_cases[]={']
for r in j['rows']:
 end=len(r['init']);ps=[]
 for p in r['phases']:
  end+=len(p['commits']);s=p['after'];ps.append('{'+f'{p["key"]},{end},'+'{'+','.join(str(s[x]) for x in f)+'}}')
 stocks=r.get('stocks',[[r.get('item',45),r['owned']]])
 seed=r.get('seed',r['before'])
 out.append('{"'+r['name']+'",'+','.join(map(str,[int(r.get('sell',False)),r.get('item',45),r['money'],r['owned'],len(ps),end,len(stocks),seed['first'],seed['last']]))+',{'+','.join('{'+str(i)+','+str(q)+'}' for i,q in stocks)+'},{'+','.join(ps)+'}},')
out.append('};');Path('npe-sdl/tests/'+prefix.replace('-','_')+'_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
