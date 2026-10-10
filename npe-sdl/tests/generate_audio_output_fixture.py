"""Export original half-scheduler states and full-buffer hashes."""
import hashlib,json
from pathlib import Path
report=json.loads(Path('tmp/audio-output-half-native/report.json').read_text())
assert report['passed'] and len(report['cases'])==14
assert report['original_prg_sha256']==hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest()
def fnv(data):
    value=14695981039346656037
    for b in data:value=((value^b)*1099511628211)&((1<<64)-1)
    return value
out=['/* Original2E:7FE4 with codec/transfer boundaries; generate_audio_output_fixture.py. */',
     'static const AudioOutputNative audio_output_native[]={']
for c in report['cases']:
    assert c['passed'] and c['stack_restored'] and c['host_guest_memory_writes']==0
    values=[c[k] for k in ['index','half','output_words','decoded','current','target','calls',
        'after_index','after_half','after_output_words','after_gain','after_flags8a',
        'codec_calls','transfer_calls','transfer_words','configured_words']]
    hashes=[]
    for suffix,size in [('pcm',352),('repeated',1296)]:
        data=Path(f"tmp/audio-output-half-native/{c['name']}.{suffix}.bin").read_bytes()
        assert len(data)==size and hashlib.sha256(data).hexdigest()==c[suffix+'_sha256']
        hashes.append(fnv(data))
    out.append('    {%s,{UINT64_C(%d),UINT64_C(%d)}},'%(','.join(map(str,values)),*hashes))
out+=['};','']
Path('npe-sdl/tests/audio_output_native.h').write_text('\n'.join(out),encoding='utf8')
