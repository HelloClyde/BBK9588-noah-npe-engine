"""Freeze independent original-runtime status records, not C output."""
import json
from pathlib import Path

names = [(str(i),2,3) for i in range(4)] + [('positive',2,3),('all',3,3),('fallen',3,3),('enemy-fallen',3,3),('single',1,3),('all',2,2),('all',1,1),('all',2,1),('positive',3,1),('enemy-negative',2,3)]
lines = ['/* Generated from tmp/battle-native-status-overlay.mjs original runtime samples. */',
         'typedef struct { unsigned hp, qi; int16_t status[4]; } OverlayRecord;',
         'typedef struct { const char *name; unsigned party,enemies,actor,icon; OverlayRecord before[6],after[6]; } OverlayCase;',
         'static const OverlayCase overlay_cases[] = {']
def record(r):
    states = [v if v < 32768 else v - 65536 for v in r['status']]
    return '{%d,%d,{%s}}' % (r['hp'],r['qi'],','.join(map(str,states)))
names += [('third-all',3,3),('empty-fallen',3,3)]
for variant,party,enemies in names:
    name=f'{variant}-{party}-{enemies}'
    source=f'tmp/native-third-status-overlay-all-3-3.json' if variant=='third-all' else f'tmp/native-status-overlay-{name}.json'
    if variant=='empty-fallen':source='tmp/native-fallen-empty-defend-fallen-3-3.json'
    d=json.loads(Path(source).read_text(encoding='utf-8'))
    lines.append('    {"%s",%d,%d,%d,%d,{%s},{%s}},' % (
        name,party,d['enemies'],d['actor'],d['menu']//10-1,
        ','.join(map(record,d['entry'])),','.join(map(record,d['records']))))
lines.append('};')
Path('npe-sdl/tests/status_overlay_native.h').write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')
