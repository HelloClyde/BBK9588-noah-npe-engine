"""Restore entry-time RAM inputs used by the existing native animation traces."""
import json
import re
from pathlib import Path

groups = {
    'group-animation': ['group_animation_native.h'],
    'enemy-animation': ['enemy_animation_native.h'],
    'enemy-animation-extra': ['enemy_animation_native.h'],
    'enemy-spell-attempts': ['enemy_spell_animation_native.h'],
    'enemy-spell-extra': ['enemy_spell_animation_native.h'],
    'party-spell-animation': ['party_spell_animation_native.h'],
    'party-spell-extra': ['party_spell_animation_native.h'],
    'revival-animation': ['revival_animation_native.h'],
    'item-animation': ['item_animation_native.h'],
    'throw-animation': ['throw_animation_native.h'],
    'character-casting': ['character_casting_native.h'],
}
entries = {}
required = set()
for stem, headers in groups.items():
    names = set()
    for header in headers:
        names.update(re.findall(r'\{"([^"]+)"', (Path('npe-sdl/tests') / header).read_text(encoding='utf-8')))
    required.update(names)
    for row in json.loads(Path('tmp/native-' + stem + '.json').read_text(encoding='utf-8-sig')):
        c = row['config']
        if c['name'] not in names:
            continue
        # reference_combine_animation.mjs uses emptyOuter alone. The other
        # reference scripts also omit a physical outer pose for zero HP.
        omit_zero = stem not in {'group-animation', 'enemy-animation', 'enemy-animation-extra'}
        outer = tuple(0 if c.get('emptyOuter') or (omit_zero and c['enemyHp'][i] == 0) else 51 for i in (0, 2))
        assert c['name'] not in entries
        entries[c['name']] = outer
assert set(entries) == required, sorted(required - set(entries))
lines = ['/* Entry RAM inputs from the unmodified original animation diagnostics. */',
         '#include <string.h>',
         'static void battle_animation_entry_fixture(NpeWorld *world,const char *name){',
         '    static const struct {const char *name;uint8_t outer[2];} cases[]={']
for name, outer in sorted(entries.items()):
    lines.append('        {' + json.dumps(name) + ',{' + ','.join(map(str, outer)) + '}},')
lines += ['    };', '    for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);++i)if(!strcmp(name,cases[i].name)){',
          '        assert(!npe_world_resident_write(world,UINT32_C(0x527626),cases[i].outer,2));return;',
          '    }', '    assert(!"Unknown native animation entry fixture");', '}', '']
Path('npe-sdl/tests/battle_animation_entry_fixture.h').write_text('\n'.join(lines),encoding='utf-8')
print(len(entries), 'native entry configurations')
