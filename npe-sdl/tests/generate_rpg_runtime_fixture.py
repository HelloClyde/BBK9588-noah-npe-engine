import hashlib,json
from pathlib import Path
import struct
report=json.loads(Path('tmp/map-bitmap-runtime-native/report.json').read_text(encoding='utf8'))
assert report['passed'] and len(report['cases'])==138
report['cases']=[row for row in report['cases'] if row['input']['service'] in ('battle','indexed') and 'ui_offset' not in row['input'] and 'inventory_offset' not in row['input'] and 'map_offset' not in row['input']]
assert len(report['cases'])==29
assert report['original_prg_sha256']==hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest()=='d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
assert report['npe_sha256']==hashlib.sha256(Path('tmp/tjfy-world-probe.npe').read_bytes()).hexdigest()=='df4b52df6b69e1105a5ea6100e43f35f832ed859c10cb8714879e44ad4d8cb40'
lines=['/* Original allocation -> file loader -> draw full buffers. */',
       'typedef struct {const char *name;int16_t number,kind,x,y;unsigned indexed;',
       ' uint32_t directory_size,directory,payload;',
       ' const char *ram,*trace,*pixels;} RpgRuntimeCase;',
       'static const RpgRuntimeCase rpg_runtime_cases[]={']
for row in report['cases']:
    assert row['passed'] and row['stack_restored'] and not row['draw_stubs'] and not row['heap_headers_seeded']
    for label,size in [('ram',131072),('trace',10),('pixels',28800),('packed',14400)]:
        data=Path(row[label]).read_bytes();assert len(data)==size and hashlib.sha256(data).hexdigest()==row[label+'_sha256']
    c=row['input'];npe=Path('tmp/tjfy-world-probe.npe').read_bytes()
    directory,directory_size=struct.unpack_from('>2I',npe,0x3a)
    payload=struct.unpack_from('>I',npe,0x42)[0]
    lines.append(' {%s,%d,%d,%d,%d,%d,UINT32_C(%d),UINT32_C(%d),UINT32_C(%d),%s,%s,%s},'%(
        json.dumps(c['name']),c['number'],c['kind'],c['x'],c['y'],c['service']=='indexed',
        c.get('directory_size',directory_size),c.get('directory',directory),c.get('payload',payload),
        json.dumps(row['ram']),json.dumps(row['trace']),json.dumps(row['pixels'])))
lines.append('};')
Path('npe-sdl/tests/rpg_runtime_native.h').write_text('\n'.join(lines)+'\n',encoding='utf8')
print('Validated29 original allocations/file loaders/actual draw references')
