"""Exercise the real platform code against a fake firmware boundary."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('CC', 'gcc'))
    args = ap.parse_args()
    with tempfile.TemporaryDirectory(prefix='npe9588-tests-') as directory:
        work = Path(directory)
        exe = work / ('platform-test.exe' if os.name == 'nt' else 'platform-test')
        subprocess.run([args.cc, '-std=c11', '-O1', '-Wall', '-Wextra',
                        '-Wno-unused-parameter', '-Wno-pointer-to-int-cast',
                        '-I', str(HERE/'tests'), str(HERE/'tests/platform_test.c'),
                        str(HERE/'touch_ui.c'), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], cwd=work, check=True)

if __name__ == '__main__':
    main()
