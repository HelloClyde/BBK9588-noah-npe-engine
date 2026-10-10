"""Original launcher/map/battle allocation prefixes, packed IO and cleanup.

All heap nodes are created by original allocations, not captured chain seeds.
File seek/read and allocation prompts have explicit diagnostic RAM adapters.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
from web.np560_web import Emulator

parser=argparse.ArgumentParser();parser.add_argument('--qemu',type=Path,required=True)
parser.add_argument('--status-cycle',action='store_true')
parser.add_argument('--shutdown',action='store_true')
parser.add_argument('--ending',action='store_true')
args=parser.parse_args();assert sum([args.status_cycle,args.shutdown,args.ending])<=1
out=Path('tmp/ending-return/native' if args.ending else 'tmp/rpg-shutdown-native' if args.shutdown else 'tmp/battle-status-resource-native' if args.status_cycle else 'tmp/rpg-resource-lifecycle-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes();npe=Path('tmp/tjfy-world-probe.npe').read_bytes()
offset,length=struct.unpack_from('>2I',npe,0x3a+9*8)
assert length<65536 and offset+length<=len(npe)
inputs=[dict(name=name,cap=cap,high_length=high,no_handles=no_handles,cleanup=cleanup)
        for name,cap,high,no_handles,cleanup in [
          ('resources-full',65535,False,False,0),
          ('resources-short',5,False,False,0),
          ('resources-eof',0,False,False,0),
          ('resources-high-length',65535,True,False,0),
          ('resources-allocation-failed',65535,False,True,0),
          ('resources-launcher-cleanup',65535,False,False,1),
          ('resources-zero-cleanup',65535,False,False,2)]]
if args.status_cycle:
    inputs=[dict(name=name,cap=cap,high_length=False,no_handles=failed,cleanup=0,cycles=cycles,released=released)
            for name,cap,failed,cycles,released in [
                ('cycle-full',65535,False,1,False),('cycle-short',5,False,2,False),
                ('cycle-eof',0,False,2,False),('cycle-released',65535,False,1,True),
                ('cycle-zero-release',0,True,0,True)]]
rows=[]
if args.shutdown:
    inputs=[dict(name=name,cap=65535,high_length=False,no_handles=False,cleanup=0,
                 flags=flag*256+166,zero_map=zero_map,zero_battle=zero_battle,repeats=repeats)
            for name,flag,zero_map,zero_battle,repeats in [
                ('shutdown-active',1,False,False,1),('shutdown-inactive',0,False,False,1),
                ('shutdown-two',2,False,False,1),('shutdown-high129',129,False,False,1),
                ('shutdown-high255',255,False,False,1),('shutdown-zero-map',1,True,False,1),
                ('shutdown-zero-battle',1,False,True,1),('shutdown-zero-both',1,True,True,1),
                ('shutdown-repeat',1,False,False,2)]]
if args.ending:
    inputs=[dict(name=name,cap=65535,high_length=False,no_handles=False,cleanup=0,
                 flags=flag*256+166,zero_map=zero_map,zero_battle=False,repeats=repeats,reallocate=reallocate)
            for name,flag,zero_map,repeats,reallocate in [
                ('ending-active',1,False,1,False),('ending-inactive',0,False,1,False),
                ('ending-two',2,False,1,False),('ending-zero-map',1,True,1,False),
                ('ending-repeat',1,False,2,False),('ending-reallocate',1,False,1,True)]]
for case in inputs:
    resident=bytearray(bytes(0x30000)+original);caller=bytearray(original)
    def install(address,words):
        data=struct.pack('<%dH'%len(words),*words)
        resident[address:address+len(data)]=data;caller[address-0x30000:address-0x30000+len(data)]=data
    # Controlled returns at boundaries after original allocation/cleanup code.
    for address in [0x1b2496,0x29d1fc,0x318bc4,0x1b2c86]:install(address,[0xc620])
    if args.shutdown:install(0x1fd568,[0xc620])
    if args.ending:install(0x2df606,[0xc620])
    payload=npe[offset:offset+min(length,case['cap'])]
    raw=bytearray(payload+bytes(len(payload)&1))
    for i in range(0,len(raw),2):raw[i],raw[i+1]=raw[i+1],raw[i]
    resident[0x100000:0x100000+len(raw)]=raw;caller[0xd0000:0xd0000+len(raw)]=raw
    copy=[0x2e00,0x1f10]+[0x58f0|r for r in range(5,8)]+[0x5640,0xac01,0]
    done=len(copy)-1;loop=0x1800+len(copy)*2
    copy += [0x0862,0x0873,0x4c71,0x5c61,0x58f6,0x1f90,0x4d71,0x29a7,
             0x0860,0x0871,0x4c71,0x5c61,0x58f6,0x1f90,0x4d71,0x0b7a,
             0x5011,0x5100,0x5031,0x5120,0x5241,0xac02,loop]
    copy[done]=0x1800+len(copy)*2
    copy += [0x380f|(r<<4) for r in reversed(range(5,8))]+[0x1f90,0x2f00,0xc620]
    install(0x31800,copy)
    seek=[0x2e00,0x1f10,0x58f0,0x58f1,0x58f2,0x780b,0xa,0x781b,0xc,
          0x3f10,0xa8f0,0x1400,0x3f10,0xa8f1,0x1402,
          0x3f10,0x88f2,0x1404,0x5021,0x3f10,0xa8f2,0x1404,
          0x382f,0x381f,0x380f,0x1f90,0x2f00,0xc620]
    install(0x31000,seek)
    read=[0x2e00,0x1f10]+[0x58f0|r for r in range(7)]+[
          0x3f10,0x88f6,0x1406,0x5061,0x3f10,0xa8f6,0x1406,
          0x782b,0xe,0x3f10,0xa8f2,0x1408,
          0x68f4,len(payload),0x3f10,0xa8f4,0x140a,
          0x780b,0xa,0x781b,0xc,0x3f10,0xa8f0,0x140c,0x3f10,0xa8f1,0x140e,
          0x68f2,0x10,0x4830,0x58f4,0xbd03,0x1800,0x384f,
          0x3f10,0x88f0,0x1400,0x3f10,0x88f1,0x1402,0x1014,0x5100,
          0x3f10,0xa8f0,0x1400,0x3f10,0xa8f1,0x1402]
    read += [0x380f|(r<<4) for r in reversed(range(7))]+[0x1f90,0x2f00,0xc620]
    install(0x31100,read)
    fail=[0x2e00,0x1f10,0x58f2,0x3f10,0x88f2,0x1410,0x5021,
          0x3f10,0xa8f2,0x1410,0x382f,0x1f90,0x2f00,0xc620]
    install(0x33197a,fail);install(0x3401c,[0xad03,0x1000]);install(0x34010,[0xad03,0x1100])
    code=[0x6800,2,0x68ff,0x6000]
    for address in [0x510000,0x518000,0x520000,0x528000]:
        code += [0x6810,address>>15,0x4810,0x68f0,0xa5a5,0xbf00,0x3fff,0x1a10]
    code += [0xbd30,0x8c1c,0xbd1b,0x23ce,0xbd29,0xd1c8]
    def word(address,value):code.extend([0x6810,address>>15,0x68f2,value,0xa8f2,address&0x7fff])
    def long(address,value):word(address,value>>16);word(address+2,value&65535)
    long(0x5279be,length+(0x10000 if case['high_length'] else 0));long(0x5279c2,offset)
    for address in range(0x1400,0x1412,2):code += [0x4800,0x3f10,0xa8f0,address]
    if case['no_handles']:
        for address in range(0x5127ea,0x51280a,2):word(address,65535)
    code += [0xbd31,0x8b80]
    if args.shutdown:
        word(0x527aa4,case['flags'])
        if case['zero_map']:word(0x527a6a,0)
        if case['zero_battle']:word(0x527a88,0)
        for _ in range(case['repeats']):code.extend([0xbd1f,0xd50e])
    if args.ending:
        word(0x527aa4,case['flags']);word(0x527934,21);word(0x527936,0xbeef);word(0x527ab4,0)
        if case['zero_map']:word(0x527a6a,0)
        for _ in range(case['repeats']):code.extend([0xbd2d,0xf59a])
        if case['reallocate']:code.extend([0xbd29,0xd1c8])
    if args.status_cycle:
        def release():
            # Original42:8E72..8E9A protocol; original heap services stay
            # unmodified. Reread the global before each service and clear
            # only the high flag byte, retaining handle and packed pointer.
            for vector in [0x4324,0x4328]:
                code.extend([0x6810,0xa4,0x88f2,0x7a88,0x58f2,0xbd03,vector,0x382f])
            code.extend([0x4950,0x6810,0xa4,0xa9f5,0x7aa4])
        for _ in range(case['cycles']):
            release();code.extend([0xbd31,0x8b80])
        if case['released']:release()
    if case['cleanup']:
        if case['cleanup']==2:
            for address in [0x52574c,0x52574e,0x525760,0x525626]:word(address,0)
        code += [0xbd1b,0x2c46]
    code += [0x68fc,0x600d];pc=len(code)*2;code.append(0xd0ff)
    install(0x30000,code)
    firmware=out/(case['name']+'.prg');rom=out/(case['name']+'.rom')
    firmware.write_bytes(caller);rom.write_bytes(resident)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,
                    boot_mode='firmware-vma03',start_paused=True)
        regions=[(0x1b23ce,0xc8),(0x29d1c8,0x34),(0x318b80,0x44),(0x31def6,0x96),
                 (0x1b2c46,0x40),(0x331df6,0x2c),(0x2577da,0x3e8),(0x315300,0xba),
                 (0x308b26,0xf6),(0x303a56,0x10c),(0x276100,0x34a),(0x314974,0xbe)]
        if args.shutdown:regions.append((0x1fd50e,0x5a))
        if args.ending:regions.append((0x2df59a,0x3a))
        for address,size in regions:
            actual=base64.b64decode(em.snapshot('code',address,size,diagnostics=False)['frame'])
            assert actual==original[address-0x30000:address-0x30000+size],(case['name'],hex(address))
        em.qmp.execute('cont');deadline=time.monotonic()+12;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(case['name']+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:1800]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1800]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000,regs
        def memory(address,size):
            data=bytearray()
            for delta in range(0,size,65536):
                data.extend(base64.b64decode(em.snapshot('data',address+delta,min(65536,size-delta),diagnostics=False)['frame']))
            for i in range(0,len(data),2):data[i],data[i+1]=data[i+1],data[i]
            return bytes(data)
        ram=memory(0x510000,131072);trace=memory(0x1400,18)
        assert int.from_bytes(trace[:4],'big')==offset+len(payload)
        calls=1+case.get('cycles',0)
        assert struct.unpack_from('>4H',trace,4)==(calls,calls,length,len(payload))
        assert int.from_bytes(trace[16:18],'big')==int(case['no_handles'])
        pointer=int.from_bytes(ram[0x179a6:0x179aa],'big')
        assert pointer==0x5169bc,hex(pointer)
        # Main-loop cleanup can compact the freed map/battle chunks; the
        # retained published pointer no longer promises an intact payload.
        if not args.shutdown and not args.ending:assert ram[pointer-0x510000:pointer-0x510000+len(payload)]==payload
        if args.shutdown:
            expected=case['flags']&255 if case['flags']>>8==1 else case['flags']
            assert int.from_bytes(ram[0x17aa4:0x17aa6],'big')==expected
        if not args.shutdown and not args.ending and not case['cleanup'] and not case['no_handles'] and not case.get('released',False):
            expected=[(0x514a40,0x8001,0x514b46),(0x514b46,0x8002,0x514c4c),
                      (0x514c4c,0x8003,0x514d52),(0x514d52,0x8004,0x514e58),
                      (0x514e58,0x8005,0x5169b6),(0x5169b6,0x8006,0x51e9bc),(0x51e9bc,0,0)]
            for address,flags,next_ in expected:
                assert struct.unpack_from('>HI',ram,address-0x510000)==(flags,next_),(case['name'],hex(address))
        if args.ending:
            assert struct.unpack_from('>HH',ram,0x17934)==(7,0xbeef)
            assert int.from_bytes(ram[0x17ab4:0x17ab6],'big')==1
            assert int.from_bytes(ram[0x17aa4:0x17aa6],'big')==case['flags']
        row=dict(input=case,passed=True,stack_restored=True,original_regions=regions,heap_headers_seeded=False)
        for label,content in [('ram',ram),('trace',trace)]:
            path=out/(case['name']+'-'+label+'.bin');path.write_bytes(content)
            row[label]=path.as_posix();row[label+'_sha256']=hashlib.sha256(content).hexdigest()
        rows.append(row);print(case['name'],'original allocations, packed read and cleanup captured',flush=True)
    finally:
        if em is not None:em.close()
report=dict(passed=True,cases=rows,packed_offset=offset,packed_length=length,
            original_prg_sha256=hashlib.sha256(original).hexdigest(),npe_sha256=hashlib.sha256(npe).hexdigest(),
            scope=('Unmodified ending dispatcher branch4A:F59A..F5D4 and original heap services; controlled return atF606, diagnostic setup and file adapters. Includes map reallocation prefix, no title UI/LCD or complete caller claim.' if args.ending else 'Unmodified main-loop resource cleanup slice3C:D50E..D568 and original heap/packed-loader services; controlled return before file close, diagnostic setup and file adapters. No complete original main-loop or filesystem equivalence.' if args.shutdown else 'Unmodified original heap and packed-loader services; diagnostic caller reproduces status release/reallocate protocol, heap reset and file adapters. No full UI caller RAM or filesystem verification.' if args.status_cycle else
                   'Unmodified launcher/map/battle allocation prefixes, packed loader and cleanup. Diagnostic heap reset and file/prompt adapters; no original filesystem/UI/LCD or game31 execution verification.'))
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
