"""Run unmodified input compaction/refill services on diagnostic RAM buffers.

No codec or file producer is called. Original 2A:87EA, 2C:0D72/0DCA and
31:0276 run in full; 2F:66F2 then runs with output halves disabled to inspect
the actual EOF comparison against the resulting input end pointer.
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
out = Path('tmp/audio-input-refill-native'); out.mkdir(exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
image = bytearray(0x30000) + original
rom = out/'original-services.rom'; rom.write_bytes(image)
cases = []
for name, consumed, active, next_words, debt, frame_bits, flags8a, file_remaining, eof_delta, output_words, sticky in [
    ('no-consumption', 0, 8, 6, 0, 168, 0, 1000, 0, 320, False),
    ('active-only', 4, 8, 6, 0, 168, 0, 1000, 0, 320, False),
    ('switch-partial', 10, 4, 16, 0, 168, 0, 1000, 0, 320, False),
    ('switch-other-bank', 10, 4, 16, 0, 168, 0x800, 1000, 0, 320, False),
    ('exact-exhaustion', 10, 4, 6, 0, 168, 0, 0, 0, 320, False),
    ('past-tail', 10, 4, 6, 0, 168, 0, 0, 1, 320, False),
    ('before-tail', 10, 4, 6, 0, 168, 0, 0, -1, 320, False),
    ('short-input', 10, 4, 2, 0, 168, 0, 0, 0, 320, False),
    ('carried-debt', 4, 3, 1, 3, 168, 0, 0, 0, 320, False),
    ('empty-next-buffer', 4, 0, 0, 0, 168, 0, 0, 0, 320, False),
    ('producer-marked', 10, 4, 6, 0, 168, 1, 1000, 0, 320, False),
    ('both-producers-busy', 10, 4, 6, 0, 168, 3, 1000, 0, 320, False),
    ('both-busy-enabled', 10, 4, 6, 0, 168, 3, 1000, 0, 320, False),
    ('wide-lookahead', 10, 4, 6, 0, 0x80000, 0, 0, 0, 320, False),
    ('zero-output', 0, 8, 6, 0, 168, 0, 1000, -1, 0, False),
    ('sticky-eof', 0, 8, 6, 0, 168, 0, 1000, -1, 320, True),
]:
    # Distinct window/source words expose both compaction and copy order.
    window = [0x1000+i for i in range(512)]
    sources = [[0x2000+i for i in range(32)], [0x3000+i for i in range(32)]]
    selected = 1 if flags8a & 0x800 else 0
    code = [0x6800, 2, 0x68ff, 0x6000, 0x6810, 1, 0x6811, 1]
    for i, value in enumerate(window): write_word(code, 0x800+2*i, value, banked=True)
    code += [0x6810, 0xa1]
    for bank, address in enumerate([0x5948, 0x6948]):
        for i, value in enumerate(sources[bank]): write_word(code, address+2*i, value, banked=True)
    code += [0x6810, 0xa2]
    metadata = {0x29dc:0x50, 0x29de:0xd948+0x1000*selected,
                0x29e0:0, 0x29e2:0x8c00, 0x29e4:0, 0x29e6:0x8800+consumed*2,
                0x29d0:frame_bits>>16, 0x29d2:frame_bits&65535,
                0x29fc:file_remaining>>16, 0x29fe:file_remaining&65535,
                0x2af4:active, 0x2af6:next_words if selected==0 else 0,
                0x2af8:next_words if selected==1 else 0, 0x2b0c:debt, 0x2ae6:output_words}
    for address, value in metadata.items(): write_word(code, address, value, banked=True)
    flags89 = 0x20 | (0x800 if sticky else 0) | (0x4010 if name=='both-busy-enabled' else 0)
    for address, value in [(0xf012, flags89), (0xf014, flags8a), (0xf01c, 0)]:
        write_word(code, address, value)
    code += [0xbd2a, 0x87ea]
    # Record refill state before original EOF check. The real B816 cursor
    # is unknown: explicitly supply tail-1/equal/tail+1 as guest diagnostics.
    code += [0x6810, 0xa2]
    for source, dest in [(0x29e0,0x2b20),(0x29e2,0x2b22)]:
        code += [0x88f0, source, 0xa8f0, dest]
    code += [0x88f0, 0x29e0, 0x88f1, 0x29e2]
    if eof_delta > 0: code += [0x5011, 0x5100]
    elif eof_delta < 0: code += [0x5211, 0x5300]
    code += [0xa8f0, 0x29e4, 0xa8f1, 0x29e6, 0xbd2f, 0x66f2,
             0x68fc, 0x600d]
    pc = len(code)*2; code += [0xd0ff]
    caller = bytearray(image[0x200000:]); caller[:len(code)*2] = struct.pack('<%dH'%len(code), *code)
    firmware = out/(name+'.prg'); firmware.write_bytes(caller)
    em = None
    try:
        em = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                      soc_rom_skeleton=True, start_paused=True)
        em.qmp.execute('cont'); deadline = time.monotonic()+8; state = ''
        while time.monotonic()<deadline:
            state = em.qmp.execute('qom-get', {'path':'/machine','property':'cpu-state'})
            if f'PC=20:{pc:04x}'.lower() in state.lower(): break
            time.sleep(.01)
        em.qmp.execute('stop'); (out/(name+'.cpu.txt')).write_text(state, encoding='utf-8')
        assert f'PC=20:{pc:04x}'.lower() in state.lower(), state[:1100]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state, state[:1100]
        regs = {int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})', '\n'.join(state.splitlines()[:5]), re.I)}
        assert regs[12] == 0x600d and regs[15] == 0x6000, regs
        def memory(address, size): return base64.b64decode(em.snapshot('data', address, size, diagnostics=False)['frame'])
        # Each data-bank selector covers 0x8000 bytes, not 0x10000.
        actual_window = memory(0x008800, 1024)
        actual_metadata = memory(0x5129d0, 0x2b24-0x29d0)
        word = lambda a: struct.unpack_from('<H', actual_metadata, a-0x29d0)[0]
        flags = struct.unpack('<3H', memory(0xf012, 6))
        assert word(0x29dc) == 0x50, (name, actual_metadata.hex())
        assert struct.unpack_from('<H', actual_window)[0] == window[consumed], name
        (out/(name+'.window.bin')).write_bytes(actual_window)
        row = dict(name=name, consumed_words=consumed, active_words=active,
                   next_words=next_words, debt=debt, frame_bits=frame_bits,
                   flags8a=flags8a, file_remaining=file_remaining, eof_delta=eof_delta,
                   output_words=output_words, initial_eof=sticky, initial_flags89=flags89,
                   window_words=list(struct.unpack('<512H', actual_window)),
                   source_pointer=[word(0x29dc),word(0x29de)],
                   remaining_words=word(0x2af4), pending_words=[word(0x2af6),word(0x2af8)],
                   debt_after=word(0x2b0c), tail=[word(0x2b20),word(0x2b22)],
                   final_flags89=flags[0], final_flags8a=flags[1],
                   stack_restored=True, host_guest_memory_writes=0, passed=True)
        assert bool(flags[0]&0x800) == (sticky or not output_words or eof_delta>0), row
        cases.append(row)
        print(json.dumps({k:row[k] for k in ['name','source_pointer','remaining_words','debt_after','tail','final_flags89','final_flags8a','passed']}),flush=True)
    finally:
        if em: em.close()
report = dict(passed=len(cases)==16, cases=cases,
              original_prg_sha256=hashlib.sha256(original).hexdigest(),
              qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
              scope='full original buffer compaction/refill/switch and EOF decision with diagnostic RAM/cursors; no codec, input file producer, actual B816 cursor, IRQ or resource reopen')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n', encoding='utf-8')
