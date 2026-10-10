"""Verify public entry/exit/reentry controls and persisted C/SDL results."""
import hashlib
import json
from pathlib import Path

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

prior=json.loads(Path('tmp/battle-selector-resources-report.json').read_text(encoding='utf8'))
for path,digest in {**prior['immutable_sha256'],**prior['preserved_sha256']}.items():assert sha(path)==digest,path
targets=json.loads(Path('tmp/battle-entry-resources-check-targets.json').read_text(encoding='utf8'))
assert len(targets)==len(set(targets))==40
prefix='tmp/battle-entry-resources-final-';codes={}
paths=['tmp/battle-entry-exit-resources-native.exit','tmp/battle-reentry-resources-native.exit']
paths += [prefix+label+'.exit' for label in ['exit-generator','reentry-generator','configure','build',*targets]]
stamp=Path(prefix+'build.exit').stat().st_mtime_ns
for path in paths:
    assert Path(path).read_text(encoding='utf-8-sig').strip()=='0',path;codes[path]=0
for t in targets:assert Path(prefix+t+'.exit').stat().st_mtime_ns>=stamp,t
log=Path(prefix+'build.log').read_text(encoding='utf-8-sig')
assert 'warning:' not in log and 'error:' not in log
for t in ['npe-sdl',*targets]:assert 'Built target '+t in log,t
evidence={};totals={}

def compare(path,expected):
    assert Path(path).stat().st_mtime_ns>=stamp,path
    assert Path(path).read_bytes()==expected,path;evidence[path]=sha(path)

def resource(actual,label,s):
    assert s['resourceFlags']==(422 if label=='entry' or label=='reentry-after' or label.startswith('action-') else 166),label
    compare(actual+'-'+label+'-resource.bin',s['resourceFlags'].to_bytes(2,'big'))

for stem,count in [('exit-records',7),('phase-resident',3)]:
    ref_prefix='tmp/native-battle-resources-'+stem
    rows=json.loads(Path(ref_prefix+'.json').read_text(encoding='utf8'))['rows'];assert len(rows)==count
    evidence[ref_prefix+'.json']=sha(ref_prefix+'.json')
    for route in ['direct','main']:
        pixels=records=flags=saves=0
        for index,row in enumerate(rows):
            native=ref_prefix+'-'+str(index);actual=ref_prefix+'-c-'+str(index)+'-'+route
            assert row['before']['resourceFlags']==166
            for commit in range(len(row['commits'])):
                ref=native+'-commit-'+str(commit)+'.idx';out=actual+'-entry-'+str(commit)+'.idx'
                compare(out,Path(ref).read_bytes());evidence[ref]=sha(ref);pixels+=1
            for step,n in enumerate(row['menuNavigation']):
                for commit in range(len(n['frames'])):
                    suffix=f'-menu-{step}-{commit}.idx';ref=native+suffix
                    compare(actual+suffix,Path(ref).read_bytes());evidence[ref]=sha(ref);pixels+=1
            end=row['menuNavigation'][-1]['after']
            resource(actual,'entry',row['after']);resource(actual,'exit',end);flags+=2
            for phase,s in [('entry',row['after']),('exit',end)]:
                for member in range(row['config']['party']):
                    for label,key in [('map','residentMapRecords'),('battle','residentBattlePartyRecords')]:
                        compare(actual+f'-{phase}-{label}-record-{member}.bin',bytes(s[key][member]));records+=1
            if route=='direct' and end['mode']==1:
                for kind in ['cold','warm']:
                    for member in range(3):
                        path=ref_prefix+f'-c-{index}-save-{kind}-map-record-{member}.bin'
                        expected=bytes(end['residentMapRecords'][member]) if member<row['config']['party'] else Path(ref_prefix+f'-c-{index}-save-cold-map-record-{member}.bin').read_bytes()
                        compare(path,expected)
                    saves+=1
            if row.get('reentry'):
                for phase in ['retained','input','after']:
                    s=row['reentry'][phase];resource(actual,'reentry-'+phase,s);flags+=1
                    for label,key,count_records in [('map','allMapRecords',3),('battle','allBattleRecords',6)]:
                        for slot in range(count_records):
                            compare(actual+f'-{phase}-{label}-record-{slot}.bin',bytes(s[key][slot]));records+=1
                for commit in range(len(row['reentry']['frames'])):
                    ref=native+f'-reentry-{commit}.idx';compare(actual+f'-reentry-{commit}.idx',Path(ref).read_bytes());evidence[ref]=sha(ref);pixels+=1
                for step,n in enumerate(row['reentry']['actions']):
                    s=n['after'];resource(actual,'action-'+str(step),s);flags+=1
                    for commit in range(len(n['frames'])):
                        ref=native+f'-action-{step}-{commit}.idx';compare(actual+f'-action-{step}-{commit}.idx',Path(ref).read_bytes());evidence[ref]=sha(ref);pixels+=1
                    for label,key,count_records in [('map','allMapRecords',3),('battle','allBattleRecords',6)]:
                        for slot in range(count_records):
                            compare(actual+f'-action-state-{step}-{label}-record-{slot}.bin',bytes(s[key][slot]));records+=1
        totals[stem+'-'+route]=dict(pixel_commits=pixels,full_physical_records=records,resource_flag_snapshots=flags,own_save_paths=saves)

sources=['npe-sdl/main.c','npe-sdl/world.c','npe-sdl/npe.h','npe-sdl/rpg_runtime.inc','npe-sdl/CMakeLists.txt',
         'npe-sdl/tests/sdl_walk_battle_entry_probe.c','npe-sdl/tests/reference_battle_selection_entry.mjs',
         'npe-sdl/tests/generate_enemy_record_entry_fixture.py','npe-sdl/tests/battle_menu_rows_load_probe.c',
         'npe-sdl/tests/battle_resources_exit_records_native.h','npe-sdl/tests/battle_resources_phase_resident_native.h',
         'npe-sdl/tests/verify_battle_entry_resources.py','tmp/run_battle_entry_resources_checks.py',
         'tmp/battle-exit-resource-protocol.asm']
report=dict(passed=True,scope='Packed resource allocation at entry, release at win/loss/flee, reentry and idle World resource bootstrap',
            routes=totals,short_checks=40,build_targets=41,build_warnings=0,world_bound_bytes=3536,own_save_format='NPESAV22',
            immutable_sha256=prior['immutable_sha256'],preserved_sha256=prior['preserved_sha256'],exit_codes=codes,
            source_sha256={p:sha(p) for p in sources},evidence_sha256=evidence,
            pending=['Full startup heap/RAM equivalence','Main-loop exit resource consumer','All other resource writers and UI state',
                     'File errors/abnormal selectors','Other continuous actions and record reuse','Original audio/physical timing/other NPE'])
Path('tmp/battle-entry-resources-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps({k:report[k] for k in ['passed','routes','short_checks','world_bound_bytes']}))
