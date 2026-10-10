"""Compile original-runtime observations into C data; no C-derived expectations."""
import json
from pathlib import Path

cases = [('cancel', 2), ('back', 2), ('use', 2), ('victory', 2),
         ('success-flee', 2), ('loss', 2), ('three', 3),
         ('failed-flee', 2), ('group', 2), ('skip', 2)]
labels = {'entry', 'empty-confirm', 'cancel', 'empty-again', 'cancel-again',
          'previous', 'settled', 'battle-exited', 'flee-result', 'loss-result', 'empty-next'}
lines = ['/* Original native empty-item fixtures: tmp/battle-native-empty-item.mjs.',
         '   Independent formation/inventory/attribute samples, not story saves. */',
         'typedef struct { const char *variant, *label;',
         '    unsigned party, occurrence, actor, mode, name; } NativeEmptyPhase;',
         'static const NativeEmptyPhase native_empty_phases[] = {']
for variant, party in cases:
    events = json.loads(Path(f'tmp/native-empty-{variant}-{party}.json').read_text())
    seen = {}
    for e in events:
        if e['label'] not in labels:
            continue
        occurrence = seen.get(e['label'], 0)
        seen[e['label']] = occurrence + 1
        # Loss RNG differs across implementations: retain only the terminal result.
        if e['label'] == 'loss-result' and e['mode'] == 3:
            continue
        if e['label'] == 'loss-result':
            occurrence = 0
        lines.append('    {"%s", "%s", %d, %d, %d, %d, %d},' %
                     (variant, e['label'], party, occurrence, e['actor'], e['mode'], e['mapName']))
lines += ['};', 'static const unsigned native_empty_item[] = {']
item = next(e['loaded'] for e in json.loads(Path('tmp/native-empty-use-2.json').read_text())
            if e['label'] == 'empty-confirm')
lines += ['    ' + ', '.join(str(item[k]) for k in ('id', 'type', 'flags', 'group', 'side', 'hp', 'qi', 'attack')), '};']
actor = next(e['party'][0] for e in json.loads(Path('tmp/native-empty-cancel-2.json').read_text())
             if e['label'] == 'empty-confirm')
lines += ['static const unsigned native_empty_target_qi[] = {%d, %d};' % (actor['qi'], actor['maxQi']), '']
Path('npe-sdl/tests/empty_item_native.h').write_text('\n'.join(lines), encoding='utf-8', newline='\n')
