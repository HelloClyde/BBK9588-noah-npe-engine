"""Invoke unmodified PRG heap services with guest-instruction setup.

Heap reset/seeds are diagnostic inputs. Full arena/bitmap/limit snapshots
prove service effects, not full NPE gameplay, file IO, or physical timing.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
from web.np560_web import Emulator

parser=argparse.ArgumentParser()
parser.add_argument('--qemu',type=Path,required=True)
parser.add_argument('--case')
args=parser.parse_args()
out=Path('tmp/resident-heap-lifecycle-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes()
definition=Path('npe-sdl/tests/resident_heap_lifecycle_cases.json')
inputs=json.loads(definition.read_text(encoding='utf8'))
if args.case:inputs=[c for c in inputs if c['name']==args.case]
assert inputs,'No matching case'
rom=out/'unmodified-services.rom';rom.write_bytes(bytes(0x30000)+original)
regions=[(0x308c1c,0xf6),(0x308b26,0xf6),(0x303a56,0x10c),
         (0x318abc,0xae),(0x276100,0x34a),(0x30699a,0x102),
         (0x323332,0x7e),(0x332512,0x28),(0x32431c,0)]
regions=[r for r in regions if r[1]]
rows=[]
for case in inputs:
    code=[0x6800,2,0x68ff,0x6000,0xbd30,0x8c1c]
    last_ds=None
    def word(address,value):
        global last_ds
        ds=address>>15
        if ds!=last_ds:code.extend([0x6810,ds]);last_ds=ds
        code.extend([0x68f2,value,0xa8f2,address&0x7fff])
    for i in range(64):word(0x514a40+2*i,0xa000+i)
    for address,value in case['payload']:word(address,value)
    for address,flags,next_pointer in case['nodes']:
        word(address,flags);word(address+2,next_pointer>>16);word(address+4,next_pointer&65535)
    word(0x5127ea,case['bitmap'])
    for operation,handle in case['operations']:
        if operation=='compact':code.extend([0xbd27,0x6100])
        else:code.extend([0x68f2,handle,0x58f2,0xbd03,0x4324 if operation=='unlock' else 0x4328,0x50f2])
    code.extend([0x68fc,0x600d]);pc=len(code)*2;code.append(0xd0ff)
    caller=bytearray(original);caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    firmware=out/(case['name']+'.prg');firmware.write_bytes(caller)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,
                    soc_rom_skeleton=True,boot_mode='firmware-vma03',start_paused=True)
        # Check CODE bus contents of every service and helper before execution.
        for address,size in regions:
            actual=base64.b64decode(em.snapshot('code',address,size,diagnostics=False)['frame'])
            expected=original[address-0x30000:address-0x30000+size]
            assert actual==expected,(case['name'],hex(address),'service code differs')
        vector=base64.b64decode(em.snapshot('code',0x34324,8,diagnostics=False)['frame'])
        assert vector==original[0x4324:0x432c],vector.hex()
        em.qmp.execute('cont');deadline=time.monotonic()+8;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(case['name']+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:1500]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1500]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000,regs
        def memory(address,size):
            raw=base64.b64decode(em.snapshot('data',address,size,diagnostics=False)['frame'])
            # QEMU snapshots word lanes in host little-endian order. Store
            # canonical guest bytes with the high lane first for C comparison.
            result=bytearray(raw)
            for i in range(0,size,2):result[i],result[i+1]=result[i+1],result[i]
            return bytes(result)
        blobs={label:memory(address,size) for label,address,size in
               [('arena',0x514a40,0xf000),('bitmap',0x5127ea,32),('limit',0x5129a0,4)]}
        paths={};hashes={}
        for label,data in blobs.items():
            path=out/(case['name']+'-'+label+'.bin');path.write_bytes(data)
            paths[label]=path.as_posix();hashes[label]=hashlib.sha256(data).hexdigest()
        row=dict(name=case['name'],input=case,files=paths,sha256=hashes,
                 arena_prefix=blobs['arena'][:44].hex(),bitmap=blobs['bitmap'].hex(),
                 limit=blobs['limit'].hex(),stack_restored=True,service_code_unmodified=True,
                 host_guest_memory_writes=0,passed=True)
        rows.append(row)
        print(json.dumps({k:row[k] for k in ['name','arena_prefix','bitmap','limit','passed']}),flush=True)
    finally:
        if em:em.close()
report=dict(passed=len(rows)==len(inputs),cases=rows,original_prg_sha256=hashlib.sha256(original).hexdigest(),
            qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
            input_sha256=hashlib.sha256(definition.read_bytes()).hexdigest(),
            scope='unmodified original heap services and helpers, guest setup and full arena/bitmap/limit; no full game or file IO')
path=out/('report-'+args.case+'.json' if args.case else 'report.json')
path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
