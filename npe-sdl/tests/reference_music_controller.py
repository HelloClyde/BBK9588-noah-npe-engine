"""Original music entry/confirmation/map-init slices; playback/drawing boundaries.

No full controller/file/codec/physical timing claim. Guest instructions set RAM.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
sys.path.insert(0, str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word
p = argparse.ArgumentParser(); p.add_argument('--qemu', type=Path, required=True); args = p.parse_args()
out = Path('tmp/music-controller-native'); out.mkdir(exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes(); cases = []; inputs = []
for enabled in (0, 1, 2, 0x100, 0x101, 0xffff):
    inputs.append((f'begin-{enabled}', 0, enabled, 0xaabb, 0x99))
    for choice in (0, 1, 2, 255): inputs.append((f'confirm-{enabled}-{choice}', 1, enabled, 0xaabb, choice))
    for pending in (0, 1, 2, 0x100, 0xffff): inputs.append((f'init-{enabled}-{pending}', 2, enabled, pending, 0x99))
for choice in (0, 1, 2, 127, 128, 255): inputs.append((f'move-{choice}', 3, 1, 0xaabb, choice))
for name, operation, enabled, pending, choice in inputs:
    initial = {0x7934:6, 0x7936:0x8182, 0x7a50:enabled, 0x7ab4:pending,
               0x7a54:0xa755, 0x7a8a:(choice << 8) | 0x92, 0x7a04:0x281}
    for i in range(3):
        initial.update({0x78f2+4*i:0x1122+i, 0x78f4+4*i:0x3344+i,
                        0x78fe+4*i:0x5566+i, 0x7900+4*i:0x7788+i})
    code = [0x6800,2,0x68ff,0x6000,0x6810,0]
    for a,v in ((0x1600,0),(0x1620,0)): write_word(code,a,v)
    for a in range(0x1602,0x1620,2): write_word(code,a,0xdead)
    for a,v in sorted(initial.items()):
        code += [0x6810,0xa4+(a >> 15)]; write_word(code,a & 0x7fff,v,banked=True)
    code += [0xbd19 if operation==3 else 0xbd1a,(0xcf5a,0xcfde,0x1324,0x4ce4)[operation],0x68fc,0x600d]
    pc = len(code)*2; code += [0xd0ff]
    play = [0x2e00,0x58f0,0x3f10,0x88f0,0x1600,0x5001,0x3f10,0xa8f0,0x1600]
    for i in range(12): play += [0x780b,6+2*i,0x3f10,0xa8f0,0x1602+2*i]
    for a,dst in ((0x7a50,0x161a),(0x7934,0x161c),(0x7ab4,0x161e)):
        play += [0x6810,0xa4,0x88f0,a,0x3f10,0xa8f0,dst]
    play += [0x380f,0x2f00,0xc620]
    stop = [0x58f0,0x3f10,0x88f0,0x1620,0x5001,0x3f10,0xa8f0,0x1620]
    for a,dst in ((0x7a50,0x161a),(0x7934,0x161c),(0x7ab4,0x161e)):
        stop += [0x6810,0xa4,0x88f0,a,0x3f10,0xa8f0,dst]
    stop += [0x380f,0xc620]
    caller = bytearray(original); caller[:len(code)*2] = struct.pack('<%dH'%len(code),*code)
    patches = [(0x438c,[0xad03,0x1800]),(0x4398,[0xad03,0x1900]),
               (0x1800,play),(0x1900,stop),(0x17d57c,[0xc620]),
               (0x17136a,[0xc620]),(0x17136e,[0xc620]),
               (0x2f1ffa,[0xc620]),(0x4188,[0xc620]),(0x4224,[0xc620]),(0x2c6ab8,[0xc620]),
               (0x302ff2,[0xc620]),(0x1651a2,[0xc620])]
    for a,body in patches: caller[a:a+len(body)*2] = struct.pack('<%dH'%len(body),*body)
    firmware = out/(name+'.prg'); firmware.write_bytes(caller); em = None
    try:
        em = Emulator(qemu=args.qemu,firmware=firmware,boot_mode='firmware-vma03',start_paused=True)
        for cpu,start,end in ((0x1acf5a,0x17cf5a,0x17cfa4),(0x1acfde,0x17cfde,0x17d02c),
                              (0x1a1324,0x171324,0x17136a),(0x31a3f4,0x2ea3f4,0x2ea498),
                              (0x194ce4,0x164ce4,0x164d00)):
            assert base64.b64decode(em.snapshot('code',cpu,end-start,diagnostics=False)['frame']) == original[start:end]
        em.qmp.execute('cont'); deadline = time.monotonic()+8; state = ''
        while time.monotonic() < deadline:
            state = em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower(): break
            time.sleep(.01)
        em.qmp.execute('stop'); (out/(name+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower() and all(k+'=none' in state for k in ('ROM_MISSING','UNSUPPORTED','INVALID_CONTROL')), state[:1500]
        regs = {int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12] == 0x600d and regs[15] == 0x6000
        def memory(a,n): return base64.b64decode(em.snapshot('data',a,n,diagnostics=False)['frame'])
        captured = list(struct.unpack('<17H',memory(0x1600,34)))
        calls,stops = captured[0],captured[16]
        assert calls == int((operation == 1 and choice == 1) or (operation == 2 and pending != 0 and enabled == 1))
        assert stops == int(operation == 1 and choice != 1)
        ram = b''.join(memory(a,0x8000) for a in range(0x510000,0x530000,0x8000))
        guest = bytearray(ram); guest[0::2],guest[1::2] = ram[1::2],ram[0::2]
        (out/(name+'.ram.bin')).write_bytes(guest)
        row = dict(name=name,operation=operation,enabled=enabled,pending=pending,choice=choice,
                   initial_words=sorted(initial.items()),calls=calls,stops=stops,arguments=captured[1:13],
                   observed=captured[13:16],passed=True,stack_restored=True,host_guest_memory_writes=0)
        cases.append(row); print(json.dumps({k:row[k] for k in ('name','calls','stops','observed')}),flush=True)
    finally:
        if em: em.close()
(out/'report.json').write_text(json.dumps(dict(passed=len(cases)==66,cases=cases,
    original_prg_sha256=hashlib.sha256(original).hexdigest(),qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
    scope='unchanged entry/confirmation/map-init slices with play/stop/drawing capture boundaries;no full controller/file/codec/physical timing proof'),indent=2)+'\n',encoding='utf8')
