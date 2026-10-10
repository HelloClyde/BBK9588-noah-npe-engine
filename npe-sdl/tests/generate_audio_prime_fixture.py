"""Export original initial-window/producer metadata and full source hashes."""
import hashlib,json
from pathlib import Path
r=json.loads(Path('tmp/audio-input-prime-native/report.json').read_text())
assert r['passed'] and len(r['cases'])==38
assert r['original_prg_sha256']==hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest()
def fnv(data):
    h=14695981039346656037
    for b in data:h=((h^b)*1099511628211)&((1<<64)-1)
    return h
out=['/* Original25:E24C initial setup with RAM read/returned PCM boundaries. */','static const AudioPrimeNative audio_prime_native[]={']
for c in r['cases']:
    assert c['passed'] and c['stack_restored'] and c['window_unchanged'] and c['host_guest_memory_writes']==0
    assert c['configured_words']==c['output_words']
    hashes=[]
    for source in range(2):
        data=Path(f"tmp/audio-input-prime-native/{c['name']}-source{source}.bin").read_bytes()
        assert len(data)==4096 and hashlib.sha256(data).hexdigest()==c['source_sha256'][source]
        hashes.append(fnv(data))
    values=[c['remaining'],*c['returns'],c['flags8a'],c['samples'],c['key'],c['final_remaining'],c['tail_pointer'],c['available'],
        *c['pending'],c['output_words'],c['flags89_after'],c['flags8a_after'],c['read_calls'],*c['requested']]
    out.append('    {%s,{UINT64_C(%d),UINT64_C(%d)}},'%(','.join(map(str,values)),*hashes))
out+=['};','']
Path('npe-sdl/tests/audio_prime_native.h').write_text('\n'.join(out),encoding='utf8')
