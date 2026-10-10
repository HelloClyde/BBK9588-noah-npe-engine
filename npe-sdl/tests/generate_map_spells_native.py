import json
from pathlib import Path
root=Path(__file__).resolve().parents[2]
cases=[(6,'single'),(11,'single'),(1,'single'),(4,'single'),(6,'fallen'),(6,'wrap'),(14,'multi'),(9,'multi'),(6,'insufficient'),(1,'fallen')]
lines=['/* Independent original runtime key captures; never C-generated expectations. */',
       'static const struct { unsigned id, variant, casts; int result; uint16_t hp[3], qi[3]; } map_native[] = {']
for skill,variant in cases:
    path=root/'tmp'/f'native-map-spell-{skill}-{variant}.json'
    if not path.exists(): path=root/'tmp'/f'native-map-spell-{skill}.json'
    sample=json.loads(path.read_text())[-1]
    party=sample.get('party',[dict(hp=sample['hp'],qi=sample['qi'])])
    hp=[p['hp'] for p in party]+[0]*(3-len(party))
    qi=[p['qi'] for p in party]+[0]*(3-len(party))
    vi=['single','fallen','wrap','multi','insufficient'].index(variant)
    result=-2 if skill==4 else -3 if variant=='insufficient' else 1
    lines.append('    {%d,%d,%d,%d,{%s},{%s}},' % (skill,vi,2 if variant=='multi' else 1,result,','.join(map(str,hp)),','.join(map(str,qi))))
lines+=['};','']
(Path(__file__).parent/'map_spells_native.h').write_text('\n'.join(lines),encoding='utf-8')
