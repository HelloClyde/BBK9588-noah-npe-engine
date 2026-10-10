"""Offline original firmware oracle; never derive expected stats from C."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[2]
fields=['hp','maxHp','qi','maxQi','attack','defense']
def attributes(p):return [p[k] for k in fields]+p['other']
def array(a):return '{'+','.join(map(str,a))+'}'
lines=['/* Independent original equipment attribute key sequences. */',
       'static const struct { const char *name; int16_t deltas[7]; uint16_t initial[9], after[2][9], status[2][4], inventory[2][2]; } equipment_attributes_native[] = {']
for name in ['wrap','negative','refresh']:
    sample=json.loads((root/'tmp'/f'native-map-equipment-attributes-{name}.json').read_text())
    events=sample['events']
    initial=next(e for e in events if e['label']=='entry')['party']
    phases=[next(e for e in events if e['label']==label) for label in ['equipped','replaced']]
    lines.append('    {"%s",%s,%s,{%s},{%s},{%s}},' % (name,array(sample['deltas']),array(attributes(initial)),
        ','.join(array(attributes(e['party'])) for e in phases),
        ','.join(array(e['party']['status']) for e in phases),
        ','.join(array(e['inventory']) for e in phases)))
lines+=['};','']
Path(__file__).with_name('equipment_attributes_native.h').write_text('\n'.join(lines),encoding='utf-8')
