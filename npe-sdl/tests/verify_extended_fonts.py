"""Verify current native font assets and the affected production SDL paths."""
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path('npe-sdl/tools').resolve()))
import extract_np560_gbk_font as font_source

out = Path('tmp/native-extended-fonts')
reference = json.loads((out / 'report.json').read_text(encoding='utf8'))
assert reference['passed'] and reference['original_lookup_cases'] == 150
assert reference['original_unpack_glyphs'] == 14551 and not reference['original_lcd_checked']
program = Path('tmp/NP560.PRG').read_bytes()
nand = Path('tmp/NP560_NAND_IMAGE.bin').read_bytes()
assert hashlib.sha256(program).hexdigest() == reference['program_sha256']
assert hashlib.sha256(nand).hexdigest() == reference['nand_sha256']
assets = [Path('npe-sdl/assets/gbk12-native.bin'), Path('npe-sdl/assets/gbk16-native.bin')]
for path, expected in zip(assets, font_source.extract(program, nand)):
    assert path.read_bytes() == expected
    assert (Path('npe-sdl/build/assets') / path.name).read_bytes() == expected
assert hashlib.sha256((out / 'unpacked12.bin').read_bytes()).hexdigest() == reference['unpacked12_sha256']
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
env['PATH'] = 'C:/msys64/ucrt64/bin;' + env.get('PATH', '')
fonts = [str(p) for p in assets]
commands = [
    ('extended-glyphs', ['npe-sdl/build/sdl-extended-fonts-probe.exe', *fonts, str(out)]),
    ('resident-text', ['npe-sdl/build/npe-text-test.exe', *fonts, 'tmp/native-text16-bytes', '--text16-bytes']),
    ('title-slots', ['npe-sdl/build/npe-text-test.exe', *fonts, 'tmp/tjfy-world-probe.npe', '--title-only']),
    ('title-load-sdl', ['npe-sdl/build/sdl-title-load-probe.exe', 'tmp/tjfy-world-probe.npe']),
    ('font-startup', [sys.executable, 'npe-sdl/tests/native_font_startup_probe.py']),
]
checks = []
for name, command in commands:
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
    log = out / (name + '.log')
    log.write_bytes(result.stdout + b'\nSTDERR\n' + result.stderr)
    assert result.returncode == 0, (name, result.returncode, result.stderr.decode('utf8', errors='replace'))
    checks.append(dict(name=name, exit_code=result.returncode, log=str(log),
                       log_sha256=hashlib.sha256(log.read_bytes()).hexdigest()))
bindings = [*assets, Path('npe-sdl/text.c'), Path('npe-sdl/world.c'), Path('npe-sdl/CMakeLists.txt'),
            Path('npe-sdl/tools/extract_np560_gbk_font.py'),
            Path('npe-sdl/tests/reference_extended_fonts.py'), Path(__file__),
            Path('npe-sdl/tests/sdl_extended_fonts_probe.c'), Path('npe-sdl/build/npe-sdl.exe'),
            out / 'report.json', out / 'glyphs.bin', out / 'unpacked12.bin', out / 'extended16.bin']
report = dict(passed=True, goal_complete=False, original_lookup_cases=150,
              original_unpack12_glyphs=14551, exact_flash16_glyphs=14551,
              c_sdl_glyph_frames=150, original_title_load_lcd_frames=3,
              reference_scope='Original glyph buffers/coordinates with adapted flash transport; title load has retained original full LCD references. No full-game or timing claim.',
              checks=checks, sha256={str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in bindings})
Path('tmp/extended-fonts-report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
print('Extended fonts: all 14551 original 12-row unpacks / 14551 flash-16 glyphs matched; 150 C/SDL glyph frames and 5 affected checks passed')
