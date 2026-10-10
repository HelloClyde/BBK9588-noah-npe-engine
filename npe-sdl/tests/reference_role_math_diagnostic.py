"""Diagnostic original int-to-float and float division; no game-state proof."""
import argparse,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
from web.np560_web import Emulator
p=argparse.ArgumentParser();p.add_argument('--qemu',type=Path,required=True);args=p.parse_args()
prg=Path('tmp/NP560.PRG').read_bytes();out=Path('tmp/role-math-native');out.mkdir(exist_ok=True)
rows=[]
for name,values,call in [('int-negative',[-7],[0xbd32,0xbca6]),('int-positive',[7],[0xbd32,0xbca6]),
                         ('divide-negative',[0,0x4040,0,0xc0e0],[0xbd2f,0x52b4]),
                         ('divide-positive',[0,0x4040,0,0x40e0],[0xbd2f,0x52b4])]:
    code=[0x6800,2,0x68ff,0x6000]
    for v in values:code += [0x68f0,v&65535,0x58f0]
    code+=call+[0x70ff,len(values)*2,0x68fc,0x600d];pc=len(code)*2;code.append(0xd0ff)
    caller=bytearray(prg);caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    firmware=out/(name+'.prg');rom=out/(name+'.rom');firmware.write_bytes(caller);rom.write_bytes(bytes(0x30000)+caller)
    em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,boot_mode='firmware-vma03',start_paused=True)
    try:
        em.qmp.execute('cont');deadline=time.monotonic()+30
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');assert f'PC=03:{pc:04x}'.lower() in state.lower()
        (out/(name+'.cpu.txt')).write_text(state,encoding='utf8')
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        bits=regs[0]*65536+regs[1];value=struct.unpack('>f',struct.pack('>I',bits))[0]
        rows.append(dict(name=name,bits=f'{bits:08x}',value=str(value)))
        print(rows[-1],flush=True)
    finally:em.close()
(out/'report.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf8')
