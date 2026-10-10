import hashlib
import json
from pathlib import Path

r=json.loads(Path('tmp/rpg-shutdown-native/report.json').read_text(encoding='utf8'))
assert r['passed'] and len(r['cases'])==9
for key,p in [('original_prg_sha256','tmp/NP560.PRG'),('npe_sha256','tmp/tjfy-world-probe.npe')]:
    assert r[key]==hashlib.sha256(Path(p).read_bytes()).hexdigest()
out=['/* Original unmodified shutdown slice and heap services. */',
     'typedef struct {const char *name;uint16_t cap;int high_length,no_handles,cleanup;unsigned flags;int zero_map,zero_battle;unsigned repeats;const char *ram,*trace;} RpgResourceCase;',
     'static const RpgResourceCase rpg_resource_cases[]={']
for row in r['cases']:
    assert row['passed'] and row['stack_restored'] and not row['heap_headers_seeded']
    assert (0x1fd50e,0x5a) in [tuple(p) for p in row['original_regions']]
    for label,length in [('ram',131072),('trace',18)]:
        raw=Path(row[label]).read_bytes();assert len(raw)==length and hashlib.sha256(raw).hexdigest()==row[label+'_sha256']
    c=row['input'];out.append('{'+json.dumps(c['name'])+','+','.join(str(int(c[k])) for k in
             ['cap','high_length','no_handles','cleanup','flags','zero_map','zero_battle','repeats'])+','+json.dumps(row['ram'])+','+json.dumps(row['trace'])+'},')
out.append('};');Path('npe-sdl/tests/rpg_shutdown_native.h').write_text('\n'.join(out)+'\n',encoding='utf8')
print('Validated nine original shutdown cases, complete RAM and IO trace')
