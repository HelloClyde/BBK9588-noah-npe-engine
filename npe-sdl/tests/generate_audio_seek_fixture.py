"""Export observed original range decisions and complete window hashes."""
import hashlib, json
from pathlib import Path
r = json.loads(Path('tmp/audio-seek-plan-native/report.json').read_text())
assert r['passed'] and len(r['cases']) == 24
assert r['original_prg_sha256'] == hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest()
def fnv(data):
    h = 14695981039346656037
    for b in data:
        h = ((h^b)*1099511628211)&((1<<64)-1)
    return h
out = ['/* Original25:E002 with explicit codec reset/restore and file IO boundaries. */',
       'static const AudioSeekNative audio_seek_native[]={']
for c in r['cases']:
    assert c['passed'] and c['stack_restored'] and c['host_guest_memory_writes'] == 0
    data = Path('tmp/audio-seek-plan-native', c['name']+'-window.bin').read_bytes()
    assert len(data) == 1024 and hashlib.sha256(data).hexdigest() == c['window_sha256']
    expected = bytearray(b'\xa5\x5a'*512)
    actual = c['supplied_return'] if c['read_calls'] else 0
    assert actual <= c['read_request']
    if actual:
        even = actual&~1
        expected[:even] = b'\x34\x12'*(even//2)
        if actual&1:
            expected[even+1] = 0x12
        for i in range(actual):
            expected[i^1] ^= c['key']
        for i in range(0, even, 2):
            expected[i], expected[i+1] = expected[i+1], expected[i]
    assert data == expected, c['name']
    if c['reset_calls']:
        assert c['reset_arguments'] == [0, 0x8800, 0]
    if c['restore_calls']:
        assert c['restore_arguments'] == [0, 0x8800, c['start_bits']&15]
    absolute = 0
    if c['seek_calls']:
        assert c['seek_arguments'][:2] == [0, 0x2400]
        absolute = c['seek_arguments'][2]<<16|c['seek_arguments'][3]
    values = [c[k] for k in ['remaining','start_bits','length_bits','header_bits','base','supplied_return','key',
                             'start_bytes','length_bytes','final_remaining','reset_calls','restore_calls','seek_calls',
                             'read_calls','read_request','returned','reached_prime']]+[absolute]
    out.append('    {%s,UINT64_C(%d)},'%(','.join(map(str,values)), fnv(data)))
out += ['};', '']
Path('npe-sdl/tests/audio_seek_native.h').write_text('\n'.join(out), encoding='utf8')
