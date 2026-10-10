from pathlib import Path
import json
report=json.loads(Path('tmp/audio-dac-ramp-native/report.json').read_text(encoding='utf-8'))
assert report['passed'] and len(report['cases'])==8
out=['/* Original PRG register-write traces; generate_audio_dac_fixture.py. */',
     'static const DacRampNative dac_ramp_native[]={']
for c in report['cases']:
 assert c['passed'] and c['host_guest_memory_writes']==0 and not c['nand_used']
 t=c['transitions'];assert len(t)<=8
 values=','.join(map(str,c['input']+c['target']+[c['repeat_nops'],len(t)]))
 transitions=','.join('{%d,%d,%d}'%(v['channel'],v['old'],v['new']) for v in t) or '{0,0,0}'
 out.append('    {%s,{%s}},'%(values,transitions))
out+=['};','']
Path('npe-sdl/tests/audio_dac_native.h').write_text('\n'.join(out),encoding='utf-8')
