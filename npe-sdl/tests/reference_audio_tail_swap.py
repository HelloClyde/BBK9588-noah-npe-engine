"""Execute original 2C:0E1E on real odd-count NPE final words.

Only byte-lane normalization is measured, not the missing codec/EOF path.
"""
import argparse, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
sys.path.insert(0,str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word,read_word

parser=argparse.ArgumentParser();parser.add_argument('--qemu',type=Path,required=True)
args=parser.parse_args();out=Path('tmp/audio-tail-swap-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes();game=Path('tmp/tjfy-world-probe.npe').read_bytes()
image=bytearray(0x30000)+original
rom=out/'original-services.rom';rom.write_bytes(image)
cases=[]
for section in range(16,38):
    offset,size=struct.unpack_from('>II',game,0x3a+section*8)
    resource=game[offset:offset+size];count=int.from_bytes(resource[2:4],'big')
    if not count&1:continue
    assert size==4+21*count+1
    final=resource[-2:];before=int.from_bytes(final,'little');expected=int.from_bytes(final,'big')
    code=[0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,1]
    write_word(code,0x0ffe,0x5aa5);write_word(code,0x1000,before);write_word(code,0x1002,0xa55a)
    for v in [1,0x1000,0]:code.extend([0x68f0,v,0x58f0])
    code.extend([0xbd2c,0x0e1e,0x50f6])
    for address,reg in [(0x1000,4),(0x0ffe,5),(0x1002,6)]:read_word(code,address,reg)
    code.extend([0x68f8,0x600d]);pc=len(code)*2;code.append(0xd0ff)
    caller=bytearray(image[0x200000:]);caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    firmware=out/f'section-{section}.prg';firmware.write_bytes(caller)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,start_paused=True)
        em.qmp.execute('cont');deadline=time.monotonic()+5;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=20:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/f'section-{section}.cpu.txt').write_text(state,encoding='utf-8')
        assert f'PC=20:{pc:04x}'.lower() in state.lower(),state[:1200]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1200]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert (regs[4],regs[5],regs[6],regs[8],regs[15])==(expected,0x5aa5,0xa55a,0x600d,0x6000),regs
        row=dict(section=section,count=count,final_file_bytes=final.hex(),
                 input_word=before,output_word=regs[4],normalized_last_byte=final[1],
                 guards_preserved=True,host_guest_memory_writes=0,passed=True)
        cases.append(row);print(json.dumps(row),flush=True)
    finally:
        if em:em.close()
report=dict(passed=bool(cases),cases=cases,
    original_prg_sha256=hashlib.sha256(original).hexdigest(),npe_sha256=hashlib.sha256(game).hexdigest(),
    qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
    scope='original word swap with current NPE last words; no codec or full EOF execution')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
