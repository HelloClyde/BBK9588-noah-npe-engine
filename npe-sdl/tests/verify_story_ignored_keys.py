"""Audit the recorded original and actual SDL story input runs; no replay."""
import difflib
import hashlib
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[2]
temp = root / 'tmp'
original = json.loads((temp / 'story-ignored-original/report.json').read_text(encoding='utf-8'))
assert original['passed'] and len(original['cases']) == 8
assert {(c['raw'], c['more']) for c in original['cases']} == {
    (key, more) for key in [0x86, 0x6b] for more in [0, 1, 2, 255]}
for case in original['cases']:
    assert case['ignoredCommits'] == 0 and case['rawSpaceDelivered']
    before, after = case['before'], case['after']
    if case['more'] == 1:
        assert not case['closes'] and len(case['commits']) == 1
        assert after['mode'] == 5 and after['offset'] == 120 and after['more'] == 0
    else:
        assert case['closes'] and after['mode'] == 1 and after['event'] == 1
        assert all(before[k] == after[k] for k in ['pointer', 'offset', 'more', 'style', 'rect'])

valid = (temp / 'story-input-before-valid.log').read_text(encoding='utf-8', errors='replace')
assert '11 actual SDL live text paths, 21 reader LCD comparisons' in valid
assert len(re.findall(r'SDL live text .*: 0 differences', valid)) == 21
failed = (temp / 'story-input-before-ignored.log').read_text(encoding='utf-8', errors='replace')
assert 'Unexpected story LCD on ignored key 1' in failed and 'Assertion failed' in failed
checks = []
for variant in ['default', 'shift', 'caps']:
    for target, paths, frames, ignored in [
        ('sdl-story-text-service-probe', 11, 21, 378),
        ('sdl-story-ignored-keys-probe', 8, 10, 180),
    ]:
        path = temp / f'story-input-after-{variant}-{target}.log'
        text = path.read_text(encoding='utf-8', errors='replace')
        assert 'Assertion failed' not in text and 'Unexpected story LCD' not in text
        assert len(re.findall(r'SDL live text .*: 0 differences', text)) == frames
        assert f'{paths} actual SDL live text paths, {frames} reader LCD comparisons' in text
        assert f'Story ignored inputs:{ignored}, no World change, resource read or LCD submit' in text
        checks.append(dict(path=str(path.relative_to(root)), variant=variant, paths=paths, frames=frames, ignored=ignored))
regressions = []
for target, frames, marker, pattern in [
    ('sdl-story-pages-probe', 23, '23 actual SDL pages', r'SDL original story .*: 0 differences'),
    ('sdl-window-story-pages-probe', 54, '54 actual SDL pages', r'SDL original story .*: 0 differences'),
    ('sdl-dialogue-return-probe', 46, '46 actual C/SDL commits', r'C=0 SDL=0'),
]:
    path = temp / f'story-input-after-regression-{target}.log'
    text = path.read_text(encoding='utf-8', errors='replace')
    assert 'Assertion failed' not in text and marker in text and len(re.findall(pattern, text)) == frames
    regressions.append(dict(path=str(path.relative_to(root)), frames=frames))
for name in ['story-input-before-build.log', 'story-input-after-build-final.log']:
    text = (temp / name).read_text(encoding='utf-8', errors='replace')
    assert not re.search(r'\b(?:warning|error):', text, re.I)
assert 'Built target npe-sdl' in (temp / 'story-input-after-build-final.log').read_text(encoding='utf-8')
assert (root / 'npe-sdl/main.c').stat().st_mtime <= (root / 'npe-sdl/build/npe-sdl.exe').stat().st_mtime
before = (temp / 'story-input-main-before.c').read_text(encoding='utf-8')
after = (root / 'npe-sdl/main.c').read_text(encoding='utf-8')
assert before != after
(temp / 'story-input-production.diff').write_text(''.join(difflib.unified_diff(
    before.splitlines(True), after.splitlines(True), fromfile='before/main.c', tofile='after/main.c')), encoding='utf-8')
hashes = {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in [
    'npe-sdl/main.c', 'npe-sdl/build/npe-sdl.exe', 'tmp/NP560.PRG', 'tmp/tjfy-world-probe.npe',
    'tmp/tianjiang-online.opennoah', 'npe-sdl/tests/story_text_native.h',
    'tmp/native-story-pages.json', 'tmp/native-window-story-pages.json',
    'tmp/native-story-text-service.json', 'npe-sdl/tests/reference_story_ignored_keys.mjs',
    'npe-sdl/tests/story_ignored_native.h', 'npe-sdl/tests/story_ignored_input.h']}
assert hashes['tmp/NP560.PRG'] == 'd555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
assert hashes['tmp/tjfy-world-probe.npe'] == 'df4b52df6b69e1105a5ea6100e43f35f832ed859c10cb8714879e44ad4d8cb40'
report = dict(passed=True, original_ignored_paths=8, raw_space_confirmations=8, checks=checks,
              regressions=regressions, ignored_sdl_inputs=sum(c['ignored'] for c in checks),
              actual_c_sdl_frames=sum(c['frames'] for c in checks + regressions),
              production_rebuilt=True, hashes=hashes, overall_estimate_percent=80, overall_complete=False,
              scope='Mode5 story input dispatch and existing page/followup behavior. Original diagnostic '
                    'text controls and raw key words; C/SDL retains World, indexed pixels and resource-read '
                    'count on unsupported input. Full RAM, physical timing, audio PCM and full games are '
                    'outside this check; repeated initial pages are not new unique original frames.')
(temp / 'story-ignored-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(f'Passed: 8 original unsupported-key paths, 8 raw Space confirmations, '
      f'{report["actual_c_sdl_frames"]} actual C/SDL comparisons, {report["ignored_sdl_inputs"]} ignored SDL inputs; '
      'production rebuilt, overall goal incomplete.')
