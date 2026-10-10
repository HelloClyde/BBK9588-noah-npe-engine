import json
from pathlib import Path
d=json.loads(Path('tmp/map-sound-eof-native/report.json').read_text(encoding='utf8'))
assert d['passed'] and len(d['cases'])==6
text='/* Original EOF IRQ dispatches real map callback; generated capture. */\nstatic const MapEofNative map_eof_native[]={\n'
for row in d['cases']:
    first=','.join('{0x%x,0x%x}' % tuple(p) for p in row['initial_words'])
    second=','.join('{0x%x,0x%x}' % tuple(p) for p in row['second_words'])
    captures=','.join('{'+','.join('0x%x'%v for v in c)+'}' for c in row['captured'])
    text+='    {"%s",%d,%d,{%s},{%s},{%s}},\n'%(row['name'],len(row['initial_words']),len(row['second_words']),first,second,captures)
text+='};\n'
Path('npe-sdl/tests/map_sound_eof_native.h').write_text(text,encoding='utf8',newline='\n')
