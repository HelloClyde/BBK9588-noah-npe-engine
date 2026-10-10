"""Extract the four engine UI glyphs from the bundled native font."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
font = (root/'assets/gbk12-native.bin').read_bytes()
assert font[:8] == b'NPEFONT1'
codes = [(0xb6,0xbe),(0xc3,0xdf),(0xc2,0xd2),(0xb7,0xe2)]
lines = ['/* Original engine labels: poison, sleep, confusion, seal.',
         '   Byte-identical glyphs from assets/gbk12-native.bin (NPEFONT1). */',
         'static const uint8_t battle_status_glyphs[4][24] = {']
for lead,trail in codes:
    offset = 8+(95+(lead-0x81)*191+trail-0x40)*24
    bits = font[offset:offset+24]
    assert len(bits)==24
    lines.append('    {'+', '.join(f'0x{v:02x}' for v in bits)+'},')
lines.append('};')
(root/'battle_status_glyphs.inc').write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')
