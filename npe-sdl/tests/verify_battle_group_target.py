"""Re-read original/C physical records, public target states and LCDs."""
import hashlib,json
from pathlib import Path

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
prior=json.loads(Path('tmp/battle-selector-target-report.json').read_text(encoding='utf8'))
preserved={**prior['preserved_sha256'],'npe-sdl/save.c':prior['source_sha256']['npe-sdl/save.c']}
for path,digest in {**prior['immutable_sha256'],**preserved}.items():assert sha(path)==digest,path
targets=json.loads(Path('tmp/battle-group-target-check-targets.json').read_text(encoding='utf8'))
assert len(targets)==len(set(targets))==49
prefix='tmp/battle-group-target-final-';stamp=Path(prefix+'build.exit').stat().st_mtime_ns;codes={}
for label in ['prepare-generator','group-generator','combine-generator','configure','build','repair-build',*targets]:
    path=prefix+label+'.exit';assert Path(path).read_text(encoding='utf-8-sig').strip()=='0',path
    if label in targets:assert Path(path).stat().st_mtime_ns>=stamp,path
    codes[path]=0
for path in ['tmp/battle-group-prepare-native.exit','tmp/battle-group-target-combine-native.exit','tmp/battle-group-target-repeat-native.exit']:
    assert Path(path).read_text(encoding='utf-8-sig').strip()=='0',path;codes[path]=0
log=Path(prefix+'build.log').read_text(encoding='utf-8-sig')
assert 'warning:' not in log and 'error:' not in log
for target in ['npe-sdl',*targets]:assert 'Built target '+target in log,target
repair_log=Path(prefix+'repair-build.log').read_text(encoding='utf-8-sig')
assert 'warning:' not in repair_log and 'error:' not in repair_log and 'Built target sdl-group-attack-probe' in repair_log
evidence={}
def keep(path):
    path=str(path).replace('\\','/');evidence[path]=sha(path)
def compare(path,expected):
    assert Path(path).stat().st_mtime_ns>=stamp,path
    assert Path(path).read_bytes()==expected,path;keep(path)
native=Path('tmp/battle-group-prepare-native');ref=json.loads((native/'report.json').read_text(encoding='utf8'))
assert ref['passed'] and len(ref['cases'])==18
assert ref['original_prg_sha256']==prior['immutable_sha256']['tmp/NP560.PRG']
original=Path('tmp/NP560.PRG').read_bytes();keep(native/'report.json')
for row in ref['cases']:
    name=row['input']['name'];before=native/(name+'-before-records.bin');after=native/(name+'-after-records.bin');ram=native/(name+'-after-ram.bin')
    assert len(before.read_bytes())==len(after.read_bytes())==888
    assert len(ram.read_bytes())==131072 and ram.read_bytes()[0x16450:0x16450+888]==after.read_bytes()
    assert [ram.read_bytes()[a-0x510000] for a in (0x527a9e,0x527aae,0x527ac0)]==row['after_controls']
    firmware=(native/(name+'.prg')).read_bytes()
    for address,size in ((0x278b94,0x342),(0x2c8178,0x1e0),(0x2c835c,12),(0x3307b0,0x90)):
        assert firmware[address-0x30000:address-0x30000+size]==original[address-0x30000:address-0x30000+size]
    assert firmware[0x2c8358-0x30000:0x2c835c-0x30000]==b'\x10\x1a\x10\x1a'
    cpu=(native/(name+'.cpu.txt')).read_text(encoding='utf8')
    assert row['stack_restored'] and all(s in cpu for s in ['R12=600d','R15=6000','ROM_MISSING=none','UNSUPPORTED=none'])
    for path in [before,after,ram,native/(name+'.prg'),native/(name+'.cpu.txt')]:keep(path)
    compare('tmp/battle-group-prepare-c-'+name+'-records.bin',after.read_bytes())
    compare('tmp/battle-group-prepare-c-'+name+'-controls.bin',bytes(row['after_controls']))
group=json.loads(Path('tmp/native-group-animation.json').read_text(encoding='utf8'));assert len(group)==6;keep('tmp/native-group-animation.json')
group_targets=0
for row in group:
    name=row['config']['name'];events=row['events']
    for route in ['direct','main']:
        for phase,event in enumerate([events[0],events[-2],events[-1]]):
            compare(f'tmp/battle-group-target-c-{name}-{route}-phase-{phase}.bin',bytes([event['displayTarget']]))
            group_targets+=1
    assert len(json.loads(Path('tmp/native-'+name+'-commits.json').read_text(encoding='utf8')))==6+9*(1 if row['config'].get('sleepSlot') is not None else row['config']['party'])
    keep('tmp/native-'+name+'-commits.json')
    for path in Path('tmp').glob('native-'+name+'-*.idx'):keep(path)

def clean(value):
    if isinstance(value,dict):return {k:clean(v) for k,v in value.items() if k!='displayTarget'}
    if isinstance(value,list):return [clean(v) for v in value]
    if isinstance(value,str):return value.removesuffix('-selector-target')
    return value
