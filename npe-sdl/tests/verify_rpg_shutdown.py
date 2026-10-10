"""Recompare original shutdown RAM/IO and persisted runtime/main exit results."""
import hashlib
import json
from pathlib import Path

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
prior=json.loads(Path('tmp/battle-entry-resources-report.json').read_text(encoding='utf8'))
for path,digest in {**prior['immutable_sha256'],**prior['preserved_sha256']}.items():assert sha(path)==digest,path
targets=json.loads(Path('tmp/rpg-shutdown-check-targets.json').read_text(encoding='utf8'))
assert len(targets)==len(set(targets))==42
prefix='tmp/rpg-shutdown-final-';codes={}
stamp=Path(prefix+'build.exit').stat().st_mtime_ns
for label in ['generator','configure','build',*targets]:
    path=prefix+label+'.exit';assert Path(path).read_text(encoding='utf-8-sig').strip()=='0',path
    if label in targets:assert Path(path).stat().st_mtime_ns>=stamp,path
    codes[path]=0
assert Path('tmp/rpg-shutdown-native.exit').read_text(encoding='utf-8-sig').strip()=='0'
codes['tmp/rpg-shutdown-native.exit']=0
log=Path(prefix+'build.log').read_text(encoding='utf-8-sig')
assert 'warning:' not in log and 'error:' not in log
for t in ['npe-sdl',*targets]:assert 'Built target '+t in log,t
evidence={}
entry_regression_outputs=0
for path,digest in prior['evidence_sha256'].items():
    if '-c-' in path:
        assert Path(path).stat().st_mtime_ns>=stamp,path
        assert sha(path)==digest,path
        evidence[path]=digest;entry_regression_outputs+=1
assert entry_regression_outputs>1000
def output(path,length):
    p=Path(path);assert p.stat().st_mtime_ns>=stamp,path
    b=p.read_bytes();assert len(b)==length,path;evidence[path]=sha(path);return b
def clear(flags):return flags&255 if flags>>8==1 else flags
def controls(ram):
    return b''.join(ram[a-0x510000:a-0x510000+n] for a,n in
                   [(0x527aa4,2),(0x527a6a,2),(0x527a88,2),(0x52797e,4),(0x5279a6,4),(0x5127ea,32)])
native=json.loads(Path('tmp/rpg-shutdown-native/report.json').read_text(encoding='utf8'))
assert native['passed'] and len(native['cases'])==9
assert native['original_prg_sha256']==sha('tmp/NP560.PRG') and native['npe_sha256']==sha('tmp/tjfy-world-probe.npe')
rows=[]
for row in native['cases']:
    assert row['passed'] and row['stack_restored'] and not row['heap_headers_seeded']
    assert (0x1fd50e,0x5a) in [tuple(r) for r in row['original_regions']]
    c=row['input'];name=c['name'];flags=c['flags']
    ram=Path(row['ram']).read_bytes();trace=Path(row['trace']).read_bytes()
    assert len(ram)==131072 and len(trace)==18
    assert sha(row['ram'])==row['ram_sha256'] and sha(row['trace'])==row['trace_sha256']
    evidence[row['ram']]=row['ram_sha256'];evidence[row['trace']]=row['trace_sha256']
    assert output('tmp/rpg-shutdown-c-'+name+'-ram.bin',131072)==ram
    assert output('tmp/rpg-shutdown-c-'+name+'-trace.bin',18)==trace
    expected=clear(flags).to_bytes(2,'big');assert controls(ram)[:2]==expected
    for route in ['runtime','close']:
        base='tmp/rpg-shutdown-'+route+'-'+name+'-'
        before=output(base+'before.bin',46)
        assert before[:2]==flags.to_bytes(2,'big')
        assert before[2:4]==(0 if c['zero_map'] else 5).to_bytes(2,'big')
        assert before[4:6]==(0 if c['zero_battle'] else 6).to_bytes(2,'big')
        assert before[6:10]==bytes.fromhex('00514e5e') and before[10:14]==bytes.fromhex('005169bc')
        assert before[14:]==bytes.fromhex('fe00')+bytes(30)
        assert output(base+'closed.bin',2)==expected
        if route=='runtime':
            after=output(base+'after.bin',46)
            assert after==controls(ram) and after[2:14]==before[2:14],name
    rows.append(dict(name=name,flag_before=flags,flag_after=clear(flags),repeats=c['repeats']))
app=[('title-inactive',166),('title-active',422),('title-two',678),('title-high129',33190),
     ('title-high255',65446),('map-inactive',166),('map-active',422),('scripted-active',422)]
app_rows=[]
for name,flags in app:
    base='tmp/rpg-shutdown-main-'+name+'-'
    before=output(base+'before.bin',46);assert before[:2]==flags.to_bytes(2,'big')
    assert int.from_bytes(before[2:4],'big')>0
    if flags>>8:assert int.from_bytes(before[4:6],'big')>0
    assert output(base+'closed.bin',2)==clear(flags).to_bytes(2,'big')
    events=output(base+'events.bin',4);frames=int.from_bytes(events[:2],'big')
    assert frames>0 and events[2:]==bytes([0,1])
    app_rows.append(dict(name=name,sdl_present_calls=frames,npe_free_calls=1,flag_before=flags,flag_after=clear(flags)))
sources=['npe-sdl/resident_heap_owner.inc','npe-sdl/rpg_runtime.inc','npe-sdl/npe.h','npe-sdl/CMakeLists.txt',
         'npe-sdl/main.c','npe-sdl/world.c','npe-sdl/npe.c','npe-sdl/tests/reference_rpg_resource_lifecycle.py',
         'npe-sdl/tests/generate_rpg_shutdown_fixture.py','npe-sdl/tests/rpg_shutdown_native.h',
         'npe-sdl/tests/rpg_resource_lifecycle_probe.c','npe-sdl/tests/sdl_rpg_shutdown_probe.c',
         'npe-sdl/tests/sdl_walk_battle_entry_probe.c',
         'npe-sdl/tests/verify_rpg_shutdown.py','tmp/run_rpg_shutdown_checks.py']
report=dict(passed=True,scope=native['scope'],original_cases=rows,original_ram_bytes_per_case=131072,
            original_io_bytes_per_case=18,runtime_shutdown_paths=9,runtime_close_paths=9,actual_main_quit_paths=app_rows,
            short_checks=42,build_targets=43,build_warnings=0,world_bound_bytes=3536,own_save_format='NPESAV22',
            entry_regression_routes=prior['routes'],entry_regression_persisted_outputs=entry_regression_outputs,
            immutable_sha256=prior['immutable_sha256'],preserved_sha256=prior['preserved_sha256'],exit_codes=codes,
            source_sha256={p:sha(p) for p in sources},evidence_sha256=evidence,
            pending=['Full original startup and shutdown stream/file ordering','All remaining RAM/control writers and UI resource lifecycle',
                     'Other continuous battle actions and record reuse','Original PCM and physical timing','Complete other NPE dynamic support'])
Path('tmp/rpg-shutdown-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps({k:report[k] for k in ['passed','short_checks','runtime_shutdown_paths','runtime_close_paths','actual_main_quit_paths']}))
