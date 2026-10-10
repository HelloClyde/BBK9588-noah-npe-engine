"""Validate independent original service snapshots and emit C inputs/paths."""
from pathlib import Path
import hashlib,json

definition=Path('npe-sdl/tests/resident_heap_lifecycle_cases.json')
inputs=json.loads(definition.read_text(encoding='utf8'))
report=json.loads(Path('tmp/resident-heap-lifecycle-native/report.json').read_text(encoding='utf8'))
assert report['passed'] and len(report['cases'])==len(inputs)==16
assert report['input_sha256']==hashlib.sha256(definition.read_bytes()).hexdigest()
assert report['original_prg_sha256']==hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest()
assert report['original_prg_sha256']=='d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
lines=['/* Independent original heap lifecycle: guest seeds and complete snapshots. */',
       'typedef struct {uint32_t address;uint16_t value;} HeapSeedWord;',
       'typedef struct {unsigned kind;uint16_t handle;} HeapOperation;',
       'typedef struct {const char *name;const HeapSeedWord *words;unsigned word_count;',
       ' const HeapOperation *operations;unsigned operation_count;const char *files[3];} HeapLifecycleCase;']
for index,(case,row) in enumerate(zip(inputs,report['cases'])):
    assert row['input']==case and row['name']==case['name']
    assert row['passed'] and row['stack_restored'] and row['service_code_unmodified']
    assert row['host_guest_memory_writes']==0
    for label,length in [('arena',0xf000),('bitmap',32),('limit',4)]:
        raw=Path(row['files'][label]).read_bytes()
        assert len(raw)==length and hashlib.sha256(raw).hexdigest()==row['sha256'][label]
    words=[(0x514a40+2*i,0xa000+i) for i in range(64)]+case['payload']
    for address,flags,next_pointer in case['nodes']:
        words.extend([(address,flags),(address+2,next_pointer>>16),(address+4,next_pointer&65535)])
    words.append((0x5127ea,case['bitmap']))
    lines.append(f'static const HeapSeedWord heap_seeds_{index}[]={{')
    lines.extend(f' {{UINT32_C(0x{address:x}),0x{value:x}}},' for address,value in words)
    lines.append('};')
    lines.append(f'static const HeapOperation heap_operations_{index}[]={{')
    lines.extend(f' {{{dict(unlock=0,release=1,compact=2)[operation]},{handle}}},' for operation,handle in case['operations'])
    lines.append('};')
lines.append('static const HeapLifecycleCase heap_lifecycle_cases[]={')
for i,(case,row) in enumerate(zip(inputs,report['cases'])):
    paths=','.join(json.dumps(row['files'][label]) for label in ['arena','bitmap','limit'])
    lines.append(f' {{{json.dumps(case["name"])},heap_seeds_{i},sizeof(heap_seeds_{i})/sizeof(*heap_seeds_{i}),')
    lines.append(f'  heap_operations_{i},sizeof(heap_operations_{i})/sizeof(*heap_operations_{i}),{{{paths}}}}},')
lines.append('};')
Path('npe-sdl/tests/resident_heap_lifecycle_native.h').write_text('\n'.join(lines)+'\n',encoding='utf8')
print('Validated',len(inputs),'full original arena/bitmap/limit cases')
