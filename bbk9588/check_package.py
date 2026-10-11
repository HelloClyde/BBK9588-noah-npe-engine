"""Check the exact release files, install layout, and published checksums."""
from pathlib import Path
import hashlib
import json
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    dist = ROOT / 'dist'
    files = {'NPE9588.bda': dist / 'bbk9588/NPE9588.bda',
             'NPE9588-install.zip': dist / 'NPE9588-install.zip'}
    sums = {}
    for line in (dist / 'SHA256SUMS.txt').read_text().splitlines():
        digest, name = line.split('  ', 1)
        sums[name] = digest
    if set(sums) != set(files):
        raise RuntimeError('Unexpected checksum entries')
    for name, path in files.items():
        if hashlib.sha256(path.read_bytes()).hexdigest() != sums[name]:
            raise RuntimeError('Checksum mismatch: ' + name)
    with zipfile.ZipFile(files['NPE9588-install.zip']) as package:
        if package.testzip() is not None:
            raise RuntimeError('Broken ZIP')
        required = {'NPE9588.bda', 'README.md', 'LICENSE.txt', 'SOURCE_NOTICE.md', 'build.json',
                    'NPE/assets/gbk12-native.bin', 'NPE/assets/gbk16-native.bin',
                    'NPE/assets/FONT-LICENSE.txt', 'NPE/assets/FONT-PROVENANCE.txt'}
        if not required.issubset(package.namelist()):
            raise RuntimeError('Incomplete install layout')
        for name in package.namelist():
            if name.startswith('/') or '..' in Path(name).parts or name.lower().endswith(('.npe', '.prg', '.npes', '.sav')):
                raise RuntimeError('Unexpected private/resource file: ' + name)
        manifest = json.loads(package.read('build.json'))
        if manifest['version'] != (ROOT / 'VERSION').read_text().strip():
            raise RuntimeError('Version mismatch')
        if manifest['sdk_commit'] != '9c4cc3333e9e959f70797d2bcdc1d0760ddb4471':
            raise RuntimeError('SDK mismatch')
        commit = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
        if manifest['source_commit'] != commit:
            raise RuntimeError('Artifact source commit does not match this checkout')
        if manifest['sha256'] != sums['NPE9588.bda'] or package.read('NPE9588.bda') != files['NPE9588.bda'].read_bytes():
            raise RuntimeError('Install ZIP does not contain the released BDA')
        if manifest['bda_size'] != files['NPE9588.bda'].stat().st_size:
            raise RuntimeError('BDA size mismatch')
    print('Release checksums and install layout verified.')

if __name__ == '__main__':
    main()
