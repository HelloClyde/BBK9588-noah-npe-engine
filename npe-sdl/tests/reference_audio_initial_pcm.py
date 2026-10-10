"""Original transfer setup and initial prime through both PCM halves.

Codec input is the retained candidate section20 first frame, supplied explicitly
at B7AA. B816 and AD92 are diagnostic boundaries. This proves original outer
initialization/gain/clock order, not original codec, DAC output or timing.
"""
import argparse
import base64
import hashlib
import json
import re
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0,str(Path('scripts').resolve()))
from web.np560_web import Emulator
from np560_assembler import _DMCUAssembler


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu',type=Path,required=True)
    parser.add_argument('--output',type=Path,default=Path('tmp/audio-initial-pcm-native'))
    args=parser.parse_args();out=args.output;out.mkdir(parents=True,exist_ok=True)
    original=Path('tmp/NP560.PRG').read_bytes()
    assert hashlib.sha256(original).hexdigest()=='d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
    raw=Path('tmp/audio-input-prime-pcm/section-20.pcm').read_bytes()[:320]
    assert len(raw)==320
    (out/'candidate-first-frame.pcm').write_bytes(raw)
    cases=[]
    for name,current,target in [('steady',7,7),('up',0,7),('down',7,0),('mute',0,0)]:
        image=bytearray(0x30000)+original

        def install(address,words):
            image[address:address+len(words)*2]=struct.pack('<%dH'%len(words),*words)

        # B7AA fills exactly one explicit frame and counts calls.
        codec=[0x1f10,0x58f1,0x58f2,0x6810,0,0x88f0,0x1600,0x5001,0xa8f0,0x1600,
               0x68f1,0x2984]
        for sample in struct.unpack('<160H',raw):codec += [0x68f0,sample,0x1a10]
        codec += [0x382f,0x381f,0x1f90,0x68f0,160,0xc620]
        install(0x33100,codec)
        install(0x1b7aa,[0xad03,0x3100])
        # Cursor query and transfer remain explicit boundaries, as in the
        # original PCM service reference; no physical transfer is executed.
        query=[0x6810,0,0x88f0,0x1602,0x5001,0xa8f0,0x1602,
               0x6810,0xa2,0x4800,0xa8f0,0x2afc,0x4800,0x68f1,0x880a,0xc620]
        install(0x33700,query);install(0x1b816,[0xad03,0x3700])
        install(0x1b84e,[0xc620])
        transfer=[0x2e00,0x58f0,0x780b,14,0x4c01,0x58f0]
        for offset in (8,6,12,10):transfer += [0x780b,offset,0x58f0]
        transfer += [0xbd18,0x657a,0x50fa,0x380f,0x2f00,0xc620]
        install(0x33600,transfer);install(0x2ad92,[0xad03,0x3600])
        wrapper=[0x2e07]+[0x58f0|r for r in range(1,6)]+[0xad25,0xe24c]
        install(0x33800,wrapper)
        code=_DMCUAssembler(0x30000)
        code.emit(0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,0)

        def word(address,value):
            code.emit(0x6810,address>>15,0x68f0,value,0xa8f0,address&0x7fff)

        for address,value in [(0xf012,0x50),(0xf014,0),(0x4a60,32000),
            (0x4a64,current<<8|target),(0x1600,0),(0x1602,0),
            (0x5129fc,0),(0x5129fe,512),(0x512afa,160),(0x512b14,2),
            (0x512af8,0),(0x512af6,0),(0x512af4,0),
            (0x512adc,0),(0x512ade,0x1000),
            (0x4a52,0),(0x4a54,0),(0x4a56,0)]:word(address,value)
        # Original24:97AC publishes both requests itself, not a guest flag
        # injection. Pointers mirror08:2F22; timer/mode are diagnostic inputs.
        for value in (0x80,0,0,0x2504,0,0x1c04,0,0x2084,0,0x1784,0):
            code.emit(0x68f0,value,0x58f0)
        code.emit(0xbd24,0x97ac,0x70ff,22,0xbd03,0x3800,0x3f10,0xa8f0,0x1604,
                  0x68fc,0x600d)
        final=code.pc&65535;code.emit(0xd0ff);install(0x30000,code.finish())
        rom,firmware=out/(name+'.rom'),out/(name+'.prg')
        rom.write_bytes(image);firmware.write_bytes(image[0x30000:])
        em=None
        try:
            em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,
                        soc_rom_skeleton=True,boot_mode='firmware-vma03',start_paused=True)
            for start,end in [(0x2497ac,0x2498ec),(0x25e24c,0x25e324),
                (0x2f66f2,0x2f6834),(0x2e7fe4,0x2e815e),
                (0x8378a,0x838a4),(0x83536,0x835ac),(0x8372a,0x8378a),
                (0x18657a,0x18660a)]:
                actual=base64.b64decode(em.snapshot('code',start,end-start,diagnostics=False)['frame'])
                assert actual==image[start:end],hex(start)
            em.qmp.execute('cont');deadline=time.monotonic()+10
            while time.monotonic()<deadline:
                state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
                if f'PC=03:{final:04x}'.lower() in state.lower():break
                time.sleep(.01)
            em.qmp.execute('stop');(out/(name+'.cpu.txt')).write_text(state,encoding='utf-8')
            assert f'PC=03:{final:04x}'.lower() in state.lower(),state[:1500]
            assert all(k+'=none' in state for k in ['ROM_MISSING','UNSUPPORTED','INVALID_CONTROL']),state[:1500]
            regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})',state,re.I)}
            assert regs[15]==0x6000 and regs[12]==0x600d

            def memory(address,size):
                return base64.b64decode(em.snapshot('data',address,size,diagnostics=False)['frame'])

            def value(address):return struct.unpack('<H',memory(address,2))[0]
            fields={label:value(address) for label,address in [
                ('codec_calls',0x1600),('cursor_calls',0x1602),('returned',0x1604),
                ('flags8e',0xf01c),('flags8a',0xf014),('gain',0x4a64),
                ('reload0',0x4aac),('reload1',0x4aae),('transfer_words',0x4ab0),
                ('clock_high',0x4a52),('clock_mid',0x4a54),('clock_low',0x4a56),
                ('increment_high',0x4a58),('increment_low',0x4a5a),
                ('index',0x512b14),('half_samples',0x512afa),('output_words',0x512ae6)]}
            for half,address in enumerate((0x1784,0x2084)):
                data=memory(address,640);(out/f'{name}-half{half}.pcm').write_bytes(data)
                fields[f'half{half}_sha256']=hashlib.sha256(data).hexdigest()
            assert fields['codec_calls']==1 and fields['cursor_calls']==2 and fields['returned']==0,fields
            assert fields['reload0']==fields['reload1']==fields['output_words']==320,fields
            assert fields['flags8e']&0xc000==0 and fields['index']==2,fields
            assert (fields['clock_high'],fields['clock_mid'],fields['clock_low'])==(0,20,0),fields
            state_fields=['gain','flags8a','clock_high','clock_mid','clock_low',
                          'increment_high','increment_low','index','half_samples',
                          'output_words','reload0','reload1']
            (out/(name+'.state')).write_bytes(struct.pack('<12H',*[fields[k] for k in state_fields]))
            cases.append(dict(name=name,current=current,target=target,fields=fields,passed=True))
            print(json.dumps(cases[-1]),flush=True)
        finally:
            if em:em.close()
    report=dict(passed=True,program_sha256=hashlib.sha256(original).hexdigest(),
                candidate_frame_sha256=hashlib.sha256(raw).hexdigest(),cases=cases,
                codec_original=False,original_complete_initialization=False,scope=__doc__)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
