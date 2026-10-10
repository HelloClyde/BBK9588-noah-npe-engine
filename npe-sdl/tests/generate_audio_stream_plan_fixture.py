"""Export original three-probe wrapper metadata, never candidate-generated values."""
import hashlib,json
from pathlib import Path
r=json.loads(Path('tmp/audio-stream-plan-native/report.json').read_text())
assert r['passed'] and len(r['cases'])==36
assert r['original_prg_sha256']==hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest()
out=['/* Original26:8D2A with diagnostic codec boundaries. */','static const AudioPlanNative audio_plan_native[]={']
for c in r['cases']:
    assert c['passed'] and c['stack_restored'] and c['host_guest_memory_writes']==0
    assert c['position_calls']==7 and c['decode_calls']==3
    out.append('    {%d,%d,%d,%d,{%s},{%s},{%s}},'%(c['count'],c['rate'],c['start'],c['length'],
        ','.join('{%d,%d}'%tuple(p) for p in c['positions']),','.join(map(str,c['samples'])),
        ','.join(str(c[k]) for k in ['header_bits','frame_bits','decoded_samples','total_ms','elapsed_ms','start_bits','length_bits'])))
out+=['};','']
Path('npe-sdl/tests/audio_stream_plan_native.h').write_text('\n'.join(out),encoding='utf8')
