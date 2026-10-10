"""Audit existing map-menu input evidence without replaying games/oracles."""
import hashlib
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[2]
temp = root / 'tmp'
native = json.loads((temp / 'native-map-menu-ignored-keys.json').read_text(encoding='utf-8'))
assert native['passed'] and len(native['families']) == 4
original_inputs = 0
calibrated_frames = 0
for family in native['families']:
    assert family['rawConfirmationCalibrated']
    assert family['calibratedFrames'] > 0
    calibrated_frames += family['calibratedFrames']
    for case in family['cases']:
        assert case['commits'] == 0 and case['statePreserved'] and case['pixelsPreserved']
        original_inputs += 1
assert original_inputs == 260
assert sum(bool(f['depletedDirectionsIgnored']) for f in native['families']) == 1

checks = []
for variant in ['default', 'shift', 'caps']:
    for target, frames, ignored in [
        ('sdl-map-item-record-probe', 18, 161),
        ('sdl-equip-item-record-probe', 36, 238),
        ('sdl-map-skill-record-probe', 51, 378),
    ]:
        path = temp / f'map-menu-after-{variant}-{target}.log'
        text = path.read_text(encoding='utf-8', errors='replace')
        assert 'Assertion failed' not in text and 'Unexpected LCD' not in text
        assert len(re.findall(r'C=0 SDL=0', text)) == frames
        assert f'Map menu ignored inputs:{ignored}, no World change, resource read or LCD submit' in text
        before = (temp / f'map-menu-before-valid-{target}.log').read_text(encoding='utf-8', errors='replace')
        assert 'Assertion failed' not in before and len(re.findall(r'C=0 SDL=0', before)) == frames
        failed = (temp / f'map-menu-before-ignored-{target}.log').read_text(encoding='utf-8', errors='replace')
        assert 'Unexpected LCD on ignored map menu key 1' in failed and 'Assertion failed' in failed
        checks.append(dict(path=str(path.relative_to(root)), variant=variant, frames=frames, ignored=ignored))

for name in ['map-menu-ignored-build-before.log', 'map-menu-ignored-build-after.log',
             'map-menu-regression-build.log', 'map-menu-flow-final-build.log']:
    text = (temp / name).read_text(encoding='utf-8', errors='replace')
    assert not re.search(r'\b(?:warning|error):', text, re.I)
assert 'Built target npe-sdl' in (temp / 'map-menu-ignored-build-after.log').read_text(encoding='utf-8')
assert (root / 'npe-sdl/main.c').stat().st_mtime <= (root / 'npe-sdl/build/npe-sdl.exe').stat().st_mtime

flow = (temp / 'map-menu-regression-final-sdl-map-item-flow-probe.log').read_text(encoding='utf-8', errors='replace')
assert 'Assertion failed' not in flow and 'Map-use native screen comparisons: 44' in flow
assert len(re.findall(r'SDL original map-use .*passed', flow)) == 15
equip = (temp / 'map-menu-regression-sdl-equip-target-flow-probe.log').read_text(encoding='utf-8', errors='replace')
assert 'Assertion failed' not in equip and 'SDL original depletion:' in equip
assert 'SDL original equipment quantity100 and signed-negative inventory guard passed' in equip

paths = ['npe-sdl/main.c', 'npe-sdl/build/npe-sdl.exe', 'tmp/NP560.PRG',
         'tmp/tjfy-world-probe.npe', 'tmp/tianjiang-online.opennoah',
         'npe-sdl/tests/reference_map_menu_ignored_keys.mjs',
         'npe-sdl/tests/map_menu_ignored_input.h']
hashes = {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in paths}
assert hashes['tmp/NP560.PRG'] == 'd555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
assert hashes['tmp/tjfy-world-probe.npe'] == 'df4b52df6b69e1105a5ea6100e43f35f832ed859c10cb8714879e44ad4d8cb40'
report = dict(passed=True, original_ignored_inputs=original_inputs,
              original_depleted_direction_confirm_inputs=3,
              raw_confirmation_calibration_frames=calibrated_frames,
              checks=checks, actual_c_sdl_frames=sum(c['frames'] for c in checks),
              ignored_sdl_inputs=sum(c['ignored'] for c in checks),
              map_use_regression_paths=15, map_use_unique_reference_screens=44,
              equipment_flow_regression=True, production_rebuilt=True, hashes=hashes,
              overall_estimate_percent=80, overall_complete=False,
              scope='Measured map item/equipment/skill pages and sampled original state. '
                    'Raw keyboard-word delivery calibrated with valid confirmations. '
                    'Final map periodic commits, hardware timing, full games and original sound are outside this check.')
path = temp / 'map-menu-ignored-report.json'
path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(f'Passed: {original_inputs} original ignored inputs, {report["actual_c_sdl_frames"]} C/SDL frames, '
      f'{report["ignored_sdl_inputs"]} SDL ignored inputs; production rebuilt, overall goal incomplete.')
