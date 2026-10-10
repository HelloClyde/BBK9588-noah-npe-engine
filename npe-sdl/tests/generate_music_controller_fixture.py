import json
from pathlib import Path
r=json.loads(Path('tmp/music-controller-native/report.json').read_bytes())
assert r['passed'] and len(r['cases'])==66
s='/* Original music entry/confirmation/map-init slices; generated. */\nstatic const MusicControlNative music_control_native[]={\n'
for c in r['cases']:
    words=','.join('{0x%x,0x%x}'%tuple(p) for p in c['initial_words'])
    args=','.join('0x%x'%n for n in c['arguments']);observed=','.join('0x%x'%n for n in c['observed'])
    s+='    {"%s",%d,%d,{%s},%d,%d,{%s},{%s}},\n'%(c['name'],c['operation'],len(c['initial_words']),words,c['calls'],c['stops'],args,observed)
s+='};\n';Path('npe-sdl/tests/music_controller_native.h').write_text(s,encoding='utf8',newline='\n')
