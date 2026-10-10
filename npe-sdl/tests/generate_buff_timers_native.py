"""Capture original runtime data as C constants; no C-derived expectations."""
import json
from pathlib import Path

variants = ['mixed', 'refresh', 'defend', 'expiry', 'group-order', 'caps', 'wrap', 'death', 'fallen', 'fallen-item']
lines = ['/* Independent original observations from tmp/battle-native-buff-timers.mjs. */',
         'typedef struct { const char *variant; unsigned phase, member, party;',
         '    uint16_t attack, defense, attack_timer, defense_timer, qi, base_attack, base_defense;',
         '    unsigned fallen; } NativeBuffPhase;',
         'static const NativeBuffPhase native_buff_phases[] = {']
for variant in variants:
    events = json.loads(Path(f'tmp/native-buff-{variant}.json').read_text())
    for e in events:
        phase = 0 if e['label'] == 'entry' else int(e['label'][5:]) + 1
        for member, p in enumerate(e['party']):
            values = [phase, member, len(e['party'])] + [p[k] for k in
                      ['attack', 'defense', 'attackTimer', 'defenseTimer', 'qi', 'baseAttack', 'baseDefense']] + [int(p['hp'] == 0)]
            lines.append('    {"%s", %s},' % (variant, ', '.join(map(str, values))))
lines += ['};', '']
Path('npe-sdl/tests/buff_timers_native.h').write_text('\n'.join(lines), encoding='utf-8', newline='\n')
print('Compiled', len(lines) - 7, 'native party state observations.')
