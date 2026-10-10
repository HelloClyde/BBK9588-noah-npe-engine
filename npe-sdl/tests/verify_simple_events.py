"""Verify completed original/C role, NPC, UI references and the checked-run provenance."""
import hashlib
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256((root / path).read_bytes()).hexdigest()


manifest = json.loads((root / 'tmp/simple-events/run-manifest.json').read_text(encoding='utf-8'))
assert all(code == 0 for code in manifest['exit_codes'].values())
for path, expected in manifest['hashes'].items():
    assert digest(path) == expected, ('changed since checked run', path)
native = json.loads((root / 'tmp/simple-events/native/report.json').read_text(encoding='utf-8'))
assert native['passed'] and len(native['cases']) == 18
assert native['program_sha256'] == 'd555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
assert digest('tmp/tjfy-world-probe.npe') == 'df4b52df6b69e1105a5ea6100e43f35f832ed859c10cb8714879e44ad4d8cb40'
groups = [('tmp/native-simple-events.json', 18), ('tmp/native-simple-events-choice.json', 3)]
cases = []
for source, count in groups:
    rows = json.loads((root / source).read_text(encoding='utf-8'))
    assert len(rows) == count
    for row in rows:
        name = row['name']
        frames = []
        for index in range(len(row['commits'])):
            original = (root / f'tmp/native-simple-events-{name}-{index}.idx').read_bytes()
            current = (root / f'tmp/c-simple-events-{name}-{index}.idx').read_bytes()
            assert len(original) == len(current) == 240 * 120 and original == current, (name, index)
            frames.append(hashlib.sha256(original).hexdigest())
        before = (root / f'tmp/native-simple-events-{name}-before.records').read_bytes()
        after = (root / f'tmp/native-simple-events-{name}-after.records').read_bytes()
        assert len(before) == len(after) == 444
        if count == 18:
            native_after = (root / f'tmp/simple-events/native/{name}.after.bin').read_bytes()
            assert len(native_after) == 65536 and native_after[0x69c6:0x6b82] == after
            assert native_after[0x6fba:0x6fba + 250] == bytes(row['after']['npcState'])
        else:
            assert row['choiceBytes'] == [11, 24, 0, 1, 0, 60]
        cases.append({'name': name, 'frames': frames, 'passed': True,
                      'original_record_sha256': hashlib.sha256(after).hexdigest()})
log = (root / 'tmp/simple-events/check-final.log').read_text(encoding='utf-8')
assert '24 original C/SDL frames' in log and len(re.findall(r'C=0 SDL=0', log)) == 24
assert len(re.findall(r'numeric state differences=0', log)) == 18
assert len(re.findall(r'SDL state differences=0', log)) == 21
assert 'Assertion failed' not in log
regressions = [
    ('special-party', '10 cases, 42 SDL/C frames'),
    ('story-followup', '46 actual C/SDL commits'),
    ('choice-public', '27 C/SDL commits'),
    ('healing', '46 original frames/56 C/SDL commits'),
]
for name, marker in regressions:
    text = (root / f'tmp/simple-events/regression-{name}.log').read_text(encoding='utf-8')
    assert marker in text and 'Assertion failed' not in text, name
build = (root / 'tmp/simple-events/build-final-fixed.log').read_text(encoding='utf-8')
assert not re.search(r'warning:|error:', build, re.I)
report = {'passed': True, 'cases': cases, 'native_numeric_cases': 18, 'actual_npe_choices': 3,
          'original_c_sdl_frames': 24, 'regressions': [r[0] for r in regressions],
          'scope': 'Original mode23/24 and actual NPE cancellation; complete444-byte roles and250 NPC states, not all C RAM',
          'native_full_segment_checked': True, 'c_full_ram_checked': False,
          'old_npc_zero_clamp_claim_corrected': True, 'original_pcm_checked': False,
          'overall_estimate_percent': 80, 'goal_complete': False,
          'hashes': {p: digest(p) for p in ['tmp/NP560.PRG', 'tmp/tjfy-world-probe.npe',
                                          'npe-sdl/world.c', 'npe-sdl/main.c', 'npe-sdl/build/npe-sdl.exe']}}
assert sum(len(c['frames']) for c in cases) == 24
(root / 'tmp/simple-events/report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print('Simple events verified:18 original CPU/WASM numeric services,3 actual NPE choices,24 original C/SDL frames,4 necessary regressions')
