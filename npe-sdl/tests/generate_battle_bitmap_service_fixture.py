"""Validate original-service snapshots and emit their independent input cases."""
import hashlib,json
from pathlib import Path

report=json.loads(Path('tmp/battle-bitmap-service-native/report.json').read_text(encoding='utf8'))
assert report['passed'] and len(report['cases'])==13
for field,path,known in [
 ('original_prg_sha256','tmp/NP560.PRG','d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'),
 ('npe_sha256','tmp/tjfy-world-probe.npe','df4b52df6b69e1105a5ea6100e43f35f832ed859c10cb8714879e44ad4d8cb40')]:
    assert report[field]==hashlib.sha256(Path(path).read_bytes()).hexdigest()==known
lines=['/* Original loader/heap snapshots; RAM IO and drawing are diagnostic boundaries. */',
 'typedef struct {uint32_t address;uint16_t flags;uint32_t next;} BitmapHeapNode;',
 'static const BitmapHeapNode bitmap_heap_nodes[]={']
for node in report['heap_chain']:
    assert hashlib.sha256(Path(node['json']).read_bytes()).hexdigest()==node['sha256']
    lines.append(f' {{UINT32_C(0x{node["address"]:x}),0x{node["flags"]:x},UINT32_C(0x{node["next"]:x})}},')
lines += ['};','typedef struct {const char *name;uint16_t number;int warm;uint8_t kind;',
          ' uint16_t first_cap,second_cap,payload_cap;int draw_handle;',
          ' const char *ram_file,*trace_file;} BitmapServiceCase;',
          'static const BitmapServiceCase bitmap_service_cases[]={']
for row in report['cases']:
    assert row['passed'] and row['stack_restored'] and row['original_loader_and_heap']
    for label,size in [('ram',0x20000),('trace',0x54)]:
        raw=Path(row[label]).read_bytes()
        assert len(raw)==size and hashlib.sha256(raw).hexdigest()==row[label+'_sha256']
    c=row['input']
    values=','.join(str(int(c[k])) for k in ['number','warm','kind','first_cap','second_cap','payload_cap','draw_handle'])
    lines.append(f' {{{json.dumps(c["name"])},{values},{json.dumps(row["ram"])},{json.dumps(row["trace"])}}},')
lines.append('};')
Path('npe-sdl/tests/battle_bitmap_service_native.h').write_text('\n'.join(lines)+'\n',encoding='utf8')
print('Validated thirteen original loader/heap snapshots: complete128KiB RAM and84-byte IO/draw records')
