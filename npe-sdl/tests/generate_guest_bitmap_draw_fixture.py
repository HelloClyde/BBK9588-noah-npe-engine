import hashlib,json
from pathlib import Path

report=json.loads(Path('tmp/guest-bitmap-draw-native/report.json').read_text(encoding='utf8'))
assert report['passed'] and len(report['cases'])==35
assert report['original_prg_sha256']==hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest()=='d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
lines=['/* Complete buffers from unmodified original draw routines. */',
       'typedef struct {const char *name;uint32_t address;uint16_t x,y,background;uint8_t kind,mask;',
       ' const char *source_hex,*packed,*pixels;} GuestBitmapDrawCase;',
       'static const GuestBitmapDrawCase guest_bitmap_draw_cases[]={']
for row in report['cases']:
    assert row['passed'] and row['stack_restored']
    for label,size in [('packed',14400),('pixels',28800)]:
        data=Path(row[label]).read_bytes()
        assert len(data)==size and hashlib.sha256(data).hexdigest()==row[label+'_sha256']
    c=row['input']
    lines.append(' {%s,0x%x,%d,%d,%d,%d,%d,%s,%s,%s},'%(json.dumps(c['name']),c['address'],c['x'],c['y'],c['background'],c['kind'],c['mask'],json.dumps(row['source_hex']),json.dumps(row['packed']),json.dumps(row['pixels'])))
lines.append('};')
Path('npe-sdl/tests/guest_bitmap_draw_native.h').write_text('\n'.join(lines)+'\n',encoding='utf8')
print('Validated35 complete original guest bitmap draw buffers')
