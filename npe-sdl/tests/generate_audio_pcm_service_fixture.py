"""Generate input configurations only; expected results stay native binaries."""
import json
from pathlib import Path
r=json.loads(Path('tmp/audio-pcm-service-native/report.json').read_bytes())
assert r['passed'] and len(r['cases'])==28
keys=['demand','index','half','words','decoded','current','target','flags89','flags8a',
      'active','pending','debt','remaining','frame_bits','tail','previous_cursor','previous_remainder']
lines=['/* Original service input configurations; see native report for evidence. */',
       'static const PcmNative pcm_native[]={']
for c in r['cases']:
    lines.append('    {"'+c['name']+'",'+','.join(str(c[k])+'u' for k in keys)+
        ',UINT64_C('+str(c['value'])+'),UINT32_C('+str(c['increment'])+'),'+
        '{'+','.join('{'+str(p)+'u,'+str(b)+'u}' for p,b in c['positions'])+'}},')
lines+=['};','']
Path('npe-sdl/tests/audio_pcm_service_native.h').write_text('\n'.join(lines),encoding='utf8')
