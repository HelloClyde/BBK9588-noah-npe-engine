import json
from pathlib import Path
r=json.loads(Path('tmp/audio-transport-control-native-v2/report.json').read_bytes())
assert r['passed'] and len(r['cases'])==66
lines=['/* Original software controller; explicit recorded transport boundaries. */',
       'static const TransportNative transport_native[]={']
for c in r['cases']:
    assert c['passed'] and c['foreground_restored']
    keys=['operation','before89','before8a','flags88','flags8d','before8e']
    row='    {"'+c['name']+'",'+','.join(str(c[k])+'u' for k in keys)+', {'
    row+=','.join(str(v)+'u' for v in c['controls'])+'},'
    row+=','.join(str(c[k]) for k in ['refill8e','resume8a'])+','+str(c['callback_or'])+'u, {'
    row+=','.join('{'+','.join(str(v)+'u' for v in e)+'}' for e in c['entries'])+'},'
    row+=','.join(str(c[k])+'u' for k in ['trace','route','after89','after8a','after8e'])+'},'
    lines.append(row)
lines+=['};','']
Path('npe-sdl/tests/audio_transport_native.h').write_text('\n'.join(lines),encoding='utf8')
