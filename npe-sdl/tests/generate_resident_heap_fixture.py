from pathlib import Path
import json

rows=[]
for request in [3,35329,65535]:
    path=Path(f'tmp/native-battle-heap-fresh-{request}-short-header-34.json')
    data=json.loads(path.read_text(encoding='utf8'))
    assert data['request']==request
    assert len(data['samples'])==4
    sample=data['samples'][0]
    assert sample['trace'][3]==1 and sample['trace'][4]==request
    assert sample['allocatedHandle']==1
    assert sample['arenaEnd']==[0x52,0x3a40]
    assert sample['head'][0]==0x8001 and sample['bitmap0']==0xc000
    high,low=sample['trace'][:2]
    rows.append((request,sample['allocatedHandle'],high*65536+low,
                 sample['head'][0],sample['head'][1]*65536+sample['head'][2],
                 sample['bitmap0']))

out=['/* Independent original guest service samples after explicit heap reset.',
     '   Instrumented diagnostics, not unmodified game or full SDL traces. */',
     'typedef struct {uint16_t request,handle;uint32_t pointer;uint16_t flags;',
     ' uint32_t next;uint16_t bitmap;} ResidentHeapNativeCase;',
     'static const ResidentHeapNativeCase resident_heap_native_cases[]={']
for request,handle,pointer,flags,next_pointer,bitmap in rows:
    out.append(f' {{{request},{handle},UINT32_C(0x{pointer:x}),0x{flags:x},UINT32_C(0x{next_pointer:x}),0x{bitmap:x}}},')
out.append('};')
out.extend(['typedef struct {uint16_t request,flags,bitmap;uint32_t limit,pointer;} ResidentHeapTailCase;',
            'static const ResidentHeapTailCase resident_heap_tail_cases[]={'])
for request,name in [(3,'short-header-34'),(35329,'extended-31')]:
    data=json.loads(Path(f'tmp/native-battle-heap-prelock-{request}-{name}.json').read_text(encoding='utf8'))
    sample=data['samples'][0]
    assert sample['preTailFlag']==0x8000 and sample['preBitmap0']==0xfe00
    assert sample['allocatedHandle']==0 and sample['arenaEnd']==[0x52,0x3a40]
    high,low=sample['trace'][:2]
    assert high*65536+low==0x51e9c2
    out.append(f' {{{request},0x8000,0xfe00,UINT32_C(0x523a40),UINT32_C(0x51e9c2)}},')
out.append('};')
Path('npe-sdl/tests/resident_heap_native.h').write_text('\n'.join(out)+'\n',encoding='utf8')
print(f'Generated {len(rows)} fresh original cases and2 equivalent locked-tail cases')
