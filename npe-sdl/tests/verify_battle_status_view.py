"""Independently compare persisted original/C battle status evidence.

Run at repository root after tmp/run_battle_status_view_checks.py.
Only native entry and before-input records seed the C fixture.
"""
import hashlib
import json
from pathlib import Path


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


prefix = 'tmp/battle-status-view-'
prior = json.loads(Path('tmp/battle-phase-resident-report.json').read_text())
immutable = prior['immutable_sha256']
for path, digest in immutable.items():
    assert sha(path) == digest, path

targets = json.loads(Path(prefix+'check-targets.json').read_text())
assert len(targets) == 27 and len(set(targets)) == 27
codes = {}
labels = ['native', 'load-native'] + ['final-'+label for label in
          ['generator', 'configure', 'build', *targets]]
for label in labels:
    path = prefix+label+'.exit'
    assert Path(path).read_text(encoding='utf-8-sig').strip() == '0', path
    codes[path] = 0
build = Path(prefix+'final-build.log').read_text(encoding='utf-8-sig')
assert 'warning:' not in build and 'error:' not in build

rows = json.loads(Path('tmp/native-battle-status-view.json').read_text())['rows']
assert [r['config']['name'] for r in rows] == ['one', 'two', 'three', 'three-retained']
evidence = {}


def compare(actual, expected, length):
    data = Path(actual).read_bytes()
    assert len(data) == length and data == expected, actual
    evidence[actual] = sha(actual)


totals = {}
for route in ['direct', 'main']:
    commits = states = records = 0
    for row in rows:
        name = row['config']['name']
        assert len(row['steps']) == 12
        assert [s['key'] for s in row['steps']] == [16,2,1,1,2,8,4,16,64,16,2,64]
        for step, sample in enumerate(row['steps']):
            after = sample['after']
            assert after['map'] == row['before']['map']
            assert after['battle'] == row['before']['battle']
            assert after['actor'] == row['config']['actor'] and after['mode'] == 3
            controls = bytes(after[k] for k in ['phase', 'actor', 'member', 'row'])
            all_records = b''.join(bytes(r) for r in after['map']+after['battle'])
            compare(f'tmp/battle-status-view-c-{route}-{name}-state-{step}.bin',
                    controls+all_records, 4+9*148)
            states += 1
            records += 9
            for frame in range(len(sample['frames'])):
                native = f'tmp/native-battle-status-view-{name}-{step}-{frame}.idx'
                compare(f'tmp/battle-status-view-c-{route}-{name}-{step}-{frame}.idx',
                        Path(native).read_bytes(), 28800)
                evidence[native] = sha(native)
                commits += 1
    assert (commits, states, records) == (71,48,432)
    totals[route] = dict(pixel_commits=commits, control_states=states,
                         full_physical_records=records)

warm = json.loads(Path('tmp/native-battle-status-member-load.json').read_text())
assert [r['entry'] for r in warm] == ['system','title','new','empty']
for row in warm:
    assert row['beforeBytes'] == row['afterBytes'] == [129,166]
    base = f"tmp/battle-status-member-own-{row['entry']}-base.dat"
    changed = f"tmp/battle-status-member-own-{row['entry']}-altered.dat"
    original = Path(base).read_bytes()
    assert original[:8] == b'NPESAV22'
    compare(changed, original, len(original))
    evidence[base] = sha(base)

sources = ['npe-sdl/'+s for s in
           ['main.c','world.c','npe.h','save.c','text.c','text.h',
            'resident_resources.inc','CMakeLists.txt']]
sources += ['npe-sdl/tests/'+s for s in
            ['reference_battle_status_view.mjs','reference_battle_selection_load.mjs',
             'generate_battle_status_view_fixture.py','battle_status_view_native.h',
             'battle_status_member_load_native.h','sdl_battle_status_view_probe.c',
             'battle_menu_rows_load_probe.c','status_overlay_probe.c',
             'sdl_status_overlay_probe.c','verify_battle_status_view.py']]
# The source includes header binding-count updates used by warm-load checks.
sources += [str(p).replace('\\','/') for p in Path('npe-sdl/tests').glob('*.c')
            if '3534' in p.read_text(encoding='utf-8')]
sources += ['tmp/run_battle_status_view_checks.py','tmp/battle-status-view-source.asm']
report = dict(passed=True,
              scope='Four battle status public-input paths, direct and actual main SDL, retained member byte and four warm constructors',
              routes=totals, paths=4, world_bound_bytes=3534,
              short_checks=27, build_targets=28, build_warnings=0,
              warm_constructors=4, own_save_format='NPESAV22',
              seed_scope='Native entry and before-input records only; no native end states',
              pending=['selector-plane 52:7AA4 and shared UI heap lifecycle',
                       'all selector writers and out-of-range member execution',
                       'other battle lifecycle/action combinations',
                       'original audio, physical timing and other NPE dynamic compatibility'],
              exit_codes=codes, immutable_sha256=immutable,
              source_sha256={p:sha(p) for p in sorted(set(sources))},
              evidence_sha256=evidence)
Path(prefix+'report.json').write_text(json.dumps(report,indent=2)+'\n')
print('Passed:142 LCD commits,96 control states,864 full physical records,4 warm constructors,27 short checks')
