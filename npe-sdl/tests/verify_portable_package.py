"""Check the installed Windows payload without a toolchain on its PATH.

The production executable is exercised separately from the existing source
probes. The probes measure SDL title pixels and a short save/load menu flow;
they do not establish original-device or full-game equivalence. Original game
files are read only. Diagnostic files remain in a fresh temporary directory.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import uuid
import zipfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', required=True, type=Path)
    parser.add_argument('--probe-build', required=True, type=Path)
    parser.add_argument('--game', action='append', required=True, type=Path)
    parser.add_argument('--report', type=Path, default=Path('tmp/npe-portable-report.json'))
    args = parser.parse_args()
    package = args.package.resolve()
    probe_build = args.probe_build.resolve()
    games = [path.resolve() for path in args.game]
    report_path = args.report.resolve()
    staging = Path(tempfile.gettempdir()) / ('npe-sdl-delivery-' + uuid.uuid4().hex)
    staging.mkdir()
    work = staging / '中文工作 目录🎮'
    work.mkdir()
    (work / 'tmp').mkdir()
    installation = staging / '中文安装 路径🎮'
    installation.mkdir()

    with zipfile.ZipFile(package) as archive:
        # Only extract the local package we just built; reject escaped paths.
        for name in archive.namelist():
            target = (installation / name).resolve()
            assert target.is_relative_to(installation.resolve()), name
        archive.extractall(installation)
    roots = [path.parent for path in installation.glob('*/npe-sdl.exe')]
    assert len(roots) == 1, roots
    installed = roots[0]
    payload = {path.relative_to(installed).as_posix(): digest(path)
               for path in installed.rglob('*') if path.is_file()}
    assert set(payload) == {
        'npe-sdl.exe', 'SDL2.dll', 'README.md', 'assets/gbk12-native.bin',
        'assets/gbk16-native.bin', 'assets/FONT-PROVENANCE.txt',
        'licenses/SDL2-LICENSE.txt',
    }, payload
    expected_fonts = {
        'assets/gbk12-native.bin': 'a7b1d94bfb8edc3f082c2a8dfba064c57d00fe2a98b2a9a996af5c521f931391',
        'assets/gbk16-native.bin': 'c122ccf2562dc2c35f20ef0a6ea44f2c06ea8db1c4eaa669e2869f03ce598e43',
    }
    for name, expected in expected_fonts.items():
        assert payload[name] == expected, name
    diagnostics = staging / '中文诊断 程序🎮'
    shutil.copytree(installed, diagnostics)
    for name in ('sdl-host-path-probe.exe', 'sdl-slot-flow-probe.exe'):
        shutil.copyfile(probe_build / name, diagnostics / name)

    environment = dict(os.environ)
    system = Path(os.environ['SystemRoot'])
    environment['PATH'] = str(system / 'System32') + os.pathsep + str(system)
    environment['SDL_VIDEODRIVER'] = 'dummy'
    environment['SDL_AUDIODRIVER'] = 'dummy'
    report = {
        'passed': False, 'package': str(package), 'package_sha256': digest(package),
        'installation': str(installed), 'working_directory': str(work),
        'path': environment['PATH'], 'payload_sha256': payload,
        'original_games': {str(path): digest(path) for path in games}, 'checks': [],
        'delivery_probe_sha256': digest(probe_build / 'sdl-host-path-probe.exe'),
        'delivery_probe_source_sha256': digest(Path(__file__).with_name('sdl_host_path_probe.c')),
        'scope': 'Actual installed executable: inspect, bounded startup and font failures. '
                 'Existing source probes: four SDL title frames, explicit cold saves and '
                 'one short three-slot menu flow. No full-game or original-audio claim.',
    }

    def persist():
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n',
                               encoding='utf-8')

    def run(kind, command, cwd=work, expected_failure=None):
        completed = subprocess.run([str(part) for part in command], cwd=cwd,
                                   env=environment, capture_output=True, timeout=40)
        row = {'kind': kind, 'command': [str(part) for part in command],
               'exit': completed.returncode,
               'stdout': completed.stdout.decode('utf-8', errors='replace'),
               'stderr': completed.stderr.decode('utf-8', errors='replace')}
        report['checks'].append(row)
        persist()
        if expected_failure is None:
            assert completed.returncode == 0, row
        else:
            assert completed.returncode != 0 and expected_failure in row['stderr'], row
        if kind.startswith('source-SDL-title-and-cold-save-'):
            assert 'cold-release retained-buffer-check=' in row['stderr'], row
            row['resources_closed_before_decode'] = True
            persist()
        print(kind + ' passed', flush=True)
        return row

    persist()
    for index, game in enumerate(games):
        run('installed-inspect-%d' % index, [installed / 'npe-sdl.exe', '--inspect', game])
        run('source-SDL-title-and-cold-save-%d' % index,
            [diagnostics / 'sdl-host-path-probe.exe', game,
             diagnostics / 'assets/gbk12-native.bin', diagnostics / 'assets/gbk16-native.bin',
             work / ('第%d槽 存档🎮.npesav' % index)])

    # Start the actual installed binary, then terminate only this owned process.
    # Staying alive proves bounded startup, not that its LCD matches the original.
    for option in (None, '--new-game'):
        command = [str(installed / 'npe-sdl.exe')]
        if option:
            command.append(option)
        command.append(str(games[0]))
        process = subprocess.Popen(command, cwd=work, env=environment,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            try:
                process.wait(timeout=1.5)
                raise AssertionError('Installed game exited during startup: %r' % command)
            except subprocess.TimeoutExpired:
                pass
        finally:
            if process.poll() is None:
                process.terminate()
            stdout, stderr = process.communicate(timeout=10)
        row = {'kind': 'installed-startup-' + (option or 'title'), 'command': command,
               'running_after_seconds': 1.5, 'terminated_owned_process': True,
               'stdout': stdout.decode('utf-8', errors='replace'),
               'stderr': stderr.decode('utf-8', errors='replace')}
        report['checks'].append(row)
        persist()
        assert not row['stderr'], row
        print(row['kind'] + ' passed', flush=True)

    for kind in ('missing-font12', 'invalid-font12', 'missing-font16'):
        bad_install = staging / kind
        bad_install.mkdir()
        for name in ('npe-sdl.exe', 'SDL2.dll'):
            shutil.copyfile(installed / name, bad_install / name)
        assets = bad_install / 'assets'
        assets.mkdir()
        if kind == 'invalid-font12':
            (assets / 'gbk12-native.bin').write_bytes(b'NPEFONT1-invalid')
        elif kind == 'missing-font16':
            shutil.copyfile(installed / 'assets/gbk12-native.bin', assets / 'gbk12-native.bin')
        # Valid fonts in cwd must not mask missing assets beside this executable.
        run(kind, [bad_install / 'npe-sdl.exe', '--new-game', games[0]],
            cwd=installed, expected_failure='required native bitmap font unavailable')

    shutil.copyfile(games[0], work / 'tmp/input.npe')
    run('source-SDL-three-slot-menu-flow',
        [diagnostics / 'sdl-slot-flow-probe.exe', 'tmp/input.npe'])
    for game, before in report['original_games'].items():
        assert digest(Path(game)) == before, game
    for name, before in payload.items():
        assert digest(installed / name) == before, name
    report['passed'] = True
    persist()
    print('Portable runtime, four title/cold-save paths, menu flow and three font failures passed')


if __name__ == '__main__':
    main()
