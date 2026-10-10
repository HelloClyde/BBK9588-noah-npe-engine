import json
from pathlib import Path
r=json.loads(Path('tmp/battle-sound-native/report.json').read_text(encoding='utf8'))
assert r['passed'] and len(r['cases'])==33
text='/* Generated from unchanged original3E:F85E by reference_battle_sound.py. */\n'
text+='static const BattleSoundNative battle_sound_native[]={\n'
for n in r['cases']:
    initial=','.join('{0x%x,0x%x}' % tuple(pair) for pair in n['initial_words'])
    args=','.join('0x%x' % v for v in n['arguments'])
    text+='    {"%s",%d,{%s},%d,%d,{%s}},\n' % (n['name'],len(n['initial_words']),initial,n['calls'],n['marker'],args)
text+='};\n'
Path('npe-sdl/tests/battle_sound_native.h').write_text(text,encoding='utf8',newline='\n')
