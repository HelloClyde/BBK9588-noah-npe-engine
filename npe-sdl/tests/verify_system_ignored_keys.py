"""Audit existing title/system/slot input results without replaying games."""
import hashlib
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[2]
temp = root / 'tmp'
native = json.loads((temp / 'system-ignored-original/report.json').read_text(encoding='utf-8'))
assert native['passed'] and len(native['cases']) == 9
original_inputs = sum(len(c['ignored']) for c in native['cases'])
confirmations = sum(c['rawConfirmations'] for c in native['cases'])
assert original_inputs == 1027 and confirmations == 17
for case in native['cases']:
    for key in case['ignored']:
        assert not key['commits'] and key['statePreserved'] and key['pixelsPreserved']
    for phase in case['phases']:
        if phase['mask'] == 16:
            assert phase['frames']

checks = []
for variant in ['default', 'shift', 'caps']:
    for target, frames, ignored, pattern in [
        ('sdl-title-slot-controls-probe', 24, 154, r'frame\d+ C0 SDL0'),
        ('sdl-system-menu-return-probe', 77, 532, r'C=0 SDL=0'),
        ('sdl-system-slot-return-probe', 33, 196, r'C=0 SDL=0'),
    ]:
        path = temp / f'system-after-{variant}-{target}.log'
        text = path.read_text(encoding='utf-8', errors='replace')
        assert 'Assertion failed' not in text and 'Unexpected LCD' not in text
        assert len(re.findall(pattern, text)) == frames
        assert f'Title/system ignored inputs:{ignored}, retained World, controls and LCD' in text
        before = (temp / f'system-before-valid-{target}.log').read_text(encoding='utf-8', errors='replace')
        assert 'Assertion failed' not in before and len(re.findall(pattern, before)) == frames
        failed = (temp / f'system-before-ignored-{target}.log').read_text(encoding='utf-8', errors='replace')
        assert 'Assertion failed' in failed
        if target != 'sdl-title-slot-controls-probe':
            assert 'Unexpected LCD on ignored title/system key 1' in failed
        checks.append(dict(path=str(path.relative_to(root)), variant=variant, frames=frames, ignored=ignored))

for name in ['system-ignored-build-before.log', 'system-ignored-build-after.log']:
    text = (temp / name).read_text(encoding='utf-8', errors='replace')
    assert not re.search(r'\b(?:warning|error):', text, re.I)
assert 'Built target npe-sdl' in (temp / 'system-ignored-build-after.log').read_text(encoding='utf-8')
assert (root / 'npe-sdl/main.c').stat().st_mtime <= (root / 'npe-sdl/build/npe-sdl.exe').stat().st_mtime
music = (temp / 'system-after-music-regression.log').read_text(encoding='utf-8', errors='replace')
assert 'Assertion failed' not in music
assert '60 C/SDL popup states' in music and 'real system-menu entry and native F4 route passed' in music
hashes = {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in [
    'npe-sdl/main.c', 'npe-sdl/build/npe-sdl.exe', 'tmp/NP560.PRG',
    'tmp/tjfy-world-probe.npe', 'tmp/tianjiang-online.opennoah',
    'npe-sdl/tests/reference_system_ignored_keys.mjs', 'npe-sdl/tests/system_ignored_input.h']}
assert hashes['tmp/NP560.PRG'] == 'd555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
assert hashes['tmp/tjfy-world-probe.npe'] == 'df4b52df6b69e1105a5ea6100e43f35f832ed859c10cb8714879e44ad4d8cb40'
report = dict(passed=True, original_ignored_inputs=original_inputs, raw_confirmations=confirmations,
              checks=checks, actual_c_sdl_frames=sum(c['frames'] for c in checks),
              ignored_sdl_inputs=sum(c['ignored'] for c in checks),
              music_popup_state_checks=60, production_rebuilt=True, hashes=hashes,
              overall_estimate_percent=80, overall_complete=False,
              scope='Title, system, slot and music input routing. Original samples compare control fields and pixels; '
                    'C World comparison excludes host defer_story_events. Audio PCM, physical timing, full games '
                    'and other UI callers are outside this check.')
(temp / 'system-ignored-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(f'Passed: {original_inputs} original ignored keys, {confirmations} raw confirmations, '
      f'{report["actual_c_sdl_frames"]} C/SDL frames, {report["ignored_sdl_inputs"]} SDL ignored inputs; '
      'production rebuilt, overall goal incomplete.')
