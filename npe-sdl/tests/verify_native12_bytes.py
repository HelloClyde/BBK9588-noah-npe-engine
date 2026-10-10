"""Bind the sampled C-font evidence, completed probe logs and current package.

This audits completed checks; it does not regenerate references or execute
the game. Raster references are sampled original glyph buffers/positions.
"""
import hashlib
import json
import re
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path('npe-sdl/tools').resolve()))
import extract_np560_gbk_font as font_source


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    reference_dir = Path('tmp/native-text12-bytes')
    reference = json.loads((reference_dir / 'report.json').read_text(encoding='utf8'))
    assert reference['passed'] and reference['original_string_cases'] == 36
    assert reference['original_glyphs'] == 128 and not reference['original_lcd_checked']
    assert reference['program_sha256'] == digest('tmp/NP560.PRG')
    assert reference['nand_sha256'] == digest('tmp/NP560_NAND_IMAGE.bin')
    assert reference['fixture_sha256'] == digest(reference_dir / 'strings.bin')
    controls = Path('npe-sdl/native12_control_glyphs.inc')
    assert controls.read_text(encoding='utf8') == font_source.controls_include(Path('tmp/NP560.PRG').read_bytes())
    prior = json.loads(Path('tmp/host-save-keys-report.json').read_text(encoding='utf8'))
    changes = [name for name, expected in prior['production_source_sha256'].items()
               if digest(name) != expected]
    assert changes == ['npe-sdl\\text.c'], changes
    bindings = [Path(name) for name in prior['production_source_sha256']]
    bindings += [controls, Path('npe-sdl/CMakeLists.txt'),
                 Path('npe-sdl/tools/extract_np560_gbk_font.py'),
                 Path('npe-sdl/tests/reference_text12_bytes.py'),
                 Path('npe-sdl/tests/sdl_native12_bytes_probe.c'),
                 Path('npe-sdl/tests/npe_text_test.c'), Path(__file__),
                 reference_dir / 'report.json', reference_dir / 'strings.bin']
    assets = {'assets/gbk12-native.bin': 'a7b1d94bfb8edc3f082c2a8dfba064c57d00fe2a98b2a9a996af5c521f931391',
              'assets/gbk16-native.bin': 'c122ccf2562dc2c35f20ef0a6ea44f2c06ea8db1c4eaa669e2869f03ce598e43'}
    for name, expected in assets.items():
        assert digest(Path('npe-sdl') / name) == expected
        bindings.append(Path('npe-sdl') / name)
    failure = Path('tmp/native12-fallback-before-failure.log')
    assert 'Original C-font string 1 pitch6: result=-1, first-byte=CB' in failure.read_text(encoding='utf8')
    probes = {
        'strings': '36 C/actual SDL rasters, 128 glyph positions passed',
        'release-strings': '36 C/actual SDL rasters, 128 glyph positions passed',
        'shop-text': '25 original frames/states',
        'magic-retain': '4 native modal paths/16 actual SDL LCD commits',
        'extended': '150 original glyphs: C and actual SDL raster passed',
        'text-test': 'NPE GBK bitmap dialogue passed',
        'text16': 'Original 16-row string glyph/position checks: 9 passed',
    }
    checks = []
    for name, expected in probes.items():
        path = Path('tmp/native12-fallback-' + name + '.log')
        log = path.read_text(encoding='utf8')
        assert expected in log and 'Assertion failed' not in log, name
        checks.append(dict(name=name, completion_found=True, log=str(path), log_sha256=digest(path)))
    warnings = {}
    for name in ['build', 'text-test-build', 'release-build', 'delivery-build']:
        path = Path('tmp/native12-fallback-' + name + '.log')
        log = path.read_text(encoding='utf8')
        warnings[name] = len(re.findall(r'\bwarning:', log, re.I))
        assert not warnings[name] and not re.search(r'\berror:', log, re.I), name
        bindings.append(path)
    package = Path('npe-sdl/dist/npe-sdl-windows-x64-dev.zip')
    portable_path = Path('tmp/native12-fallback-portable-report.json')
    portable = json.loads(portable_path.read_text(encoding='utf8'))
    assert portable['passed'] and len(portable['checks']) == 14
    assert portable['package_sha256'] == digest(package)
    assert portable['payload_sha256']['npe-sdl.exe'] == digest('tmp/npe-portable-release-build/npe-sdl.exe')
    assert portable['payload_sha256']['npe-sdl.exe'] == digest('npe-sdl/dist/windows-x64/npe-sdl.exe')
    for name, expected in assets.items():
        assert portable['payload_sha256'][name] == expected
    with zipfile.ZipFile(package) as archive:
        for name, expected in portable['payload_sha256'].items():
            raw = archive.read('npe-sdl-windows-x64-dev/' + name)
            assert hashlib.sha256(raw).hexdigest() == expected, name
    for name, expected in portable['original_games'].items():
        assert digest(name) == expected, name
    for name in ['npe-sdl/build/npe-sdl.exe', 'tmp/npe-portable-release-build/npe-sdl.exe',
                 'tmp/npe-portable-probe-build/sdl-native12-bytes-probe.exe',
                 'npe-sdl/assets/FONT-PROVENANCE.txt', 'plan.md']:
        bindings.append(Path(name))
    report = dict(passed=True, overall_complete=False, overall_progress_estimate=80,
                  original_string_cases=36, original_glyphs_and_positions=128,
                  c_sdl_string_cases_per_build=36, verified_builds=['normal', 'Release'],
                  existing_original_menu_lcd_frames=41, existing_glyph_rasters=150,
                  existing_text16_strings=9, font_assets_unchanged=True,
                  changed_existing_production_sources=changes,
                  added_production_source=str(controls), checks=checks, build_warnings=warnings,
                  reference_scope='Original C-font default string parsing, glyph buffers and coordinates; '
                                  'font backing/flash transport relocated and LCD drawing adapted. '
                                  'Bounded strings use two explicit zero padding bytes. '
                                  'Existing shop/magic regression uses original full LCD references. '
                                  'No claim about all callers, warm adjacent bytes, original audio or full games.',
                  package_sha256=digest(package), package_bytes=package.stat().st_size,
                  portable_checks=14, portable_report=str(portable_path),
                  sha256={str(path): digest(path) for path in bindings})
    Path('tmp/native12-fallback-report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print('Native C font: 36 original strings / 128 glyph positions, normal and Release SDL passed; '
          '41 original menu LCDs, 150 glyph rasters, 9 D-font strings and 14 delivery checks passed')


if __name__ == '__main__':
    main()
