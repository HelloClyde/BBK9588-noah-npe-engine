from pathlib import Path
import sys
sys.path.insert(0,str(Path('scripts').resolve()))
from np560_disasm import decode
raw=Path('tmp/NP560.PRG').read_bytes();lines=[]
for address,size in [(0x31e8ae,0x21a),(0x4f1ffa,0x84),(0x3d8d38,0x40),
                     (0x381238,0x96),(0x38170e,0x36),(0x490170,0xa0),
                     (0x364b62,0x116)]:
    data=raw[address-0x200000:address-0x200000+size]
    words=[0]*((address&65535)//2)+[int.from_bytes(data[i:i+2],'little') for i in range(0,len(data),2)]
    cursor=(address&65535)//2;lines.append(f'--- physical {address:06X}+{size:X}; CODE {address-0x1d0000:06X}')
    while cursor<len(words):
        ins=decode(words,cursor,address>>16)
        lines.append(str(ins.address)+' '+' '.join(f'{w:04X}' for w in ins.words)+' '+ins.text)
        cursor+=ins.size
Path('tmp/ui-bitmap-source.asm').write_text('\n'.join(lines)+'\n',encoding='utf8')
print('Saved group initializer and original menu/portrait draws')
