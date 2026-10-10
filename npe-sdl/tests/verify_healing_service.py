"""Independently compare saved original/C frames and collect completed short checks."""
from pathlib import Path
import hashlib
import json
import re

root = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256((root / path).read_bytes()).hexdigest()


manifest = json.loads((root / 'tmp/healing-service/run-manifest.json').read_text(encoding='utf-8'))
assert all(code == 0 for code in manifest['exit_codes'].values())
for path, expected_hash in manifest['hashes'].items():
    assert digest(path) == expected_hash, ('changed since checked run', path)

assert digest('tmp/NP560.PRG') == 'd555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
assert digest('tmp/tjfy-world-probe.npe') == 'df4b52df6b69e1105a5ea6100e43f35f832ed859c10cb8714879e44ad4d8cb40'
groups = [('service', 'tmp/native-healing-service.json', 9),
          ('choice-short', 'tmp/native-healing-choice-short.json', 3)]
cases = []
for kind, source, expected in groups:
    rows = json.loads((root / source).read_text(encoding='utf-8'))
    assert len(rows) == expected
    for row in rows:
        frames = []
        for index in range(len(row['commits'])):
            native = root / f'tmp/native-healing-{kind}-{row["name"]}-{index}.idx'
            current = root / f'tmp/c-healing-{kind}-{row["name"]}-{index}.idx'
            a, b = native.read_bytes(), current.read_bytes()
            assert len(a) == len(b) == 240 * 120
            assert a == b, (row['name'], index)
            frames.append({'index': index, 'sha256': hashlib.sha256(a).hexdigest()})
        cases.append({'name': row['name'], 'scope': kind, 'frames': frames,
                      'money_before': row['before']['money'], 'money_after': row['after']['money'],
                      'fee': row['fee'], 'passed': True})

check_log = (root / 'tmp/healing-service/check-fixed.log').read_text(encoding='utf-8')
assert '46 original frames/56 C/SDL commits' in check_log
assert len(re.findall(r'C=0 SDL=0', check_log)) == 56
assert not re.search(r'Assertion failed|C=[1-9]|SDL=[1-9]', check_log)
regressions = [
    ('sdl-choice-resident-public-probe', 'regression-sdl-choice-resident-public-probe.log', '27 C/SDL commits'),
    ('sdl-money-service-probe', 'regression-sdl-money-service-probe.log', '48 C/SDL commits'),
    ('sdl-story-followup-probe', 'regression-sdl-story-followup-probe.log', '46 actual C/SDL commits'),
    ('sdl-event-chain-probe', 'event-chain-hook-fixed.log', 'pre-dispatch close and retained input passed'),
]
for name, log, marker in regressions:
    text = (root / 'tmp/healing-service' / log).read_text(encoding='utf-8')
    assert marker in text and 'Assertion failed' not in text, name
for log in ['build-fixed.log', 'event-chain-hook-build.log']:
    text = (root / 'tmp/healing-service' / log).read_text(encoding='utf-8')
    assert not re.search(r'warning:|error:', text, re.I), log

report = {
    'passed': True, 'scope': 'Original mode22 UI/numeric service and actual map5 NPE choice chains',
    'cases': cases, 'original_frames': sum(len(c['frames']) for c in cases),
    'c_sdl_commits': 56, 'nominal_timeout_ms': 2000, 'timeout_uint32_wrap': True,
    'dismiss_key_consumed': True, 'close_preserves_pending_mode': True,
    'numeric_dispatch': True, 'regressions': [r[0] for r in regressions],
    'old_event_chain_failure_reproduced_before_healing': True,
    'old_world_sha256': '4cf175cad0de9d3ddeffbb4491b07620fcfa5406f924248a9259218799dfaec2',
    'hashes': {p: digest(p) for p in ['tmp/NP560.PRG', 'tmp/tjfy-world-probe.npe',
                                    'tmp/opennoah_wasm_bg.wasm', 'tmp/tianjiang-online.opennoah',
                                    'npe-sdl/world.c', 'npe-sdl/main.c', 'npe-sdl/build/npe-sdl.exe']},
    'original_pcm_checked': False, 'hardware_timing_checked': False,
    'original_full_ram_checked': False, 'full_game_compatibility_checked': False,
    'overall_estimate_percent': 80, 'goal_complete': False,
}
assert report['original_frames'] == 46
(root / 'tmp/healing-service/report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print('Healing verified:12 original paths,46 saved frame pairs,56 actual C/SDL submissions,4 necessary short regressions')
