"""Summarize completed original routing and current SDL input runs."""
import hashlib, json, re
from pathlib import Path

root = Path('.')
def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()
native = json.loads(Path('tmp/battle-letter-native/report.json').read_text(encoding='utf-8'))
assert native['passed'] and native['case_count']==66 and all(c['passed'] for c in native['cases'])
assert digest('tmp/NP560.PRG')==native['original_prg_sha256']
rows = []
targets = {'sdl-battle-reentry-mixed-probe':226,
           'sdl-battle-skill-record-probe':33,
           'sdl-battle-item-record-probe':56,
           'sdl-battle-reentry-two-attacks-probe':125}
for target,count in targets.items():
    variants = ['arrows','letters'] if target.endswith('two-attacks-probe') else ['arrows','letters','shift','caps']
    for variant in variants:
        path = Path(f'tmp/battle-letter-after-{variant}-{target}.log')
        log = path.read_text(encoding='utf-8')
        frames = re.findall(r'\bC=(\d+) SDL=(\d+)\b',log)
        assert len(frames)==count and all(a==b=='0' for a,b in frames),(target,variant)
        assert 'Assertion failed' not in log and ('zero differences' in log or 'matched' in log)
        rows.append(dict(target=target,variant=variant,c_sdl_comparisons=count,log_sha256=digest(path)))
for target in list(targets)[:3]:
    log = Path(f'tmp/battle-letter-before-{target}.log').read_text(encoding='utf-8')
    assert 'Assertion failed' in log,target
for stem in ['build','attack-build','quiz-build','map-list-build']:
    log = Path('tmp/battle-letter-'+stem+'.log').read_text(encoding='utf-8')
    assert not re.search(r'\b(?:warning|error):',log,re.I),stem
quiz = Path('tmp/battle-letter-quiz-check.log').read_text(encoding='utf-8')
assert 'Six original UI quiz-state lifetimes' in quiz and '10 actual main/SDL map commits matched' in quiz
assert 'Assertion failed' not in quiz
quiz_before = Path('tmp/battle-letter-quiz-before.log').read_text(encoding='utf-8')
assert 'Assertion failed' in quiz_before and 'line 100' in quiz_before
quiz_aligned = Path('tmp/battle-letter-quiz-before-aligned.log').read_text(encoding='utf-8')
assert 'Six original UI quiz-state lifetimes' in quiz_aligned and 'Assertion failed' not in quiz_aligned
map_log = Path('tmp/battle-letter-map-list-check.log').read_text(encoding='utf-8')
map_frames = re.findall(r'\bC=(\d+) SDL=(\d+)\b',map_log)
assert map_frames and all(a==b=='0' for a,b in map_frames) and '6 actual SDL paths' in map_log
old_title = json.loads(Path('tmp/title-slot-controls/report.json').read_text(encoding='utf-8'))
assert digest('tmp/battle-letter-main-before.c')==old_title['hashes']['npe-sdl/main.c']
paths = ['npe-sdl/main.c','npe-sdl/build/npe-sdl.exe',
         'npe-sdl/tests/reference_battle_letter_keys.py','npe-sdl/tests/battle_input_keys.h',
         'npe-sdl/tests/sdl_walk_battle_entry_probe.c','npe-sdl/tests/sdl_battle_skill_record_probe.c',
         'npe-sdl/tests/sdl_battle_item_record_probe.c','npe-sdl/tests/sdl_quiz_state_probe.c',
         'npe-sdl/tests/battle_reentry_mixed_native.h','npe-sdl/tests/battle_reentry_two_attacks_native.h',
         'npe-sdl/tests/battle_skill_record_native.h','npe-sdl/tests/battle_item_record_native.h',
         'npe-sdl/tests/quiz_state_native.h','tmp/tjfy-world-probe.npe']
report = dict(passed=True,original_dispatch_cases=66,before_fix_failed_paths=3,
              battle_runs=rows,battle_c_sdl_comparisons=sum(r['c_sdl_comparisons'] for r in rows),
              quiz_original_state_paths=6,quiz_c_sdl_comparisons=10,quiz_literal_key_checks=20,
              quiz_aligned_before_fix_passed=True,quiz_probe_only_repairs=['Remove WTBL before historical payload conversion','Observe native empty-slot constructor'],
              map_item_paths=6,map_item_c_sdl_comparisons=len(map_frames),
              original_lcd_references='Existing independent fixtures reused; comparison count is not a count of newly captured original frames.',
              original_dispatch_scope=native['scope'],
              production_files_changed=['npe-sdl/main.c'],
              goal_complete=False,overall_rough_estimate=80,
              hashes={p:digest(p) for p in paths})
Path('tmp/battle-letter-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:report[k] for k in ['passed','original_dispatch_cases','battle_c_sdl_comparisons','quiz_c_sdl_comparisons','map_item_c_sdl_comparisons','goal_complete']}))
