"""Independently compare7AC0 snapshots, modal LCDs, entry/exit and warm saves."""
import hashlib
import json
from pathlib import Path

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
prior=json.loads(Path('tmp/rpg-shutdown-report.json').read_text(encoding='utf8'))
preserved={p:d for p,d in prior['preserved_sha256'].items() if p!='npe-sdl/save.c'}
for path,digest in {**prior['immutable_sha256'],**preserved}.items():assert sha(path)==digest,path
targets=json.loads(Path('tmp/battle-selector-target-check-targets.json').read_text(encoding='utf8'))
assert len(targets)==len(set(targets))==45
prefix='tmp/battle-selector-target-final-';stamp=Path(prefix+'build.exit').stat().st_mtime_ns;codes={}
for label in ['skill-generator','item-generator','load-generator','exit-records-generator','phase-resident-generator','configure','build',*targets]:
    path=prefix+label+'.exit';assert Path(path).read_text(encoding='utf-8-sig').strip()=='0',path
    if label in targets:assert Path(path).stat().st_mtime_ns>=stamp,path
    codes[path]=0
for label in ['skill','item','load']:
    path='tmp/battle-selector-target-'+label+'-native.exit'
    assert Path(path).read_text(encoding='utf-8-sig').strip()=='0',path;codes[path]=0
log=Path(prefix+'build.log').read_text(encoding='utf-8-sig')
assert 'warning:' not in log and 'error:' not in log
for t in ['npe-sdl',*targets]:assert 'Built target '+t in log,t
evidence={};totals={}
def compare(path,expected):
    assert Path(path).stat().st_mtime_ns>=stamp,path
    assert Path(path).read_bytes()==expected,path;evidence[path]=sha(path)
def strip_target(value):
    if isinstance(value,dict):return {k:strip_target(v) for k,v in value.items() if k!='selectorTarget'}
    if isinstance(value,list):return [strip_target(v) for v in value]
    return value
for kind,menus,size,count in [('skill',(21,27),144,13),('item',(33,37),142,14)]:
    native_prefix='tmp/native-battle-'+kind+'-selector-target'
    actual_prefix='tmp/battle-selector-target-c-'+kind+'-'
    rows=json.loads(Path(native_prefix+'.json').read_text(encoding='utf8'))
    old=json.loads(Path('tmp/native-battle-'+kind+'-resources.json').read_text(encoding='utf8'))
    assert len(rows)==count and strip_target(rows)==old
    evidence[native_prefix+'.json']=sha(native_prefix+'.json')
    frames_total=states_total=outer_total=0
    for row in rows:
        name=row['name'];frames=[{'index':65535,'state':row['before']}];states=[row['before']]
        for phase in row['phases']:
            modal=[c for c in phase['commits'] if c['state']['menu'] in menus and c['state']['actor']==0]
            frames+=modal;states += [c['state'] for c in modal]+[phase['after']]
            outer_total+=len(phase['commits'])-len(modal)
        for n,f in enumerate(frames):
            suffix='-'+name+('-before' if f['index']==65535 else '-'+str(f['index']))+'.idx'
            path=native_prefix+suffix;original=Path(path).read_bytes();assert len(original)==28800
            assert original==Path('tmp/native-battle-'+kind+'-resources'+suffix).read_bytes()
            evidence[path]=sha(path);compare(actual_prefix+name+'-frame-'+str(n)+'.idx',original)
        for n,s in enumerate(states):
            expected=bytes([s['menu'],s['actor']])+s['resourceFlags'].to_bytes(2,'big')+bytes(s['bytes'])
            assert len(expected)==size+4
            compare(actual_prefix+name+'-state-'+str(n)+'.bin',expected)
            compare(actual_prefix+name+'-target-'+str(n)+'.bin',bytes([s['selectorTarget']]))
        frames_total+=len(frames);states_total+=len(states)
    totals[kind]=dict(public_paths=count,modal_pixel_commits=frames_total,global_target_snapshots=states_total,outer_lcds_excluded=outer_total)

