"""Generate expectations solely from independent original-runtime captures."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2]
lines = ['/* Original runtime entry/exit, isolated party/equipment fixtures. */',
         'static const struct { const char *route; unsigned count; uint16_t before[3][4], entry[3][4], exit[3][4], input[3][6], battle[3][6]; } battle_lifecycle_native[] = {']
for route, count in [('win', 1), ('win', 3), ('flee', 1), ('flee', 3),
                     ('loss', 1), ('loss', 3), ('win-fallen', 3), ('flee-buff', 1)]:
    events = json.loads((root / f'tmp/native-equipment-lifecycle-{route}-{count}.json').read_text())
    phases = {event['label']: event for event in events}
    def rows(phase, field):
        return '{' + ','.join('{' + ','.join(map(str, p[field])) + '}'
                              for p in phases[phase]['party']) + '}'
    lines.append('    {"%s",%d,%s,%s,%s,%s,%s},' %
                 (route, count, rows('before-entry', 'map'), rows('entry', 'map'),
                  rows('exit', 'map'), rows('before-exit', 'battle'), rows('exit', 'battle')))
lines += ['};', '']
Path(__file__).with_name('battle_lifecycle_native.h').write_text('\n'.join(lines), encoding='utf-8')
