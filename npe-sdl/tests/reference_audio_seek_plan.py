"""Original25:E002..E24A range seek with explicit codec/file boundaries.

The original conversions, branch choices, XOR/swap and08:37CE run intact.
Guest adapters supply reset/restore, seek and RAM read. Stop before prime.
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
out = Path('tmp/audio-seek-plan-native'); out.mkdir(exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
npe = Path('tmp/tjfy-world-probe.npe').read_bytes()

# A boundary records all three16-bit arguments after its far-pointer pair.
def codec_boundary(counter, record):
    return [0x2e00, 0x1f10, 0x58f2, 0x6810, 0,
            0x3f10, 0x88f2, counter, 0x5021, 0x3f10, 0xa8f2, counter,
            0x782b, 6, 0x3f10, 0xa8f2, record,
            0x782b, 8, 0x3f10, 0xa8f2, record+2,
            0x782b, 10, 0x3f10, 0xa8f2, record+4,
            0x382f, 0x1f90, 0x4800, 0x2f00, 0xc620]

reset = codec_boundary(0x1300, 0x1320)
restore = codec_boundary(0x1302, 0x1326)
seek = [0x2e00, 0x1f10, 0x58f2, 0x6810, 0,
        0x3f10, 0x88f2, 0x1304, 0x5021, 0x3f10, 0xa8f2, 0x1304]
for i in range(4):
    seek += [0x782b, 6+i*2, 0x3f10, 0xa8f2, 0x1330+i*2]
seek += [0x382f, 0x1f90, 0x4800, 0x2f00, 0xc620]
wrapper = [0x2e07]+[0x58f0|r for r in range(1, 6)]+[0xad25, 0xe002]
# Preserve the original frame and saved registers, then mark successful arrival.
finish = [0x6810, 0, 0x68f0, 1, 0x3f10, 0xa8f0, 0x1312,
          0x4800]+[0x380f|(r<<4) for r in reversed(range(1, 6))]+[0x2f00, 0xc620]

inputs = []
for section in [16, 17, 31, 36]:
    offset, size = struct.unpack_from('>2I', npe, 0x3a+section*8)
    count = int.from_bytes(npe[offset+2:offset+4], 'big')
    inputs.append((f'npe-section-{section}', size, 32, count*168, 32, offset, 0, 0, True))
for row in [
    ('before-header', 2000, 16, 8000, 32, 0x20000, 0, 0),
    ('header-equal', 2000, 32, 8000, 32, 0x20000, 0, 0),
    ('after-header-even', 2000, 48, 8000, 32, 0x20000, 1000, 0),
    ('after-header-odd-bit', 2000, 63, 8000, 32, 0x20000, 1000, 0),
    ('remaining-equal', 2000, 16000, 8000, 32, 0x20000, 0, 0),
    ('remaining-past', 2000, 16016, 8000, 32, 0x20000, 0, 0),
    ('clip-equal', 2000, 8000, 8000, 32, 0x20000, 1000, 0),
    ('clip-past', 2000, 8000, 12000, 32, 0x20000, 1000, 0),
    ('clip-before', 2000, 8000, 4000, 32, 0x20000, 500, 0),
    ('empty-range', 2000, 8000, 0, 32, 0x20000, 0, 0),
    ('zero-read', 2000, 48, 8000, 32, 0x20000, 0, 0),
    ('short-read', 2000, 48, 8000, 32, 0x20000, 8, 0),
    ('odd-read-xor', 2000, 63, 8000, 32, 0x20000, 5, 0x5a),
    ('exact-window', 9000, 48, 8192, 32, 0x20000, 1024, 0),
    ('over-window', 9000, 48, 8208, 32, 0x20000, 1024, 0),
    ('wide-start', 0x2000000, 0x80000030, 8000, 32, 0x20000, 1000, 0),
    ('wide-length', 9000, 48, 0x80002000, 32, 0x20000, 1024, 0),
    ('absolute-wrap', 2000, 48, 8000, 32, 0xfffffffc, 1000, 0),
    ('zero-remaining', 0, 48, 8000, 32, 0x20000, 0, 0),
    ('wide-header', 9000, 8192, 8192, 65535, 0x20000, 0, 0),
]:
    inputs.append((*row, False))

cases = []
for name, remaining, start, length, header, base, returned, key, real in inputs:
    read = [0x2e00, 0x1f10, 0x1f11]+[0x58f0|r for r in range(2, 7)]
    read += [0x782b, 12, 0x786b, 16, 0x3f10, 0xa8f6, 0x1308,
             0x3f10, 0x88f5, 0x1306, 0x5051, 0x3f10, 0xa8f5, 0x1306,
             0x68f1, 0x9000, 0x68f4, returned, 0x6810, 1, 0x6811, 1,
             0xff72, 0x0834, 0x5640, 0xac01, 0]
    skip = len(read)-1
    read += [0x5231, 0xcf03, 0x5b21]
    read[skip] = 0x1000+len(read)*2
    read += [0x0814]+[0x380f|(r<<4) for r in reversed(range(2, 7))]+[0x1f91, 0x1f90, 0x4800, 0x2f00, 0xc620]
    adapters = [(0x1000, read), (0x1100, reset), (0x1200, restore), (0x1400, finish), (0x1500, wrapper), (0x1600, seek)]
    patches = [(0x1cccd4, 0x1000), (0x1b6de, 0x1100), (0x1b84e, 0x1200), (0x25e24c, 0x1400), (0x1bc5ec, 0x1600)]
    resident = bytearray(0x30000)+original
    caller = bytearray(original)
    for address, entry in patches:
        resident[address:address+4] = struct.pack('<2H', 0xad03, entry)
        if address >= 0x30000:
            caller[address-0x30000:address-0x30000+4] = struct.pack('<2H', 0xad03, entry)
    for entry, body in adapters:
        data = struct.pack('<%dH'%len(body), *body)
        resident[0x30000+entry:0x30000+entry+len(data)] = data
        caller[entry:entry+len(data)] = data
    code = [0x6800, 2, 0x68ff, 0x6000, 0x6810, 1,
            0x68f1, 0x800, 0x68f0, 0x5aa5, 0xbf00, 511, 0x1a10,
            0x68f1, 0x1000, 0x68f0, 0x1234, 0xbf00, 511, 0x1a10,
            0x6810, 0xa2]
    for address, value in [(0x29fc, remaining>>16), (0x29fe, remaining&65535), (0x2838, header),
                           (0x2a1c, 0), (0x2a1e, 0x2400), (0x2842, 0), (0x2844, 1234)]:
        write_word(code, address, value, banked=True)
    for address in range(0x1300, 0x1314, 2):
        write_word(code, address, 0)
    for address in range(0x1320, 0x1338, 2):
        write_word(code, address, 0xffff)
    write_word(code, 0x4bb6, key<<8)
    for value in [length&65535, length>>16, start&65535, start>>16, base&65535, base>>16]:
        code += [0x68f0, value, 0x58f0]
    code += [0xbd03, 0x1500, 0x50fc, 0x3f10, 0xa8f0, 0x1310, 0x68fc, 0x600d]
    pc = len(code)*2; code += [0xd0ff]
    caller[:len(code)*2] = struct.pack('<%dH'%len(code), *code)
    rom = out/(name+'.rom'); rom.write_bytes(resident)
    firmware = out/(name+'.prg'); firmware.write_bytes(caller)
    em = None
    try:
        em = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom, soc_rom_skeleton=True, boot_mode='firmware-vma03', start_paused=True)
        for address, entry in patches:
            assert base64.b64decode(em.snapshot('code', address, 4, diagnostics=False)['frame']) == struct.pack('<2H', 0xad03, entry)
        em.qmp.execute('cont'); deadline = time.monotonic()+8; state = ''
        while time.monotonic() < deadline:
            state = em.qmp.execute('qom-get', {'path':'/machine', 'property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():
                break
            time.sleep(.01)
        em.qmp.execute('stop'); (out/(name+'.cpu.txt')).write_text(state, encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(), state[:1500]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state, state[:1500]
        regs = {int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})', '\n'.join(state.splitlines()[:5]), re.I)}
        assert regs[12] == 0x600d and regs[15] == 0x6000, regs
        def memory(address, size):
            return base64.b64decode(em.snapshot('data', address, size, diagnostics=False)['frame'])
        def word(address):
            return struct.unpack('<H', memory(address, 2))[0]
        def pair(address):
            hi, lo = struct.unpack('<2H', memory(address, 4)); return hi<<16|lo
        window = memory(0x8800, 1024); (out/(name+'-window.bin')).write_bytes(window)
        row = dict(name=name, remaining=remaining, start_bits=start, length_bits=length, header_bits=header,
                   base=base, supplied_return=returned, key=key, real_npe_parameters=real,
                   start_bytes=pair(0x5ff8), length_bytes=pair(0x5ffc),
                   final_remaining=pair(0x5129fc), reset_calls=word(0x1300), restore_calls=word(0x1302),
                   seek_calls=word(0x1304), read_calls=word(0x1306), read_request=word(0x1308),
                   reset_arguments=list(struct.unpack('<3H', memory(0x1320,6))),
                   restore_arguments=list(struct.unpack('<3H', memory(0x1326,6))),
                   seek_arguments=list(struct.unpack('<4H', memory(0x1330,8))),
                   returned=word(0x1310), reached_prime=word(0x1312),
                   window_sha256=hashlib.sha256(window).hexdigest(), stack_restored=True,
                   host_guest_memory_writes=0, passed=True)
        assert row['reset_calls']+row['seek_calls'] == 1, row
        assert row['seek_calls'] == row['read_calls'], row
        assert row['restore_calls'] == (row['seek_calls'] and returned > 0), row
        assert row['reached_prime'] == (row['reset_calls'] or returned > 0), row
        cases.append(row); print(json.dumps(row), flush=True)
    finally:
        if em:
            em.close()
report = dict(passed=len(cases)==len(inputs), cases=cases,
              original_prg_sha256=hashlib.sha256(original).hexdigest(),
              qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(), npe_sha256=hashlib.sha256(npe).hexdigest(),
              scope='original range conversions/seek decisions/window XOR/swap; diagnostic B6DE/B84E and file seek/read boundaries; stop before initial prime/PCM')
(out/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
