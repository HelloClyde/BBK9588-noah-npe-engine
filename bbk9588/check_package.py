"""Check the exact release files, install layout, and published checksums."""
from pathlib import Path
import hashlib
import json
import subprocess
import struct
import re
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
                    'NPE/assets/FONT-LICENSE.txt', 'NPE/assets/FONT-PROVENANCE.txt',
                    'NPE/assets/ICON-PROVENANCE.md','NPE-ENGINE-LICENSE.txt','NPE-ENGINE-NOTICE.md'}
        if not required.issubset(package.namelist()):
            raise RuntimeError('Incomplete install layout')
        for name in package.namelist():
            if name.startswith('/') or '..' in Path(name).parts or name.lower().endswith(('.npe', '.prg', '.npes', '.sav')):
                raise RuntimeError('Unexpected private/resource file: ' + name)
        manifest = json.loads(package.read('build.json'))
        if manifest['version'] != (ROOT / 'VERSION').read_text().strip():
            raise RuntimeError('Version mismatch')
        if manifest['upstream_commit'] != '567ae64c1ba67c8a55d0c4fbdf874f9ac58471cf':
            raise RuntimeError('Upstream mismatch')
        if manifest['sdk_commit'] != '9c4cc3333e9e959f70797d2bcdc1d0760ddb4471':
            raise RuntimeError('SDK mismatch')
        commit = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
        if manifest['source_commit'] != commit:
            raise RuntimeError('Artifact source commit does not match this checkout')
        if manifest['sha256'] != sums['NPE9588.bda'] or package.read('NPE9588.bda') != files['NPE9588.bda'].read_bytes():
            raise RuntimeError('Install ZIP does not contain the released BDA')
        if manifest['bda_size'] != files['NPE9588.bda'].stat().st_size:
            raise RuntimeError('BDA size mismatch')
        data = files['NPE9588.bda'].read_bytes()
        header_png = ROOT / 'bbk9588/assets/header-logo.png'
        if manifest['header_source_sha256'] != hashlib.sha256(header_png.read_bytes()).hexdigest():
            raise RuntimeError('Unexpected header image source')
        header_text = (ROOT / 'bbk9588/assets/header_rgb565.h').read_text(encoding='ascii')
        pixels = b''.join(int(word,16).to_bytes(2,'little') for word in re.findall(r'0x([0-9a-f]{4})',header_text))
        width,height = manifest['header_size']
        if len(pixels) != width*height*2 or not (1 <= width <= 150 and height == 24):
            raise RuntimeError('Malformed native header image')
        if manifest['header_pixels_sha256'] != hashlib.sha256(pixels).hexdigest() or pixels not in data[0x95f8:]:
            raise RuntimeError('Header image is not embedded in executable')
        source = ROOT / 'bbk9588/assets/npe-icon.png'
        if manifest['icon_source_sha256'] != hashlib.sha256(source.read_bytes()).hexdigest():
            raise RuntimeError('Unexpected icon source')
        if manifest['icon_region_sha256'] != hashlib.sha256(data[0x88:0x95f8]).hexdigest():
            raise RuntimeError('Icon region mismatch')
        if manifest['payload_sha256'] != hashlib.sha256(data[0x95f8:]).hexdigest():
            raise RuntimeError('Executable payload mismatch')
        pos = 0x88
        for width, height in [(80,80), (80,80), (54,54), (58,58)]:
            size = 24 + width*height*2
            block = data[pos:pos+size]
            if len(block) != size or block[:2] != b'VX' or struct.unpack_from('<II', block, 6) != (width,height):
                raise RuntimeError('Malformed packed icon')
            for pixel in [0,width-1,(height-1)*width,height*width-1]:
                if struct.unpack_from('<H', block, 24+pixel*2)[0] != 0xf81f:
                    raise RuntimeError('Icon transparent corner missing')
            pos += size
        if pos != 0x95f8:
            raise RuntimeError('Icon resources overlap executable')
    print('Release checksums and install layout verified.')

if __name__ == '__main__':
    main()
