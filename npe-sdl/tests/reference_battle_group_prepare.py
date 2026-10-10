"""Original group/combine record preparation before the round executor.

Diagnostic RAM only; the combine auxiliary call at8358 is skipped so its
record-writing prefix and original epilogue can finish without keyboard IO.
"""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
from web.np560_web import Emulator

p=argparse.ArgumentParser();p.add_argument('--qemu',type=Path,required=True);p.add_argument('--type-words',action='store_true');p.add_argument('--capture',action='store_true');args=p.parse_args()
assert not args.capture or args.type_words
out=Path('tmp/battle-selector-capture-native' if args.capture else 'tmp/battle-group-type-words-native' if args.type_words else 'tmp/battle-group-prepare-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes();rows=[]
cases=[]
def add(name,party=3,count=3,hp=(0,30000,0),calls=('group',),actor=129):
    cases.append(dict(name=name,party=party,count=count,hp=list(hp),calls=list(calls),actor=actor))
add('group-single-middle',party=1)
add('group-two-first',party=2,hp=(30000,30000,0))
add('group-three-last',hp=(0,0,30000))
add('group-negative-first',hp=(65535,30000,0))
add('group-sign-bit-first',hp=(32768,30000,0))
add('group-all-zero',hp=(0,0,0))
add('group-zero-count',count=0,hp=(0,30000,0))
add('group-one-count-zero',count=1,hp=(0,30000,0))
add('group-two-count-zero',count=2,hp=(0,0,30000))
add('group-zero-party',party=0,hp=(0,30000,0))
for party in (0,1,2,3):add('combine-party-'+str(party),party=party,calls=('combine',))
add('group-then-combine',calls=('group','combine'),hp=(0,0,30000))
add('combine-then-group',calls=('combine','group'))
add('group-twice',calls=('group','group'))
add('combine-twice',calls=('combine','combine'))
if args.capture:
    cases=[]
    for actor in range(6):add('capture-actor-'+str(actor),calls=('capture',),actor=actor)
for ci,c in enumerate(cases):
    records=bytearray((i*37+ci*11+19)&255 for i in range(888))
    for slot in range(6):
        hp=0 if slot==2 else 300 if slot<c['party'] else c['hp'][slot-c['party']] if slot-c['party']<3 else 0
        struct.pack_into('>H',records,slot*148+80,hp)
        for status in range(4):struct.pack_into('>H',records,slot*148+62+status*2,65532 if slot==1 and status==1 else 0)
    code=[0x6800,2,0x68ff,0x6000]
    setup={}
    def word(address,value):setup[address]=(value>>8)&255;setup[address+1]=value&255
    def byte(address,value):setup[address]=value&255
    for address in (0x510000,0x518000,0x520000,0x528000):
        code.extend([0x6810,address>>15,0x4810,0x68f0,0xa5a5,0xbf00,0x3fff,0x1a10])
    for i,v in enumerate(records):byte(0x526450+i,v)
    byte(0x527afa,c['party']);byte(0x527af0,c['count'])
    byte(0x527a9e,c['actor']);byte(0x527aae,166);byte(0x527ac0,255)
    if args.type_words:
        for address,value in zip((0x527aa0,0x527aa6,0x527aaa),(0x81a6,0xcbf0,0x153a)):word(address,value)
    for address in sorted({a&~1 for a in setup}):
        value=(setup.get(address,0xa5)<<8)|setup.get(address+1,0xa5)
        code.extend([0x6810,address>>15,0x68f2,value,0xa8f2,address&0x7fff])
    for call in c['calls']:code.extend([0xbd12,0xc914] if call=='capture' else [0xbd27,0x8b94] if call=='group' else [0xbd2c,0x8178])
    code.extend([0x68fc,0x600d]);pc=len(code)*2;code.append(0xd0ff)
    caller=bytearray(original);caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    # Original49:8178..8354 writes records and globals; skip only its next
    # auxiliary SCALL and execute the original register/stack epilogue.
    struct.pack_into('<2H',caller,0x2c8358-0x30000,0x1a10,0x1a10)
    if args.capture:struct.pack_into('<H',caller,0x12ca44-0x30000,0xc620)
    path=out/(c['name']+'.prg');path.write_bytes(caller)
    (out/(c['name']+'-before-records.bin')).write_bytes(records)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=path,boot_mode='firmware-vma03',start_paused=True)
        regions=((0x12c914,0x130),(0x3307b0,0x90)) if args.capture else ((0x278b94,0x342),(0x2c8178,0x1e0),(0x2c835c,12),(0x3307b0,0x90))
        for address,size in regions:
            raw=base64.b64decode(em.snapshot('code',address,size,diagnostics=False)['frame'])
            assert raw==original[address-0x30000:address-0x30000+size],hex(address)
        em.qmp.execute('cont');deadline=time.monotonic()+8;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(c['name']+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower() and 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1500]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000
        raw=bytearray().join(base64.b64decode(em.snapshot('data',a,0x8000,diagnostics=False)['frame'])
                             for a in (0x510000,0x518000,0x520000,0x528000))
        for i in range(0,len(raw),2):raw[i],raw[i+1]=raw[i+1],raw[i]
        (out/(c['name']+'-after-ram.bin')).write_bytes(raw)
        after=bytes(raw[0x16450:0x16450+888]);(out/(c['name']+'-after-records.bin')).write_bytes(after)
        controls=[raw[a-0x510000] for a in (0x527a9e,0x527aae,0x527ac0)]
        row=dict(input=c,before_controls=[c['actor'],166,255],after_controls=controls,
                 after_targets=[after[i*148+126] for i in range(6)],stack_restored=True)
        if args.type_words:
            row['before_type_words']=[0x81a6,0xcbf0,0x153a]
            row['after_type_words']=[int.from_bytes(raw[a-0x510000:a-0x510000+2],'big') for a in (0x527aa0,0x527aa6,0x527aaa)]
        rows.append(row);print(json.dumps(row),flush=True)
    finally:
        if em:em.close()
report=dict(passed=True,cases=rows,original_prg_sha256=hashlib.sha256(original).hexdigest(),
            scope='Original2F:C914..CA40 selector capture; diagnostic RAM; return inserted atCA44; subsequent actor search not sampled.' if args.capture else 'Original44:8B94 and49:8178..8354 record preparation, original epilogues; diagnostic RAM; combine auxiliary8358 skipped; full execution not sampled.')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
