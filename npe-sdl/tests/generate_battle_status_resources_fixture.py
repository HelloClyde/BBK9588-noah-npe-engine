import hashlib
import json
from pathlib import Path

report=json.loads(Path('tmp/battle-status-resource-native/report.json').read_text(encoding='utf-8'))
assert report['passed'] and len(report['cases'])==5
for key,path in [('original_prg_sha256','tmp/NP560.PRG'),('npe_sha256','tmp/tjfy-world-probe.npe')]:
    assert report[key]==hashlib.sha256(Path(path).read_bytes()).hexdigest()
out=['/* Original heap services and packed loader; independent caller inputs. */',
     'typedef struct {const char *name;uint16_t cap;int high_length,no_handles,cleanup;unsigned cycles;int released;const char *ram,*trace;} RpgResourceCase;',
     'static const RpgResourceCase rpg_resource_cases[]={']
for row in report['cases']:
    assert row['passed'] and row['stack_restored'] and not row['heap_headers_seeded']
    for label,length in [('ram',131072),('trace',18)]:
        raw=Path(row[label]).read_bytes()
        assert len(raw)==length and hashlib.sha256(raw).hexdigest()==row[label+'_sha256']
    c=row['input']
    out.append('{'+json.dumps(c['name'])+','+','.join(str(int(c[k])) for k in
               ['cap','high_length','no_handles','cleanup','cycles','released'])+','+
               json.dumps(row['ram'])+','+json.dumps(row['trace'])+'},')
out.append('};')
Path('npe-sdl/tests/battle_status_resource_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
loads=json.loads(Path('tmp/native-battle-resource-flags-load.json').read_text(encoding='utf-8'))
assert [r['entry'] for r in loads]==['system','title','new','empty']
assert all(r['beforeBytes']==r['afterBytes']==[129,166] for r in loads)
out=['/* Four original warm constructors retain both resource flag bytes. */',
     'static const MenuRowsLoad menu_rows_load[]={']
out += ['{'+json.dumps(r['entry'])+',{129,166},{129,166}},' for r in loads]
out.append('};')
Path('npe-sdl/tests/battle_resource_flags_load_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
print('Validated five original resource cycles and four warm constructors')
