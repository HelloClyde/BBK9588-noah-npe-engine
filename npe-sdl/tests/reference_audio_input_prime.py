"""Run original25:E24C..E322 initial buffer/output setup with a RAM reader.

Guest prologue enters the original tail. Only CCD4 is a diagnostic reader;
2F:66F2 returns at the PCM boundary, so no codec or actual EOF is certified.
"""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
sys.path.insert(0,str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word
parser=argparse.ArgumentParser();parser.add_argument('--qemu',type=Path,required=True)
args=parser.parse_args();out=Path('tmp/audio-input-prime-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes();image=bytearray(0x30000)+original
inputs=[]
npe=Path('tmp/tjfy-world-probe.npe').read_bytes()
for section in range(16,38):
    offset,size=struct.unpack_from('>2I',npe,0x3a+section*8)
    left=size-4-1024
    first=min(max(left,0),4096);second=min(max(left-first,0),4096)
    inputs.append((f'npe-section-{section}',size-4,[first,second],3,160,0,True))
for name,remaining,returns,flags,samples,key in [
    ('empty',0,[0,0],3,160,0),('small-two',2,[0,0],3,160,0),
    ('exact-window',1024,[0,0],3,160,0),('window-plus-one',1025,[1,0],3,160,0),
    ('two-full-sources',9216,[4096,4096],3,160,0),('short-last-source',9000,[4096,3880],3,160,0),
    ('zero-reader',9000,[0,0],0x43,160,0),('short-reader',9000,[8,4],0x43,160,0),
    ('one-request',9000,[4096,0],0x41,160,0),('no-request',8192,[0,0],0,160,0),
    ('xor-odd',1029,[5,0],0x41,160,0x5a),('one-sample',2,[0,0],3,1,0),
    ('samples-8192',2,[0,0],3,8192,0),('samples-16384',2,[0,0],3,16384,0),
    ('samples-32768',2,[0,0],3,32768,0),('samples-max',2,[0,0],3,65535,0),
]:inputs.append((name,remaining,returns,flags,samples,key,False))
cases=[]
for name,remaining,returns,flags,samples,key,real in inputs:
    body=[0x2e00,0x1f10,0x1f11]+[0x58f0|r for r in range(2,7)]
    body += [0x782b,0xc,0x786b,0x10,0x76f2,0xe948,0xac01,0];branch=len(body)-1
    body += [0x68f1,0x9000,0x68f4,returns[0],0x3f10,0xa8f6,0x1304,0xac00,0];join=len(body)-1
    body[branch]=0x1000+len(body)*2
    body += [0x68f1,0xb000,0x68f4,returns[1],0x3f10,0xa8f6,0x1306];body[join]=0x1000+len(body)*2
    body += [0x3f10,0x88f5,0x1300,0x5051,0x3f10,0xa8f5,0x1300,
             0x6810,0xa1,0x6811,1,0xff72,0x0834,0x5640,0xac01,0];skip=len(body)-1
    body += [0x5231,0xcf03,0x5b21];body[skip]=0x1000+len(body)*2
    body += [0x0814]+[0x380f|(r<<4) for r in reversed(range(2,7))]+[0x1f91,0x1f90,0x4800,0x2f00,0xc620]
    wrapper=[0x2e07]+[0x58f0|r for r in range(1,6)]+[0xad25,0xe24c]
    resident=bytearray(image);resident[0x1cccd4:0x1cccd8]=struct.pack('<2H',0xad03,0x1000)
    # Explicit PCM boundary: the original prime metadata/configuration and
    # final zero/nonzero return remain, the subsequent codec service doesn't.
    resident[0x2f66f2:0x2f66f4]=struct.pack('<H',0xc620)
    for entry,b in [(0x1000,body),(0x1200,wrapper)]:resident[0x30000+entry:0x30000+entry+len(b)*2]=struct.pack('<%dH'%len(b),*b)
    rom=out/(name+'.rom');rom.write_bytes(resident)
    code=[0x6800,2,0x68ff,0x6000,0x6810,1,0x68f1,0x800,0x68f0,0x6bb6,0xbf00,511,0x1a10,
          0x6810,0xa1,0x68f1,0x5948,0x68f0,0x5aa5,0xbf00,4095,0x1a10,0x6810,1]
    for pointer,pattern in [(0x1000,0x1234),(0x3000,0xabcd)]:code += [0x68f1,pointer,0x68f0,pattern,0xbf00,2047,0x1a10]
    code += [0x6810,0xa2]
    for address,value in [(0x29fc,remaining>>16),(0x29fe,remaining&65535),(0x2af8,17),(0x2af6,19),(0x2af4,31),
                          (0x29e0,0),(0x29e2,0x8c00),(0x2afa,samples),(0x2a1c,0),(0x2a1e,0x2400)]:write_word(code,address,value,banked=True)
    for address,value in [(0xf012,0x4000),(0xf014,flags),(0x4bb6,key<<8),(0x1300,0),(0x1304,0xffff),(0x1306,0xffff)]:write_word(code,address,value)
    code += [0xbd03,0x1200,0x3f10,0xa8f0,0x1310,0x68fc,0x600d];pc=len(code)*2;code += [0xd0ff]
    caller=bytearray(original);caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    caller[0x19ccd4:0x19ccd8]=struct.pack('<2H',0xad03,0x1000)
    caller[0x2c66f2:0x2c66f4]=struct.pack('<H',0xc620)
    for entry,b in [(0x1000,body),(0x1200,wrapper)]:caller[entry:entry+len(b)*2]=struct.pack('<%dH'%len(b),*b)
    firmware=out/(name+'.prg');firmware.write_bytes(caller);em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,boot_mode='firmware-vma03',start_paused=True)
        for address,expected in [(0x1cccd4,struct.pack('<2H',0xad03,0x1000)),(0x2f66f2,struct.pack('<H',0xc620))]:
            actual=base64.b64decode(em.snapshot('code',address,len(expected),diagnostics=False)['frame'])
            assert actual==expected,(hex(address),actual.hex(),expected.hex())
        em.qmp.execute('cont');deadline=time.monotonic()+8;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(name+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:1600]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1600]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000,regs
        def memory(address,size):return base64.b64decode(em.snapshot('data',address,size,diagnostics=False)['frame'])
        def word(a):return struct.unpack('<H',memory(a,2))[0]
        def pair(a):h,l=struct.unpack('<2H',memory(a,4));return h<<16|l
        hashes=[]
        for source in range(2):
            data=memory(0x50d948+source*4096,4096);(out/f'{name}-source{source}.bin').write_bytes(data);hashes.append(hashlib.sha256(data).hexdigest())
        assert memory(0x8800,1024)==b'\xb6\x6b'*512,name
        row=dict(name=name,remaining=remaining,returns=returns,flags8a=flags,samples=samples,key=key,real_npe_parameters=real,
                 final_remaining=pair(0x5129fc),tail_pointer=pair(0x5129e0),available=word(0x512af4),pending=[word(0x512af8),word(0x512af6)],
                 output_words=word(0x512ae6),configured_words=word(0x4a62),flags89_after=word(0xf012),flags8a_after=word(0xf014),
                 read_calls=word(0x1300),requested=[word(0x1304),word(0x1306)],returned=word(0x1310),source_sha256=hashes,
                 stack_restored=True,window_unchanged=True,host_guest_memory_writes=0,passed=True)
        assert row['read_calls']==((bool(flags&1)+bool(flags&2)) if remaining>1024 else 0),row
        cases.append(row);print(json.dumps(row),flush=True)
    finally:
        if em:em.close()
report=dict(passed=len(cases)==38,cases=cases,original_prg_sha256=hashlib.sha256(original).hexdigest(),qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
            npe_sha256=hashlib.sha256(npe).hexdigest(),scope='original input prime tail/producer/configuration; diagnostic RAM read and returned PCM boundary; no earlier initialization, codec or actual EOF')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
