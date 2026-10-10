"""Export the original header parser outputs, including real NPE lookahead."""
import hashlib,json
from pathlib import Path
report=json.loads(Path('tmp/audio-header-native/report.json').read_text())
assert report['passed'] and len(report['cases'])==64
assert report['original_prg_sha256']==hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest()
out=['/* Unmodified original2C:0C56; generate_audio_header_fixture.py. */',
     'static const AudioHeaderNative audio_header_native[]={']
for c in report['cases']:
    assert c['passed'] and c['stack_restored'] and c['host_guest_memory_writes']==0
    assert c['metadata'][8]==0x5aa5 #2838 is deliberately untouched by this parser.
    raw=bytes.fromhex(c['raw']);m=c['metadata'];values=m[:8]+m[9:]
    out.append('    {{%s},%d,{%s}},'%(','.join(map(str,raw)),c['returned'],','.join(map(str,values))))
out+=['};','']
Path('npe-sdl/tests/audio_header_native.h').write_text('\n'.join(out),encoding='utf8')
