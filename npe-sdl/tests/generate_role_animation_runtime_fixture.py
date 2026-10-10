import hashlib,json,struct
from pathlib import Path
report=json.loads(Path('tmp/map-bitmap-runtime-native/report.json').read_text(encoding='utf8'))
assert report['passed'] and len(report['cases'])==138
assert report['original_prg_sha256']==hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest()
assert report['npe_sha256']==hashlib.sha256(Path('tmp/tjfy-world-probe.npe').read_bytes()).hexdigest()
rows=[row for row in report['cases'] if row['input']['service']=='role'];assert len(rows)==20
lines=['/* Unmodified role loader, softfloat, rounding and draw. Clear=0. */',
       'typedef struct {const char *ram,*trace,*pixels;int16_t x,y;} RoleRuntimeFrame;',
       'typedef struct {const char *name;int16_t group,x,y,end_x,end_y;uint8_t kind;',
       ' uint32_t directory,payload;const char *ram,*trace,*pixels;',
       ' unsigned frames;const RoleRuntimeFrame *references;} RoleRuntimeCase;']
def validate(row,sizes):
    for label,size in sizes:
        b=Path(row[label]).read_bytes()
        assert len(b)==size and hashlib.sha256(b).hexdigest()==row[label+'_sha256']
for i,row in enumerate(rows):
    assert row['passed'] and row['stack_restored'] and not row['heap_headers_seeded'] and not row['draw_stubs']
    validate(row,[('ram',131072),('trace',10),('pixels',28800),('packed',14400)])
    if row['frames']:
        lines.append('static const RoleRuntimeFrame role_runtime_frames_%d[]={'%i)
        for frame in row['frames']:
            validate(frame,[('ram',131072),('trace',8),('pixels',28800)])
            lines.append(' {%s,%s,%s,%d,%d},'%tuple([json.dumps(frame[l]) for l in ['ram','trace','pixels']]+[frame['x'],frame['y']]))
        lines.append('};')
b=Path('tmp/tjfy-world-probe.npe').read_bytes()
directory=struct.unpack_from('>I',b,0x3a+6*8)[0];payload=struct.unpack_from('>I',b,0x3a+7*8)[0]
lines.append('static const RoleRuntimeCase role_runtime_cases[]={')
for i,row in enumerate(rows):
    c=row['input'];lines.append(' {%s,%d,%d,%d,%d,%d,%d,UINT32_C(%d),UINT32_C(%d),%s,%s,%s,%d,%s},'%(
        json.dumps(c['name']),c['number'],c['x'],c['y'],c['end_x'],c['end_y'],c['kind'],
        c.get('role_directory',directory),c.get('role_payload',payload),json.dumps(row['ram']),json.dumps(row['trace']),json.dumps(row['pixels']),
        len(row['frames']),'role_runtime_frames_%d'%i if row['frames'] else 'NULL'))
lines.append('};')
Path('npe-sdl/tests/role_animation_runtime_native.h').write_text('\n'.join(lines)+'\n',encoding='utf8')
print('Validated20 role cases with',sum(len(row['frames']) for row in rows),'independent original frame buffers/coordinates')
