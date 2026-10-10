import json
from pathlib import Path
r=json.loads(Path('tmp/audio-service-gates-native/report.json').read_bytes())
assert r['passed'] and len(r['cases'])==72
lines=['/* Recorded unmodified callback gates/returns; hardware boundaries captured. */',
       'static const ServiceNative service_native[]={']
for c in r['cases']:
    assert c['passed'] and c['foreground_restored']
    keys=['service','before89','before8a','flags8d','before8e','callback_or','callback_clear']
    row='    {"'+c['name']+'",'+','.join(str(c[k])+'u' for k in keys)+', {'
    row+=','.join('{'+','.join(str(x)+'u' for x in entry)+'}' for entry in c['entries'])+'},'
    row+=','.join(str(c[k])+'u' for k in ['after89','after8a','after8d','after8e','route'])+'},'
    lines.append(row)
lines += ['};','']
Path('npe-sdl/tests/audio_service_gates_native.h').write_text('\n'.join(lines),encoding='utf8')
