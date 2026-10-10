"""Check actual-title opening traces and NPE-derived initial positions.

No original LCD/PCM claim: the probe compares C with actual SDL output.
"""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[2]
report_path = root/'tmp/cross-npe-opening/final/report.json'
report = json.loads(report_path.read_text(encoding='utf-8'))
assert report['passed'] == len(report['games']) == 18
assert not report['diagnostic_attributes'] and not report['plot_state_injection']
assert not report['original_pixel_reference'] and not report['original_audio_checked']
cases = []
counts = {'story_trigger': 0, 'npc_dialogue': 0, 'exit': 0}
for case in report['games']:
    assert case['passed'] and case['exit'] == 0 and not case['errors']
    source = Path(case['source'])
    data = source.read_bytes()
    def record(section, index):
        start = int.from_bytes(data[0x3a+section*8:0x3e+section*8], 'big')
        return start+int.from_bytes(data[start+index*4:start+index*4+4], 'big')
    character = data[record(12, 0):record(12, 0)+94]
    events = case['events']
    initial = [e for e in events if e['phase'] == 'new-game']
    route = [e for e in events if e['phase'] == 'route']
    trigger = [e for e in events if e['phase'] == 'trigger']
    assert len(initial) == len(route) == len(trigger) == 1
    initial, route, trigger = initial[0], route[0], trigger[0]
    assert initial['map'] == initial['mode'] == initial['progress'] == 1
    assert [initial['x'], initial['y']] == [character[80]+7, character[81]+1]
    assert route['found'] and 0 < route['length'] <= 255
    assert [trigger[k] for k in ['map', 'mode', 'event']] == [route['expected_map'], route['expected_mode'], route['expected_event']]
    assert events[-1]['phase'] == 'complete' and events[-1]['mode'] == 1
    frames = [e for e in events if e['phase'] == 'frame']
    assert len(frames) == events[-1]['frames']
    assert [e['frames'] for e in frames] == list(range(1, len(frames)+1))
    assert sum(e['phase'] == 'title-confirm' for e in events) == 1
    kind = 'npc_dialogue' if route['npc_dialogue'] else 'exit' if route['expected_move'] == 2 else 'story_trigger'
    counts[kind] += 1
    cases.append({'source': str(source), 'source_sha256': hashlib.sha256(data).hexdigest(),
                  'initial_position': [initial['x'], initial['y']], 'kind': kind,
                  'walking_inputs': route['length'], 'trigger': trigger,
                  'complete': events[-1], 'frames': len(frames)})
assert sum(c['frames'] for c in cases) == report['frames'] == 1302
build = (root/'tmp/cross-npe-opening-final-build.log').read_text(encoding='utf-8')
assert 'Built target sdl-cross-npe-opening-probe' in build and 'warning:' not in build and 'error:' not in build
previous = json.loads((root/'tmp/special-map-save-report.json').read_text(encoding='utf-8'))
production = ['npe-sdl/save.c', 'npe-sdl/save_terrain.inc', 'npe-sdl/rpg_runtime.inc',
              'npe-sdl/world.c', 'npe-sdl/npe.h', 'npe-sdl/build/npe-sdl.exe']
for path in production:
    assert hashlib.sha256((root/path).read_bytes()).hexdigest() == previous['source_sha256'][path]
result = {'passed': True, 'games': 18, 'frames': report['frames'], 'kinds': counts,
          'cases': cases, 'production_gameplay_changed': False,
          'limitations': ['Each file covers only title/new-game and one reachable trigger, NPC or exit.',
                          'Fixed host seed1 and virtual wait clock; real timing is not verified.',
                          'C/SDL output consistency, not original LCD; audio device is disabled.',
                          'No full-game completion or all event combinations are proved.'],
          'overall_estimate_percent': 80, 'goal_complete': False}
(root/'tmp/cross-npe-opening/verified-report.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print(json.dumps({k: result[k] for k in ['passed', 'games', 'frames', 'kinds', 'production_gameplay_changed']}, ensure_ascii=False))
