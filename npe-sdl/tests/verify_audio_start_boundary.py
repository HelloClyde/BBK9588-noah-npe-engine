"""Compare original driver-start states, old failure and current SDL checks.

This is an isolated software publication boundary, not original codec/PCM,
complete initialization, track EOF or physical DAC timing evidence.
"""
import hashlib
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[2]
def digest(path):
    return hashlib.sha256((root/path).read_bytes()).hexdigest()

native = json.loads((root/'tmp/audio-start-boundary-native/report.json').read_text(encoding='utf-8'))
assert native['passed'] and len(native['cases']) == 8
assert not native['original_codec_checked'] and not native['physical_timing_checked']
assert native['original_prg_sha256'] == digest('tmp/NP560.PRG')
assert native['fixture_sha256'] == digest('tmp/audio-start-boundary-native/states.bin')
pattern = r'start boundary (\d+): flags=([0-9a-f]{4}); (\d+) original Timer2 ticks; differences=(\d+)'
def rows(path):
    text = (root/path).read_text(encoding='utf-8')
    values = [(int(i), int(flags, 16), int(ticks), int(bad)) for i, flags, ticks, bad in re.findall(pattern, text)]
    assert len(values) == 8 and [r[0] for r in values] == list(range(8))
    return values, text

before, before_log = rows('tmp/sdl-start-boundary-before.log')
after, after_log = rows('tmp/start-boundary-sdl-start-boundary-probe.log')
assert '7 failed' in before_log and '0 failed' in after_log
for index, case in enumerate(native['cases']):
    assert case['passed'] and case['stack_restored']
    assert before[index][1] == case['flags89_before']
    assert before[index][2] == after[index][2] == case['timer2_ticks']
    assert after[index][1] == case['flags89_after'] and after[index][3] == 0
assert sum(r[3] != 0 for r in before) == 7
tests = json.loads((root/'tmp/audio-start-boundary-tests.json').read_text(encoding='utf-8'))
assert len(tests) == 8 and all(test['passed'] and test['exit_code'] == 0 for test in tests)
for test in tests:
    text = (root/test['log']).read_text(encoding='utf-8')
    assert not any(marker in text.lower() for marker in ['assertion failed', 'assertion `', 'abort', 'unsupported'])
    test['log_sha256'] = digest(test['log'])
for name in ['audio-start-boundary-final-build.log', 'audio-start-boundary-probe-rebuild.log']:
    text = (root/'tmp'/name).read_text(encoding='utf-8')
    assert 'warning:' not in text.lower() and 'error:' not in text.lower()
assert 'Built target npe-sdl' in (root/'tmp/audio-start-boundary-final-build.log').read_text(encoding='utf-8')
assert 'Building C object' in (root/'tmp/audio-start-boundary-probe-rebuild.log').read_text(encoding='utf-8')
previous = json.loads((root/'tmp/audio-initial-pcm-report.json').read_text(encoding='utf-8'))
for name in ['npe-sdl/sound_sdl.h', 'npe-sdl/audio.c', 'npe-sdl/audio_decode.c']:
    assert digest(name) == previous['source_sha256'][name]
gameplay = json.loads((root/'tmp/special-map-save-report.json').read_text(encoding='utf-8'))
for name in ['npe-sdl/world.c', 'npe-sdl/save.c', 'npe-sdl/save_terrain.inc', 'npe-sdl/npe.h']:
    assert digest(name) == gameplay['source_sha256'][name]
sources = ['npe-sdl/sound_sdl.c', 'npe-sdl/CMakeLists.txt',
           'npe-sdl/tests/reference_audio_start_boundary.py',
           'npe-sdl/tests/sdl_start_boundary_probe.c']
result = dict(passed=True, original_cases=8, old_failed_cases=7, current_failed_cases=0,
              native_reference='tmp/audio-start-boundary-native/report.json',
              tests=tests, regressions=7, candidate_pcm_sections=22, candidate_pcm_frames=10486,
              source_sha256={name: digest(name) for name in sources},
              executable_sha256=digest('npe-sdl/build/npe-sdl.exe'),
              original_prg_sha256=digest('tmp/NP560.PRG'),
              current_npe_sha256=digest('tmp/tjfy-world-probe.npe'),
              limitations=['Post-prefill flags are explicit diagnostic inputs, not natural codec exhaustion.',
                           'Original start reference uses zero DAC targets and no codec or file open.',
                           'Candidate PCM is unchanged, not proved equal to the original codec.',
                           'Full DAC ramp, actual track EOF/loop and remaining game behaviors are unfinished.'],
              overall_estimate_percent=80, goal_complete=False)
(root/'tmp/audio-start-boundary-report.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
print(json.dumps({key: result[key] for key in ['passed', 'original_cases', 'old_failed_cases', 'current_failed_cases', 'regressions']}))
