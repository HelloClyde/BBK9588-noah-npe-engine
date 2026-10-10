"""Unmodified original28:031E substring; inspect full128KiB data RAM."""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
from web.np560_web import Emulator
p=argparse.ArgumentParser();p.add_argument('--qemu',type=Path,required=True);p.add_argument('--case');args=p.parse_args()
out=Path('tmp/text-substring-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes()
cases=[]
def add(name,text,start=0,count=82,source=0x522000,destination=0x522400):
    cases.append(dict(name=name,text=text,start=start&65535,count=count&65535,source=source,destination=destination))
ascii100=bytes(65+i%26 for i in range(100))
odd=bytearray(ascii100);odd[81]=0xa1;odd[82]=0xa2
back=bytearray(ascii100);back[8]=0xa1;back[9]=0xa2;back[89]=0xa3
add('empty',b'');add('ascii-page',ascii100);add('odd-forward',bytes(odd))
add('gbk-page',bytes.fromhex('b5c0bedf')*25)
add('tail',ascii100,82);add('equal-length',ascii100,100);add('past-length',ascii100,101)
add('negative-start',ascii100,-1);add('zero-count',ascii100,10,0)
add('back-to-zero-odd',bytes(odd),82,-82);add('back-odd',bytes(back),90,-82)
add('back-equal-length',ascii100,100,-82);add('back-past-length',ascii100,101,-82)
add('minimum-negative-count',ascii100,90,-32768)
add('word-sum-wrap',b'A'*32764,32760,20,0x521000,0x52f000)
add('overlap-forward',b'ABCDEFG',0,7,0x522000,0x522001)
add('overlap-backward',b'ABCDEFG',0,7,0x522000,0x521fff)
add('bank-crossing',b'ABCDE',0,5,0x51fffd,0x522401)
rows=[]
for cfg in cases:
    if args.case and cfg['name']!=args.case:continue
    root=out/cfg['name'];root.mkdir(exist_ok=True)
    ram=bytearray().join(struct.pack('>H',(0x9e37+73*i)&65535) for i in range(65536))
    start=cfg['source']-0x510000;ram[start:start+len(cfg['text'])+1]=cfg['text']+b'\0'
    caller=bytearray(original);code=[0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,0]
    for v in [cfg['count'],cfg['start'],cfg['source']&65535,cfg['source']>>16,cfg['destination']&65535,cfg['destination']>>16]:
        code.extend([0x68f0,v,0x58f0])
    code.extend([0xbd28,0x031e,0x50fc,0x68fc,0x600d]);pc=len(code)*2;code.append(0xd0ff)
    caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    firmware=root/'caller.prg';firmware.write_bytes(caller);em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,boot_mode='firmware-vma03',start_paused=True)
        actual=base64.b64decode(em.snapshot('code',0x28031e,0x320,diagnostics=False)['frame'])
        assert actual==original[0x25031e:0x25063e]
        for i in range(0,len(ram),0x1000):
            data=bytearray(ram[i:i+0x1000]);data[0::2],data[1::2]=data[1::2],data[0::2];em.write_data(0x510000+i,data)
        def snapshot():
            data=bytearray()
            for a in range(0x510000,0x530000,0x8000):
                block=bytearray(base64.b64decode(em.snapshot('data',a,0x8000,diagnostics=False)['frame']))
                block[0::2],block[1::2]=block[1::2],block[0::2];data+=block
            return bytes(data)
        before=snapshot();assert before==ram;(root/'before.bin').write_bytes(before)
        em.qmp.execute('cont');deadline=time.monotonic()+10
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.001)
        em.qmp.execute('stop');(root/'cpu.txt').write_text(state,encoding='utf-8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:1400]
        assert all(k+'=none' in state for k in ['ROM_MISSING','UNSUPPORTED','INVALID_CONTROL']),state[:1400]
        compact=state.replace(' ','').lower();assert 'r12=600d' in compact and 'r15=6000' in compact
        result=int(re.search(r'\br0=([0-9a-f]+)',compact).group(1),16)
        after=snapshot();(root/'after.bin').write_bytes(after)
        row={k:v for k,v in cfg.items() if k!='text'}
        row.update(passed=True,result=result,changed_bytes=sum(a!=b for a,b in zip(before,after)))
        (root/'report.json').write_text(json.dumps(row,indent=2)+'\n',encoding='utf-8')
        rows.append(row);print(cfg['name'],result,row['changed_bytes'],flush=True)
    finally:
        if em:em.close()
(out/'report.json').write_text(json.dumps(dict(passed=True,cases=rows,original_prg_sha256=hashlib.sha256(original).hexdigest(),
    qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),original_region=dict(cpu=0x28031e,file=0x25031e,size=0x320)),indent=2)+'\n',encoding='utf-8')
