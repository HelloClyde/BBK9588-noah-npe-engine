"""Audit the walking-trigger fix and actual multi-entry traces.

Successful short paths are not full-game compatibility. Keep the real failed
zero-distance NPC move visible; do not turn original fault evidence into a
claim that this game's entire original route has been reproduced.
"""
from collections import Counter
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[2]


def read(path):
    return json.loads((root/path).read_text(encoding='utf-8'))


native = read('tmp/move-trigger-entry-native/report.json')
current = read('tmp/move-trigger-entry-c-checks.json')
before = read('tmp/move-trigger-entry-before-checks.json')
assert native['passed'] and len(native['cases']) == len(current) == len(before) == 4
assert not native['original_lcd_checked']
original = (root/'tmp/NP560.PRG').read_bytes()
assert hashlib.sha256(original).hexdigest() == native['original_prg_sha256']
image = bytes(0x30000)+original
for original_case, c, old in zip(native['cases'], current, before):
    assert original_case['passed'] and original_case['stack_restored']
    assert original_case['name'] == c['name'] == old['name']
    assert c['exit'] == 0 and old['exit'] == 1 and not c['stderr'] and not old['stderr']
    actual, previous = json.loads(c['stdout']), json.loads(old['stdout'])
    assert actual['passed'] and not previous['passed']
    expected = original_case['after']
    assert [actual[k] for k in ('mode', 'event', 'map_init')] == [expected[k] for k in ('mode', 'event', 'map_init')]
    assert previous['map_init'] == original_case['before_flag'] != expected['map_init']
    for region in original_case['code_regions']:
        data = image[region['address']:region['address']+region['size']]
        assert hashlib.sha256(data).hexdigest() == region['sha256']

world_before = (root/'tmp/cross-npe-followup/world-before.c').read_bytes()
world_after = (root/'npe-sdl/world.c').read_bytes()
needle = b'        world->game_mode = trigger.mode;\r\n        world->story_event_id = trigger.event_id;\r\n'
addition = (b'        /* Collision3A:073A..0742 requests fresh initialization even when\r\n'
            b'           the trigger enters mode2 after a previous dialogue has closed. */\r\n'
            b'        world->map_init_pending=1;\r\n')
assert world_before.count(needle) == 1
assert world_after == world_before.replace(needle, needle+addition)

baseline = read('tmp/cross-npe-followup/first/report.json')
mine_before = next(c for c in baseline['games'] if Path(c['source']).name == '我的世界.npe')
assert not baseline['diagnostic_attributes'] and not baseline['plot_state_injection']
assert not mine_before['passed'] and 'mode-2 followup is unsupported' in mine_before['errors']
natural = read('tmp/cross-npe-followup/trigger-fix/report.json')
assert natural['passed'] == 1 and not natural['diagnostic_attributes'] and not natural['plot_state_injection']
mine_after = natural['games'][0]
assert mine_after['passed'] and not mine_after['errors']
entry = next(e for e in mine_after['events'] if e['phase'] == 'trigger' and e['checkpoint'] == 1)
assert [entry[k] for k in ('map', 'mode', 'event', 'x', 'y')] == [1, 2, 4, 6, 10]
followup = next(e for e in mine_after['events'] if e['phase'] == 'followup' and e['checkpoint'] == 2)
assert followup['map'] == 4 and followup['mode'] == 1

