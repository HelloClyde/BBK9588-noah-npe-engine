"""Call the three unmodified original bitmap draw routines against RAM.

The LCD buffer pointer and initial packed pixels are diagnostic inputs.
No draw stubs; capture the complete240x120 packed display buffer.
"""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
from web.np560_web import Emulator

parser=argparse.ArgumentParser();parser.add_argument('--qemu',type=Path,required=True);parser.add_argument('--case')
args=parser.parse_args();out=Path('tmp/guest-bitmap-draw-native');out.mkdir(exist_ok=True)
prg=Path('tmp/NP560.PRG').read_bytes();rom=out/'unmodified-draw.rom';rom.write_bytes(bytes(0x30000)+prg)
shapes=[('even',5,3,2,3,0x517ffa),('odd',5,3,3,3,0x517ff0),
        ('one-odd',1,3,3,3,0x517ffd),('clip',255,3,237,119,0x517ffa),
        ('negative-x',5,3,65535,3,0x517ffa),('negative-y',5,3,3,65535,0x517ffa),
        ('outside',5,3,240,120,0x517ffa),('zero-width',0,3,3,3,0x517ffa),
        ('zero-height',5,0,3,3,0x517ffa),('odd-even-width',6,3,1,3,0x517ff8),
        ('odd-retained-register',5,3,3,3,0x517ffa)]
cases=[dict(name=f'{name}-kind{kind}',width=w,height=h,x=x,y=y,address=address,kind=kind)
       for name,w,h,x,y,address in shapes for kind in range(3)]
for cfg in cases:
    cfg.update(background=0xa05f,pattern_hex='a123f03405677809',mask=0)
    if cfg['name'].startswith('odd-retained-register'):
        cfg.update(background=0x0005,pattern_hex='a0234056')
for mask in [0x0f,0xf0]:
    cases.append(dict(name=f'odd-banked-mask-{mask:02x}',width=5,height=2,x=3,y=3,
                      address=0x517ffa,kind=2,background=0xa05f,pattern_hex='a0234056',mask=mask))
if args.case:cases=[c for c in cases if c['name']==args.case]
assert cases
rows=[]
for cfg in cases:
    w,h=cfg['width'],cfg['height'];stride=((w+3)//4)*2
    # Nonstandard signature, header positions and high dimension bytes are
    # deliberate inputs: the original decoder ignores those fields.
    header=bytes([0xde,0xad,0x91,0x92,0x93,0x94,w,0x81,h,0x82])
    pattern=bytes.fromhex(cfg['pattern_hex'])
    payload=(pattern*((stride*h+len(pattern)-1)//len(pattern)))[:stride*h]
    data=header+payload
    code=[0x6800,2,0x68ff,0x6000,0x6810,0xa8,0x4810,0x68f0,cfg['background'],0xbf00,7199,0x1a10]
    def word(address,value):code.extend([0x6810,address>>15,0x68f2,value,0xa8f2,address&0x7fff])
    for mask_address in [0x5100f0,0x5180f0]:word(mask_address,cfg['mask']<<8)
    # Even-address word stores include a retained leading/trailing byte when
    # the actual bitmap address is odd.
    base=cfg['address']&~1;source=bytes([0x9a])*(cfg['address']&1)+data
    source+=bytes([0xbc])*(len(source)&1)
    for i in range(0,len(source),2):word(base+i,int.from_bytes(source[i:i+2],'big'))
    # The original accesses84FC/84FE through ES=0 (physical04FC/04FE),
    # not the IGNDS internal84FC window.
    code += [0x6811,0,0x68f2,0x54,0xa8f2,0x84fc,0x4820,0xa8f2,0x84fe]
    for value in [cfg['y'],cfg['x'],cfg['address']&65535,cfg['address']>>16]:code += [0x68f0,value,0x58f0]
    code += [0xbd03,0x42c0+4*cfg['kind'],0x50f8,0x68fc,0x600d]
    pc=len(code)*2;code.append(0xd0ff)
    assert len(code)*2<0x2000
    firmware=out/(cfg['name']+'.prg');caller=bytearray(prg)
    caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code);firmware.write_bytes(caller)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,
                    boot_mode='firmware-vma03',start_paused=True)
        address,size=[(0xd7c34,0x29a),(0xd7ece,0x364),(0xd8232,0x32e)][cfg['kind']]
        actual=base64.b64decode(em.snapshot('code',address,size,diagnostics=False)['frame'])
        assert actual==prg[address-0x30000:address-0x30000+size]
        em.qmp.execute('cont');deadline=time.monotonic()+10;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(cfg['name']+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:1700]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1700]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000,regs
        packed=bytearray(base64.b64decode(em.snapshot('data',0x540000,14400,diagnostics=False)['frame']))
        assert len(packed)==14400
        for i in range(0,len(packed),2):packed[i],packed[i+1]=packed[i+1],packed[i]
        pixels=bytes((packed[(119-y)*120+x//2]>>(0 if x&1 else 4))&15 for y in range(120) for x in range(240))
        # Bounds/zero-width cases are no-ops. A nonempty kind1 draw can also
        # retain every pixel when the destination is already nonzero.
        initial=bytes((cfg['background']>>shift)&15 for shift in [12,8,4,0])*7200
        if not w or not h or cfg['x']>=240 or cfg['y']>=120:assert pixels==initial,cfg['name']
        elif cfg['name']=='even-kind0':assert pixels!=initial,cfg['name']
        row=dict(input=cfg,source_hex=data.hex(),passed=True,stack_restored=True,original_code_region=[address,size])
        for label,content in [('packed',bytes(packed)),('pixels',pixels)]:
            path=out/(cfg['name']+'-'+label+'.bin');path.write_bytes(content)
            row[label]=path.as_posix();row[label+'_sha256']=hashlib.sha256(content).hexdigest()
        rows.append(row);print(cfg['name'],'unmodified draw complete packed/pixel buffer captured',flush=True)
    finally:
        if em is not None:em.close()
report=dict(passed=True,cases=rows,original_prg_sha256=hashlib.sha256(prg).hexdigest(),
            scope='Unmodified original draw routines into explicitly seeded RAM framebuffer. Complete buffer output; not LCD commit transport, timing, main pipeline or resource loader validation.')
(out/('report-'+args.case+'.json' if args.case else 'report.json')).write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
