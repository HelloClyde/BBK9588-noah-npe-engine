import hashlib
import json
import re
from pathlib import Path

root=Path('tmp/title-slot-controls')
rows=json.loads((root/'original/report.json').read_text(encoding='utf-8'))
assert [r['name'] for r in rows]==['cancel','empty']
frames=0
for row in rows:
    for p in row['phases']:
        sounds=[e for e in p['events'] if e.get('type')=='audio']
        if p['label']=='new-from-empty':
            assert [e['command'] for e in sounds]==['stop','play']
            assert sounds[1]['track_id']==20 and sounds[1]['looping']
        else:
            assert not sounds
        for i in range(len(p['commits'])):
            expected=(root/f"original/{row['name']}-{p['label']}-{i}.idx").read_bytes()
            actual=(root/f"c-{row['name']}-{p['label']}-{i}.idx").read_bytes()
            assert len(expected)==28800 and expected==actual
            frames+=1
assert frames==22
for label in ['down','open']:
    assert (root/f'c-read-failure-{label}-0.idx').read_bytes()==(root/f'original/empty-{label}-0.idx').read_bytes()
log=(root/'check-final.log').read_text(encoding='utf-8')
assert len(re.findall(r'frame\d+ C0 SDL0',log))==24
assert 'empty-slot new game and occupied read failure passed' in log
before=(root/'before.log').read_text(encoding='utf-8')
assert 'controls 1,2,2 original 2,2,2' in before and 'Assertion failed' in before
assert 'frame0 C8 SDL8' in (root/'first-fix.log').read_text(encoding='utf-8')
regressions={
 'regression-ending-final.log':'24 C/SDL frames',
 'regression-title-load-final.log':'3 C/SDL submits',
 'regression-system-slots-final.log':'33 original C/SDL commits',
 'regression-scene-final.log':'281 C/actual SDL native comparisons',
}
for name,marker in regressions.items():
    text=(root/name).read_text(encoding='utf-8')
    assert marker in text and 'Assertion failed' not in text
for name in ['build-final.log','build-probe-final.log']:
    assert not re.search(r'warning:|error:',(root/name).read_text(encoding='utf-8'),re.I)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
previous=json.loads(Path('tmp/ending-return/report.json').read_text(encoding='utf-8'))
assert sha(root/'main.c.before')==previous['hashes']['npe-sdl/main.c']
assert sha('npe-sdl/world.c')==previous['hashes']['npe-sdl/world.c']
assert sha('tmp/NP560.PRG')=='d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
inputs=['npe-sdl/main.c','npe-sdl/build/npe-sdl.exe',
        'npe-sdl/tests/reference_title_slot_controls.mjs',
        'npe-sdl/tests/sdl_title_slot_controls_probe.c',
        'npe-sdl/tests/title_slot_controls_native.h',
        'tmp/tjfy-world-probe.npe','tmp/tianjiang-online.opennoah','tmp/opennoah_wasm_bg.wasm']
report={'passed':True,'original_paths':2,'unique_original_frames':frames,
        'c_sdl_comparisons':24,'necessary_regressions':list(regressions),
        'occupied_read_failure':'Own occupied file removed after chooser metadata; mode1, exit word1, stop and no chooser redraw checked. Original failure branch supported by static PRG, not a new original filesystem failure run.',
        'goal_complete':False,'overall_rough_estimate':80,
        'hashes':{p:sha(p) for p in inputs}}
(root/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('Verified:22 unique original title/slot frames,24 C/SDL comparisons,actual own read failure and four necessary regressions')
