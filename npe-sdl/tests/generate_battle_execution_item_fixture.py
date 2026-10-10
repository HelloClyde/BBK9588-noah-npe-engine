import json,sys
from pathlib import Path
prepared='--prepared-records' in sys.argv
stem='battle-prepared-item-record' if prepared else 'battle-execution-item-record'
rows=json.loads(Path('tmp/native-'+stem+'.json').read_text())
def arr(v):return '{'+','.join(map(str,v))+'}'
def records(v):return '{'+','.join(arr([r[k] for k in ['hp','maxHp','qi','maxQi','attack','defense','level']]+r['status']) for r in v)+'}'
out=['/* Native queued-metadata/shared-record execution fixtures. */',
 'typedef struct {const char *name;unsigned item,cached,second,throwing,frames,after_id;uint8_t before[142],after[142];unsigned records[6][11];'+('uint8_t prepared_before[6][12],prepared_after[6][12];unsigned group;' if prepared else '')+'} ExecItemCase;',
 'static const ExecItemCase exec_item_cases[]={']
for r in rows:
 c=r['cfg'];extra=(','+'{'+','.join(arr(v) for v in r['before']['prepared'])+'}'+','+'{'+','.join(arr(v) for v in r['after']['prepared'])+'}'+','+str(c.get('group',0))) if prepared else ''
 out.append('{"'+c['name']+'",'+','.join(map(str,[c['item'],c['cached'],c.get('second',0),int(c.get('throwing',False)),len(r['frames']),r['after']['cache']['id']]))+','+arr(r['before']['cache']['bytes'])+','+arr(r['after']['cache']['bytes'])+','+records(r['after']['records'])+extra+'},')
out.append('};')
Path('npe-sdl/tests/battle_prepared_item_native.h' if prepared else 'npe-sdl/tests/battle_execution_item_native.h').write_text('\n'.join(out)+'\n')
print(json.dumps({'paths':len(rows),'lcds':sum(len(r['frames']) for r in rows)}))
