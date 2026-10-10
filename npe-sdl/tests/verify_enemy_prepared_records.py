"""Check independent original warm enemy action samples against C/SDL outputs.

RAM references are stable tick-end samples within the enemy action and at round
end. They cover only the twelve prepared bytes, not a complete 148-byte record.
The probe also runs the production SDL controller with actual menu key events.
"""
from pathlib import Path
import hashlib
import json


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

reference = 'tmp/native-enemy-prepared-records.json'
rows = json.loads(Path(reference).read_text(encoding='utf8'))
expected_names = {'enemy-prepared-' + name for name in [
    'attack', 'attack-one-party', 'single-damage', 'group-damage', 'single-heal',
    'group-heal', 'single-status', 'group-status', 'own-status']}
assert len(rows) == 9 and {r['config']['name'] for r in rows} == expected_names
log_path = 'tmp/enemy-prepared-regression-new.log'
log = Path(log_path).read_text(encoding='utf8')
assert 'Assertion failed' not in log
cases = []
total_frames = action_records = final_records = 0
for row in rows:
    config, events = row['config'], row['events']
    name, count = config['name'], config['party'] + 3
    for slot, values in enumerate(events[0]['prepared']):
        seed = b''.join((100 * (slot + 1) + word + 1).to_bytes(2, 'big') for word in range(6))
        assert bytes(values) == seed, (name, slot)
    phases = row['preparedPhases']
    assert phases and all(p['execution'] == config['party'] + config['enemyActor'] + 1 for p in phases)
    for suffix, values in [('action', phases[-1]['prepared']), ('final', events[-1]['prepared'])]:
        assert len(values) == count
        for slot, expected in enumerate(values):
            assert len(expected) == 12
            path = f'tmp/native-{name}-{suffix}-record-{slot}-c.bin'
            assert Path(path).read_bytes() == bytes(expected), path
    action_records += count
    final_records += count
    commits = json.loads(Path(f'tmp/native-{name}-commits.json').read_text(encoding='utf8'))
    for frame in range(len(commits)):
        stem = f'tmp/native-{name}-commit-{frame}'
        expected = Path(stem + '.idx').read_bytes()
        assert len(expected) == 240 * 120 and Path(stem + '-c.idx').read_bytes() == expected, stem
        assert f'C original {name} commit{frame}: 0 differences' in log
        assert f'SDL original {name} animation commit{frame}: 0 differences' in log
    total_frames += len(commits)
    cases.append({'name': name, 'party': config['party'], 'enemy_actor': config['enemyActor'],
                  'lcd_commits': len(commits), 'action_prepared_records': count, 'final_prepared_records': count})
assert (total_frames, action_records, final_records) == (221, 52, 52)
assert 'Enemy prepared records: 9 rounds, 52 action and 52 final 12-byte records, 221 C/SDL LCD commits passed' in log

regressions = {}
for name, expected in {
    'attack': '6 final SDL frames and 123 C/SDL animation commits passed',
    'heal': '4 final SDL frames and 104 C/SDL animation commits passed',
    'damage': '3 final SDL frames and 72 C/SDL animation commits passed',
    'status': '6 final SDL frames and 178 C/SDL animation commits passed',
    'party-skill': 'Prepared skill record:6 original rounds/216 actual C/SDL LCD commits',
    'party-item': 'Prepared item record:5 original rounds/138 actual C/SDL LCD commits',
    'rules': 'Enemy AI, original skill metadata, healing, damage, immunity, sleep and poison probes passed',
}.items():
    path = f'tmp/enemy-prepared-regression-{name}.log'
    result = Path(path).read_text(encoding='utf8')
    assert expected in result and 'Assertion failed' not in result, path
    regressions[name] = result.splitlines()[-1]
build = Path('tmp/enemy-prepared-regression-build.log').read_text(encoding='utf8').lower()
assert 'warning:' not in build and 'error:' not in build

report = {'passed': True, 'goal_complete': False, 'cases': cases,
          'original_c_sdl_commits': total_frames, 'action_prepared_records': action_records,
          'final_prepared_records': final_records, 'prepared_bytes_per_record': 12,
          'regressions': regressions, 'original_input_sha256': inputs,
          'source_sha256': {path: digest(path) for path in [
              'npe-sdl/world.c', 'npe-sdl/main.c', 'npe-sdl/CMakeLists.txt',
              'npe-sdl/tests/reference_enemy_spell_animation.mjs',
              'npe-sdl/tests/generate_enemy_prepared_records_fixture.py',
              'npe-sdl/tests/enemy_prepared_records_native.h',
              'npe-sdl/tests/sdl_enemy_spell_status_probe.c',
              'npe-sdl/tests/verify_enemy_prepared_records.py', reference, log_path]},
          'main_binary_sha256': digest('npe-sdl/build/npe-sdl.exe'),
          'limitations': ['Only offsets102..113 are compared as raw records',
                          'Original RAM samples are taken at tick ends, not every instruction or display commit',
                          'Diagnostic seeds match existing action contexts, not proven original boot RNG',
                          'Other formations, mixed actions, party group status, audio and full cross-NPE behavior remain']}
Path('tmp/enemy-prepared-records-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
print(json.dumps({key: report[key] for key in ['passed', 'original_c_sdl_commits',
                                             'action_prepared_records', 'final_prepared_records']}))
