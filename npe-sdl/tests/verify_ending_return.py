"""Verify saved ending resources, original displays and necessary regressions."""
import hashlib
import json
from pathlib import Path
import re

root=Path('tmp/ending-return')
native=json.loads((root/'native/report.json').read_text())
assert native['passed'] and len(native['cases'])==6
for row in native['cases']:
    assert row['passed'] and row['stack_restored'] and not row['heap_headers_seeded']
    assert [0x2df59a,0x3a] in row['original_regions']
    for label,size in [('ram',131072),('trace',18)]:
        b=Path(row[label]).read_bytes()
        assert len(b)==size and hashlib.sha256(b).hexdigest()==row[label+'_sha256']
        assert (root/f"c-{row['input']['name']}-{label}.bin").read_bytes()==b
assert hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest()==native['original_prg_sha256']
assert hashlib.sha256(Path('tmp/tjfy-world-probe.npe').read_bytes()).hexdigest()==native['npe_sha256']
previous=json.loads(Path('tmp/walk-trigger/run-manifest.json').read_text())['hashes']
for name in ['world.c','main.c','rpg_runtime.inc','save.c']:
    assert hashlib.sha256((root/(name+'.before')).read_bytes()).hexdigest()==previous['npe-sdl/'+name]
rows=json.loads((root/'original/report.json').read_text())
assert len(rows)==4 and {(r['music'],r['kind']) for r in rows}=={(m,k) for m in [0,1] for k in ['new','load']}
total=0
for row in rows:
    for p in row['phases']:
        for i in range(len(p['commits'])):
            tail=f"{row['music']}-{row['kind']}-{p['label']}-{i}.idx"
            original=(root/'original'/tail).read_bytes()
            assert len(original)==28800 and original==(root/('c-'+tail)).read_bytes()
            total+=1
        audio=[e for e in p['events'] if e.get('type')=='audio']
        if row['kind']=='new' and row['music'] and p['label']=='new':
            assert [e['command'] for e in audio]==['stop','play'] and audio[1]['track_id']==20 and audio[1]['looping']
        else:assert not audio
assert total==24
log=(root/'sdl-save-final-check.log').read_text()
assert len(re.findall(r'frame\d+ C0 SDL0',log))==24
assert 'Ending return:four original actual-main paths,24 C/SDL frames' in log
assert 'Before fix:two of two original ending control paths differ' in (root/'before-final-check.log').read_text()
markers={
    'regression-title-save-final.log':'Real title occupied-slot first display:3 C/SDL submits',
    'regression-slot-final.log':'System slot return:33 original C/SDL commits',
    'regression-ending-final.log':'37 full-screen animation frames matching native pixels',
    'regression-shutdown-final.log':'Shutdown runtime:9 explicit/9 close paths; actual main:8 SDL quit paths verified',
    'regression-scene-final.log':'281 C/actual SDL native comparisons'
}
for path,marker in markers.items():assert marker in (root/path).read_text()
assert not re.search(r'warning:|error:',(root/'build-save-final.log').read_text(),re.I)
files=['npe-sdl/'+name for name in ['world.c','main.c','npe.c','npe.h','save.c','rpg_runtime.inc','resident_heap_owner.inc','CMakeLists.txt']]
files+=['npe-sdl/tests/'+name for name in ['reference_rpg_resource_lifecycle.py','reference_ending_return.mjs',
       'generate_ending_resource_fixture.py','generate_ending_return_fixture.py','ending_resource_probe.c',
       'sdl_ending_return_probe.c','ending_return_before_probe.c','rpg_ending_native.h','ending_return_native.h',
       'sdl_scene_animation_probe.c','verify_ending_return.py']]
files+=['npe-sdl/build/npe-sdl.exe','npe-sdl/build/sdl-ending-return-probe.exe','npe-sdl/build/ending-resource-probe.exe',
        'tmp/opennoah_wasm_bg.wasm','tmp/tianjiang-online.opennoah']
report={'passed':True,'original_numeric_cases':6,'complete_numeric_ram_bytes_per_case':131072,
        'original_display_paths':4,'original_c_sdl_frames':24,'before_fix_control_failures':2,
        'necessary_regressions':list(markers),'warm_event_word_retained':True,
        'scope':'Ending dispatcher and original heap services; original live title/new-game/occupied-slot display controls and audio requests. No complete original UI RAM, physical timing, PCM or whole-game claim.',
        'goal_complete':False,'overall_rough_estimate':80,
        'hashes':{p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in files}}
(root/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print('Verified:6 original resource cases,4 actual-main paths/24 C/SDL frames,new/read restore and5 necessary regressions')
