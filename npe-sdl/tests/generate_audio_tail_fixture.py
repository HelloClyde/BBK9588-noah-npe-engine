"""Fields after the original final-word swap (not original codec PCM)."""
import hashlib,json,sys
from pathlib import Path
sys.path.insert(0,str(Path('.').resolve()))
from scripts.np560_mobivoice_fields import parse_mode5_frame
import struct

report=json.loads(Path('tmp/audio-tail-swap-native/report.json').read_text())
game=Path('tmp/tjfy-world-probe.npe').read_bytes()
assert report['passed'] and hashlib.sha256(game).hexdigest()==report['npe_sha256']
lines=['/* Last-frame fields; final word normalization verified by original PRG. */',
       'static const AudioTailReference audio_tail_reference[] = {']
for case in report['cases']:
    section=case['section'];offset,size=struct.unpack_from('>II',game,0x3a+section*8)
    payload=game[offset+4:offset+size]
    normalized=bytes(payload[i^1] for i in range(len(payload)))
    assert normalized[-2:]==case['output_word'].to_bytes(2,'little')
    count=case['count'];frame=normalized[(count-1)*21:count*21]
    fields=parse_mode5_frame(frame)
    lines.append('    {%d,%d,{%s}},'%(section,count,','.join(str(f.value) for f in fields)))
lines.append('};')
Path('npe-sdl/tests/audio_tail_reference.h').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('Original final words converted to %d last-frame field references'%len(report['cases']))
