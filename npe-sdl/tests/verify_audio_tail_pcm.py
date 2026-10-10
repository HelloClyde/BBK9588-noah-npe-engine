"""Corrected word-normalized candidate PCM versus the actual C decoder.

Preserve and verify the historical trimmed-tail hashes before updating the
candidate fixtures. None of these hashes certify the absent mask ROM PCM.
"""
import argparse, copy, hashlib, json, struct, subprocess, sys
from pathlib import Path
sys.path.insert(0,str(Path('.').resolve()))
from scripts.np560_mobivoice_decoder import CandidateMode5Decoder
from scripts.tests.test_np560_mobivoice_decoder import FULL_SECTIONS

parser=argparse.ArgumentParser();parser.add_argument('--update-fixtures',action='store_true')
args=parser.parse_args();game=Path('tmp/tjfy-world-probe.npe').read_bytes()
out=Path('tmp/audio-tail-pcm');out.mkdir(exist_ok=True);cases=[]
# Preserve the actual historical fixtures, independently of whether the
# current tests or local reports have already been corrected.
history={
    17: '062184b06cdb454ff0cefedb0644a7c6731be645804f353aa0ef7b4d7642cfe3',
    18: 'b3d7f0cf513568cf0ed37b39109dfd2340c34b4c032c364337d1bebe615c7cd7',
    19: '4c53a63a318643c2cd4c18f68faaa633689c5787caeed34f1debe5dce7c41efa',
    21: 'efa505383f9740d59c68fbfc9782f32ee34620c822bafb5d5973aa843b6b4c00',
    26: '92890b85dddb8152648c411259b243e5a59f5d3dd745d683f8f500a82ef3847c',
    29: '27934d73fee16a376627e0bd7aaa23b9570fe422df4d09476f9be35eb7cc75c2',
    30: '208308aafb6d260e9746e5c469546e5f39f573fc11358e613f3c712ce2a20df5',
    31: '00f637eb55b9fe70a35c46e4d737c1bfaf9c5fa3fc093cd788cb25b68e978563',
    32: '138b8113f5af287131b7d68206aaff5eaa986e2cfa9665f117d2990b065df6da',
    33: '914cb11077be1b667321c6909dd4e3e56d16daf2792a93cdbd0c65ab631eeb75',
    34: 'dffaa9a5978343206cd3c18bd37a011ec01c9cd94f742adee88b018fa62382d1',
    37: '535a3b139dd3fe1fb3030792612b966c08d959af55252f79b2c4a0026eb6bb83',
}
for section,(count,fixture_sha) in FULL_SECTIONS.items():
    old_sha=history.get(section,fixture_sha)
    offset,size=struct.unpack_from('>II',game,0x3a+section*8)
    payload=game[offset+4:offset+size];assert len(payload)==(count*21+1)&~1
    normalized=bytes(payload[i^1] for i in range(len(payload)))
    decoder=CandidateMode5Decoder();digest=hashlib.sha256()
    for i in range(count-1):
        pcm=decoder.decode_frame(normalized[i*21:(i+1)*21]);digest.update(struct.pack('<160h',*pcm))
    last=normalized[(count-1)*21:count*21]
    old_last=last[:-1]+payload[count*21-1:count*21] if count&1 else last
    old_decoder=copy.deepcopy(decoder);old_digest=digest.copy()
    old_digest.update(struct.pack('<160h',*old_decoder.decode_frame(old_last)))
    assert old_digest.hexdigest()==old_sha,(section,'historical baseline mismatch')
    digest.update(struct.pack('<160h',*decoder.decode_frame(last)));expected=digest.hexdigest()
    assert fixture_sha in (old_sha,expected),(section,'unrecognized fixture')
    pcm_path=out/f'section-{section}.pcm'
    process=subprocess.run(['npe-sdl/build/audio-dump.exe','tmp/tjfy-world-probe.npe',str(section),str(pcm_path)],capture_output=True,text=True)
    assert process.returncode==0,(section,process.returncode,process.stderr)
    raw=pcm_path.read_bytes();actual=hashlib.sha256(raw).hexdigest()
    assert len(raw)==count*320 and actual==expected,(section,len(raw),actual,expected)
    row=dict(section=section,frames=count,old_trimmed_tail_sha256=old_sha,
             corrected_candidate_sha256=expected,c_pcm_sha256=actual,changed=expected!=old_sha,
             last_word_file_bytes=payload[-2:].hex(),passed=True)
    cases.append(row);print(json.dumps(dict(section=section,changed=row['changed'],passed=True)),flush=True)
report=dict(passed=len(cases)==22,cases=cases,np560_mask_rom_pcm_certified=False,
    scope='candidate PCM equality after original byte-lane normalization; no full EOF or original codec',
    npe_sha256=hashlib.sha256(game).hexdigest())
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
if args.update_fixtures:
    path=Path('scripts/tests/test_np560_mobivoice_decoder.py');text=path.read_text(encoding='utf-8')
    for case in cases:
        if case['changed']:
            if case['old_trimmed_tail_sha256'] in text:
                text=text.replace(case['old_trimmed_tail_sha256'],case['corrected_candidate_sha256'],1)
            else:assert case['corrected_candidate_sha256'] in text
    path.write_text(text,encoding='utf-8')
print(json.dumps(dict(passed=True,sections=22,changed=sum(c['changed'] for c in cases))))
