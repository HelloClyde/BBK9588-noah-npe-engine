"""Exercise the Windows UTF-8 host paths through the built application.

Run from the workspace root. Only diagnostic files under tmp are written.
Original NPE files, firmware and continuous story saves are not changed.
"""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


build = Path('npe-sdl/build').resolve()
installation = Path('tmp/中文安装 路径🎮').resolve()
installation.mkdir(parents=True, exist_ok=True)
(installation / 'assets').mkdir(exist_ok=True)
copies = [(build / name, installation / name)
          for name in ['npe-sdl.exe', 'sdl-host-path-probe.exe']]
copies += [(build / 'assets' / name, installation / 'assets' / name)
           for name in ['gbk12.bin', 'gbk12-native.bin', 'gbk16-native.bin']]
copies.append((Path('C:/msys64/ucrt64/bin/SDL2.dll'), installation / 'SDL2.dll'))
for source, destination in copies:
    shutil.copyfile(source, destination)
    assert digest(source) == digest(destination)

games = json.loads(Path('tmp/np560-npe-audio-corpus-20261001-v2.json').read_text(encoding='utf-8'))['games']
tianjiang_hash = digest(Path('tmp/tjfy-world-probe.npe'))
tianjiang = next(g for g in games if g['sha256'] == tianjiang_hash)
other = json.loads(Path('tmp/sound-player-cross-game.json').read_text(encoding='utf-8'))
paths = [Path(tianjiang['path'])] + [Path(g['path']) for g in other]
results = []
for i, source in enumerate(paths):
    before = digest(source)
    operations = [
        ('inspect', [installation / 'npe-sdl.exe', '--inspect', source]),
        ('sound', [build / 'sdl-sound-probe.exe', source]),
        ('host', [installation / 'sdl-host-path-probe.exe', source,
                  installation / 'assets/gbk12-native.bin',
                  installation / 'assets/gbk16-native.bin',
                  installation / ('第%d槽 存档🎮.npesav' % (i+1))]),
    ]
    for kind, command in operations:
        completed = subprocess.run([str(part) for part in command], capture_output=True, timeout=30)
        row = {'kind': kind, 'path': str(source), 'sha256': before,
               'command': [str(p) for p in command], 'exit': completed.returncode,
               'stdout': completed.stdout.decode('utf-8', errors='replace'),
               'stderr': completed.stderr.decode('utf-8', errors='replace')}
        results.append(row)
        Path('tmp/host-path-%d-%s.log' % (i, kind)).write_text(
            row['stdout'] + row['stderr'], encoding='utf-8', newline='\n')
        Path('tmp/host-path-report.json').write_text(
            json.dumps(results, ensure_ascii=False, indent=2)+'\n', encoding='utf-8', newline='\n')
        print('%s case%d exit=%d' % (kind, i, completed.returncode), flush=True)
        assert completed.returncode == 0, row
    assert digest(source) == before
for source, destination in copies:
    assert digest(source) == digest(destination)
print('4 original Unicode NPE paths, Unicode installation/font/save paths passed')
