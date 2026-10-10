import hashlib, json
from pathlib import Path

report = json.loads(Path('tmp/resident-heap-owner-native/report.json').read_text(encoding='utf8'))
assert report['passed'] and len(report['cases']) == 6
assert report['original_prg_sha256'] == hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest() == 'd555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
lines = ['/* Independent original owner-helper and launcher-prefix RAM snapshots. */',
         'typedef struct {const char *name;uint32_t request;int prefix,no_handles,no_space;',
         ' const char *ram,*trace;} HeapOwnerCase;',
         'static const HeapOwnerCase heap_owner_cases[]={']
for row in report['cases']:
    assert row['passed'] and row['stack_restored']
    for label, size in [('ram', 131072), ('trace', 6)]:
        content = Path(row[label]).read_bytes()
        assert len(content) == size and hashlib.sha256(content).hexdigest() == row[label + '_sha256']
    c = row['input']
    lines.append(' {%s,UINT32_C(%d),%d,%d,%d,%s,%s},' %
                 (json.dumps(c['name']), c['request'], c['prefix'], c['no_handles'], c['no_space'], json.dumps(row['ram']), json.dumps(row['trace'])))
lines.append('};')
Path('npe-sdl/tests/resident_heap_owner_native.h').write_text('\n'.join(lines) + '\n', encoding='utf8')
print('Validated 6 original full-RAM resource owner cases')
