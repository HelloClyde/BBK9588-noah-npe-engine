"""Original selector/controller and tile service, with explicit UI boundaries.

Diagnostic guest entry into1A:141E bypasses keyboard delivery. Map rendering,
item loading, string copying and the timed notice are recording boundaries;
they are not original UI or filesystem evidence. No host guest-memory writes.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
sys.path.insert(0, str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word

parser = argparse.ArgumentParser()
parser.add_argument('--qemu', type=Path, required=True)
args = parser.parse_args()
out = Path('tmp/object-pickup-native'); out.mkdir(exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
cases = []
# Boundaries record their order and the inventory at the exact call site.
def boundary(tag, args_count=0):
    body = [0x2e00, 0x1f10, 0x58f0, 0x58f1, 0x58f2,
            0x6810, 0, 0x3f10, 0x88f0, 0x1300,
            0x68f1, 0x1340, 0x1010, 0x1010, 0x68f2, tag, 0x3f10, 0x0a12,
            0x5001, 0x3f10, 0xa8f0, 0x1300]
    for i in range(args_count):
        body += [0x782b, 6+i*2, 0x3f10, 0xa8f2, 0x1400+tag*16+i*2]
    body += [0x6810, 0xa4, 0x88f2, 0x6852, 0x6810, 0,
             0x3f10, 0xa8f2, 0x1500+tag*2,
             0x382f, 0x381f, 0x380f, 0x1f90, 0x4800, 0x2f00, 0xc620]
    return body

for name, quantity, done, direction in [
    ('zero',0,0,3), ('98',98,0,3), ('99',99,0,3), ('100',100,0,3),
    ('32767',32767,0,3), ('32768',32768,0,3), ('65534',65534,0,3),
    ('65535',65535,0,3), ('taken',8,1,3), ('flag2',8,2,3),
    ('direction4',8,0,4), ('direction255',8,0,255),
]:
    caller = bytearray(original); resident = bytearray(0x30000)+original
    def install(address, body):
        data=struct.pack('<%dH'%len(body),*body)
        resident[address:address+len(data)]=data
        if address>=0x30000: caller[address-0x30000:address-0x30000+len(data)]=data
    for tag,(address,nargs) in enumerate([(0x2fe15a,0),(0x2f6ab8,0),(0x22c370,1),
                                         (0x3407c,4),(0x34080,4),(0x34050,3)],1):
        entry=0x1800+tag*0x80
        install(0x30000+entry,boundary(tag,nargs))
        install(address,[0xad03,entry])
    # The controller's real epilogue restores these eight registers.
    install(0x31200,[0x2e14]+[0x58f0|r for r in range(8)]+[0xad1a,0x141e])
    code=[0x6800,2,0x68ff,0x6000,0x6810,0xa4]
    for address,value in [(0x69d4,direction<<8),(0x69d6,4<<8),(0x7a2e,0x0700),
                           (0x7a2c,0x1000),(0x7a56,0x2000),(0x742c+4,done),
                           (0x759e,6),(0x75a2,69),(0x75a4,0x1a14),
                           (0x6852,quantity),(0x797e,0x53),(0x7980,0x2000)]:
        write_word(code,address,value,banked=True)
    # First object's coordinates26,20; player27,20 faces left.
    write_word(code,0x69d4,(direction<<8)|7,banked=True)
    for i in range(1,8):
        write_word(code,0x759e+i*16,0,banked=True)
        write_word(code,0x75a4+i*16,0xffff,banked=True)
    code += [0x6810,0xa6]
    write_word(code,0x2534,0x1357,banked=True) # index20*32+26 in53:2000
    for address in range(0x1300,0x1540,2):write_word(code,address,0)
    code += [0xbd03,0x1200,0x68fc,0x600d]
    pc=len(code)*2;code += [0xd0ff]
    assert pc<0x1200
    caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    firmware=out/(name+'.prg');firmware.write_bytes(caller)
    rom=out/(name+'.rom');rom.write_bytes(resident)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,
                    boot_mode='firmware-vma03',start_paused=True)
        em.qmp.execute('cont');deadline=time.monotonic()+8;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(name+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:1500]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1500]
        def memory(address,size):return base64.b64decode(em.snapshot('data',address,size,diagnostics=False)['frame'])
        def word(address):return struct.unpack('<H',memory(address,2))[0]
        count=word(0x1300);order=list(struct.unpack('<%dH'%count,memory(0x1340,count*2))) if count else []
        acted=not done and direction<4
        signed=quantity if quantity<32768 else quantity-65536
        expected=(quantity+1)&65535 if acted and signed<99 else quantity
        row=dict(name=name,before=quantity,done_before=done,direction=direction,
                 after=word(0x526852),done_after=memory(0x527430,2)[0],tile=word(0x532534),
                 order=order,item_argument=word(0x1430),duration=word(0x1464),
                 inventory_at_calls=[word(0x1500+tag*2) for tag in order],
                 host_guest_memory_writes=0,passed=True)
        assert row['after']==expected and row['done_after']==(1 if acted else done),row
        assert row['tile']==(53 if acted else 0x1357),row
        assert order==([1,2,3,4,5,6] if acted else []),row
        if acted:assert row['item_argument']==69 and row['duration']==5000,row
        cases.append(row);print(json.dumps(row),flush=True)
    finally:
        if em:em.close()
report=dict(passed=True,cases=cases,original_prg_sha256=hashlib.sha256(original).hexdigest(),
            scope=__doc__,goal_complete=False)
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
