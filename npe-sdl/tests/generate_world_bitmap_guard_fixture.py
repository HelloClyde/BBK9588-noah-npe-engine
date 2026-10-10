import json
from pathlib import Path

rows = json.loads(Path('tmp/native-world-bitmap-guard.json').read_text(encoding='utf8'))['rows']
assert len(rows)==6
fields = ('id', 'graphic', 'first', 'last', 'direction', 'x', 'y', 'moving')
out = ['/* Generated from real firmware periodic LCD input/output. */',
       'static const CompositorCase compositor_cases[] = {']
for row in rows:
    out.append('    {"%s", %d, {' % (row['name'],row['before']['heroY']))
    for which in ('before', 'after'):
        for npc in row[which]['npc']:
            out.append('        {' + ','.join(str(npc[f]) for f in fields) + '},')
        out.append('    }, {' if which == 'before' else '    }},')
out.append('};')
Path('npe-sdl/tests/world_bitmap_guard_native.h').write_text('\n'.join(out)+'\n', newline='\n')