report = read('tmp/cross-npe-followup/final2/report.json')
assert report['passed'] == 17 and len(report['games']) == 18 and report['limit'] == 12
assert report['diagnostic_attributes'] and not report['plot_state_injection']
assert not report['original_pixel_reference'] and not report['original_audio_checked']
assert not report['audio_device_enabled']
cases = []
counts = Counter()
failed = []
for case in report['games']:
    data = Path(case['source']).read_bytes()
    assert hashlib.sha256(data).hexdigest() == case['source_sha256']
    assert case['probe_sha256'] == hashlib.sha256((root/'npe-sdl/build/sdl-cross-npe-followup-probe.exe').read_bytes()).hexdigest()
    base = int.from_bytes(data[0x3a+12*8:0x3e+12*8], 'big')
    start = base+int.from_bytes(data[base:base+4], 'big')
    hero = data[start:start+94]
    events = case['events']
    initial = [e for e in events if e['phase'] == 'new-game']
    assert len(initial) == 1
    initial = initial[0]
    assert [initial[k] for k in ('map', 'mode', 'progress', 'x', 'y')] == [1, 1, 1, hero[80]+7, hero[81]+1]
    assert sum(e['phase'] == 'title-confirm' for e in events) == 1
    frames = [e['frames'] for e in events if e['phase'] == 'frame']
    assert frames == list(range(1, case['frames']+1))
    assert case['frames'] == max(e.get('frames', 0) for e in events)
    assert case['checkpoints'] == sum(e['phase'] == 'followup' for e in events)
    route = pending = None
    keys = set()
    local = Counter()
    for e in events:
        if e['phase'] == 'route' and e['found']:
            route = e
            assert e['checkpoint'] == len(keys) and 0 < e['length'] <= 1024
        elif e['phase'] == 'trigger':
            assert route is not None and pending is None
            assert [e[k] for k in ('map', 'mode', 'event')] == [route[k] for k in ('expected_map', 'expected_mode', 'expected_event')]
            if route['kind'] in (1, 2):
                assert e['map_init'] == 1
                key = (route['entry_map'], route['entry_progress'], route['kind'], route['expected_mode'], route['expected_event'])
            else:
                key = (route['entry_map'], route['kind'], route['expected_map'], e['x'], e['y'])
            assert key not in keys
            pending = (key, route['kind'])
        elif e['phase'] == 'followup':
            assert pending is not None and e['mode'] == 1 and not e['exit_choice']
            key, kind = pending
            keys.add(key)
            local[kind] += 1
            assert e['checkpoint'] == len(keys)
            pending = None
    assert len(keys) == case['checkpoints']
    if case['passed']:
        assert case['exit'] == 0 and not case['errors']
        assert events[-1]['phase'] in ('complete', 'exhausted') and events[-1]['mode'] == 1
        assert pending is None and 8 <= len(keys) <= 12
        if events[-1]['phase'] == 'complete':
            assert len(keys) == report['limit']
    else:
        assert Path(case['source']).name == '天降风云xab.npe' and case['exit'] == 1
        assert case['errors'] == ['NPC move animation is unsupported', 'resource event is unsupported']
        error = events[-1]
        assert error == dict(phase='npc-move-failure', event_found=1, npc_id=23, slot=6,
                             x=21, y=21, target_x=21, target_y=21, next_mode=0, next_event=0)
        assert len(keys) == 9 and pending is not None
        failed.append(dict(source=case['source'], checkpoint=9, state=error))
    counts.update(local)
    cases.append(dict(source=case['source'], passed=case['passed'], checkpoints=len(keys),
                      frames=case['frames'], stop=events[-1]['phase'],
                      attribute_edits=case['attribute_edits'], kinds=dict(local)))
assert len(failed) == 1 and sum(c['checkpoints'] for c in cases) == report['checkpoints'] == 198
assert sum(c['frames'] for c in cases) == report['frames'] == 20623
fault = read('tmp/native-npc-move-resident-zero-fault.json')
assert fault['code'] == 'execution_fault'
assert fault['before']['npc'][0]['x'] == fault['before']['cacheBytes'][7] == 6
assert fault['before']['npc'][0]['y'] == fault['before']['cacheBytes'][9] == 9
regressions = read('tmp/move-trigger-entry-regressions.json')
assert len(regressions) == 3 and all(r['exit'] == 0 for r in regressions)
for path, target in [('tmp/move-trigger-entry-build.log', 'move-trigger-entry-probe'),
                     ('tmp/cross-npe-followup-final2-build.log', 'sdl-cross-npe-followup-probe'),
                     ('tmp/move-trigger-entry-regression-build.log', 'npe-sdl')]:
    text = (root/path).read_text(encoding='utf-8')
    assert 'Built target '+target in text and 'warning:' not in text and 'error:' not in text
result = dict(rule_fix_verified=True, original_collision_cases=4, old_collision_failures=4,
              regressions=regressions, completed_short_checks=17, games_attempted=18,
              completed_entries=198, sdl_submissions=20623, kinds=dict(counts),
              cases=cases, failed=failed, all_cross_game_checks_passed=False,
              zero_distance_fault_inferred_from_same_original_service=True,
              original_xab_full_route_checked=False,
              production_world_sha256=hashlib.sha256(world_after).hexdigest(),
              limitations=['Only 8 to 12 distinct reachable entries per completed game; not full compatibility.',
                           'Fixed seed1, virtual waits and disabled audio; no original full LCD or PCM reference.',
                           'Optional diagnostic character stats before battle actions; no plot, NPC, resource or outcome injection.',
                           'The xab zero-distance move remains failed and needs its original route checked.'],
              overall_estimate_percent=80, goal_complete=False)
(root/'tmp/cross-npe-followup/verified-report.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print(json.dumps({k: result[k] for k in ('rule_fix_verified', 'original_collision_cases', 'completed_short_checks', 'completed_entries', 'sdl_submissions', 'kinds', 'all_cross_game_checks_passed')}, ensure_ascii=False))
