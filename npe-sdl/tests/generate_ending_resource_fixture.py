import hashlib
import json
from pathlib import Path

r=json.loads(Path('tmp/ending-return/native/report.json').read_text())
assert r['passed'] and len(r['cases'])==6
assert hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest()==r['original_prg_sha256']
assert hashlib.sha256(Path('tmp/tjfy-world-probe.npe').read_bytes()).hexdigest()==r['npe_sha256']
out=['/* Unmodified original ending dispatcher branch and heap allocations. */',
     'typedef struct {const char *name;uint16_t cap;int high_length,no_handles,cleanup;unsigned flags;int zero_map,zero_battle;unsigned repeats;int reallocate;const char *ram,*trace;} RpgResourceCase;',
     'static const RpgResourceCase rpg_resource_cases[]={']
for row in r['cases']:
    assert row['passed'] and row['stack_restored'] and not row['heap_headers_seeded']
    assert [0x2df59a,0x3a] in row['original_regions']
    for label,size in [('ram',131072),('trace',18)]:
        raw=Path(row[label]).read_bytes()
        assert len(raw)==size and hashlib.sha256(raw).hexdigest()==row[label+'_sha256']
    c=row['input']
    out.append('{'+json.dumps(c['name'])+','+','.join(str(int(c[k])) for k in
               ['cap','high_length','no_handles','cleanup','flags','zero_map','zero_battle','repeats','reallocate'])+','+json.dumps(row['ram'])+','+json.dumps(row['trace'])+'},')
out.append('};')
Path('npe-sdl/tests/rpg_ending_native.h').write_text('\n'.join(out)+'\n')
print('Validated six original ending cases, complete128KiB RAM and IO trace')
