"""Actual original menu sequence: equip 85, replace with 91, equip 26."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[2]
events=json.loads((root/'tmp/native-map-equipment.json').read_text())
lines=['/* Independent original key sequence, including replacement effects. */',
       'static const struct { unsigned id; uint16_t hp,qi,attack,defense,status[4]; uint8_t equipment[6]; uint16_t inventory[3]; } map_equipment_native[] = {']
for label,ident in [('key8',85),('key12',91),('key16',26)]:
    event=next(e for e in events if e['label']==label)
    p=event['party']
    lines.append('    {%d,%d,%d,%d,%d,{%s},{%s},{%s}},' % (ident,p['hp'],p['qi'],p['attack'],p['defense'],
        ','.join(map(str,p['status'])),','.join(map(str,p['equipment'])),','.join(map(str,event['inventory']))))
lines+=['};','']
Path(__file__).with_name('map_equipment_native.h').write_text('\n'.join(lines),encoding='utf-8')
