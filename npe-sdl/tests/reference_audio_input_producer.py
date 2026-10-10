"""Run original2B:2894 producer with a guest RAM file-read boundary.

Only1C:CCD4 is routed to diagnostic RAM copying/returned lengths. The
producer, XOR service and word swapping run as unmodified original code.
No filesystem, NAND, codec or actual original file reader is certified.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
sys.path.insert(0,str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word

parser=argparse.ArgumentParser();parser.add_argument('--qemu',type=Path,required=True)
args=parser.parse_args();out=Path('tmp/audio-input-producer-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes();image=bytearray(0x30000)+original
resident=bytearray(image);resident[0x1cccd4:0x1cccd8]=struct.pack('<2H',0xad20,0x1000)
rom=out/'producer-with-ram-reader.rom';rom.write_bytes(resident)
cases=[]
for name, flags8a, remaining, returns, key, calls in [
    ('no-request',0,10000,[0,0],0,1),
    ('source0-full',0x41,8192,[4096,0],0,1),
    ('source1-last',0x42,8,[0,8],0,1),
    ('source0-short',0x41,10000,[8,0],0,1),
    ('source0-zero',0x41,10000,[0,0],0,1),
    ('zero-remaining',0x41,0,[0,0],0,1),
    ('both-last',0x43,5000,[4096,904],0,1),
    ('both-first-zero',0x43,10000,[0,4],0,1),
    ('odd-short',0x41,10000,[5,0],0,1),
    ('xor-even',0x42,8,[0,8],0x5a,1),
    ('xor-odd',0x41,5,[5,0],0x5a,1),
    ('positive-then-idle',0x41,8,[8,0],0,2),
]:
    code=[0x6800,2,0x68ff,0x6000,0x6810,0xa1,0x6811,1]
    # Two complete destination buffers, followed by two RAM source blocks.
    code += [0x68f1,0x5948,0x68f0,0x5aa5,0xbf00,4095,0x1a10]
    code += [0x6810,1]
    for pointer,pattern in [(0x1000,0x1234),(0x3000,0xabcd)]:
        code += [0x68f1,pointer,0x68f0,pattern,0xbf00,2047,0x1a10]
    code += [0x6810,0xa2]
    for address,value in [(0x29fc,remaining>>16),(0x29fe,remaining&65535),
                          (0x2af8,17),(0x2af6,19),(0x2a1c,0),(0x2a1e,0x2400)]:
        write_word(code,address,value,banked=True)
    for address,value in [(0xf012,0x4040),(0xf014,flags8a),(0x4bb6,key<<8),
                          (0x1300,0),(0x1304,0xffff),(0x1306,0xffff)]:
        write_word(code,address,value)
    for _ in range(calls):code += [0xbd2b,0x2894]
    code += [0x68fc,0x600d];pc=len(code)*2;code += [0xd0ff]
    # Replace just the file IO boundary; capture each requested byte length.
    body=[0x2e00,0x1f10,0x1f11]+[0x58f0|r for r in range(2,7)]
    body += [0x782b,0xc,0x786b,0x10,0x76f2,0xe948,0xac01,0]
    branch=len(body)-1
    body += [0x68f1,0x9000,0x68f4,returns[0],0x3f10,0xa8f6,0x1304,0xac00,0]
    join=len(body)-1;body[branch]=0x1000+len(body)*2
    body += [0x68f1,0xb000,0x68f4,returns[1],0x3f10,0xa8f6,0x1306]
    body[join]=0x1000+len(body)*2
    body += [0x3f10,0x88f5,0x1300,0x5051,0x3f10,0xa8f5,0x1300,
             0x6810,0xa1,0x6811,1,0xff72,0x0834,0x5640,0xac01,0]
    skip=len(body)-1
    body += [0x5231,0xcf03,0x5b21]
    body[skip]=0x1000+len(body)*2
    body += [0x0814]+[0x380f|(r<<4) for r in reversed(range(2,7))]
    body += [0x1f91,0x1f90,0x4800,0x2f00,0xc620]
    caller=bytearray(image[0x200000:])
    caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    caller[0x1000:0x1000+len(body)*2]=struct.pack('<%dH'%len(body),*body)
    firmware=out/(name+'.prg');firmware.write_bytes(caller)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,start_paused=True)
        code_boundary=base64.b64decode(em.snapshot('code',0x1cccd4,4,diagnostics=False)['frame'])
        assert code_boundary==struct.pack('<2H',0xad20,0x1000),code_boundary.hex()
        adapter_code=base64.b64decode(em.snapshot('code',0x201000,8,diagnostics=False)['frame'])
        assert adapter_code==struct.pack('<4H',*body[:4]),adapter_code.hex()
        em.qmp.execute('cont');deadline=time.monotonic()+8;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=20:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(name+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=20:{pc:04x}'.lower() in state.lower(),state[:1300]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1300]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000,regs
        def memory(address,size):return base64.b64decode(em.snapshot('data',address,size,diagnostics=False)['frame'])
        sources=[memory(0x50d948+0x1000*i,4096) for i in range(2)]
        pending=struct.unpack('<2H',memory(0x512af6,4))
        rem_hi,rem_lo=struct.unpack('<2H',memory(0x5129fc,4))
        final89,final8a=struct.unpack('<2H',memory(0xf012,4))
        requests=struct.unpack('<2H',memory(0x1304,4));read_calls=struct.unpack('<H',memory(0x1300,2))[0]
        expected_calls=bool(flags8a&1)+bool(flags8a&2)
        assert read_calls==expected_calls,(name,read_calls)
        assert (rem_hi<<16|rem_lo)==remaining-sum(returns),(name,rem_hi,rem_lo)
        for source,data in enumerate(sources):
            expected=bytearray(b'\xa5\x5a'*2048)
            raw=(b'\x34\x12' if source==0 else b'\xcd\xab')*2048
            count=returns[source]
            if count:
                even=count&~1
                expected[:even]=bytes(v^key for v in raw[:even])
                for i in range(0,even,2):expected[i],expected[i+1]=expected[i+1],expected[i]
                # DMCU byte addressing visits a word's high lane first.
                # The unpaired byte is not included in0E1E's word swap.
                if count&1:expected[count]=raw[count]^key
            assert data==expected,(name,source,data[:12].hex(),expected[:12].hex())
            (out/f'{name}-source{source}.bin').write_bytes(data)
        row=dict(name=name,flags8a=flags8a,remaining=remaining,returns=returns,key=key,calls=calls,
                 requested=list(requests),read_calls=read_calls,final_remaining=rem_hi<<16|rem_lo,
                 final_pending=[pending[1],pending[0]],final_flags89=final89,final_flags8a=final8a,
                 source_sha256=[hashlib.sha256(v).hexdigest() for v in sources],
                 source_prefix=[v[:12].hex() for v in sources],stack_restored=True,
                 host_guest_memory_writes=0,file_io_boundary_replaced=True,passed=True)
        cases.append(row);print(json.dumps({k:row[k] for k in ['name','requested','final_remaining','final_pending','final_flags89','final_flags8a','passed']}),flush=True)
    finally:
        if em:em.close()
report=dict(passed=len(cases)==12,cases=cases,
    original_prg_sha256=hashlib.sha256(original).hexdigest(),qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
    scope='original producer, XOR and word swap with diagnostic RAM file reader; no original filesystem, NAND, codec or timing proof')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
