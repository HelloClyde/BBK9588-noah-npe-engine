"""Audit completed original framebuffer comparisons, regressions and package.

The reference runs original font drawing into the complete RAM display buffer.
It does not prove LCD transfer, arbitrary coordinates or natural game routes.
This verifier binds completed evidence; it does not execute the game.
"""
import hashlib
import json
import re
import struct
import zipfile
from pathlib import Path


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    root = Path('tmp/native-font-edges')
    reference = json.loads((root / 'report.json').read_text(encoding='utf8'))
    assert reference['passed'] and reference['original_framebuffer_cases'] == 60
    assert not reference['original_lcd_transport_checked'] and not reference['draw_adapters']
    assert reference['font_backing_relocated'] and reference['explicit_zero_padding'] == 2
    assert reference['program_sha256'] == digest('tmp/NP560.PRG')
    assert reference['fixture_sha256'] == digest(root / 'frames.bin')
    assert [row[0] for row in reference['checked_code']] == [
        0x281F08, 0x179244, 0x28FD38, 0x186454, 0x18657A, 0x3307B0]
    raw = (root / 'frames.bin').read_bytes()
    assert raw[:8] == b'NPEFED01' and struct.unpack_from('<I', raw, 8)[0] == 60
    pos = 12
    for case in reference['cases']:
        height, pitch, left, top, color, size, end_x, background = struct.unpack_from('<8H', raw, pos)
        pos += 16
        assert (height, pitch, left, top, color, end_x, background) == (
            case['height'], case['pitch'], case['left'], case['top'], case['color'], case['end_x'], 7)
        assert raw[pos:pos + size] == bytes.fromhex(case['text_hex'])
        pos += size
        pixels = raw[pos:pos + 28800]
        pos += 28800
        assert hashlib.sha256(pixels).hexdigest() == case['pixels_sha256']
        assert sum(value != 7 for value in pixels) == case['changed_pixels']
    assert pos == len(raw) and len(reference['cases']) == 60

    prior = json.loads(Path('tmp/native12-fallback-report.json').read_text(encoding='utf8'))
    production = {name: expected for name, expected in prior['sha256'].items()
                  if Path(name).parent == Path('npe-sdl') and Path(name).suffix in {'.c', '.h', '.inc'}}
    assert len(production) == 60
    changes = [Path(name).as_posix() for name, expected in production.items() if digest(name) != expected]
    assert changes == ['npe-sdl/text.c'], changes
    for name in ['tmp/native-text12-bytes/report.json', 'tmp/native-text12-bytes/strings.bin']:
        assert digest(name) == prior['sha256'][str(Path(name))]
    failure = Path('tmp/font-edges-before-failure.log').read_text(encoding='utf8')
    assert 'Original font edge 1 height12 pitch6 x231 y16 end243: result=-1' in failure
    probes = {
        'frames': '60 complete C/actual SDL framebuffer comparisons passed',
        'release-frames': '60 complete C/actual SDL framebuffer comparisons passed',
        'strings': '36 C/actual SDL rasters, 128 glyph positions passed',
        'release-strings': '36 C/actual SDL rasters, 128 glyph positions passed',
        'shop-text': '25 original frames/states',
        'magic-retain': '4 native modal paths/16 actual SDL LCD commits',
        'story-text': '11 actual SDL live text paths, 21 reader LCD comparisons',
        'title-slots': '24 C/SDL comparisons,wrap,cancel,reopen',
        'text16': 'Original 16-row string glyph/position checks: 9 passed',
        'text-test': 'NPE GBK bitmap dialogue passed',
    }
    checks = []
    bindings = [Path(name) for name in production]
    for name, expected in probes.items():
        path = Path('tmp/font-edges-' + name + '.log')
        log = path.read_text(encoding='utf8')
        assert expected in log and not re.search(r'Assertion failed|differences[1-9]', log), name
        checks.append(dict(name=name, completed=True, log=str(path), log_sha256=digest(path)))
        bindings.append(path)
    warnings = {}
    for name in ['before-build', 'probe-build', 'regression-build', 'release-build', 'delivery-build']:
        path = Path('tmp/font-edges-' + name + '.log')
        log = path.read_text(encoding='utf8')
        warnings[name] = len(re.findall(r'\bwarning:', log, re.I))
        assert not warnings[name] and not re.search(r'\berror:', log, re.I), name
        bindings.append(path)
    assets = {'assets/gbk12-native.bin': 'a7b1d94bfb8edc3f082c2a8dfba064c57d00fe2a98b2a9a996af5c521f931391',
              'assets/gbk16-native.bin': 'c122ccf2562dc2c35f20ef0a6ea44f2c06ea8db1c4eaa669e2869f03ce598e43'}
    package = Path('npe-sdl/dist/npe-sdl-windows-x64-dev.zip')
    portable_path = Path('tmp/font-edges-portable-report.json')
    portable = json.loads(portable_path.read_text(encoding='utf8'))
    assert portable['passed'] and len(portable['checks']) == 14
    assert portable['package_sha256'] == digest(package)
    assert portable['payload_sha256']['npe-sdl.exe'] == digest('tmp/npe-portable-release-build/npe-sdl.exe')
    assert portable['payload_sha256']['npe-sdl.exe'] == digest('npe-sdl/dist/windows-x64/npe-sdl.exe')
    for name, expected in assets.items():
        assert digest(Path('npe-sdl') / name) == portable['payload_sha256'][name] == expected
        bindings.append(Path('npe-sdl') / name)
    with zipfile.ZipFile(package) as archive:
        for name, expected in portable['payload_sha256'].items():
            assert hashlib.sha256(archive.read('npe-sdl-windows-x64-dev/' + name)).hexdigest() == expected, name
    for name, expected in portable['original_games'].items():
        assert digest(name) == expected, name
    bindings += [Path(name) for name in [
        'npe-sdl/CMakeLists.txt', 'npe-sdl/tests/reference_native_font_edges.py',
        'npe-sdl/tests/sdl_native_font_edges_probe.c', 'npe-sdl/build/npe-sdl.exe',
        'npe-sdl/build/sdl-native-font-edges-probe.exe',
        'tmp/npe-portable-release-build/npe-sdl.exe',
        'tmp/npe-portable-probe-build/sdl-native-font-edges-probe.exe',
        'tmp/font-edges-before-failure.log', 'doc/games/np560_npe_native_font_edges.md',
        'plan.md', 'README.md', 'npe-sdl/README.md']]
    bindings += [root / 'report.json', root / 'frames.bin', portable_path, Path(__file__)]
    report = dict(passed=True, overall_complete=False, overall_progress_estimate=80,
                  original_framebuffer_cases=60, c_sdl_edge_cases_per_build=60,
                  verified_builds=['normal', 'Release'], original_lcd_transport_checked=False,
                  original_draw_adapters=False, existing_menu_story_title_comparisons=86,
                  existing_text12_string_cases_per_build=36, existing_text16_strings=9,
                  font_assets_unchanged=True, changed_existing_production_sources=changes,
                  checks=checks, build_warnings=warnings, portable_checks=14,
                  package_sha256=digest(package), package_bytes=package.stat().st_size,
                  reference_scope='Complete original RAM font framebuffer for 60 explicit C/D strings '
                                  'and coordinates, unchanged resident font bytes relocated. '
                                  'No drawing adapters; no physical LCD transfer, arbitrary word '
                                  'coordinates, adjacent RAM effects or natural game-route proof.',
                  production_source_sha256={name: digest(name) for name in production},
                  sha256={str(path): digest(path) for path in bindings})
    Path('tmp/font-edges-report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print('Verified:60 original full font frames per build,86 existing C/SDL comparisons,'
          '0 build warnings,14 current package checks; overall incomplete')


if __name__ == '__main__':
    main()
