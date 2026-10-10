"""Extract diagnostic inputs from the unchanged original mode4 sampler."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2]
directory = root / 'tmp/native-mode4-rejected-entry'
report = json.loads((directory / 'report.json').read_text(encoding='utf8'))
assert report['original'] and report['diagnostic_controls']
rows = report['rows']
common = ('map', 'progress', 'sx', 'sy', 'vx', 'vy', 'direction', 'npc')
for row in rows:
    assert not row['init'] and row['afterInit']['init'] == 0
    assert row['phases'][-1]['after']['mode'] == 1
    assert all(row['before'][key] == rows[0]['before'][key] for key in common)
    if row['ui'] != 1:
        assert (directory / f"{row['name']}-0.idx").read_bytes() == (
            directory / f"{row['name']}-before.idx").read_bytes()

text = ['/* Diagnostic inputs sampled from the unchanged original engine. */',
        'typedef struct {const char *name;unsigned context,ui,kind;} Mode4RejectedCase;',
        'static const Mode4RejectedCase mode4_rejected_cases[]={']
for row in rows:
    text.append('    {"%s",%u,%u,%u},' % (row['name'], row['context'], row['ui'], row['kind']))
text.extend(['};', 'static const unsigned mode4_rejected_view[]={'])
before = rows[0]['before']
text.extend(['    ' + ','.join(str(before[key]) for key in common[:-1]) + ',', '};',
             'static const unsigned mode4_rejected_npc[20][3]={'])
for npc in before['npc']:
    text.append('    {%u,%u,%u},' % (npc['x'], npc['y'], npc['direction']))
text.append('};')
(root / 'npe-sdl/tests/mode4_rejected_entry_native.h').write_text(
    '\n'.join(text) + '\n', encoding='utf8')
