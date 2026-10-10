"""Preserve original buffer contents, pointers and flags as C fixtures."""
import json
from pathlib import Path

report=json.loads(Path('tmp/audio-input-refill-native/report.json').read_text())
assert report['passed'] and len(report['cases'])==16
out=['/* Original input refill/EOF diagnostic; generate_audio_input_fixture.py. */',
     'static const AudioInputNative audio_input_native[]={']
for c in report['cases']:
    assert c['passed'] and c['host_guest_memory_writes']==0 and c['stack_restored']
    assert len(c['window_words'])==512
    fields=[c[k] for k in ['consumed_words','active_words','next_words','debt','frame_bits',
                           'flags8a','file_remaining','eof_delta','output_words','initial_flags89']]
    fields += [c['source_pointer'][1],c['remaining_words'],*c['pending_words'],c['debt_after'],
               (c['tail'][0]<<16)|c['tail'][1],c['final_flags89'],c['final_flags8a']]
    out.append('    {%s,UINT32_C(%d),%s,{%s}},'%(
        ','.join(str(v) for v in fields[:15]),fields[15],
        ','.join(str(v) for v in fields[16:]),','.join(str(v) for v in c['window_words'])))
out+=['};','']
Path('npe-sdl/tests/audio_input_native.h').write_text('\n'.join(out),encoding='utf8')
