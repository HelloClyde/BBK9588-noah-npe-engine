"""Summarize the focused original-reference and actual SDL checks."""
import hashlib,json
from pathlib import Path

root=Path.cwd()
rows=json.loads(Path('tmp/native-other-npe-skill.json').read_text(encoding='utf-8'))
assert {r['cfg']['name'] for r in rows}=={'dragon-enemy','dragon-enemy-fallen','roc-unknown','soul-zero'}
checks=json.loads(Path('tmp/other-npe-skill-checks.json').read_text(encoding='utf-8'))
assert len(checks)==7 and all(c['exit']==0 for c in checks)
paths=set()
for c in checks:
    log=Path(c['log']);text=log.read_text(encoding='utf-8')
    assert c['success_marker'] in text,c['target']
    paths.add(log)
builds=[Path('tmp/other-npe-skill-final-build.log'),Path('tmp/other-npe-skill-probe-repair-build.log'),Path('tmp/other-npe-skill-magic-probe-repair-build.log')]
for log in builds:
    text=log.read_text(encoding='utf-8');assert 'warning:' not in text and 'error:' not in text
    paths.add(log)
gates={'dragon-enemy':[2,3,4],'dragon-enemy-fallen':[2,3],'roc-unknown':[2],'soul-zero':[2,3]}
for row in rows:
    c=row['cfg'];name=c['name'];raw=c['raw'];cache=c['cache']
    assert len(cache)==144 and cache[114:117]==raw[12:15]
    assert cache[117:119]==raw[37:39]
    assert row['after']['cache']=={'id':6,'bytes':cache}
    if c.get('reject',False):
        assert row['after']['actor']==0 and row['after']['menu']==27 and row['after']['records'][0]['qi']==100
    else:
        assert row['after']['actor']==0 and row['after']['mode']==3
        assert row['before']['commands'][0]['type']==raw[14]
        assert row['after']['records'][0]['qi']==100-(raw[39]*256+raw[40])
    paths.add(Path(c['source']))
    for index in gates[name]:paths.add(Path(f'tmp/native-other-npe-skill-{name}-gate-{index}.idx'))
    for frame in row['frames']:paths.add(Path(f'tmp/native-other-npe-skill-{name}-{frame["index"]}.idx'))
    paths.add(Path(f'tmp/native-other-npe-skill-{name}-gates.json'))
for path in paths:
    if path.suffix=='.idx':
        pixels=path.read_bytes();assert len(pixels)==28800 and max(pixels)<=15,path
paths.update(Path(p) for p in [
 'tmp/native-other-npe-skill.json','tmp/other-npe-skill-checks.json','tmp/other-npe-skill-gate-candidates.json',
 'tmp/tjfy-world-probe.npe','tmp/NP560.PRG','tmp/opennoah_wasm_bg.wasm','tmp/opennoah-headless.mjs',
 'tmp/tianjiang-online.opennoah','tmp/battle-fresh-start.bin',
 'npe-sdl/npe.c','npe-sdl/npe.h','npe-sdl/world.c','npe-sdl/main.c','npe-sdl/CMakeLists.txt',
 'npe-sdl/tests/reference_other_npe_skill_dispatch.mjs','npe-sdl/tests/generate_other_npe_skill_fixture.py',
 'npe-sdl/tests/other_npe_skill_native.h','npe-sdl/tests/sdl_other_npe_skill_probe.c',
 'npe-sdl/tests/sdl_friendly_hp_picker_probe.c','npe-sdl/tests/enemy_action_probe.c',
 'npe-sdl/assets/gbk12-native.bin','npe-sdl/assets/gbk16-native.bin','npe-sdl/build/npe-sdl.exe',
 'tmp/other-npe-skill-2f-b84a-source.asm','tmp/other-npe-skill-2a-0c4c-source.asm',
 'tmp/other-npe-skill-2d-3e80-source.asm','tmp/check_other_skill_baseline.py',
 'tmp/other-npe-skill-baseline-regressions.log','tmp/other-npe-skill-repaired-baseline.log',
 'tmp/other-npe-skill-before-change-enemy-action-probe.log',
 'tmp/other-npe-skill-before-change-sdl-friendly-hp-picker-probe.log',
 'tmp/other-npe-skill-repaired-baseline-enemy-action-probe.log',
 'tmp/other-npe-skill-repaired-baseline-sdl-friendly-hp-picker-probe.log',
 'tmp/other-npe-skill-baseline.log','tmp/other-npe-skill-first.log',
 'tmp/other-npe-skill-controller-owner.log','tmp/other-npe-skill-enemy-action-probe.log',
 'doc/games/np560_npe_other_skill_dispatch.md','plan.md'])
report={
 'engine_complete':False,'overall_estimate_percent':80,
 'previous_goal_turn':{'classification':'no_progress','reason':'Status reporting and queue restatement; no implementation advanced.'},
 'current_goal_turn':{'classification':'progress','reason':'Fixed native default skill dispatch, hostile revival selection/target/commit, and live target-page status rendering; original-reference C/SDL acceptance passed.'},
 'skill_dispatch_batch_complete':True,'other_games_dynamic_compatibility_complete':False,
 'reference_scope':'Actual external skill records substituted only in shared RAM of unmodified NP560/Tianjiang original runtime; not full other-game runtime or artwork acceptance.',
 'original_program_or_resource_modified':False,'user_save_modified':False,
 'native_paths':len(rows),'original_rounds':sum(not r['cfg'].get('reject',False) for r in rows),
 'actual_sdl_selection_page_comparisons':sum(map(len,gates.values())),
 'core_and_actual_sdl_round_lcd_comparisons':sum(len(r['frames']) for r in rows),
 'actual_external_npe_cache_load_comparisons':4,'record_fields_per_round':66,
 'warning_free_final_build_targets':8,'checks':checks,
 'probe_repairs_baseline':{'magic_rules':0,'friendly_picker':0,'original_reference_changed':False},
 'unresolved_preexisting_check':{'target':'enemy-action-probe full mode','current_exit':1,'before_change_failed_at_same_assertion':True,'scope':'Inactive-member map status after own save/load; assertion or implementation not yet adjudicated.'},
 'metadata_correction':{'file_plus_12':'animation_group','file_plus_13':'required_level','previous_candidate_summary_retained_in':'tmp/npc-condition-dialogue-report.json'},
 'remaining':['Own save/load inactive-member state audit','Remaining actual game behavior','Original audio decoding/reference','Other NPE dynamic compatibility','Independent delivery audit'],
 'evidence_hash_scope':'Only this batch inputs, native frames, changed sources and focused checks; no historical all-RAM or all-evidence repetition.',
 'evidence':{str(p).replace('\\','/'):{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(paths,key=str)}
}
Path('tmp/other-npe-skill-dispatch-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:report[k] for k in ['native_paths','original_rounds','actual_sdl_selection_page_comparisons','core_and_actual_sdl_round_lcd_comparisons']}))
