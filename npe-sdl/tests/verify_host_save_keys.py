"""Audit recorded SDL host-key checks and the updated portable payload.

F5/F9 are SDL desktop keys, not sampled original hardware key values. The
unchanged original fixtures verify the surrounding map/menu paths; the new
checks verify that these host keys do not invoke a separate save/load route.
"""
import hashlib
import json
import re
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256((root / path).read_bytes()).hexdigest()


source = (root / 'npe-sdl/main.c').read_text(encoding='utf-8')
assert 'SDLK_F5' not in source and 'SDLK_F9' not in source
assert 'SDL_ShowSimpleMessageBox' not in source
assert 'F9: load save' not in source and 'D: defend; F: flee' not in source
assert 'else if (walk_preview)\n            snprintf(caption, sizeof(caption), "NPE");' in source
assert 'npe_world_save(&file, &saved, path)' in source
assert 'npe_world_restore(&file, &world, path)' in source
assert 'F5／F9' not in (root / 'npe-sdl/PORTABLE.md').read_text(encoding='utf-8')
failure = (root / 'tmp/host-save-keys-before-failure.log').read_text(encoding='utf-8')
assert 'Non-original F5 reached the save service' in failure and 'Assertion failed' in failure

checks = []
for target, frames, ignored, pattern in [
    ('sdl-walk-pose-probe', 84, 20, r'commit\d+ (?:C differences=0|C=0 SDL=0)'),
    ('sdl-system-menu-return-probe', 77, 152, r'C=0 SDL=0'),
    ('sdl-system-slot-return-probe', 33, 56, r'C=0 SDL=0'),
    ('sdl-title-slot-controls-probe', 24, 44, r'frame\d+ C0 SDL0'),
]:
    log = Path('tmp/host-save-keys-' + target + '.log')
    text = (root / log).read_text(encoding='utf-8')
    assert 'Assertion failed' not in text and 'Unexpected LCD' not in text, target
    assert len(re.findall(pattern, text)) == frames, target
    if target == 'sdl-walk-pose-probe':
        assert len(re.findall(r'commit\d+ SDL differences=0', text)) == 42
        assert len(re.findall(r'commit\d+ C=0 SDL=0', text)) == 42
        assert f'Map F5/F9 ignored: {ignored} actual inputs without save/restore calls or LCD/state/timer changes' in text
    else:
        assert f'Title/system ignored inputs:{ignored}, retained World, controls and LCD' in text
    checks.append(dict(target=target, frames=frames, ignored=ignored,
                       verified_exit_code=0, log=str(log), log_sha256=sha(log)))
slot_log = Path('tmp/host-save-keys-slot-flow.log')
assert 'SDL title and system slot flow passed' in (root / slot_log).read_text(encoding='utf-8')
warnings = {}
for name in ('host-save-keys-build.log', 'host-save-keys-release-build.log',
             'host-save-keys-delivery-build.log'):
    text = (root / 'tmp' / name).read_text(encoding='utf-8')
    warnings[name] = len(re.findall(r'warning:', text, re.I))
    assert not re.search(r'\b(?:warning|error):', text, re.I), name

portable_path = root / 'tmp/host-save-keys-portable-report.json'
portable = json.loads(portable_path.read_text(encoding='utf-8'))
assert portable['passed'] and len(portable['checks']) == 14
package = Path('npe-sdl/dist/npe-sdl-windows-x64-dev.zip')
assert portable['package_sha256'] == sha(package)
with zipfile.ZipFile(root / package) as archive:
    exe = [name for name in archive.namelist() if name == 'npe-sdl.exe' or name.endswith('/npe-sdl.exe')]
    assert len(exe) == 1
    assert hashlib.sha256(archive.read(exe[0])).hexdigest() == sha('tmp/npe-portable-release-build/npe-sdl.exe')
assert sha('tmp/npe-portable-release-build/npe-sdl.exe') == sha('npe-sdl/dist/windows-x64/npe-sdl.exe')
production = {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
              for pattern in ('*.c', '*.h', '*.inc') for path in sorted((root / 'npe-sdl').glob(pattern))}
before = json.loads((root / 'tmp/host-save-keys-before/npe-portable-report.json').read_text(encoding='utf-8'))
changed = [name for name, digest in production.items() if before['production_source_sha256'].get(name) != digest]
assert changed == [str(Path('npe-sdl/main.c'))], changed
portable.update(production_source_sha256=production, release_warnings=0,
                release_build='tmp/npe-portable-release-build',
                production_code_changed_this_batch=True, overall_complete=False)
portable_path.write_text(json.dumps(portable, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
(root / 'tmp/npe-portable-report.json').write_bytes(portable_path.read_bytes())
original_hashes = {name: sha(name) for name in ('tmp/NP560.PRG', 'tmp/tjfy-world-probe.npe')}
assert original_hashes['tmp/NP560.PRG'] == 'd555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
assert original_hashes['tmp/tjfy-world-probe.npe'] == 'df4b52df6b69e1105a5ea6100e43f35f832ed859c10cb8714879e44ad4d8cb40'
report = dict(passed=True, checks=checks, original_c_sdl_frames=sum(c['frames'] for c in checks),
              ignored_sdl_host_inputs=sum(c['ignored'] for c in checks),
              before_fix_save_call_blocked_by_diagnostic=True,
              slot_flow_verified_exit_code=0, slot_flow_log=str(slot_log),
              build_warnings=warnings, production_source_changes=changed,
              production_source_sha256=production, original_hashes=original_hashes,
              reference_headers_sha256={name: sha('npe-sdl/tests/' + name) for name in (
                  'walk_native.h', 'system_menu_return_native.h',
                  'system_slot_return_native.h', 'title_slot_controls_native.h')},
              primary_exe_sha256=sha('npe-sdl/build/npe-sdl.exe'),
              package_sha256=sha(package), package_bytes=(root / package).stat().st_size,
              portable_checks=14, portable_report=str(portable_path.relative_to(root)),
              overall_estimate_percent=80, overall_complete=False,
              scope='removed non-original global F5/F9 save/load and stale gameplay captions; original map/menu fixtures and owned three-slot flow preserved; SDL host key invariance, not original hardware F5/F9 sampling or full-game proof')
(root / 'tmp/host-save-keys-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(f'Passed: {report["ignored_sdl_host_inputs"]} SDL host inputs, '
      f'{report["original_c_sdl_frames"]} original C/SDL frames, three-slot flow and14 portable checks; goal incomplete.')
