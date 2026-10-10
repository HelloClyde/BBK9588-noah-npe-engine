"""Expected states from independent original firmware key captures."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2]
variants = ['single', 'wrap', 'multi', 'fallen-target', 'signed', 'group', 'inactive']
lines = [
    '/* Original firmware RAM captures; no C-produced expectations. */',
    'static const struct {',
    '    unsigned variant, casts, count;',
    '    uint16_t hp[3], qi[3], states[3][4], aliases[4];',
    '} map_status_native[] = {'
]
for vi, variant in enumerate(variants):
    events = json.loads((root / 'tmp' / f'native-map-status-40-{variant}.json').read_text())
    for event in events:
        if event['label'] not in ('cast', 'cast-again'):
            continue
        party = event['party']
        assert event['qi'] == (84 if event['label'] == 'cast' else 68)
        def field(name):
            return ','.join(str(p[name]) for p in party) + ',0' * (3-len(party))
        states = ['{' + ','.join(map(str, p['status'])) + '}' for p in party]
        states += ['{0,0,0,0}'] * (3-len(party))
        lines.append('    {%d,%d,%d,{%s},{%s},{%s},{%s}},' % (
            vi, 1 if event['label'] == 'cast' else 2, len(party),
            field('hp'), field('qi'), ','.join(states), ','.join(map(str,event['aliases']))))
lines += ['};', '']
Path(__file__).with_name('map_status_native.h').write_text('\n'.join(lines), encoding='utf-8')
