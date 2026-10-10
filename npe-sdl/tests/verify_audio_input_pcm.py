"""Compare new buffered C decoding against the preserved corrected PCM hashes."""
import argparse, hashlib, json, subprocess
from pathlib import Path

parser=argparse.ArgumentParser();parser.add_argument('--output-dir',type=Path,default=Path('tmp/audio-input-pcm'))
args=parser.parse_args()
baseline=json.loads(Path('tmp/audio-tail-pcm/report.json').read_text())
assert baseline['passed'] and len(baseline['cases'])==22
out=args.output_dir;out.mkdir(exist_ok=True)
cases=[]
for previous in baseline['cases']:
    section=previous['section'];path=out/f'section-{section}.pcm'
    process=subprocess.run(['npe-sdl/build/audio-dump.exe','tmp/tjfy-world-probe.npe',str(section),str(path)],capture_output=True,text=True)
    assert process.returncode==0,(section,process.returncode,process.stderr)
    data=path.read_bytes();digest=hashlib.sha256(data).hexdigest()
    # The baseline was independently compared to corrected Python input.
    assert digest==previous['c_pcm_sha256'],(section,digest,previous)
    assert len(data)==previous['frames']*160*2,(section,len(data),previous)
    row=dict(section=section,frames=previous['frames'],pcm_bytes=len(data),pcm_sha256=digest,passed=True)
    cases.append(row);print(json.dumps(row),flush=True)
report=dict(passed=len(cases)==22,cases=cases,
    npe_sha256=hashlib.sha256(Path('tmp/tjfy-world-probe.npe').read_bytes()).hexdigest(),
    baseline='tmp/audio-tail-pcm/report.json',
    scope='buffered C versus corrected candidate PCM; not original mask-ROM PCM')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
