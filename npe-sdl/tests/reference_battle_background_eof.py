"""Original EOF IRQ dispatches real32:649C; capture its live replay request.

The end bit is set by guest diagnostic instructions. Playback vector captures
arguments and calls original cleanup/start services; no file/codec proof.
"""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
sys.path.insert(0,str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word,read_word
parser=argparse.ArgumentParser();parser.add_argument('--qemu',type=Path,required=True)
args=parser.parse_args();out=Path('tmp/battle-background-eof-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes();image=bytearray(0x30000)+original
resident=bytearray(image)
resident[0x100d0:0x100d4]=struct.pack('<2H',0xad08,0x3b48)
resident[0x3438c:0x34390]=struct.pack('<2H',0xad20,0x1800)
rom=out/'original-battle-background-eof.rom';rom.write_bytes(resident)
cases=[]
for enabled,slot in zip((0,1,2,0x100,0x101,0xffff),(0,1,2,3,255,1)):
    name=f'word-{enabled}'
    initial={0x7a50:1,0x7996:0x0102,0x7998:0x0304,0x799a:0x1122,0x799c:0x3344}
    second={0x7a50:enabled,0x7996:0x8123,0x7998:0x4567,0x799a:0xaabb,0x799c:0xccdd}
    code=[0x6800,2,0x68ff,0x6000,0x68e9,0x8000,0xee00,
          0x682e,0,0xfe00,0x6810,0,0x6831,0x40,0x6870,3,0x6871,0xfffd,0x6872,1]
    write_word(code,0x1420,0)
    for address in range(0x1400,0x141e,2):write_word(code,address,0xdead)
    for turn,values in enumerate((initial,second)):
        if turn:
            code+=[0x6810,0]
            for address in range(0x1400,0x141e,2):write_word(code,address,0xdead)
        for address,value in sorted(values.items()):
            code+=[0x6810,0xa4+(address>>15)];write_word(code,address&0x7fff,value,banked=True)
        code+=[0x6810,0]
        for address,value in ((0xf010,1),(0xf012,0xf800),(0xf01a,0),
                               (0xd0,0xad08),(0xd2,0x3b48),(0x4aa8,0x32),(0x4aaa,0x649c)):
            write_word(code,address,value)
        code+=[0x68f1,0x1234,0x68f2,0x2345,0x68f3,0x3456,0x68f7,0x789a,0x0f34]
        if not turn:
            for offset in range(0,30,2):
                read_word(code,0x1400+offset,0);code+=[0x3f10,0xa8f0,0x1500+offset]
    code+=[0x68fc,0x600d];pc=len(code)*2;code+=[0xd0ff]
    stub=[0x2e00,0x58f0,0x58f1]
    for i in range(12):stub+=[0x780b,6+2*i,0x3f10,0xa8f0,0x1400+2*i]
    stub+=[0x6810,0xa4,0x88f0,0x7a50,0x3f10,0xa8f0,0x1418]
    for address,destination in ((0xf010,0x141a),(0xf012,0x141c)):
        read_word(stub,address,0);stub+=[0x3f10,0xa8f0,destination]
    read_word(stub,0x1420,0);stub+=[0x5001,0x3f10,0xa8f0,0x1420]
    stub+=[0xbd08,0x3262,0xbd08,0x4702,0x381f,0x380f,0x2f00,0xc620]
    caller=bytearray(image[0x200000:]);caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    caller[0x1800:0x1800+len(stub)*2]=struct.pack('<%dH'%len(stub),*stub)
    firmware=out/(name+'.prg');firmware.write_bytes(caller);em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,start_paused=True,icount_shift=0)
        for address,start,end in ((0x32649c,0x2f649c,0x2f6508),(0x83b48,0x53b48,0x53c00)):
            assert base64.b64decode(em.snapshot('code',address,end-start,diagnostics=False)['frame'])==original[start:end]
        em.qmp.execute('qom-set',{'path':'/machine','property':'instruction-clock','value':True})
        em.qmp.execute('cont');deadline=time.monotonic()+12;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=20:{pc:04x}'.lower() in state.lower():break
            time.sleep(.02)
        em.qmp.execute('stop');(out/(name+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=20:{pc:04x}'.lower() in state.lower() and all(k+'=none' in state for k in ('ROM_MISSING','UNSUPPORTED','INVALID_CONTROL')),state[:1500]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert [regs[r] for r in (1,2,3,7,12,15)]==[0x1234,0x2345,0x3456,0x789a,0x600d,0x6000],regs
        def memory(a,n):return base64.b64decode(em.snapshot('data',a,n,diagnostics=False)['frame'])
        calls=struct.unpack('<H',memory(0x1420,2))[0];assert calls==1+int(enabled!=0)
        captured=[list(struct.unpack('<15H',memory(a,30))) for a in (0x1500,0x1400)]
        assert captured[0][12:]==[1,1,0x8808]
        if enabled:assert captured[1][12:]==[enabled,1,0x8808]
        else:assert captured[1]==[0xdead]*15
        timer=em.qmp.execute('qom-get',{'path':'/machine','property':'timer2-state'})
        ticks=int(re.search(r'EXPIRED:(\d+)',timer)[1]);assert ticks==(600 if enabled else 350),timer
        ram=b''.join(memory(a,0x8000) for a in range(0x510000,0x530000,0x8000))
        guest=bytearray(ram);guest[0::2],guest[1::2]=ram[1::2],ram[0::2]
        (out/(name+'.ram.bin')).write_bytes(guest)
        row=dict(name=name,enabled=enabled,initial_words=sorted(initial.items()),second_words=sorted(second.items()),
                 calls=calls,captured=captured,timer2_ticks=ticks,passed=True,stack_restored=True,host_guest_memory_writes=0)
        cases.append(row);print(json.dumps({k:row[k] for k in ('name','calls','timer2_ticks','passed')}),flush=True)
    finally:
        if em:em.close()
(out/'report.json').write_text(json.dumps(dict(passed=len(cases)==6,cases=cases,
 original_prg_sha256=hashlib.sha256(original).hexdigest(),qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
 scope='guest-set EOF bit,original IRQ dispatches original battle background callback,playback capture then original cleanup/start;no refill/file/codec/physical timing proof'),indent=2)+'\n',encoding='utf8')
