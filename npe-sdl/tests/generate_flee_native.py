"""Convert recorded original-runtime traces; never infer damage from C output."""
import json
from pathlib import Path

cases = [(1, 0, 'scripted'), (2, 1, 'scripted'), (3, 2, 'scripted'),
         (3, 2, 'fallen'), (2, 1, 'asleep'), (2, 1, 'random-retry')]
lines = ['/* Original runtime traces: tmp/battle-native-flee.mjs. Independent',
         '   formation/attribute fixtures; original NPE and PRG unchanged.',
         '   Damage/RNG states are not compared across unsynchronized seeds. */',
         'typedef struct {',
         '    unsigned party, flee_actor, blocked, random, next_actor, quantity;',
         '    unsigned attack_count, attack_order[3];',
         '} NativeFleeCase;',
         'static const NativeFleeCase native_flee_cases[] = {']
for party, actor, variant in cases:
    events = json.loads(Path(f'tmp/native-flee-{party}-{actor}-{variant}.json').read_text())
    start = max(i for i, e in enumerate(events) if e['label'].startswith('before-flee'))
    events = events[start:]
    setup = next(e for e in events if all(p['kind'] == 0 and p['parameter2'] == 1 for p in e['party']))
    assert all(p['target'] == party + 1 for p in setup['party'])
    order = []
    previous = events[0]
    for event in events[1:]:
        if event['enemy'][0] < previous['enemy'][0]:
            assert 1 <= event['executing'] <= party
            order.append(event['executing'] - 1)
        previous = event
    final = events[-1]
    assert final['mode'] == 3 and order
    padded = order + [0] * (3 - len(order))
    blocked = {'fallen': 1, 'asleep': 2}.get(variant, 0)
    lines.append('    {%d, %d, %d, %d, %d, %d, %d, {%s}}, /* %s */' %
                 (party, actor, blocked, int(variant.startswith('random')), final['actor'],
                  final['quantity'], len(order), ', '.join(map(str, padded)), variant))
lines += ['};', '']
Path('npe-sdl/tests/flee_native.h').write_text('\n'.join(lines), encoding='utf-8', newline='\n')
