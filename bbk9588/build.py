"""Build and package the native 9588 target using the pinned SDK."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
SDK_COMMIT = '9c4cc3333e9e959f70797d2bcdc1d0760ddb4471'
UPSTREAM_COMMIT = '567ae64c1ba67c8a55d0c4fbdf874f9ac58471cf'

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--sdk', type=Path, default=ROOT / 'sdk')
    ap.add_argument('--prefix', default=os.environ.get('BDA_TOOLCHAIN_PREFIX'))
    args = ap.parse_args()
    sdk = args.sdk.resolve()
    sdk_commit = subprocess.check_output(['git', '-C', str(sdk), 'rev-parse', 'HEAD'], text=True).strip()
    if sdk_commit != SDK_COMMIT:
        raise RuntimeError('Initialize the pinned SDK with: git submodule update --init sdk')
    dirty = subprocess.check_output(['git', '-C', str(sdk), 'status', '--porcelain', '--',
                                    'sdk/include', 'bda_packer'], text=True).strip()
    if dirty:
        raise RuntimeError('SDK headers/packer must be unmodified for a reproducible build.')
    sys.path.insert(0, str(sdk))
    from bda_packer.build import ENTRY_OFFSET, ENTRY_VA, ICON_START, ICON_SIZES, build_icons, find_tool
    from bda_packer.header import BdaHeaderFields, write_header
    from bda_packer.validate import validate_bda
    from header_asset import compile_header
    header = compile_header(sdk)
    toolchain = sdk / '.toolchain'
    if not (toolchain / 'bin/mipsel-none-elf-gcc.exe').exists():
        toolchain /= 'g++-mipsel-none-elf-15.2.0'
    prefix = args.prefix or str(toolchain / 'bin/mipsel-none-elf-')
    cc, objcopy = find_tool(prefix, 'gcc'), find_tool(prefix, 'objcopy')
    build = ROOT / 'build-bbk9588'
    build.mkdir(exist_ok=True)
    linker = build / 'bda.ld'
    # Keep BSS out of the file and clear it before touching any C state.
    linker.write_text(f'''ENTRY(bda_start)
SECTIONS {{
 . = 0x{ENTRY_VA:x};
 .text : {{ KEEP(*(.text.bda_start)) *(.text.bda_main) *(.text*) }}
 .rodata : {{ *(.rodata*) }}
 .data : {{ *(.data*) *(.sdata*) }}
 . = ALIGN(16);
 __bss_start = .;
 .bss (NOLOAD) : {{ *(.bss*) *(.sbss*) *(COMMON) }}
 __bss_end = .;
 /DISCARD/ : {{ *(.MIPS.abiflags) *(.reginfo) *(.comment) *(.eh_frame*) }}
}}
''', encoding='ascii')
    flags = ['-EL', '-march=mips32', '-msoft-float', '-mno-abicalls', '-G0', '-fno-pic', '-Os',
             '-ffreestanding', '-fno-builtin', '-ffunction-sections', '-fdata-sections',
             '-DNPE_BBK9588=1', '-std=c11', '-Wall', '-Wextra',
             '-I', str(HERE / 'include'), '-I', str(sdk / 'sdk/include'),
             '-I', str(ROOT / 'npe-sdl')]
    sources = [ROOT / 'npe-sdl' / (s + '.c') for s in
               ['main', 'sound_sdl', 'npe', 'audio', 'audio_decode', 'world', 'save', 'gbk', 'text']]
    sources += [HERE / s for s in ['runtime.c', 'platform.c', 'touch_ui.c', 'entry.c', 'stack.S', 'start.S']]
    objects = []
    for source in sources:
        obj = build / (source.stem + '.o')
        extra = ['-Dmain=npe_host_main'] if source.name == 'main.c' else []
        subprocess.run([cc, *flags, *extra, '-c', str(source), '-o', str(obj)], check=True)
        objects.append(str(obj))
    elf, raw = build / 'NPE9588.elf', build / 'NPE9588.bin'
    subprocess.run([cc, *flags, '-nostdlib', '-Wl,--gc-sections', '-Wl,--build-id=none',
                    f'-Wl,-T,{linker}', f'-Wl,-Map,{build / "NPE9588.map"}',
                    *objects, '-lgcc', '-o', str(elf)], check=True)
    subprocess.run([objcopy, '-O', 'binary', str(elf), str(raw)], check=True)
    data = bytearray(ENTRY_OFFSET)
    icon = HERE / 'assets/npe-icon.png'
    data[ICON_START:ENTRY_OFFSET] = build_icons(icon, (0, 0, 0))
    data.extend(raw.read_bytes())
    data.extend(bytes((-len(data)) % 4))
    write_header(data, BdaHeaderFields(category=4, file_size_minus_4=len(data)-4,
                 entry_offset=ENTRY_OFFSET, icon_start=ICON_START,
                 icon0_size=ICON_SIZES[0], icon1_size=ICON_SIZES[1],
                 icon2_size=ICON_SIZES[2], icon3_size=ICON_SIZES[3]), 'NPE9588')
    dist = ROOT / 'dist/bbk9588'
    dist.mkdir(parents=True, exist_ok=True)
    out = dist / 'NPE9588.bda'
    out.write_bytes(data)
    report = validate_bda(out)
    if not report['ok']:
        raise RuntimeError(report['errors'])
    assets = dist / 'NPE/assets'
    assets.mkdir(parents=True, exist_ok=True)
    packaged = ['NPE9588.bda', 'README.md', 'LICENSE.txt', 'SOURCE_NOTICE.md', 'build.json', 'NPE-ENGINE-LICENSE.txt', 'NPE-ENGINE-NOTICE.md']
    for name in ['gbk12-native.bin', 'gbk16-native.bin', 'FONT-LICENSE.txt', 'FONT-PROVENANCE.txt']:
        shutil.copy2(ROOT / 'npe-sdl/assets' / name, assets / name)
        packaged.append('NPE/assets/' + name)
    shutil.copy2(HERE / 'assets/README.md', assets / 'ICON-PROVENANCE.md')
    packaged.append('NPE/assets/ICON-PROVENANCE.md')
    shutil.copy2(HERE / 'README.md', dist / 'README.md')
    shutil.copy2(HERE / 'LICENSE', dist / 'LICENSE.txt')
    shutil.copy2(ROOT / 'SOURCE_NOTICE.md', dist / 'SOURCE_NOTICE.md')
    shutil.copy2(ROOT / 'npe-sdl/third-party/NPE-ENGINE-GPL-2.0.txt', dist / 'NPE-ENGINE-LICENSE.txt')
    shutil.copy2(ROOT / 'npe-sdl/third-party/NPE-ENGINE-NOTICE.md', dist / 'NPE-ENGINE-NOTICE.md')
    qa = dist / 'qa'
    qa.mkdir(exist_ok=True)
    for name in ['VERIFICATION.md', 'emulator-result.json', 'audio-result.json', 'touch-ui-result.json',
                 'icon-result.json', 'packed-icons.png', 'icon-title.png', 'icon-keyboard.png',
                 'symbols.png', 'functions.png', 'keyboard-shift.png', 'audio-muted.json',
                 'audio-restored.json', 'audio-device-log.txt', 'alpha3-result.json',
                 'header-result.json', 'header-title.png', 'header-keyboard.png', 'header-pad.png',
                 'header-symbols.png', 'header-functions.png', 'header-exit.png',
                 'header-muted.png', 'header-restored.png',
                 'about-result.json', 'about.png', 'about-keyboard.png',
                 'about-restored.png', 'about-map.png', 'about-map-restored.png', 'about-exit.png', 'about-audio.json',
                 'package-layout-result.json',
                 'title.png', 'map-before.png', 'map-moved.png', 'map-restored.png',
                 'keyboard.png', 'pad.png', 'keyboard-pressed.png', 'exit.png', 'continue.png']:
        source = HERE / 'qa' / name
        if source.exists():
            shutil.copy2(source, qa / name)
            packaged.append('qa/' + name)
    manifest = {'version': (ROOT / 'VERSION').read_text().strip(),
                'source_commit': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
                'upstream_commit': UPSTREAM_COMMIT, 'sdk_commit': sdk_commit,
                'icon_source_sha256': hashlib.sha256(icon.read_bytes()).hexdigest(),
                'icon_region_sha256': hashlib.sha256(data[ICON_START:ENTRY_OFFSET]).hexdigest(),
                'payload_sha256': hashlib.sha256(data[ENTRY_OFFSET:]).hexdigest(),
                'bda_size': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    manifest.update(header)
    manifest['install_layout'] = {'application':'应用/程序/NPE9588.bda',
                                  'resources':'NPE', 'documentation':'说明'}
    (dist / 'build.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    archive = ROOT / 'dist/NPE9588-install.zip'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as package:
        for name in sorted(packaged):
            if name == 'NPE9588.bda':
                install_name = '应用/程序/NPE9588.bda'
            elif name == 'README.md' or name.startswith('NPE/'):
                install_name = name
            else:
                install_name = '说明/' + name
            info = zipfile.ZipInfo(install_name, date_time=(2020, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            package.writestr(info, (dist / name).read_bytes())
    sums = [hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name
            for p in [out, archive]]
    (ROOT / 'dist/SHA256SUMS.txt').write_text('\n'.join(sums)+'\n', encoding='ascii')
    print(out)

if __name__ == '__main__':
    main()
