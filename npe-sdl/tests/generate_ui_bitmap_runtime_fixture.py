import hashlib,json,struct
from pathlib import Path
r=json.loads(Path('tmp/map-bitmap-runtime-native/report.json').read_text(encoding='utf8'))
assert r['passed'] and len(r['cases'])==138
assert r['original_prg_sha256']==hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest()
b=Path('tmp/tjfy-world-probe.npe').read_bytes();assert r['npe_sha256']==hashlib.sha256(b).hexdigest()
vectors=struct.unpack_from('>17H',b,24)
rows=[c for c in r['cases'] if 'ui_offset' in c['input']];assert len(rows)==22
lines=['/* Original indexed loader and draw, UI resource ordinals and guest group slots. */',
       'typedef struct {const char *name;uint16_t index,group,offset,base;int16_t x,y;uint8_t kind;',
       ' const char *ram,*trace,*pixels;} UiRuntimeCase;','static const UiRuntimeCase ui_runtime_cases[]={']
for row in rows:
    assert row['passed'] and row['stack_restored'] and not row['draw_stubs'] and not row['heap_headers_seeded']
    for label,size in [('ram',131072),('trace',10),('pixels',28800),('packed',14400)]:
        data=Path(row[label]).read_bytes();assert len(data)==size and hashlib.sha256(data).hexdigest()==row[label+'_sha256']
    c=row['input'];group=c['ui_group'];base=c.get('ui_base' if group==7 else 'portrait_base',vectors[group])
    lines.append(' {%s,%d,%d,%d,%d,%d,%d,%d,%s,%s,%s},'%tuple([json.dumps(c['name']),c['number'],group,c['ui_offset'],base,c['x'],c['y'],c['kind']]+[json.dumps(row[l]) for l in ['ram','trace','pixels']]))
lines.append('};');Path('npe-sdl/tests/ui_bitmap_runtime_native.h').write_text('\n'.join(lines)+'\n',encoding='utf8')
print('Validated22 independent original UI/portrait/panel full buffers')
