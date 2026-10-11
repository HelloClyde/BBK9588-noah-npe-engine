"""UI save/move/load regression for Hero's opening map in an isolated NAND.

Start a new game and leave it on the map before running this script. Uses
normal input and the emulator file export API; never modifies guest RAM.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import time
import urllib.parse
import urllib.request

OUT = Path(__file__).resolve().parent / 'qa'

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--base', default='http://127.0.0.1:8026')
    args = ap.parse_args()
    base = args.base.rstrip('/')
    OUT.mkdir(exist_ok=True)

    def request(endpoint, post=False, **params):
        url = base+endpoint+('?' + urllib.parse.urlencode(params) if params else '')
        with urllib.request.urlopen(urllib.request.Request(url, method='POST' if post else 'GET'), timeout=45) as response:
            return response.read()

    status = json.loads(request('/api/status'))
    nand = Path(status['nand_image']).resolve()
    assert Path(__file__).resolve().parents[1] / 'build-bbk9588' in nand.parents, 'Requires this port\'s isolated test NAND'

    def touch(x, y):
        request('/api/touch', True, x=x, y=y, down=1)
        time.sleep(.2)
        request('/api/touch', True, x=x, y=y, down=0)
        time.sleep(1.2)

    def screenshot(name):
        time.sleep(.2)
        (OUT / (name+'.png')).write_bytes(request('/screen.png'))

    def fnv(data):
        value = 14695981039346656037
        for byte in data:
            value = ((value ^ byte) * 1099511628211) & ((1 << 64)-1)
        return value

    def save(name, slot_number=1):
        touch(90, 256)  # F4: open system menu
        for _ in range(3):
            touch(90, 216)  # Down: System
        touch(210, 216)
        touch(90, 216)  # Save
        touch(210, 216)
        for _ in range(slot_number-1):
            touch(90, 216)
        touch(210, 216)
        entries = json.loads(request('/api/files', path='/NPE/saves'))['entries']
        suffix = '.npes' if slot_number == 1 else f'-{slot_number}.npes'
        slot = next(e['path'] for e in entries if e['name'].endswith(suffix) and
                    (slot_number != 1 or not e['name'].endswith(('-2.npes', '-3.npes'))))
        data = request('/api/files/export', path=slot)
        assert data[:8] in (b'NPESAV22', b'NPESAV23', b'NPESAV24')
        assert struct.unpack_from('<I', data, 24)[0] == len(data)-36
        assert struct.unpack_from('<Q', data, 28)[0] == fnv(data[36:]), 'Save checksum mismatch'
        fingerprint = struct.unpack_from('<Q', data, 16)[0]
        assert slot.endswith(f'save-{fingerprint:016x}' + suffix), '64-bit filename formatter mismatch'
        screenshot(name)
        return {'position': list(struct.unpack_from('<3H', data, 36)), 'bytes': len(data),
                'sha256': hashlib.sha256(data).hexdigest(), 'path': slot}

    before = save('map-before')
    # A and D-pad release are observed separately, exercising packet edge input.
    for _ in range(2):
        request('/api/key', True, code=7, down=1)
        time.sleep(.2)
        request('/api/key', True, code=7, down=0)
        time.sleep(1.2)
    moved = save('map-moved', 2)
    assert moved['position'] != before['position'], 'Right input did not move the player'
    touch(90, 256)
    for _ in range(3):
        touch(90, 216)
    touch(210, 216)  # System
    touch(210, 216)  # Load
    touch(210, 216)  # slot one
    restored = save('map-restored')
    assert restored['position'] == before['position'], (before, restored)
    transport = json.loads(request('/api/status'))['audio_transport']
    assert transport['received_packets'] > 0
    report = {'result': 'PASS', 'before': before, 'moved': moved, 'restored': restored,
              'audio_transport': transport,
              'nand': nand.relative_to(Path(__file__).resolve().parents[1]).as_posix()}
    (OUT / 'emulator-result.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
