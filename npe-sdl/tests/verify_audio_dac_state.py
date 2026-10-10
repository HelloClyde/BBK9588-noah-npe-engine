"""Audit recorded original DAC state and affected SDL regressions; no replay."""
import hashlib
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[2]
temp = root / 'tmp'
native = json.loads((temp / 'audio-dac-state-native/report.json').read_text(encoding='utf-8'))
assert native['passed'] and len(native['cases']) == 10
assert native['original_prg_sha256'] == 'd555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
assert native['qemu_sha256'] == 'db37a4e04e9fd1d9190b36aaea0a33c68ce75773c966ee1199d6a62836b12cb6'
names = {c['name']: c for c in native['cases']}
assert names['first-positive']['after']['saved'] == [5, 5]
assert names['first-negative']['after']['saved'] == [-4, -4]
assert names['preserved-targets']['after']['saved'] == [3, -2]
assert names['resume-targets']['after']['current'] == [3, -2]
assert names['muted-first']['after']['current'] == [0, 0]
assert names['stop-muted']['after']['ticks'] == 50
assert names['idle-stop']['after']['current'] == [3, -3]
assert names['stop-first-start']['after']['saved'] == [-2, -2]
for case in native['cases']:
    assert case['passed']
    state = (temp / 'audio-dac-state-native' / (case['name'] + '.cpu.txt')).read_text(encoding='utf-8')
    assert all(marker + '=none' in state for marker in ['ROM_MISSING', 'UNSUPPORTED', 'INVALID_CONTROL'])

checks = []
def check(name, markers):
    path = temp / name
    text = path.read_text(encoding='utf-8', errors='replace')
    assert 'Assertion failed' not in text
    assert all(marker in text for marker in markers)
    checks.append(str(path.relative_to(root)))
    return text

dac = check('audio-dac-state-after-dac.log', [
    'SDL DAC delivery: current-NPE first target, complete/odd word retention, replacement, repeated stop and mute passed',
    '10 original DAC states and actual SDL state lifecycle passed'])
assert len(re.findall(r'Original DAC state .*: 0 differences', dac)) == 10
for stage in ['before', 'after']:
    sound = f'audio-dac-state-{stage}-' + ('sound.log' if stage == 'before' else 'sdl-sound-probe.log')
    initial = f'audio-dac-state-{stage}-' + ('initial.log' if stage == 'before' else 'sdl-initial-pcm-probe.log')
    foreground = f'audio-dac-state-{stage}-' + ('foreground.log' if stage == 'before' else 'sdl-foreground-sound-probe.log')
    check(sound, ['22 loop tails and odd splits, 8 DAC ramp traces and 10 gain calls passed'])
    text = check(initial, ['Original initial PCM:4 gain states, two retained halves and split callback outputs; 0 failed'])
    assert len(re.findall(r'initial .*: state=0 PCM=0 splits=0', text)) == 4
    check(foreground, ['Original 200/250/450/50/0/0 foreground counts:', 'real SDL wakeups passed'])
check('audio-dac-state-after-sdl-quiz-audio-probe.log', ['Six original quiz audio routes and six initially-stopped C cases:'])
check('audio-dac-state-after-sdl-scene-animation-probe.log', ['281 C/actual SDL native comparisons'])
for name in ['audio-dac-state-before-build.log', 'audio-dac-state-after-build.log', 'audio-dac-state-configure.log']:
    text = (temp / name).read_text(encoding='utf-8', errors='replace')
    assert not re.search(r'\b(?:warning|error):', text, re.I)
build = (temp / 'audio-dac-state-after-build.log').read_text(encoding='utf-8')
assert 'Built target npe-sdl' in build and 'Built target sdl-dac-state-probe' in build
assert max((root / 'npe-sdl' / name).stat().st_mtime for name in ['sound_sdl.c', 'sound_sdl.h']) <= (root / 'npe-sdl/build/npe-sdl.exe').stat().st_mtime
hashes = {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in [
    'npe-sdl/sound_sdl.c', 'npe-sdl/sound_sdl.h', 'npe-sdl/build/npe-sdl.exe', 'npe-sdl/main.c',
    'tmp/NP560.PRG', 'tmp/tjfy-world-probe.npe', 'npe-sdl/tests/reference_audio_dac_state.py',
    'npe-sdl/tests/audio_dac_state_native.h', 'npe-sdl/tests/sdl_dac_state_probe.c']}
assert hashes['tmp/NP560.PRG'] == native['original_prg_sha256']
assert hashes['tmp/tjfy-world-probe.npe'] == 'df4b52df6b69e1105a5ea6100e43f35f832ed859c10cb8714879e44ad4d8cb40'
report = dict(passed=True, original_state_paths=10, checks=checks, scene_c_sdl_comparisons=281,
              production_rebuilt=True, hashes=hashes, audible_ramp_complete=False,
              original_codec_verified=False, overall_estimate_percent=80, overall_complete=False,
              scope='Original DAC signed state, saved/first-word targets, mute and output-control sequence. '
                    'Transfer-start calls are explicit original-reference boundaries. SDL mono word delivery '
                    'updates logical DAC state. REP budget is retained; no audible ramp, physical timing, '
                    'mask-ROM codec, full RAM or full-game equivalence claim.')
(temp / 'audio-dac-state-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print('Passed:10 original DAC states, current-NPE SDL state lifecycle and5 affected regressions; '
      'production rebuilt. Audible ramp scheduling and original codec remain incomplete.')
