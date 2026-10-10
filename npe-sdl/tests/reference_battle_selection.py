"""Run the unchanged original formation selector, without enemy file loading."""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
from web.np560_web import Emulator

p=argparse.ArgumentParser();p.add_argument('--qemu',type=Path,required=True);args=p.parse_args()
out=Path('tmp/battle-selection-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes()
cases=[]
def add(name,**kw):
    c=dict(name=name,flag=0,count=2,ids=[0x1234,0x8001,0xffff],enemies=[],context=0,
           chase=0x1234,seed=1,key=254,monster=None)
    c.update(kw);cases.append(c)
for count in [0,1,2,3,255]:add('empty-retain-'+str(count),count=count)
for flag in [1,2,255]:add('empty-flag-'+str(flag),flag=flag)
for enemy in [1,127,128,255]:add('single-'+str(enemy),enemies=[enemy])
add('hole',enemies=[1,0,2])
for seed in [1,14,12,537]:add('two-'+str(seed),enemies=[1,2],seed=seed)
for seed in [12,5945]:add('eight-'+str(seed),enemies=list(range(1,9)),seed=seed)
add('signed-random',enemies=[128,255],seed=12)
for context in [32768,65535]:add('negative-context-'+str(context),enemies=[1,2],context=context,seed=12)
for context,chase in [(1,0),(1,256),(1,32768),(1,65535),(2,3),(3,0x1234)]:
    add('chase-'+str(context)+'-'+str(chase),context=context,chase=chase)
for flag in [1,2,255]:add('match-flag-'+str(flag),flag=flag,key=128,monster=[255,128,129],enemies=[2])
add('match-last',flag=1,key=51,monster=[1,51,2],slot=3)
rows=[]
for c in cases:
    code=[0x6800,2,0x68ff,0x6000]
    setup={}
    def word(address,value):setup[address]=(value>>8)&255;setup[address+1]=value&255
    def byte(address,value):setup[address]=value&255
    for address in [0x510000,0x518000,0x520000,0x528000]:
        code.extend([0x6810,address>>15,0x4810,0x68f0,0xa5a5,0xbf00,0x3fff,0x1a10])
    word(0x5129b0,0x51);word(0x5129b2,0x3000)
    word(0x51307e,c['seed']>>16);word(0x513080,c['seed'])
    byte(0x527afa,1);byte(0x527a9a,c['flag']);byte(0x527af0,c['count'])
    word(0x527936,1);word(0x527938,c['context'])
    for i,v in enumerate(c['ids']):word(0x52792e+i*2,v)
    for i in range(8):byte(0x527916+i,c['enemies'][i] if i<len(c['enemies']) else 0)
    byte(0x5277fc,c['key'])
    for i in range(4):
        for j in range(3):byte(0x527625+i*14+j,0)
    if c['monster']:
        left,middle,right=c['monster'];slot=c.get('slot',0)
        for j,v in enumerate([middle,left,right]):byte(0x527625+slot*14+j,v)
    if 0<c['context']<32768:word(0x527840+(c['context']-1)*14,c['chase'])
    for address in sorted({a&~1 for a in setup}):
        value=(setup.get(address,0xa5)<<8)|setup.get(address+1,0xa5)
        code.extend([0x6810,address>>15,0x68f2,value,0xa8f2,address&0x7fff])
    code.extend([0xbd1c,0xae64,0x68fc,0x600d]);pc=len(code)*2;code.append(0xd0ff)
    caller=bytearray(original);caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    firmware=out/(c['name']+'.prg');firmware.write_bytes(caller)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,boot_mode='firmware-vma03',start_paused=True)
        for address,size in [(0x1cae64,0x7a6),(0x34124,4),(0x336566,10),(0x31bb5a,0x9e),(0x3307b0,0x90)]:
            raw=base64.b64decode(em.snapshot('code',address,size,diagnostics=False)['frame'])
            assert raw==original[address-0x30000:address-0x30000+size],hex(address)
        em.qmp.execute('cont');deadline=time.monotonic()+8;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(c['name']+'.cpu.txt')).write_text(state)
        assert f'PC=03:{pc:04x}'.lower() in state.lower() and 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1500]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000
        def memory(address,size):
            raw=bytearray(base64.b64decode(em.snapshot('data',address,size,diagnostics=False)['frame']))
            for i in range(0,len(raw),2):raw[i],raw[i+1]=raw[i+1],raw[i]
            return bytes(raw)
        ids=list(struct.unpack('>3H',memory(0x52792e,6)));count=memory(0x527af0,2)[0]
        hp=[int.from_bytes(memory(0x5264a0+148*(1+i),2),'big') for i in range(3)]
        rng=int.from_bytes(memory(0x51307e,4),'big');flag=memory(0x527a9a,2)[0]
        row=dict(input=c,ids=ids,count=count,hp=hp,rng=rng,flag=flag,stack_restored=True)
        assert hp==[0,0,0] and flag==c['flag'], row
        rows.append(row);print(json.dumps(row),flush=True)
    finally:
        if em:em.close()
report=dict(passed=True,cases=rows,original_prg_sha256=hashlib.sha256(original).hexdigest(),
            scope='Unchanged original formation selector and RNG; diagnostic RAM setup; enemy file loading, rendering and exceptions not executed.')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
