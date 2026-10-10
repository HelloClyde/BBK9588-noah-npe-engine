"""Check the saved original frames, numeric reference and bounded C/SDL runs."""
import hashlib
import json
from pathlib import Path
import re
import struct

root=Path('tmp/walk-trigger')
manifest=json.loads((root/'run-manifest.json').read_text())
assert all(value==0 for value in manifest['exit_codes'].values())
for path,expected in manifest['hashes'].items():
    assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==expected,path
assert hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest()=='d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
assert hashlib.sha256(Path('tmp/tjfy-world-probe.npe').read_bytes()).hexdigest()=='df4b52df6b69e1105a5ea6100e43f35f832ed859c10cb8714879e44ad4d8cb40'
previous=json.loads(Path('tmp/simple-events/run-manifest.json').read_text())['hashes']['npe-sdl/world.c']
assert hashlib.sha256((root/'world-before.c').read_bytes()).hexdigest()==previous
reference=json.loads(Path('tmp/native-walk-trigger-resident.json').read_text())
native=json.loads((root/'native/report.json').read_text())
assert native['passed'] and len(native['cases'])==len(reference['services'])==10
assert len(reference['loads'])==8 and len(reference['walks'])==6
for row,result in zip(reference['services'],native['cases']):
    assert row['name']==result['name'] and result['passed'] and result['full_segment_checked']
    before=(root/f"native/{row['name']}.before.bin").read_bytes()
    after=(root/f"native/{row['name']}.after.bin").read_bytes()
    assert len(before)==len(after)==65536
    assert before[0x7490:0x74f4]==after[0x7490:0x74f4]==bytes(row['before']['cache'])
    assert struct.unpack_from('>HH',after,0x7934)==(result['mode'],result['event'])
    assert struct.unpack_from('>H',after,0x7ab4)[0]==1
    assert row['after']['event']==result['event']
total=0
for row in reference['services']+reference['walks']:
    for i in range(len(row['commits'])):
        original=Path(f"tmp/native-walk-trigger-{row['name']}-{i}.idx").read_bytes()
        current=(root/f"c-{row['name']}-{i}.idx").read_bytes()
        assert len(original)==len(current)==28800 and original==current
        total+=1
assert total==40
log=(root/'check-save-final.log').read_text()
assert len(re.findall(r'frame\d+ C0 SDL0',log))==40
assert '40 original C/SDL frames,warm/cold lifetime and aliases passed' in log
assert 'Before fix:10 of10 original mode20 services differ' in (root/'before-check-final.log').read_text()
markers={'trigger':'Main original walking/battle commits=71, zero differences',
         'cache':'3 original live-cache paths, 14 C/SDL frames',
         'teleport':'Teleport followup:six original real/diagnostic paths,54 C/SDL commits',
         'menu':'System menu return:77 original C/SDL commits',
         'special-save':'special-map-save four checksum-valid malformed V24 slots rejected transactionally'}
for name,marker in markers.items():
    assert marker in (root/('regression-special-save.log' if name=='special-save' else f'regression-final-{name}.log')).read_text()
assert not re.search(r'warning:|error:',(root/'build-save-final.log').read_text(),re.I)
assert not re.search(r'warning:|error:',(root/'map-save-build.log').read_text(),re.I)
assert all(f'\"map\":{m},\"warm_diff\":0,\"cold_diff\":0,\"move_diff\":0' in (root/'regression-special-save.log').read_text() for m in [139,140])
report={'passed':True,'original_loads':8,'original_numeric_services':10,
        'original_walking_paths':6,'original_c_sdl_frames':40,
        'before_fix_numeric_failures':10,'necessary_regressions':list(markers),
        'scope':'100-byte trigger table, numeric mode/event/init, displayed event/view/menu fields and complete LCD; no complete C RAM, physical timing or PCM claim',
        'legacy_self_save_migration_checked':True,'goal_complete':False,'overall_rough_estimate':80,
        'hashes':manifest['hashes']}
(root/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print('Verified:8 original loads,10 numeric services,6 walking paths,40 original C/SDL frames,legacy own saves and5 necessary regressions')
