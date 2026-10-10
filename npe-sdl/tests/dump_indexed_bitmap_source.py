"""Reproduce generic indexed loader and production caller source evidence."""
from pathlib import Path
import sys
sys.path.insert(0,str(Path('scripts').resolve()))
from np560_disasm import decode
raw=Path('tmp/NP560.PRG').read_bytes()
ranges=[(0x480000,0x248),(0x31eb60,0x220),(0x4a82b0,0xa0),
        (0x4a90e0,0x90),(0x46bd80,0x7e),(0x46be2a,0x5a)]
lines=[]
for address,size in ranges:
    data=raw[address-0x200000:address-0x200000+size]
    words=[0]*((address&65535)//2)+[int.from_bytes(data[i:i+2],'little') for i in range(0,len(data),2)]
    cursor=(address&65535)//2;lines.append(f'--- physical {address:06X}+{size:X}; CODE {address-0x1d0000:06X}')
    while cursor<len(words):
        ins=decode(words,cursor,address>>16)
        lines.append(str(ins.address)+' '+' '.join(f'{w:04X}' for w in ins.words)+'  '+ins.text)
        cursor+=ins.size
Path('tmp/indexed-bitmap-source.asm').write_text('\n'.join(lines)+'\n',encoding='utf8')
print('Saved indexed loader, section metadata and original callers')
