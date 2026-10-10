"""Build controller numeric fixtures and a separately composed source UI oracle.

The notice oracle comes from firmware strings/geometry and extracted fonts.
It is not a captured original pickup frame.
"""
import json, struct
from pathlib import Path
report=json.loads(Path('tmp/object-pickup-native/report.json').read_text(encoding='utf8'))
assert report['passed'] and len(report['cases'])==12
rows=report['cases']
header='/* Original isolated controller, explicit UI boundaries. */\nstatic const PickupCase pickup_cases[]={\n'
for c in rows:
    header+='{"%s",%d,%d,%d,%d,%d,%d},\n'%(c['name'],c['before'],c['done_before'],c['direction'],c['after'],c['done_after'],bool(c['order']))
Path('npe-sdl/tests/object_pickup_native.h').write_text(header+'};\n',encoding='utf8')
raw=Path('tmp/NP560.PRG').read_bytes();p=0x300000+0x6e0a
prefix=b''.join(raw[i:i+2][::-1] for i in range(p,p+16,2)).split(b'\0')[0]
assert prefix==bytes.fromhex('b5c3b5bd20')
npe=Path('tmp/tjfy-world-probe.npe').read_bytes();section=struct.unpack_from('>I',npe,0x3a+16)[0]
record=section+struct.unpack_from('>I',npe,section+68*4)[0]
name=npe[record:record+12].split(b'\0')[0]
message=prefix+name
font=Path('npe-sdl/assets/gbk16-native.bin').read_bytes();assert font[:8]==b'NPEF16V3' and len(font)==8+(95+94*94+33+32*191+87*97)*32
overlay=bytearray([255])*28800
left,right=120-len(message)*4-5,120+len(message)*4+5
for y in range(48,73):
    for x in range(left,right+1):
        overlay[y*240+x]=0 if y in (48,72) or x in (left,right) else 15 if y in (49,71) or x in (left+1,right-1) else 5
cursor=left+5;p=0
while p<len(message):
    first=message[p]
    if first<128:index,width,advance=first-32,8,1
    else:index,width,advance=95+(first-161)*94+message[p+1]-161,16,2
    bits=font[8+index*32:8+(index+1)*32]
    for y in range(16):
        line=int.from_bytes(bits[y*2:y*2+2],'big')
        for x in range(width):
            if line&(32768>>x):overlay[(52+y)*240+cursor+x]=15
    cursor+=width;p+=advance
Path('tmp/object-pickup-notice-overlay.bin').write_bytes(overlay)
# Diagnostic resource with45 story cells and eight distinct object cells.
# Expand map80's record and relocate subsequent section/record offsets.
fixture=bytearray(npe);section8,section_size=struct.unpack_from('>2I',fixture,0x3a+8*8)
map_count=struct.unpack_from('>I',fixture,section8)[0]//4
map_start=section8+struct.unpack_from('>I',fixture,section8+79*4)[0]
width,height=fixture[map_start+2:map_start+4];tail=map_start+17+width*height*2
directory=tail+652;assert fixture[directory+2]==0
for i in range(17):fixture[directory+3*i:directory+3*i+2]=b'\0\0'
fixture[directory+2]=3
for i in range(8):
    object_start=tail+i*10
    struct.pack_into('>HH',fixture,object_start,i+1,69)
    fixture[object_start+6:object_start+8]=bytes([26-i,20])
data=b''.join(struct.pack('>HB',1,15)+b''.join(struct.pack('>HH',i*15+j,10) for j in range(15)) for i in range(3))
for i in range(80,map_count):
    pos=section8+i*4;struct.pack_into('>I',fixture,pos,struct.unpack_from('>I',fixture,pos)[0]+len(data))
for i in range(100):
    pos=0x3a+i*8;offset,size=struct.unpack_from('>2I',fixture,pos)
    if i==8:struct.pack_into('>I',fixture,pos+4,size+len(data))
    elif offset>section8:struct.pack_into('>I',fixture,pos,offset+len(data))
fixture[tail+703:tail+703]=data
Path('tmp/object-pickup-capacity.npe').write_bytes(fixture)
print(json.dumps({'cases':len(rows),'notice_hex':message.hex(),'scope':__doc__},ensure_ascii=True))
