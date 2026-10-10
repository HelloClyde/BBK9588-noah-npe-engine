"""Execute original runtime25:3082 bulk object-cell service without UI stubs."""
import argparse,base64,hashlib,json,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
sys.path.insert(0,str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word

p=argparse.ArgumentParser();p.add_argument('--qemu',type=Path,required=True);args=p.parse_args()
original=Path('tmp/NP560.PRG').read_bytes();out=Path('tmp/object-bulk-native');out.mkdir(exist_ok=True)
cases=[]
for id,flag in [(100,1),(101,1),(200,1),(201,1),(6,2)]:
    name=f'id{id}-flag{flag}';caller=bytearray(original);resident=bytearray(0x30000)+original
    code=[0x6800,2,0x68ff,0x6000,0x6810,0xa4]
    for address,value in [(0x7a56,0x2000),(0x797e,0x53),(0x7980,0x2000)]:write_word(code,address,value,banked=True)
    for i in range(145):write_word(code,0x742c+i*2,0,banked=True)
    index=id-1;write_word(code,0x742c+(index//2)*2,flag<<8 if index%2==0 else flag,banked=True)
    for i in range(8):
        for j in range(4):write_word(code,0x759e+i*8+j*2,0,banked=True)
    write_word(code,0x759e,id,banked=True);write_word(code,0x75a4,0x1a14,banked=True)
    code += [0x6810,0xa6];write_word(code,0x2534,0x1357,banked=True)
    code += [0x68f0,0xffff,0x58f0,0x68f0,1,0x58f0,0xbd25,0x3082,0x88ff,4]
    pc=len(code)*2;code += [0xd0ff]
    assert pc<0x1000
    caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    firmware=out/(name+'.prg');firmware.write_bytes(caller)
    rom=out/(name+'.rom');rom.write_bytes(resident)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,boot_mode='firmware-vma03',start_paused=True)
        em.qmp.execute('cont');deadline=time.monotonic()+8;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(name+'.cpu.txt')).write_text(state)
        assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:1200]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1200]
        raw=base64.b64decode(em.snapshot('data',0x532534,2,diagnostics=False)['frame'])
        value=struct.unpack('<H',raw)[0];expected=53 if id<=100 and flag==1 else 0x1357
        assert value==expected,(id,flag,value,expected)
        row=dict(id=id,flag=flag,before=0x1357,after=value,host_guest_memory_writes=0)
        cases.append(row);print(json.dumps(row),flush=True)
    finally:
        if em:em.close()
report=dict(cases=cases,original_prg_sha256=hashlib.sha256(original).hexdigest(),passed=True,
            scope=__doc__,physical_device_proven=False)
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
