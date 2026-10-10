"""Summarize existing map/menu input checks; does not run or generate oracles."""
import hashlib, json, re
from pathlib import Path

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

native_path = Path('tmp/map-system-letter-native/report.json')
native = json.loads(native_path.read_text(encoding='utf-8'))
assert native['passed'] and native['case_count']==64 and all(c['passed'] for c in native['cases'])
assert native['original_prg_sha256']==sha('tmp/NP560.PRG')
assert native['host_guest_memory_writes']==0 and not native['nand_used']
targets = {
    'sdl-walk-viewport-probe': ('main ',94,19),
    'sdl-system-menu-return-probe': ('system menu ',77,9),
    'sdl-map-skill-record-probe': ('Map skill ',51,13),
    'sdl-map-item-record-probe': ('',18,6),
    'sdl-equip-item-record-probe': ('',36,9),
    'sdl-exit-resident-probe': ('main ',127,10),
}

def frames(path, prefix, count):
    log = Path(path).read_text(encoding='utf-8')
    assert 'Assertion failed' not in log and 'unsupported' not in log.lower(),path
    matches = re.findall(r'^'+re.escape(prefix)+r'.* commit\d+ C=(\d+) SDL=(\d+)$',log,re.M)
    assert len(matches)==count,(path,len(matches),count)
    assert all(c=='0' and s=='0' for c,s in matches),path
    return dict(log=str(path),log_sha256=sha(path),actual_lcd_comparisons=count)

runs = []
for variant in ['arrows','letters','shift','caps']:
    for target,(prefix,count,paths) in targets.items():
        path = f'tmp/map-system-letter-final-{variant}-{target}.log'
        runs.append(dict(variant=variant,target=target,paths=paths,**frames(path,prefix,count)))
for target in list(targets)[:5]:
    old = Path(f'tmp/map-system-letter-before-{target}.log').read_text(encoding='utf-8')
    assert 'Assertion failed' in old,target
old_exit = Path('tmp/map-system-letter-before-aligned-letters-sdl-exit-resident-probe.log').read_text(encoding='utf-8')
assert 'Assertion failed' in old_exit
before_ignored = Path('tmp/map-system-letter-before-aligned-ignored-guard.log').read_text(encoding='utf-8')
assert 'C=306 SDL=306' in before_ignored and 'Assertion failed' in before_ignored
ignored = []
for target,label,count in [('sdl-walk-viewport-probe','Map Q/U/E',57),('sdl-system-menu-return-probe','System root U/E',18)]:
    path = f'tmp/map-system-letter-ignored-{target}.log'
    log = Path(path).read_text(encoding='utf-8')
    assert f'{label} ignored: {count} actual inputs' in log
    ignored.append(dict(ignored_inputs=count,**frames(path,*targets[target][:2])))
quiz_path = Path('tmp/map-system-letter-final-sdl-quiz-state-probe.log')
quiz = quiz_path.read_text(encoding='utf-8')
assert 'Six original UI quiz-state lifetimes:' in quiz and 'and10 actual main/SDL map commits matched' in quiz
frames(quiz_path,'Quiz lifetime ',10)
ending_path = Path('tmp/map-system-letter-final-sdl-ending-flow-probe.log')
ending = ending_path.read_text(encoding='utf-8')
assert 'Assertion failed' not in ending and '37 full-screen animation frames matching native pixels' in ending and 'passed' in ending
build = Path('tmp/map-system-letter-verified-build.log')
assert not re.search(r'warning:|error:',build.read_text(encoding='utf-8'),re.I)
sources = ['npe-sdl/main.c','npe-sdl/build/npe-sdl.exe','tmp/map-system-letter-main-before.c',
           'tmp/map-system-letter-main-before-ignored.c','npe-sdl/tests/battle_input_keys.h',
           'npe-sdl/tests/reference_map_letter_keys.py']
sources += ['npe-sdl/tests/'+n for n in ['sdl_walk_pose_probe.c','sdl_system_menu_return_probe.c',
            'sdl_map_skill_record_probe.c','sdl_map_item_record_probe.c','sdl_equip_item_record_probe.c',
            'sdl_exit_choice_probe.c','sdl_ending_flow_probe.c','walk_viewport_native.h',
            'system_menu_return_native.h','map_skill_record_native.h','map_item_record_native.h',
            'equip_item_record_native.h','exit_resident_native.h']]
report = dict(passed=True,original_dispatch_cases=64,original_report_sha256=sha(native_path),
              actual_lcd_comparisons=sum(r['actual_lcd_comparisons'] for r in runs),runs=runs,
              ignored_shortcut_inputs=75,ignored_runs=ignored,quiz_lcd_comparisons=10,
              ending_animation_lcd_comparisons=37,build_log_sha256=sha(build),
              npe_sha256=sha('tmp/tjfy-world-probe.npe'),sources_sha256={s:sha(s) for s in sources},
              scope='Original map routing plus existing original LCD/state fixtures through actual C/SDL consumers; diagnostic state inputs, no full-game/audio/timing equivalence claim.',
              overall_rough_estimate=80,goal_complete=False)
assert report['actual_lcd_comparisons']==1612
periodic=[]
for variant in ['baseline','ignored']:
    path=Path(f'tmp/map-system-letter-deadline-{variant}.log')
    log=path.read_text(encoding='utf-8')
    assert 'Assertion failed' not in log and 'SDL periodic map: 0 differences' in log
    assert 'modal/timer gates and actual SDL timed map passed' in log
    periodic.append(dict(variant=variant,log=str(path),log_sha256=sha(path)))
old_periodic=Path('tmp/map-system-letter-before-aligned-deadline.log').read_text(encoding='utf-8')
assert 'SDL periodic map: 536 differences' in old_periodic and 'Assertion failed' in old_periodic
report.update(periodic_deadline_runs=periodic,quiz_literal_key_checks=20)
for source in ['npe-sdl/tests/sdl_npc_periodic_probe.c','npe-sdl/tests/npc_periodic_native.h',
               'tmp/map-system-letter-main-before-deadline.c']:
    report['sources_sha256'][source]=sha(source)
assert not re.search(r'warning:|error:',Path('tmp/map-system-letter-deadline-aligned-build.log').read_text(encoding='utf-8'),re.I)
Path('tmp/map-system-letter-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('64 original dispatch cases; 1612 actual C/SDL LCD comparisons; 75 ignored inputs; quiz/ending checks passed')
