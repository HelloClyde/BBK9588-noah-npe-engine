import json,hashlib
from pathlib import Path
report=json.loads(Path('tmp/np560-npe-gain-contract-20260922-v3/report.json').read_text())
assert report['passed'] and report['original_prg_sha256']==hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest()
out=['/* Captured original PRG08:3536 output; codec not involved. */','static const GainNative gain_native[]={']
n=0
for c in report['cases']:
    m=c.get('metadata',{})
    if m.get('original_function')!='08:3536..35AA':continue
    assert c['passed'] and c['registers']['2']==c['expected']['2'] and c['registers']['3']==c['expected']['3'] and c['registers']['4']==c['expected']['4']
    def signed(x):return x if x<32768 else x-65536
    word=c['registers']['4']
    vals=[m['current'],m['target'],m['count'],word>>8,word&255,*m['diagnostic_pcm'],signed(c['registers']['2']),signed(c['registers']['3'])]
    out.append('{'+','.join(map(str,vals))+'},');n+=1
out.append('};');assert n==10
Path('npe-sdl/tests/audio_gain_native.h').write_text('\n'.join(out)+'\n')
print(n,'original gain calls, normal/icount5, including zero count')
