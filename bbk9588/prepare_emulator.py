"""Create an isolated emulator NAND; never write the user's current image."""
from pathlib import Path
import argparse
import io
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--emulator-root', type=Path, required=True)
    ap.add_argument('--game', type=Path, required=True)
    ap.add_argument('--nand', type=Path, default=ROOT / 'build-bbk9588/emulator/bbk9588_nand.bin')
    args = ap.parse_args()
    nand = args.nand.resolve()
    if nand.exists():
        raise SystemExit('Refusing to replace an existing test NAND. Use a fresh --nand path.')
    nand.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.emulator_root / 'bbk9588_nand-v1.2.0.zip') as archive:
        with archive.open('bbk9588_nand.bin') as source, nand.open('wb') as target:
            import shutil
            shutil.copyfileobj(source, target, 1024*1024)
    sys.path.insert(0, str(args.emulator_root.resolve()))
    from emu.qemu.nand_fs import mutate_nand_files, replace_fat_file
    dist = ROOT / 'dist/bbk9588'
    def operation(fs):
        fs.makedirs('/NPE/assets', recreate=True)
        fs.makedirs('/NPE/saves', recreate=True)
        for name in ['gbk12-native.bin', 'gbk16-native.bin']:
            replace_fat_file(fs, '/NPE/assets/'+name, io.BytesIO((dist / 'NPE/assets' / name).read_bytes()))
        replace_fat_file(fs, '/NPE/GAME.NPE', io.BytesIO(args.game.read_bytes()))
        replace_fat_file(fs, '/应用/程序/宠物单词.bda', io.BytesIO((dist / 'NPE9588.bda').read_bytes()))
    mutate_nand_files(nand, operation)
    print(nand)

if __name__ == '__main__':
    main()
