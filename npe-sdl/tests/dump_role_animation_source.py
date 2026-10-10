from pathlib import Path
import sys
sys.path.insert(0,str(Path('scripts').resolve()))
from np560_disasm import decode
raw=Path('tmp/NP560.PRG').read_bytes()
lines=[]
for address,size in [(0x3c43d4,0x60e),(0x4e8756,0xae),(0x4975d6,0x1f2),
                     (0x4fbca6,0x100),(0x31ed76,0x120),(0x4fd218,0x48),
                     (0x4c52b4,0x360)]:
    data=raw[address-0x200000:address-0x200000+size]
    words=[0]*((address&65535)//2)+[int.from_bytes(data[i:i+2],'little') for i in range(0,len(data),2)]
    cursor=(address&65535)//2
    lines.append(f'--- physical {address:06X}+{size:X}; CODE {address-0x1d0000:06X}')
    while cursor<len(words):
        ins=decode(words,cursor,address>>16)
        lines.append(str(ins.address)+' '+' '.join(f'{w:04X}' for w in ins.words)+' '+ins.text)
        cursor+=ins.size
# DS is a 32KiB bank. DATA5F:2A44 is physical2FAA44 in the
# runtime image, corresponding to PRG file2CAA44 (physical4CAA44).
constants=bytearray(raw[0x2caa44:0x2caa54])
for i in range(0,len(constants),2):constants[i],constants[i+1]=constants[i+1],constants[i]
lines.append('DATA5F:2A44..2A53 canonical bytes '+constants.hex())
Path('tmp/role-animation-source-complete.asm').write_text('\n'.join(lines)+'\n',encoding='utf8')
print('Saved role service, drawing, coordinate rounding and resource initialization')