# Strengthened existing original entry/exit/continuous references.
entry_targets=0
for stem in ['exit-records','phase-resident']:
    native_prefix='tmp/native-battle-resources-'+stem
    rows=json.loads(Path(native_prefix+'.json').read_text(encoding='utf8'))['rows']
    for index,row in enumerate(rows):
        states=[('entry',row['after']),('exit',row['menuNavigation'][-1]['after'])]
        if row.get('reentry'):
            states += [('reentry-'+phase,row['reentry'][phase]) for phase in ['retained','input','after']]
            states += [('action-'+str(i),s['after']) for i,s in enumerate(row['reentry']['actions'])]
        for route in ['direct','main']:
            for label,s in states:
                compare(native_prefix+f'-c-{index}-{route}-{label}-display-target.bin',bytes([s['displayTarget']]))
                entry_targets+=1
# Preserve previous pixel/physical-record/service/shutdown outputs while rerun.
regression_outputs=0
for path,digest in prior['evidence_sha256'].items():
    if '-c-' in path or path.startswith(('tmp/rpg-shutdown-runtime-','tmp/rpg-shutdown-close-','tmp/rpg-shutdown-main-')):
        assert Path(path).stat().st_mtime_ns>=stamp,path
        assert sha(path)==digest,path;evidence[path]=digest;regression_outputs+=1
assert regression_outputs>1900
warm=json.loads(Path('tmp/native-battle-selector-target-load.json').read_text(encoding='utf8'))
assert [r['entry'] for r in warm]==['system','title','new','empty']
for r in warm:
    assert r['beforeBytes']==r['afterBytes']==[129,166]
    base='tmp/battle-selector-target-own-'+r['entry']+'-'
    a=Path(base+'base.dat').read_bytes();b=Path(base+'altered.dat').read_bytes()
    assert a==b and a[:8]==b'NPESAV22'
    compare(base+'base.dat',a);compare(base+'altered.dat',a)
sources=['npe-sdl/main.c','npe-sdl/world.c','npe-sdl/npe.h','npe-sdl/save.c','npe-sdl/resident_resources.inc','npe-sdl/CMakeLists.txt',
         'npe-sdl/tests/reference_battle_selection_load.mjs','npe-sdl/tests/generate_battle_selector_target_load_fixture.py',
         'npe-sdl/tests/battle_selector_target_load_native.h','npe-sdl/tests/battle_menu_rows_load_probe.c',
         'npe-sdl/tests/generate_enemy_record_entry_fixture.py','npe-sdl/tests/sdl_walk_battle_entry_probe.c',
         'npe-sdl/tests/battle_resources_exit_records_native.h','npe-sdl/tests/battle_resources_phase_resident_native.h',
         'npe-sdl/tests/verify_battle_selector_target.py','tmp/run_battle_selector_target_checks.py',
         'tmp/battle-selector-target-writers-source.asm','tmp/battle-selector-target-all-writers-source.asm']
sources += [str(p).replace('\\','/') for p in Path('npe-sdl/tests').glob('*.c')
            if '3537' in p.read_text(encoding='utf8') and str(p).replace('\\','/') not in sources]
sources += ['npe-sdl/tests/sdl_battle_item_flow_probe.c']
for kind in ['skill','item']:
    sources += ['npe-sdl/tests/'+p for p in ['reference_battle_'+kind+'_record.mjs','generate_battle_'+kind+'_record_fixture.py',
                'sdl_battle_'+kind+'_record_probe.c','battle_'+kind+'_selector_target_native.h']]
report=dict(passed=True,scope='Shared7AC0 target, independent friendly/enemy movement, modal confirmation/cancel and entry/exit/continuous observations',
            routes=totals,entry_exit_reentry_global_target_snapshots=entry_targets,warm_constructors=4,own_save_pairs=4,
            regression_persisted_outputs=regression_outputs,short_checks=45,build_targets=46,build_warnings=0,
            world_bound_bytes=3537,own_save_format='NPESAV22',immutable_sha256=prior['immutable_sha256'],preserved_sha256=preserved,
            source_sha256={p:sha(p) for p in sources},evidence_sha256=evidence,exit_codes=codes,
            pending=['All other7AC0 writers/consumers, mutation and physical out-of-range targets','Other selector/execution controls and continuous actions',
                     'Complete startup/shutdown/file ordering','Original PCM/physical timing/other NPE'])
Path('tmp/battle-selector-target-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps({k:report[k] for k in ['passed','routes','entry_exit_reentry_global_target_snapshots','short_checks','world_bound_bytes']}))
