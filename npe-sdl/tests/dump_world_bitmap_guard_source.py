from pathlib import Path
import sys
sys.path.insert(0,str(Path('scripts').resolve()))
from np560_disasm import decode

raw=Path('tmp/NP560.PRG').read_bytes();lines=[]
for address,size in [(0x4b25a2,0x120),(0x46bd8a,0xf0),(0x31e936,0x17a)]:
    bank=address&~0xffff;data=raw[bank-0x200000:bank-0x200000+0x10000]
    words=[int.from_bytes(data[i:i+2],'little') for i in range(0,len(data),2)]
    cursor=(address&65535)//2;stop=cursor+size//2
    lines.append(f'--- physical {address:06X}+{size:X}; CODE {address-0x1d0000:06X}')
    while cursor<stop:
        ins=decode(words,cursor,address>>16)
        lines.append(str(ins.address)+' '+' '.join(f'{w:04X}' for w in ins.words)+' '+ins.text)
        cursor+=ins.size
Path('tmp/world-bitmap-guard-source.asm').write_text('\n'.join(lines)+'\n',encoding='utf8')
print('Saved original NPC/player draw and header group initialization')
