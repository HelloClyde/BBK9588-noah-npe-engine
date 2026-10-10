"""Execute unchanged original driver clock arithmetic; no codec boundary."""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
sys.path.insert(0,str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word
parser=argparse.ArgumentParser();parser.add_argument('--qemu',type=Path,required=True)
args=parser.parse_args();out=Path('tmp/audio-clock-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes()
inputs=[]
for rate,words in [(8000,160),(32000,320),(44100,576),(11025,80),(65535,65535),(7,1),(1,65535),(3,1),(65535,1),(32000,0)]:
    inputs.append((f'configure-{rate}-{words}',True,rate,words,0x1234,0x5aa55aa5,2,3,0x10001))
for name,value,increment,advance,retract,ms in [
 ('fraction-carry',0xffff,1,1,0,0),('middle-carry',0xffffffff,1,1,0,0),
 ('wrap48',0xffffffffffff,1,1,0,0),('subtract-equal',0x100000,0x100000,0,1,0),
 ('subtract-underflow',0xfffff,0x100000,0,1,0),('subtract-fraction',0x10001,2,0,1,0),
 ('negative-subtract',0x800000000001,1,0,1,0),('negative-wrap-positive',0x800000000000,1,0,1,0),
 ('negative-zero-step',0x800000000000,0,0,1,0),('wide-increment',0xffffffffffff,0xffffffff,2,1,0),
 ('add-coarse-wrap',0xffffffffabcd,0,0,0,1),('add-max-ms',0x12345678abcd,0,0,0,0xffffffff),
]:inputs.append((name,False,0,0,value,increment,advance,retract,ms))
cases=[]
for name,configure,rate,words,value,increment,advances,retracts,ms in inputs:
    code=[0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,0]
    for address,v in [(0x4a50,0x1234),(0x4a52,value>>32),(0x4a54,(value>>16)&65535),(0x4a56,value&65535),
                      (0x4a58,increment>>16),(0x4a5a,increment&65535),(0x4a5c,0xabcd)]:write_word(code,address,v)
    trace=[]
    def capture(label):
        slot=len(trace);trace.append(label)
        for j,address in enumerate(range(0x4a52,0x4a5c,2)):
            code.extend([0x3f10,0x88f0,address,0x3f10,0xa8f0,0x1600+slot*10+j*2])
    capture('before')
    if configure:
        code.extend([0x68f0,words,0x58f0,0x68f0,rate,0x58f0,0xbd08,0x385c,0x50f4])
        capture('configured')
    for i in range(advances):code.extend([0xbd08,0x37f4]);capture('advance'+str(i))
    code.extend([0x68f0,ms&65535,0x58f0,0x68f0,ms>>16,0x58f0,0xbd08,0x37ce,0x50f4]);capture('add-ms')
    for i in range(retracts):code.extend([0xbd08,0x3820]);capture('retract'+str(i))
    code.extend([0xbd08,0x37ba,0x3f10,0xa8f0,0x1800,0x3f10,0xa8f1,0x1802,0x68fc,0x600d])
    pc=len(code)*2;code.append(0xd0ff);caller=bytearray(original)
    caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code);firmware=out/(name+'.prg');firmware.write_bytes(caller)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,boot_mode='firmware-vma03',start_paused=True)
        for start,end in [(0x837ba,0x837f4),(0x837f4,0x83820),(0x83820,0x8385c),(0x8385c,0x838a4)]:
            raw=base64.b64decode(em.snapshot('code',start,end-start,diagnostics=False)['frame'])
            assert raw==original[start-0x30000:end-0x30000]
        em.qmp.execute('cont');deadline=time.monotonic()+8;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(name+'.cpu.txt')).write_text(state)
        assert f'PC=03:{pc:04x}'.lower() in state.lower() and 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1500]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000
        def memory(a,n):return base64.b64decode(em.snapshot('data',a,n,diagnostics=False)['frame'])
        data=memory(0x1600,len(trace)*10);(out/(name+'.trace.bin')).write_bytes(data)
        samples=[]
        for i,label in enumerate(trace):
            a,b,c,d,e=struct.unpack_from('<5H',data,i*10)
            samples.append(dict(label=label,value=(a<<32)|(b<<16)|c,increment=(d<<16)|e))
        h,l=struct.unpack('<2H',memory(0x1800,4));result=(h<<16)|l
        assert result==samples[-1]['value']>>16
        assert memory(0x4a50,2)==b'\x34\x12' and memory(0x4a5c,2)==b'\xcd\xab'
        row=dict(name=name,configure=configure,rate=rate,words=words,value=value,increment=increment,
                 advances=advances,retracts=retracts,add_ms=ms,trace=samples,milliseconds=result,
                 stack_restored=True,host_guest_memory_writes=0,passed=True)
        cases.append(row);print(json.dumps(row),flush=True)
    finally:
        if em:em.close()
report=dict(passed=len(cases)==22,cases=cases,original_prg_sha256=hashlib.sha256(original).hexdigest(),
            qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),codec_executed=False,
            scope='unchanged driver clock configure,advance,retract,coarse-add/get; no codecs, file IO, IRQ timing or physical DAC')
assert report['passed'];(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
