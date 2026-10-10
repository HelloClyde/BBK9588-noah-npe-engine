import json
from pathlib import Path
d=json.loads(Path('tmp/background-sound-native/report.json').read_text(encoding='utf8'))
assert d['passed'] and len(d['cases'])==34
text='/* Original background/scene request captures. */\nstatic const BackgroundNative background_native[]={\n'
for r in d['cases']:
    initial=','.join('{0x%x,0x%x}'%tuple(p) for p in r['initial_words'])
    args=','.join('0x%x'%v for v in r['arguments'])
    text+='    {"%s",%d,%d,%d,%d,%d,{%s},%d,%d,{%s}},\n'%(r['name'],r['phase'],r['enabled'],r['roll'],r['group'],len(r['initial_words']),initial,r['calls'],r['rng_calls'],args)
text+='};\n'
d=json.loads(Path('tmp/battle-background-eof-native/report.json').read_text(encoding='utf8'))
assert d['passed'] and len(d['cases'])==6
text+='static const BackgroundEofNative background_eof_native[]={\n'
for r in d['cases']:
    first=','.join('{0x%x,0x%x}'%tuple(p) for p in r['initial_words'])
    second=','.join('{0x%x,0x%x}'%tuple(p) for p in r['second_words'])
    args=','.join('{'+','.join('0x%x'%v for v in c)+'}' for c in r['captured'])
    text+='    {"%s",%d,{%s},{%s},{%s}},\n'%(r['name'],r['enabled'],first,second,args)
text+='};\n'
Path('npe-sdl/tests/background_sound_native.h').write_text(text,encoding='utf8',newline='\n')
