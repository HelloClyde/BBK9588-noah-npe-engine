import hashlib,json
from pathlib import Path

report=json.loads(Path('tmp/rpg-resource-lifecycle-native/report.json').read_text(encoding='utf8'))
assert report['passed'] and len(report['cases'])==7
for field,path,known in [
    ('original_prg_sha256','tmp/NP560.PRG','d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'),
    ('npe_sha256','tmp/tjfy-world-probe.npe','df4b52df6b69e1105a5ea6100e43f35f832ed859c10cb8714879e44ad4d8cb40')]:
    assert report[field]==hashlib.sha256(Path(path).read_bytes()).hexdigest()==known
lines=['/* Unmodified original allocation/IO/cleanup results, no header seeds. */',
       'typedef struct {const char *name;uint16_t cap;int high_length,no_handles,cleanup;',
       ' const char *ram,*trace;} RpgResourceCase;',
       'static const RpgResourceCase rpg_resource_cases[]={']
for row in report['cases']:
    assert row['passed'] and row['stack_restored'] and not row['heap_headers_seeded']
    for label,size in [('ram',131072),('trace',18)]:
        data=Path(row[label]).read_bytes()
        assert len(data)==size and hashlib.sha256(data).hexdigest()==row[label+'_sha256']
    c=row['input']
    lines.append(' {%s,%d,%d,%d,%d,%s,%s},'%(json.dumps(c['name']),c['cap'],c['high_length'],c['no_handles'],c['cleanup'],json.dumps(row['ram']),json.dumps(row['trace'])))
lines.append('};')
Path('npe-sdl/tests/rpg_resource_lifecycle_native.h').write_text('\n'.join(lines)+'\n',encoding='utf8')
print('Validated 7 original RPG resource lifecycle snapshots')
