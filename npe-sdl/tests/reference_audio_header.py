"""Run original2C:0C56 header parser, without codec or file-reader replacements."""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
sys.path.insert(0,str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word
parser=argparse.ArgumentParser();parser.add_argument('--qemu',type=Path,required=True)
args=parser.parse_args();out=Path('tmp/audio-header-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes();image=bytearray(0x30000)+original
rom=out/'original-header.rom';rom.write_bytes(image)
npe=Path('tmp/tjfy-world-probe.npe').read_bytes()
inputs=[]
for section in range(16,38):
    offset,size=struct.unpack_from('>2I',npe,0x3a+section*8)
    inputs.append((f'npe-section-{section}',npe[offset:offset+6].hex(),True))
for fmt in range(32):inputs.append((f'format-{fmt}',bytes([0x20,fmt,0xbe,0xef,0xf1,0x6b]).hex(),False))
for first,third,fourth,fifth,sixth in [(0x10,0xff,0xff,0xff,0xff),(0x2f,0,0,0,0),(0xf1,0xe0,0,0x80,0),(0x01,0x20,0,0,0),(0x10,0x1f,0xff,0,0)]:
    for fmt in [1,13]:inputs.append((f'bits-{first:02x}-{third:02x}-{fmt}',bytes([first,fmt,third,fourth,fifth,sixth]).hex(),False))
cases=[]
for name,hexbytes,real in inputs:
    raw=bytes.fromhex(hexbytes)
    code=[0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,0]
    for i in range(0,6,2):write_word(code,0x8800+i,int.from_bytes(raw[i:i+2],'little'))
    code += [0x6810,0xa2]
    for address in range(0x2828,0x283e,2):write_word(code,address,0x5aa5,banked=True)
    code += [0x68f0,0x8800,0x58f0,0x4800,0x58f0,0xbd2c,0x0c56,0x50f4,
             0x3f10,0xa8f0,0x1600,0x68fc,0x600d]
    pc=len(code)*2;code += [0xd0ff]
    caller=bytearray(original);caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    firmware=out/(name+'.prg');firmware.write_bytes(caller)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,boot_mode='firmware-vma03',start_paused=True)
        em.qmp.execute('cont');deadline=time.monotonic()+8;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(name+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:1300]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1300]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000,regs
        def memory(address,size):return base64.b64decode(em.snapshot('data',address,size,diagnostics=False)['frame'])
        metadata=list(struct.unpack('<11H',memory(0x512828,22)))
        returned=struct.unpack('<H',memory(0x1600,2))[0]
        tables=list(struct.unpack('<41H',memory(0x32b788,82)))
        assert tables==list(struct.unpack_from('<41H',image,0x32b788)),tables
        row=dict(name=name,raw=hexbytes,real_npe=real,metadata=metadata,returned=returned,
                 stack_restored=True,host_guest_memory_writes=0,passed=True)
        cases.append(row);print(json.dumps(row),flush=True)
    finally:
        if em:em.close()
report=dict(passed=len(cases)==64,cases=cases,rate_table=tables[:16],bit_rate_table=tables[16:],
            original_prg_sha256=hashlib.sha256(original).hexdigest(),qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
            npe_sha256=hashlib.sha256(npe).hexdigest(),scope='unmodified original header parser and rate tables; no codec, filesystem or timing')
assert report['passed'],len(cases)
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
