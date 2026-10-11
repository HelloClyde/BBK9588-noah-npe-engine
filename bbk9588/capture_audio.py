"""Read-only PCM capture from the emulator WebSocket, without extra packages."""
import argparse
import base64
import json
import math
import os
from pathlib import Path
import socket
import struct
from urllib.parse import urlparse

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--base', default='http://127.0.0.1:8026')
    args = ap.parse_args()
    url = urlparse(args.base)
    result = {'packets': 0, 'samples': 0, 'peak': 0, 'sum_squares': 0, 'formats': []}
    with socket.create_connection((url.hostname, url.port or 80), timeout=10) as connection:
        key = base64.b64encode(os.urandom(16)).decode()
        connection.sendall((f'GET /audio HTTP/1.1\r\nHost: {url.netloc}\r\n'
                            'Upgrade: websocket\r\nConnection: Upgrade\r\n'
                            f'Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n').encode())
        pending = bytearray()
        while b'\r\n\r\n' not in pending:
            chunk = connection.recv(4096)
            if not chunk:
                raise RuntimeError('Connection closed during handshake')
            pending.extend(chunk)
        header, rest = pending.split(b'\r\n\r\n', 1)
        assert b'101 Switching Protocols' in header, header
        pending = bytearray(rest)
        def read(size):
            while len(pending) < size:
                chunk = connection.recv(65536)
                if not chunk:
                    raise RuntimeError('Audio stream closed')
                pending.extend(chunk)
            data = bytes(pending[:size])
            del pending[:size]
            return data
        while result['packets'] < 100:
            first, second = read(2)
            length = second & 127
            if length == 126:
                length = struct.unpack('>H', read(2))[0]
            elif length == 127:
                length = struct.unpack('>Q', read(8))[0]
            assert not second & 128
            data = read(length)
            if first & 15 != 2:
                continue
            magic, sequence, rate, channels, bits, size = struct.unpack_from('<8sIIHHI', data)
            assert bits == 16 and len(data) == size+24
            samples = struct.unpack('<'+'h'*(size//2), data[24:])
            result['packets'] += 1
            result['samples'] += len(samples)
            result['peak'] = max(result['peak'], max(map(abs, samples), default=0))
            result['sum_squares'] += sum(value*value for value in samples)
            fmt = [rate, channels, bits]
            if fmt not in result['formats']:
                result['formats'].append(fmt)
    result['rms'] = math.sqrt(result.pop('sum_squares')/result['samples'])
    assert result['peak'] > 0, 'Only silence received'
    out = Path(__file__).resolve().parent / 'qa/audio-result.json'
    out.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
