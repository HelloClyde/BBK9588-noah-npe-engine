import json
from pathlib import Path
rows=json.loads(Path('tmp/native-friendly-hp-picker.json').read_text(encoding='utf-8'))
frames=json.loads(Path('tmp/native-friendly-hp-picker-frames.json').read_text(encoding='utf-8'))
rows += [r for r in frames if r['config']['hp']==0]
rows += json.loads(Path('tmp/native-friendly-hp-picker-refreshed.json').read_text(encoding='utf-8'))
rows += json.loads(Path('tmp/native-friendly-hp-picker-qi.json').read_text(encoding='utf-8'))
assert len(rows)==13 and all('error' not in r for r in rows)
lines=['#ifndef FRIENDLY_HP_PICKER_NATIVE_H','#define FRIENDLY_HP_PICKER_NATIVE_H','#include "negative_hp_native.h"',
       'typedef struct { const char *name, *frames; unsigned skill, refreshed, targets[4]; NegativeHpRecord records[6]; } FriendlyHpPickerCase;',
       'static const FriendlyHpPickerCase friendly_hp_picker_cases[] = {']
for row in rows:
    c=row['config'];events=row['events'][:6]
    assert [e['menu'] for e in events]==[30 if c.get('refreshed') else 10,21,27,27,27,27]
    assert all(e['mode']==3 and e['actor']==0 and e['records']==events[0]['records'] for e in events)
    targets=[e['target']-1 for e in events[2:]]
    assert targets==[0,1,2,1]
    prefix='\"%s\"'%c['name'] if c.get('refreshed') else 'NULL' # Only refreshed frames qualify.
    records=[]
    for r in events[0]['records']:
        values=[r[k] for k in ['hp','maximum','qi','mental','attack','defense','level','experience','attackTimer','defenseTimer']]
        st=[v if v<32768 else v-65536 for v in r['status']]
        records.append('{'+','.join(map(str,values))+',{'+','.join(map(str,st))+'}}')
    lines.append('{"%s",%s,%u,%u,{%s},{%s}},'%(c['name'],prefix,c['skill'],int(c.get('refreshed',False)),','.join(map(str,targets)),','.join(records)))
lines+=['};','#endif','']
Path('npe-sdl/tests/friendly_hp_picker_native.h').write_text('\n'.join(lines),encoding='utf-8',newline='\n')
print('13 original picker sequences, 468 complete record phases, 20 whole frame references')
