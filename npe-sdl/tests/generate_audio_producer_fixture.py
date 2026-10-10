"""Generate canonical word hashes from original producer output buffers."""
import hashlib,json
from pathlib import Path

report=json.loads(Path('tmp/audio-input-producer-native/report.json').read_text())
assert report['passed'] and len(report['cases'])==12
def fnv(data):
    value=14695981039346656037
    for b in data:value=((value^b)*1099511628211)&((1<<64)-1)
    return value
out=['/* Original2B:2894 with guest RAM file boundary; generate_audio_producer_fixture.py. */',
     'static const AudioProducerNative audio_producer_native[]={']
for c in report['cases']:
    assert c['passed'] and c['stack_restored'] and c['file_io_boundary_replaced'] and c['host_guest_memory_writes']==0
    values=[c['flags8a'],c['remaining'],*c['returns'],c['key'],c['calls'],*c['requested'],c['read_calls'],
            c['final_remaining'],*c['final_pending'],c['final_flags89'],c['final_flags8a']]
    hashes=[]
    for source in range(2):
        data=Path(f"tmp/audio-input-producer-native/{c['name']}-source{source}.bin").read_bytes()
        assert len(data)==4096 and hashlib.sha256(data).hexdigest()==c['source_sha256'][source]
        hashes.append(fnv(data))
    out.append('    {%s,{UINT64_C(%d),UINT64_C(%d)}},'%(','.join(map(str,values)),*hashes))
out+=['};','']
Path('npe-sdl/tests/audio_producer_native.h').write_text('\n'.join(out),encoding='utf8')
