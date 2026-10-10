"""Execute original2E:7FE4 with controlled codec-return and transfer boundaries.

Only B7AA and AD92 entries are replaced. Half selection, gain, repetition and
output-length configuration execute original instructions. No codec/DAC proof.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
sys.path.insert(0, str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word

parser=argparse.ArgumentParser();parser.add_argument('--qemu',type=Path,required=True)
args=parser.parse_args();out=Path('tmp/audio-output-half-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes();image=bytearray(0x30000)+original
resident=bytearray(image)
for address,entry in [(0x01b7aa,0x1000),(0x02ad92,0x1100)]:
    resident[address:address+4]=struct.pack('<2H',0xad20,entry)
rom=out/'output-with-boundaries.rom';rom.write_bytes(resident)
pattern=[-32768,32767,-30000,30000,-1,0,1,1000]
cases=[]
for name,index,half,output_words,decoded,current,target,calls in [
    ('first-existing',0,80,320,0,7,7,1),
    ('second-existing',1,80,320,0,7,7,1),
    ('new-frame',2,17,68,160,0,7,1),
    ('two-halves',2,17,68,160,0,7,2),
    ('third-half',2,17,68,160,0,7,3),
    ('zero-return',2,80,320,0,3,7,1),
    ('zero-return-again',2,80,320,0,3,7,2),
    ('one-sample',2,80,320,1,0,7,2),
    ('odd-five',2,80,320,5,7,7,2),
    ('length-change',2,80,320,162,7,0,2),
    ('stable-length',2,80,320,160,0,0,1),
    ('mute-after-ramp',0,80,320,0,1,0,2),
    ('index-three',3,80,320,160,4,4,1),
    ('index-max',65535,80,320,2,7,7,1),
]:
    code=[0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,0]
    for i in range(176):write_word(code,0x2984+i*2,pattern[i%8]&65535)
    code += [0x68f1,0x1000,0x68f0,0x5aa5,0xbf00,647,0x1a10]
    for address,value in [(0x1600,0),(0x1602,0),(0x1604,0xffff),
                          (0x4a64,current<<8|target),(0xf014,0x48c3)]:
        write_word(code,address,value)
    code += [0x6810,0xa2]
    for address,value in [(0x2b14,index),(0x2afa,half),(0x2ae6,output_words)]:
        write_word(code,address,value,banked=True)
    for _ in range(calls):
        # Original destination far pointer, captured by diagnostic AD92.
        code += [0x68f0,0x2000,0x58f0,0x4800,0x58f0,0xbd2e,0x7fe4,0x50f4]
    code += [0x68fc,0x600d];pc=len(code)*2;code += [0xd0ff]
    # B7AA supplies only the return value; PCM buffer was guest initialized.
    probe=[0x58f1,0x3f10,0x88f1,0x1600,0x5011,0x3f10,0xa8f1,0x1600,
           0x381f,0x68f0,decoded,0xc620]
    # AD92 captures its count and call count, without a DAC or buffer copy.
    transfer=[0x2e00,0x58f0,0x780b,0xe,0x3f10,0xa8f0,0x1604,
              0x3f10,0x88f0,0x1602,0x5001,0x3f10,0xa8f0,0x1602,
              0x380f,0x2f00,0xc620]
    caller=bytearray(image[0x200000:])
    assert len(code)*2<0x1000
    caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    for entry,body in [(0x1000,probe),(0x1100,transfer)]:
        caller[entry:entry+len(body)*2]=struct.pack('<%dH'%len(body),*body)
    firmware=out/(name+'.prg');firmware.write_bytes(caller)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,start_paused=True)
        for address,entry in [(0x01b7aa,0x1000),(0x02ad92,0x1100)]:
            assert base64.b64decode(em.snapshot('code',address,4,diagnostics=False)['frame'])==struct.pack('<2H',0xad20,entry)
        em.qmp.execute('cont');deadline=time.monotonic()+8;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=20:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(name+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=20:{pc:04x}'.lower() in state.lower(),state[:1600]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1600]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000,regs
        def memory(address,size):return base64.b64decode(em.snapshot('data',address,size,diagnostics=False)['frame'])
        def word(address):return struct.unpack('<H',memory(address,2))[0]
        pcm=memory(0x2984,352);repeated=memory(0x1000,1296)
        (out/(name+'.pcm.bin')).write_bytes(pcm);(out/(name+'.repeated.bin')).write_bytes(repeated)
        row=dict(name=name,index=index,half=half,output_words=output_words,decoded=decoded,current=current,target=target,calls=calls,
                 after_index=word(0x512b14),after_half=word(0x512afa),after_output_words=word(0x512ae6),
                 after_gain=word(0x4a64),after_flags8a=word(0xf014),codec_calls=word(0x1600),
                 transfer_calls=word(0x1602),transfer_words=word(0x1604),configured_words=word(0x4a62),
                 pcm_sha256=hashlib.sha256(pcm).hexdigest(),repeated_sha256=hashlib.sha256(repeated).hexdigest(),
                 stack_restored=True,host_guest_memory_writes=0,passed=True)
        assert row['codec_calls']>0 if index>=2 else row['codec_calls']==0,row
        cases.append(row);print(json.dumps({k:row[k] for k in ['name','after_index','after_half','after_output_words','after_gain','after_flags8a','codec_calls','transfer_calls','transfer_words']}),flush=True)
    finally:
        if em:em.close()
report=dict(passed=len(cases)==14,cases=cases,pattern=pattern,
            original_prg_sha256=hashlib.sha256(original).hexdigest(),qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
            scope='original half scheduler/gain/repetition/configuration with codec-return and transfer capture boundaries; no original codec or AD92/DAC timing')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