combine_targets=combine_pixels=combine_paths=0
for stem,expected_paths in [('combine-animation',6),('combine-repeat',2)]:
    path='tmp/native-'+stem+'-selector-target.json';rows=json.loads(Path(path).read_text(encoding='utf8'))
    old=json.loads(Path('tmp/native-'+stem+'.json').read_text(encoding='utf8'))
    assert len(rows)==expected_paths and clean(rows)==clean(old);keep(path);keep('tmp/native-'+stem+'.json')
    combine_paths+=len(rows)
    for row,previous in zip(rows,old):
        name=row['config']['name'];old_name=previous['config']['name'];c_name=old_name.removesuffix('-animation')
        for phase,event in enumerate(row['events']):
            native_path='tmp/native-'+name+'-'+str(phase)+'.idx';old_path='tmp/native-'+old_name+'-'+str(phase)+'.idx'
            assert Path(native_path).read_bytes()==Path(old_path).read_bytes();keep(native_path);keep(old_path)
            for route in ['direct','main']:
                compare(f'tmp/battle-combine-target-c-{c_name}-{route}-phase-{phase}.bin',bytes([event['displayTarget']]))
                combine_targets+=1
        new_commits=json.loads(Path('tmp/native-'+name+'-commits.json').read_text(encoding='utf8'))
        old_commits=json.loads(Path('tmp/native-'+old_name+'-commits.json').read_text(encoding='utf8'))
        assert len(new_commits)==len(old_commits)==28*row['config'].get('rounds',1)
        for commit in new_commits:
            n=commit['index'];a='tmp/native-'+name+'-commit-'+str(n)+'.idx';b='tmp/native-'+old_name+'-commit-'+str(n)+'.idx'
            assert len(Path(a).read_bytes())==28800 and Path(a).read_bytes()==Path(b).read_bytes();keep(a);keep(b);combine_pixels+=1
        keep('tmp/native-'+name+'-commits.json');keep('tmp/native-'+old_name+'-commits.json')
regression_outputs=0
for path,digest in prior['evidence_sha256'].items():
    assert sha(path)==digest,path
    if '-c-' in path or path.startswith(('tmp/rpg-shutdown-runtime-','tmp/rpg-shutdown-close-','tmp/rpg-shutdown-main-','tmp/battle-selector-target-own-')):
        assert Path(path).stat().st_mtime_ns>=stamp,path;regression_outputs+=1
    evidence[path]=digest
assert regression_outputs>2600
sources=['npe-sdl/world.c','npe-sdl/npe.h','npe-sdl/CMakeLists.txt','npe-sdl/tests/reference_battle_group_prepare.py',
         'npe-sdl/tests/generate_battle_group_prepare_fixture.py','npe-sdl/tests/battle_group_prepare_native.h',
         'npe-sdl/tests/battle_group_prepare_probe.c','npe-sdl/tests/reference_combine_animation.mjs',
         'npe-sdl/tests/generate_group_animation_native.py','npe-sdl/tests/group_animation_native.h',
         'npe-sdl/tests/generate_battle_combine_target_fixture.py','npe-sdl/tests/battle_combine_targets_native.h',
         'npe-sdl/tests/sdl_group_animation_probe.c','npe-sdl/tests/sdl_combine_attack_probe.c',
         'npe-sdl/tests/sdl_group_attack_probe.c',
         'npe-sdl/tests/verify_battle_group_target.py','tmp/run_battle_group_target_checks.py',
         'tmp/battle-group-assignment-source.asm','tmp/battle-combine-entry-source.asm']
report=dict(passed=True,scope='Original group/combine preparation records and shared actor/kind/target; six group and eight combine public paths',
            original_preparation_cases=18,full_physical_records=108,preparation_controls=54,
            group_public_paths=6,group_target_snapshots=group_targets,combine_public_paths=combine_paths,
            combine_target_snapshots=combine_targets,combine_original_lcds_rechecked=combine_pixels,
            group_animation_commits=153,combine_animation_commits=280,regression_persisted_outputs=regression_outputs,
            short_checks=49,build_targets=50,build_warnings=0,world_bound_bytes=3537,own_save_format='NPESAV22',
            immutable_sha256=prior['immutable_sha256'],preserved_sha256=preserved,
            source_sha256={p:sha(p) for p in sources},evidence_sha256=evidence,exit_codes=codes,
            pending=['Other global7AC0 consumers and physical out-of-range mutations','Group mirrored words7AA0/7AA6/7AAA and other execution controls',
                     'Full anomalous entry/rounds for counts0/255','Other continuous actions and complete record reuse','Original PCM/physical timing/other NPE'])
Path('tmp/battle-group-target-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps({k:report[k] for k in ['passed','original_preparation_cases','full_physical_records','group_target_snapshots','combine_target_snapshots','short_checks']}))
