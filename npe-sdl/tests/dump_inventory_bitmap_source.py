from pathlib import Path
import sys
sys.path.insert(0,str(Path('scripts').resolve()))
from np560_disasm import decode
raw=Path('tmp/NP560.PRG').read_bytes();lines=[]
for address,size in [(0x31ea14,0x98),(0x3f0dc4,0x68),(0x2f5490,0x58),
                     (0x2f59ba,0x66),(0x3b60cc,0xb4),(0x33ef58,0x34),
                     (0x33f1ae,0x7a)]:
    data=raw[address-0x200000:address-0x200000+size]
    words=[0]*((address&65535)//2)+[int.from_bytes(data[i:i+2],'little') for i in range(0,len(data),2)]
    cursor=(address&65535)//2;lines.append(f'--- physical {address:06X}+{size:X}; CODE {address-0x1d0000:06X}')
    while cursor<len(words):
        ins=decode(words,cursor,address>>16)
        lines.append(str(ins.address)+' '+' '.join(f'{w:04X}' for w in ins.words)+' '+ins.text)
        cursor+=ins.size
Path('tmp/inventory-bitmap-source.asm').write_text('\n'.join(lines)+'\n',encoding='utf8')
print('Saved original group initialization, inventory cursor/icons and target/list portrait draws')
