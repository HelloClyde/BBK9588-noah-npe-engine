import argparse, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
from web.np560_web import Emulator, RunJournal

parser = argparse.ArgumentParser(description='Execute unchanged PRG arithmetic in an independent DMCU CPU')
parser.add_argument('--qemu', type=Path, required=True)
args = parser.parse_args()
out = Path('tmp/attribute-cpu-slice'); out.mkdir(exist_ok=True)
prg = Path('tmp/NP560.PRG').read_bytes()
offset = (0x2d-0x20)*65536+0x37b2
slice_words = list(struct.unpack('<8H', prg[offset:offset+16]))
assert slice_words == [0x1030,0xc203,0xc017,0x68f1,80,0xc201,0x0867,0x5061], slice_words
results = []
pairs = [(1,1),(100,-99),(100,-101),(1000,-999),(100,-100),(0,0),(-32768,32767),(32767,32767),(-32768,-32768)]
for actor,target in pairs:
    for roll in [1,44,99]:
        name = f'{actor}_{target}_{roll}'
        words = [0x6800,2,0x68ff,0x6000,0x68f3,actor&65535,0x68f0,target&65535,
                 0x68f7,actor&65535,0x68f1,roll] + slice_words
        final_pc = len(words)*2; words.append(0xc701)
        firmware = out/(name+'.prg'); firmware.write_bytes(struct.pack(f'<{len(words)}H',*words))
        emu = None
        try:
            emu = Emulator(qemu=args.qemu.resolve(),firmware=firmware.resolve(),
                           start_paused=True,icount_shift=0,journal=RunJournal(out/'runs',case=name))
            emu.qmp.execute('qom-set',{'path':'/machine','property':'instruction-clock','value':True})
            emu.qmp.execute('cont')
            marker = f'UNSUPPORTED=20:{final_pc:04x} WORD=c701 COUNT=1'
            for attempt in range(300):
                state = emu.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
                if marker in state: break
                time.sleep(.01)
            emu.qmp.execute('stop')
            assert marker in state and 'ROM_MISSING=none' in state and 'INVALID_CONTROL=none' in state, state[:1000]
            header='\n'.join(state.splitlines()[:5])
            regs={int(k):int(v,16) for k,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]+)',header,re.I)}
            multiplier=regs[6] if regs[6]<32768 else regs[6]-65536
            result=dict(actor=actor,target=target,roll=roll,multiplier=multiplier)
            results.append(result); print(json.dumps(result),flush=True)
            (out/(name+'.cpu.txt')).write_text(state)
        finally:
            if emu: emu.close()
Path('tmp/attribute-cpu-slice.json').write_text(json.dumps(results,indent=2)+'\n')
