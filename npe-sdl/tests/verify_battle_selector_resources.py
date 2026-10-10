"""Recompare persisted modal LCDs, shared controls and current resource caches.

Public originals supply the inputs; outer controller LCDs are excluded by the
modal probe. The previously verified original heap/loader fixtures remain a
separate service proof, not a claim of complete boot/UI RAM equivalence.
"""
import hashlib
import json
from pathlib import Path

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

prior=json.loads(Path('tmp/battle-status-resources-report.json').read_text(encoding='utf8'))
preserved=json.loads(Path('tmp/battle-selector-preserved-inputs.json').read_text(encoding='utf8'))
for path,digest in {**prior['immutable_sha256'],**preserved}.items():assert sha(path)==digest,path
targets=json.loads(Path('tmp/battle-selector-resources-check-targets.json').read_text(encoding='utf8'))
assert len(targets)==36 and len(set(targets))==36
codes={}
paths=['tmp/battle-selector-'+kind+part+'-native.exit' for kind in ['skill','item'] for part in ['', '-extra']]
paths+=['tmp/battle-selector-resources-final-'+label+'.exit' for label in ['skill-generator','item-generator','configure','build',*targets]]
if Path('tmp/battle-selector-resources-final-repair-build.exit').exists():paths.append('tmp/battle-selector-resources-final-repair-build.exit')
for path in paths:
    assert Path(path).read_text(encoding='utf-8-sig').strip()=='0',path
    codes[path]=0
build_stamp=Path('tmp/battle-selector-resources-final-build.exit').stat().st_mtime_ns
for target in targets:
    assert Path('tmp/battle-selector-resources-final-'+target+'.exit').stat().st_mtime_ns>=build_stamp,target
build=Path('tmp/battle-selector-resources-final-build.log').read_text(encoding='utf-8-sig')
assert 'warning:' not in build and 'error:' not in build
for target in ['npe-sdl',*targets]:assert 'Built target '+target in build,target
if Path('tmp/battle-selector-resources-final-repair-build.log').exists():
    repair=Path('tmp/battle-selector-resources-final-repair-build.log').read_text(encoding='utf-8-sig')
    assert 'warning:' not in repair and 'error:' not in repair
evidence={};totals={}

def compare(path,expected):
    assert Path(path).stat().st_mtime_ns>=build_stamp,path
    assert Path(path).read_bytes()==expected,path
    evidence[path]=sha(path)

for kind,menus,size,count in [('skill',(21,27),144,13),('item',(33,37),142,14)]:
    native_prefix='tmp/native-battle-'+kind+'-resources'
    actual_prefix='tmp/battle-selector-resources-c-'+kind+'-'
    rows=json.loads(Path(native_prefix+'.json').read_text(encoding='utf8'))
    assert len(rows)==count and len({r['name'] for r in rows})==count
    assert {'repeat-party-target','party-confirm','enemy-confirm' if kind=='skill' else 'throw-confirm'}<={r['name'] for r in rows}
    frames_total=states_total=outer_total=0
    for row in rows:
        name=row['name'];assert row['seed']['resourceFlags']==422 and row['before']['resourceFlags']==166
        frames=[{'index':65535,'state':row['before']}];states=[row['before']]
        for phase in row['phases']:
            modal=[c for c in phase['commits'] if c['state']['menu'] in menus and c['state']['actor']==0]
            frames+=modal;states += [c['state'] for c in modal]+[phase['after']]
            outer_total+=len(phase['commits'])-len(modal)
        for n,f in enumerate(frames):
            path=native_prefix+'-'+name+('-before' if f['index']==65535 else '-'+str(f['index']))+'.idx'
            original=Path(path).read_bytes();assert len(original)==28800
            evidence[path]=sha(path);compare(actual_prefix+name+'-frame-'+str(n)+'.idx',original)
        for n,s in enumerate(states):
            assert s['resourceFlags']==(166 if s['menu'] in (21,33) else 422),(kind,name,n)
            expected=bytes([s['menu'],s['actor']])+s['resourceFlags'].to_bytes(2,'big')+bytes(s['bytes'])
            assert len(expected)==size+4
            compare(actual_prefix+name+'-state-'+str(n)+'.bin',expected)
        assert not Path(actual_prefix+name+'-state-'+str(len(states))+'.bin').exists(),name
        frames_total+=len(frames);states_total+=len(states)
    totals[kind]=dict(public_paths=count,modal_pixel_commits=frames_total,control_cache_snapshots=states_total,outer_lcds_excluded=outer_total)
    evidence[native_prefix+'.json']=sha(native_prefix+'.json')
    log=Path('tmp/battle-selector-resources-final-sdl-battle-'+kind+'-resources-probe.log').read_text(encoding='utf-8-sig')
    assert f'{count} native paths/{frames_total} actual SDL LCD commits' in log

# Reuse, and independently recompare, the five original service cases.
native=json.loads(Path('tmp/battle-status-resource-native/report.json').read_text(encoding='utf8'))
assert native['passed'] and len(native['cases'])==5
for row in native['cases']:
    assert row['passed'] and row['stack_restored'] and not row['heap_headers_seeded']
    for label,size in [('ram',131072),('trace',18)]:
        original=Path(row[label]).read_bytes();assert len(original)==size and sha(row[label])==row[label+'_sha256']
        evidence[row[label]]=sha(row[label])
        compare('tmp/battle-status-resource-c-'+row['input']['name']+'-'+label+'.bin',original)

sources=['npe-sdl/main.c','npe-sdl/CMakeLists.txt','npe-sdl/resident_heap_owner.inc','npe-sdl/rpg_runtime.inc',
         'tmp/run_battle_selector_resources_checks.py','tmp/run_battle_selector_resources_extra.py',
         'tmp/battle-selector-resource-protocol.asm','npe-sdl/tests/verify_battle_selector_resources.py']
for kind in ['skill','item']:
    sources += ['npe-sdl/tests/'+name for name in ['reference_battle_'+kind+'_record.mjs',
                'generate_battle_'+kind+'_record_fixture.py','sdl_battle_'+kind+'_record_probe.c',
                'battle_'+kind+'_resources_native.h']]
for label in ['build','skill-check']:
    for suffix in ['.log','.exit']:
        path='tmp/battle-selector-resources-first-'+label+'-failed'+suffix
        assert Path(path).exists();evidence[path]=sha(path)
report=dict(passed=True,scope='Magic/item modal packed-resource lifecycle and full shared flag word',
            routes=totals,short_checks=len(targets),build_targets=len(targets)+1,build_warnings=0,
            world_bound_bytes=3536,own_save_format='NPESAV22',original_service_cases=5,
            pending=['Outer-controller complete LCD/RAM equivalence','All other flag writers/consumers and UI lifecycle',
                     'File failure/abnormal selectors','Original audio/physical timing/other NPE'],
            exit_codes=codes,immutable_sha256=prior['immutable_sha256'],preserved_sha256=preserved,
            source_sha256={p:sha(p) for p in sources},evidence_sha256=evidence)
Path('tmp/battle-selector-resources-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps({k:report[k] for k in ['passed','routes','short_checks','world_bound_bytes']}))
