"""Six existing candidate PCM references, composed with the verified gain.

This is a sequential-decoder regression, not NP560 mask-ROM PCM evidence.
"""
import hashlib, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path('.').resolve()))
from scripts.tests.test_np560_mobivoice_decoder import SAMPLES, GAME_SHA256
from scripts.np560_mobivoice_decoder import CandidateMode5Decoder

def fnv(data):
    result=14695981039346656037
    for value in data: result=((result^value)*1099511628211)&((1<<64)-1)
    return result

def gain7(sample):
    accumulator=0
    for _ in range(2):
        accumulator=((accumulator+sample*0x7fff*2+32768)//65536)*65536
        accumulator=min(2147483647,max(-2147483648,accumulator))
    return accumulator//65536

game=Path('tmp/tjfy-world-probe.npe').read_bytes()
assert hashlib.sha256(game).hexdigest()==GAME_SHA256
lines=['/* Existing six candidate PCM hashes, not original mask-ROM PCM. */',
       'static const AudioStreamReference audio_stream_reference[] = {']
for section,known in SAMPLES.items():
    offset,size=struct.unpack_from('>II',game,0x3a+section*8)
    payload=game[offset+4:offset+size]
    normalized=bytes(payload[i^1] for i in range(len(payload)))
    decoder=CandidateMode5Decoder();values=[]
    for index,(compressed_hex,pcm_sha256) in enumerate(known):
        frame=normalized[index*21:(index+1)*21]
        assert frame.hex()==compressed_hex
        pcm=decoder.decode_frame(frame);raw=struct.pack('<160h',*pcm)
        assert hashlib.sha256(raw).hexdigest()==pcm_sha256
        output=b''.join(struct.pack('<h',gain7(v))*4 for v in pcm)
        values.append('{UINT64_C(0x%016x),UINT64_C(0x%016x)}'%(fnv(raw),fnv(output)))
    lines.append('    {%d,{%s}},'%(section,','.join(values)))
lines.append('};')
Path('npe-sdl/tests/audio_stream_reference.h').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('Six known candidate PCM SHA256 values checked; SDL output reference generated')
