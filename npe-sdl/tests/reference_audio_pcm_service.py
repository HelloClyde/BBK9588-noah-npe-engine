"""Run original PCM outer service, half preparation, gain, clock and refill.

Only missing B7AA/B816/B84E/AD92 boundaries are diagnostic replacements.
Cursor positions/codec samples are explicit inputs, not original codec proof.
"""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
sys.path.insert(0,str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word

parser=argparse.ArgumentParser();parser.add_argument('--qemu',type=Path,required=True)
parser.add_argument('--case');parser.add_argument('--output-dir',type=Path,default=Path('tmp/audio-pcm-service-native'))
args=parser.parse_args();out=args.output_dir;out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes();image=bytearray(0x30000)+original
entries={0x01b7aa:0x1000,0x02ad92:0x1400,0x01b816:0x1800,0x01b84e:0x1c00}
resident=bytearray(image)
for addr,entry in entries.items():resident[addr:addr+4]=struct.pack('<2H',0xad20,entry)
rom=out/'codec-boundaries.rom';rom.write_bytes(resident)

# Trace32 words: boundary arguments, flags, reloads, five clock words,
# output, snapshot, cursor, tail, source accounting and gain.
near=[0xf012,0xf014,0xf01c,0x4aac,0x4aae,0x4ab0,
      0x4a52,0x4a54,0x4a56,0x4a58,0x4a5a]
banked=[0x2ae6,0x2b14,0x2afa,0x2842,0x2844,0x29e4,0x29e6,0x2afc,
        0x29e0,0x29e2,0x2af4,0x2af6,0x2af8,0x2b0c,0x29dc,0x29de]

def trace(event,params):
    body=[0x3f10,0x88f2,0x3500,0x4c26,0x68f0,0x3000,0x1020]
    for offset,ops in enumerate([[0x68f0,event],*params]):
        body+=ops+[0x3f10,0x6a02,offset*2]
    for i,addr in enumerate(near):body += [0x3f10,0x88f0,addr,0x3f10,0x6a02,(i+4)*2]
    body += [0x6810,0xa2]
    for i,addr in enumerate(banked):body += [0x88f0,addr,0x3f10,0x6a02,(i+15)*2]
    body += [0x3f10,0x88f0,0x4a64,0x3f10,0x6a02,62,
             0x3f10,0x88f0,0x3500,0x5001,0x3f10,0xa8f0,0x3500]
    return body

def prologue():return [0x2e00,0x1f10,0x58f0,0x58f1,0x58f2,0x58f3]
def epilogue():return [0x383f,0x382f,0x381f,0x380f,0x1f90,0x2f00,0xc620]

def config(name,**changes):
    row=dict(name=name,demand=0xc123,index=2,half=80,words=320,decoded=160,
             current=0,target=7,flags89=0x20,flags8a=0,active=32,pending=32,debt=0,
             remaining=1000,frame_bits=168,tail=0x8c00,
             positions=[[0x8808,8],[0x880a,0]],
             value=0x12345678abcd,increment=0x10002,
             previous_cursor=0x8800,previous_remainder=0x55aa)
    row.update(changes);return row

inputs=[config('both-refill'),config('first-refill',demand=0x4123),
 config('second-refill',demand=0x8123),config('no-demand',demand=0x0123),
 config('both-no-refill',flags89=0),config('first-existing',index=0),
 config('second-existing',index=1),config('zero-codec',decoded=0),
 config('one-codec-sample',decoded=1),config('five-codec-samples',decoded=5),
 config('changed-length',decoded=162),config('index-max',index=65535,decoded=2),
 config('active-source-switch',active=4,pending=16,positions=[[0x8814,8],[0x8808,0]]),
 config('other-source-switch',flags8a=0x800,active=4,pending=16,positions=[[0x8814,8],[0x8808,0]]),
 config('both-producers-busy',flags89=0x4030,flags8a=3,active=4,pending=6,
        positions=[[0x8814,8],[0x8808,0]]),
 config('exhaust-refill',remaining=0,active=4,pending=6,positions=[[0x8814,8],[0x8800,0]]),
 config('debt-refill',remaining=0,active=3,pending=1,debt=3,positions=[[0x8808,8],[0x8800,0]]),
 config('pointer-equal',flags89=0,positions=[[0x8c00,8],[0x8c00,0]]),
 config('pointer-before',flags89=0,positions=[[0x8bfe,8],[0x8bfe,0]]),
 config('pointer-after',flags89=0,positions=[[0x8c02,8],[0x8c02,0]]),
 config('wide-pointer-unsigned',flags89=0,positions=[[0x80008800,8],[0xffff8800,0]]),
 config('sticky-eof',flags89=0x800),
 config('no-demand-zero-output',demand=0,words=0),
 config('no-demand-past-tail',demand=0,previous_cursor=0x8c02),
 config('clock-wrap',index=0,value=0xffffffffffff,increment=0xffffffff),
 config('fraction-carry',index=0,value=0xffff,increment=1),
 config('snapshot-before-both',index=0,value=0xffffffffabcd,increment=0x10002),
 config('muted-existing',index=0,current=0,target=0)]
if args.case:
    inputs=[r for r in inputs if r['name']==args.case];assert len(inputs)==1,args.case
cases=[]
for row in inputs:
    name=row['name'];code=[0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,0]
    # REP writes are guest RAM initialization, not host memory patches.
    for address,count,value in [(0x8800,512,0x1001),(0x50d948,2048,0x2002),
                                (0x50e948,2048,0x3003),(0x2984,176,0x8ad0)]:
        code += [0x6810,address>>15,0x68f1,address&0x7fff,0x68f0,value,0xbf00,count-1,0x1a10]
    value=row['value'];increment=row['increment']
    for address,v in [(0xf012,row['flags89']),(0xf014,row['flags8a']),
        (0xf01c,row['demand']),(0x4aac,0x1111),(0x4aae,0x2222),(0x4ab0,row['words']),
        (0x4a52,value>>32),(0x4a54,value>>16&65535),(0x4a56,value&65535),
        (0x4a58,increment>>16),(0x4a5a,increment&65535),(0x4a60,32000),
        (0x4a64,row['current']<<8|row['target']),(0x3500,0),(0x3502,0)]:write_word(code,address,v)
    for i,(pointer,remainder) in enumerate(row['positions']):
        for j,v in enumerate([pointer>>16,pointer&65535,remainder]):write_word(code,0x3620+i*6+j*2,v)
    selected=bool(row['flags8a']&0x800)
    meta={0x2842:0xa55a,0x2844:0x5aa5,
        0x29d0:row['frame_bits']>>16,0x29d2:row['frame_bits']&65535,
        0x29dc:0x50,0x29de:0xd948+0x1000*selected,
        0x29e0:row['tail']>>16,0x29e2:row['tail']&65535,
        0x29e4:row['previous_cursor']>>16,0x29e6:row['previous_cursor']&65535,
        0x29fc:row['remaining']>>16,0x29fe:row['remaining']&65535,
        0x2ae6:row['words'],0x2af4:row['active'],
        0x2af6:row['pending'] if not selected else 0,
        0x2af8:row['pending'] if selected else 0,
        0x2afa:row['half'],0x2afc:row['previous_remainder'],0x2b0c:row['debt'],0x2b14:row['index']}
    code += [0x6810,0xa2]
    for addr,v in meta.items():write_word(code,addr,v,banked=True)
    code += [0x68f0,0x1234,0x68f1,0x5678,0x68f2,0x9abc,0x68f3,0xdef0,
             0xbd2f,0x66f2,0x68fc,0x600d]
    pc=len(code)*2;code += [0xd0ff];assert len(code)*2<0x1000
    # Codec return supplies count and a freshly diagnostic-initialized PCM.
    codec=prologue()+trace(1,[[0x68f0,0],[0x68f0,0x2984],[0x68f0,row['decoded']]])
    codec += [0x6810,0,0x68f1,0x2984,0x68f0,0x8ad0,0xbf00,175,0x1a10]
    codec += epilogue()[:-1]+[0x68f0,row['decoded'],0xc620]
    transfer=prologue()+trace(2,[[0x780b,10],[0x780b,12],[0x780b,14]])+epilogue()
    # R3=3620+6*cursor_call; at most two cursor calls per service.
    query = prologue()+[0x3f10,0x88f3,0x3502,0x0813,0x4c31,0x4c12,0x1031,0x68f0,0x3620,0x1030]
    query += trace(3,[[0x3f10,0x7803,0],[0x3f10,0x7803,2],[0x3f10,0x7803,4]])
    query += [0x3f10,0x7803,4,0x6810,0xa2,0xa8f0,0x2afc,
              0x3f10,0x88f0,0x3502,0x5001,0x3f10,0xa8f0,0x3502,
              0x3f10,0x7803,0,0x3f10,0x7813,2,
              0x383f,0x382f,0x50f4,0x1f90,0x2f00,0xc620]
    restore=prologue()+trace(4,[[0x780b,6],[0x780b,8],[0x780b,10]])+epilogue()
    caller=bytearray(image[0x200000:]);caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    for entry,body in [(0x1000,codec),(0x1400,transfer),(0x1800,query),(0x1c00,restore)]:
        assert len(body)*2<=0x400,(name,entry,len(body))
        caller[entry:entry+len(body)*2]=struct.pack('<%dH'%len(body),*body)
    firmware=out/(name+'.prg');firmware.write_bytes(caller);em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,start_paused=True)
        for addr,entry in entries.items():
            assert base64.b64decode(em.snapshot('code',addr,4,diagnostics=False)['frame'])==struct.pack('<2H',0xad20,entry)
            actual=base64.b64decode(em.snapshot('code',0x200000+entry,16,diagnostics=False)['frame'])
            assert actual==caller[entry:entry+16],(hex(entry),actual.hex(),caller[entry:entry+16].hex())
        for start,end in [(0x2f66f2,0x2f6834),(0x2e7fe4,0x2e815e),
                          (0x2499f6,0x249a1e),(0x8378a,0x838a4)]:
            assert base64.b64decode(em.snapshot('code',start,end-start,diagnostics=False)['frame'])==image[start:end]
        em.qmp.execute('cont');deadline=time.monotonic()+8;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=20:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(name+'.cpu.txt')).write_text(state,encoding='utf8')
        if f'PC=20:{pc:04x}'.lower() not in state.lower():
            (out/(name+'.failure-data.bin')).write_bytes(base64.b64decode(em.snapshot('data',0x3000,0x628,diagnostics=False)['frame']))
        assert f'PC=20:{pc:04x}'.lower() in state.lower() and 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1800]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert [regs[r] for r in [0,1,2,3,12,15]]==[0x1234,0x5678,0x9abc,0xdef0,0x600d,0x6000],regs
        def memory(addr,count):return base64.b64decode(em.snapshot('data',addr,count,diagnostics=False)['frame'])
        def word(addr):return struct.unpack('<H',memory(addr,2))[0]
        count=word(0x3500);assert 0<=count<=8,(name,count)
        print('diagnostic',name,count,hex(word(0xf01c)),hex(word(0x512b14)),flush=True)
        raw=memory(0x3000,count*64) if count else b''
        row['trace']=[list(struct.unpack_from('<32H',raw,i*64)) for i in range(count)]
        (out/(name+'.trace.bin')).write_bytes(raw)
        final=[0,0,0,0]+[word(a) for a in near]+[word(0x510000+a) for a in banked]+[word(0x4a64)]
        (out/(name+'.final.bin')).write_bytes(struct.pack('<32H',*final));row['final']=final
        for label,addr,size in [('window',0x8800,1024),('repeated',0x1000,1296)]:
            (out/(name+'.'+label+'.bin')).write_bytes(memory(addr,size))
        row.update(passed=True,stack_registers_restored=True,host_guest_memory_writes=0)
        cases.append(row);print(name,count,hex(final[4]),hex(final[6]),flush=True)
    finally:
        if em:em.close()
report=dict(passed=len(cases)==len(inputs),cases=cases,
    original_prg_sha256=hashlib.sha256(original).hexdigest(),qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
    scope='unmodified PCM service/half/gain/repeat/clock/refill and software acknowledgements with explicit codec/cursor/transfer boundaries; no original PCM, physical DTU/IRQ or file reopen')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
