"""Verify resource lifecycle, normal public SDL paths and warm flags.

The original service fixtures use diagnostic callers and file adapters;
the SDL fixtures use the unmodified original public controller.
"""
import hashlib
import json
from pathlib import Path


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


prefix='tmp/battle-status-resources-'
prior=json.loads(Path('tmp/battle-status-view-report.json').read_text(encoding='utf-8'))
for path,digest in prior['immutable_sha256'].items():assert sha(path)==digest,path
targets=json.loads(Path(prefix+'check-targets.json').read_text(encoding='utf-8'))
assert len(targets)==32 and len(set(targets))==32
codes={}
paths=['tmp/battle-status-resource-native.exit','tmp/battle-resource-flags-load-native.exit',
       prefix+'native.exit']
paths += [prefix+'final-'+label+'.exit' for label in
          ['resource-generator','view-generator','configure','build',*targets]]
if Path(prefix+'final-repair-build.exit').exists():paths.append(prefix+'final-repair-build.exit')
for path in paths:
    assert Path(path).read_text(encoding='utf-8-sig').strip()=='0',path
    codes[path]=0
build=Path(prefix+'final-build.log').read_text(encoding='utf-8-sig')
assert 'warning:' not in build and 'error:' not in build
if Path(prefix+'final-repair-build.log').exists():
    repair=Path(prefix+'final-repair-build.log').read_text(encoding='utf-8-sig')
    assert 'warning:' not in repair and 'error:' not in repair
for target in ['npe-sdl',*targets]:assert 'Built target '+target in build,target

evidence={}


def compare(path,expected,length):
    data=Path(path).read_bytes()
    assert len(data)==length and data==expected,path
    evidence[path]=sha(path)


native=json.loads(Path('tmp/battle-status-resource-native/report.json').read_text(encoding='utf-8'))
assert native['passed'] and len(native['cases'])==5
for row in native['cases']:
    assert row['passed'] and row['stack_restored'] and not row['heap_headers_seeded']
    for label,length in [('ram',131072),('trace',18)]:
        path=row[label];assert Path(path).stat().st_size==length and sha(path)==row[label+'_sha256']
        evidence[path]=sha(path)
        compare('tmp/battle-status-resource-c-'+row['input']['name']+'-'+label+'.bin',
                Path(path).read_bytes(),length)
    data=Path(row['ram']).read_bytes()
    assert data[0x17aa4:0x17aa6]==bytes([0 if row['input']['released'] else 1,165])
log=Path(prefix+'final-battle-status-resource-cycle-probe.log').read_text(encoding='utf-8-sig')
assert log.count('complete128KiB RAM/18-byte IO trace match, no captured heap headers')==5

rows=json.loads(Path('tmp/native-battle-status-resources.json').read_text(encoding='utf-8'))['rows']
assert [r['config']['name'] for r in rows]==['one','two','three','three-retained']
totals={}
for route in ['direct','main']:
    commits=states=records=0
    for row in rows:
        name=row['config']['name'];assert len(row['steps'])==12
        for step,sample in enumerate(row['steps']):
            s=sample['after']
            assert s['map']==row['before']['map'] and s['battle']==row['before']['battle']
            assert s['actor']==row['config']['actor'] and s['mode']==3
            assert s['resourceFlags']==(166 if s['phase']==35 else 422)
            controls=bytes(s[k] for k in ['phase','actor','member','row'])+s['resourceFlags'].to_bytes(2,'big')
            full=b''.join(bytes(r) for r in s['map']+s['battle'])
            compare(prefix+f'c-{route}-{name}-state-{step}.bin',controls+full,6+9*148)
            states+=1;records+=9
            for frame in range(len(sample['frames'])):
                ref=f'tmp/native-battle-status-resources-{name}-{step}-{frame}.idx'
                compare(prefix+f'c-{route}-{name}-{step}-{frame}.idx',Path(ref).read_bytes(),28800)
                evidence[ref]=sha(ref);commits+=1
    assert (commits,states,records)==(71,48,432)
    totals[route]=dict(pixel_commits=commits,control_states=states,full_physical_records=records)

warm=json.loads(Path('tmp/native-battle-resource-flags-load.json').read_text(encoding='utf-8'))
assert [r['entry'] for r in warm]==['system','title','new','empty']
for row in warm:
    assert row['beforeBytes']==row['afterBytes']==[129,166]
    base=f"tmp/battle-resource-flags-own-{row['entry']}-base.dat"
    altered=f"tmp/battle-resource-flags-own-{row['entry']}-altered.dat"
    original=Path(base).read_bytes();assert original[:8]==b'NPESAV22'
    compare(altered,original,len(original));evidence[base]=sha(base)

sources=['npe-sdl/'+s for s in ['main.c','world.c','save.c','npe.h','resident_resources.inc',
         'resident_heap_owner.inc','rpg_runtime.inc','CMakeLists.txt']]
sources += ['npe-sdl/tests/'+s for s in ['reference_rpg_resource_lifecycle.py',
            'rpg_resource_lifecycle_probe.c','reference_battle_status_view.mjs',
            'reference_battle_selection_load.mjs','generate_battle_status_view_fixture.py',
            'generate_battle_status_resources_fixture.py','sdl_battle_status_view_probe.c',
            'battle_menu_rows_load_probe.c','battle_status_resource_native.h',
            'battle_status_resources_native.h','battle_resource_flags_load_native.h',
            'verify_battle_status_resources.py']]
sources += [str(p).replace('\\','/') for p in Path('npe-sdl/tests').glob('*.c')
            if '3536' in p.read_text(encoding='utf-8')]
sources += ['tmp/run_battle_status_resources_checks.py']
report=dict(passed=True,scope='Status-page packed-buffer release/reallocate/read lifecycle and full shared flag word',
            original_service_cases=5,service_ram_bytes=5*131072,service_io_bytes=5*18,
            service_scope='Unmodified original heap and packed-load functions with diagnostic callers/file adapters',
            public_scope='Four original public-input paths, both C routes attached to actual NPE resource runtime',
            routes=totals,world_bound_bytes=3536,short_checks=32,build_targets=33,build_warnings=0,
            warm_constructors=4,own_save_format='NPESAV22',
            pending=['all flag writers/consumers and other battle UI resource lifecycle',
                     'out-of-range selectors and file failure recovery','original audio/physical timing/other NPE'],
            exit_codes=codes,immutable_sha256=prior['immutable_sha256'],
            source_sha256={p:sha(p) for p in sorted(set(sources))},evidence_sha256=evidence)
Path(prefix+'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('Passed:5 full RAM/IO resource cycles,142 LCD commits,96 controls,864 records,4 warm flags,32 checks')
