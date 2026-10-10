"""Verify original party status/buff/revival displays against C and actual SDL.

RAM comparisons cover twelve prepared bytes per physical record, sampled at
stable tick ends. They do not certify complete 148-byte character records.
"""
from pathlib import Path
import hashlib
import json
import re


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


inputs = {
    'tmp/NP560.PRG': 'd555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5',
    'tmp/tjfy-world-probe.npe': 'df4b52df6b69e1105a5ea6100e43f35f832ed859c10cb8714879e44ad4d8cb40',
    'tmp/tianjiang-online.opennoah': 'c59772070ca7beec488a86a61bfec3ac1e60cd70c248ab36239ff81e2b862148',
    'tmp/opennoah_wasm_bg.wasm': 'af98dd54ebaa6073cfc3809a7387a0f7af087605c79056449d88e63075ef4d86',
}
for path, expected in inputs.items():
    assert digest(path) == expected, path

names = {
    'spell': {'party-prepared-' + suffix for suffix in [
        'group-status', 'group-status-cure', 'group-mixed-status',
        'own-immunity', 'group-buff', 'group-status-two-party']},
    'item': {'item-prepared-' + suffix for suffix in [
        'group-attack', 'group-defense', 'group-revival', 'group-revival-negative-display']},
}
cases, references, logs = [], [], []
total_frames = action_records = final_records = 0
for kind in ['spell', 'item']:
    reference = ('tmp/native-party-spell-prepared-labels.json' if kind == 'spell'
                 else 'tmp/native-item-prepared-labels.json')
    references.append(reference)
    rows = json.loads(Path(reference).read_text(encoding='utf8'))
    assert len(rows) == len(names[kind]) and {r['config']['name'] for r in rows} == names[kind]
    target = f'sdl-party-prepared-{kind}-labels-probe'
    log_path = f'tmp/party-prepared-label-regression-{target}.log'
    logs.append(log_path)
    log = Path(log_path).read_text(encoding='utf8')
    assert 'Assertion failed' not in log
    kind_records = kind_frames = 0
    for row in rows:
        config, events = row['config'], row['events']
        name, count = config['name'], config['party'] + 3
        assert len(events[0]['prepared']) == count
        for slot, values in enumerate(events[0]['prepared']):
            words = (config.get('preparedWords') if slot == 0 else None)
            words = words or [100 * (slot + 1) + word + 1 for word in range(6)]
            assert bytes(values) == b''.join(v.to_bytes(2, 'big') for v in words), (name, slot)
        phases = row['preparedPhases']
        assert phases and all(p['execution'] == 1 for p in phases)
        for suffix, values in [('action', phases[-1]['prepared']), ('final', events[-1]['prepared'])]:
            assert len(values) == count
            for slot, expected in enumerate(values):
                assert len(expected) == 12
                path = f'tmp/native-{name}-{suffix}-record-{slot}-c.bin'
                assert Path(path).read_bytes() == bytes(expected), path
        commits = json.loads(Path(f'tmp/native-{name}-commits.json').read_text(encoding='utf8'))
        for frame, commit in enumerate(commits):
            assert commit['index'] == frame
            stem = f'tmp/native-{name}-commit-{frame}'
            expected = Path(stem + '.idx').read_bytes()
            assert len(expected) == 240 * 120 and Path(stem + '-c.idx').read_bytes() == expected, stem
            assert f'C original {name} commit{frame}: 0 differences' in log
            assert f'SDL original {name} animation commit{frame}: 0 differences' in log
        if name.endswith('two-party'):
            # Tick-end metadata identifies the defense action's selected first
            # receiver; original commits expose the backing-plane transition.
            assert all(c['execution'] == 2 and c['displayTarget'] == 1 for c in commits[21:25])
            assert commits[21]['kinds'][1] == 5
            positions = [87 * 240 + 170, 88 * 240 + 169, 88 * 240 + 170, 89 * 240 + 170]
            old_lcd = Path(f'tmp/native-{name}-commit-22.idx').read_bytes()
            restored = Path(f'tmp/native-{name}-commit-23.idx').read_bytes()
            assert [old_lcd[p] for p in positions] == [9, 9, 12, 9]
            assert all(restored[p] == 0 for p in positions)
        cases.append({'name': name, 'party': config['party'], 'lcd_commits': len(commits),
                      'action_prepared_records': count, 'final_prepared_records': count})
        kind_frames += len(commits)
        kind_records += count
    assert (kind_frames, kind_records) == ((198, 35) if kind == 'spell' else (92, 24))
    summary = (f'Party prepared {kind} labels: {len(rows)} rounds, {kind_records} action and '
               f'{kind_records} final 12-byte records, {kind_frames} C/SDL LCD commits passed')
    assert summary in log
    total_frames += kind_frames
    action_records += kind_records
    final_records += kind_records
assert (len(cases), total_frames, action_records, final_records) == (10, 290, 59, 59)

regressions = {}
for target in ['sdl-party-spell-animation-probe', 'sdl-item-animation-probe',
               'sdl-revival-animation-probe', 'sdl-battle-prepared-skill-record-probe',
               'sdl-battle-prepared-item-record-probe', 'sdl-attack-defend-probe',
               'sdl-enemy-spell-status-probe']:
    path = f'tmp/party-prepared-label-regression-{target}.log'
    logs.append(path)
    result = Path(path).read_text(encoding='utf8')
    assert 'Assertion failed' not in result
    last = result.splitlines()[-1]
    if 'prepared-' in target:
        assert re.search(r'original rounds/[1-9]\d* actual C/SDL LCD commits', last) and last.endswith('passed')
    else:
        assert re.search(r'[1-9]\d* C/SDL animation commits passed$', last)
    regressions[target] = last
build_path = 'tmp/party-prepared-label-final-build.log'
build = Path(build_path).read_text(encoding='utf8').lower()
assert 'warning:' not in build and 'error:' not in build
assert 'built target npe-sdl' in build

sources = ['npe-sdl/world.c', 'npe-sdl/main.c', 'npe-sdl/npe.h', 'npe-sdl/CMakeLists.txt',
           'npe-sdl/tests/reference_party_spell_animation.mjs', 'npe-sdl/tests/reference_item_animation.mjs',
           'npe-sdl/tests/generate_party_prepared_labels_fixture.py',
           'npe-sdl/tests/party_prepared_spell_labels_native.h', 'npe-sdl/tests/party_prepared_item_labels_native.h',
           'npe-sdl/tests/battle_prepared_label_probe.h', 'npe-sdl/tests/sdl_party_spell_animation_probe.c',
           'npe-sdl/tests/sdl_item_animation_probe.c', 'npe-sdl/tests/verify_party_prepared_labels.py']
report = {'passed': True, 'goal_complete': False, 'cases': cases,
          'original_c_sdl_commits': total_frames, 'action_prepared_records': action_records,
          'final_prepared_records': final_records, 'prepared_bytes_per_record': 12,
          'regressions': regressions, 'original_input_sha256': inputs,
          'source_sha256': {path: digest(path) for path in sources + references + logs + [build_path]},
          'main_binary_sha256': digest('npe-sdl/build/npe-sdl.exe'),
          'limitations': ['Raw records cover only offsets102..113',
                          'Original RAM metadata is sampled at tick ends, not at each LCD commit',
                          'Diagnostic attributes and initial seed61 are not original boot RNG proof',
                          'Other mixed actions, formations, audio, fonts and full cross-NPE behavior remain']}
Path('tmp/party-prepared-labels-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
print(json.dumps({key: report[key] for key in ['passed', 'original_c_sdl_commits',
                                             'action_prepared_records', 'final_prepared_records']}))
