import json
from pathlib import Path

samples=[json.loads(p.read_text(encoding='utf-8')) for p in sorted(Path('tmp').glob('native-status-item-*.json'))]
lines=['/* Generated from original-runtime status item samples, not C output. */',
 '#ifndef STATUS_ITEMS_NATIVE_H','#define STATUS_ITEMS_NATIVE_H',
 'typedef struct { uint16_t hp, qi, status[4]; } StatusItemRecord;',
 'typedef struct { const char *name; unsigned party, enemies, side, group, target, rounds, refresh, defend; uint16_t effects[4]; unsigned poison_level; } StatusItemCase;',
 'typedef struct { unsigned scenario, phase, quantity, count; StatusItemRecord records[6]; unsigned actor; } StatusItemPhase;',
 'static const StatusItemCase status_item_cases[] = {']
for sample in samples:
 c=sample['config'];lines.append('{"%s",%u,%u,%u,%u,%u,%u,%u,%u,{%s},%u},' % (sample['variant'],
  c['party'],c['enemies'],c['side'],c['group'],c['target'],c['rounds'],bool(c.get('refresh')),bool(c.get('defend')),
  ','.join(str(v&65535) for v in c['effects']),c.get('poisonLevel',0)))
lines+=['};','static const StatusItemPhase status_item_phases[] = {']
for scenario,sample in enumerate(samples):
 assert len(sample['events'])==sample['config']['rounds']+1
 for phase,event in enumerate(sample['events']):
  assert event['mode']==3
  records=','.join('{%u,%u,{%s}}' % (r['hp'],r['qi'],','.join(map(str,r['status']))) for r in event['records'])
  lines.append('{%u,%u,%u,%u,{%s},%u},' % (scenario,phase,event['quantity'],len(event['records']),records,event['actor']))
lines+=['};','#endif','']
Path('npe-sdl/tests/status_items_native.h').write_text('\n'.join(lines),encoding='utf-8',newline='\n')
print(len(samples),'original status item sequences,',sum(len(e['records']) for s in samples for e in s['events']),'record phases')
