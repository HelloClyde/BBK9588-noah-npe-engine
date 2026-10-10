"""Capture the original map replay and settings request boundaries."""
import json
from pathlib import Path

report = json.loads(Path('tmp/map-sound-native/report.json').read_text(encoding='utf8'))
assert report['passed'] and len(report['cases']) == 29
text = '/* Original4E:A3F4 and37 controller/settings slices; generated. */\n'
text += 'static const MapSoundNative map_sound_native[]={\n'
for row in report['cases']:
    words = ','.join('{0x%x,0x%x}' % tuple(pair) for pair in row['initial_words'])
    arguments = ','.join('0x%x' % value for value in row['arguments'])
    text += '    {"%s",%d,%d,%d,%d,%d,{%s},%d,%d,%d,%d,%d,{%s}},\n' % (
        row['name'], row['operation'], row['enabled'], row['pending'], row['slot'],
        len(row['initial_words']), words, row['calls'], row['stops'],
        row['play_observed_word'], row['stop_observed_word'], row['after_enabled'], arguments)
text += '};\n'
Path('npe-sdl/tests/map_sound_native.h').write_text(text, encoding='utf8', newline='\n')
