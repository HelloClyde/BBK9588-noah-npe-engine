import argparse,hashlib,json
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('--report',type=Path,default=Path('tmp/map-bitmap-runtime-native/report.json'))
parser.add_argument('--npe',type=Path,default=Path('tmp/tjfy-world-probe.npe'))
parser.add_argument('--output',type=Path,default=Path('npe-sdl/tests/scene_animation_runtime_native.h'))
args=parser.parse_args()
report=json.loads(args.report.read_text(encoding='utf8'))
assert report['passed'] and report['cases']
if args.report==Path('tmp/map-bitmap-runtime-native/report.json'):assert len(report['cases'])==138
assert report['original_prg_sha256']==hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest()
assert report['npe_sha256']==hashlib.sha256(args.npe.read_bytes()).hexdigest()
lines=['/* Unmodified scene loader/loop and draw with IO/presentation boundaries. */',
       'typedef struct {const char *ram,*trace,*pixels;} SceneRuntimeFrame;',
       'typedef struct {const char *name;int16_t group,x,y;uint8_t kind;',
       ' uint32_t directory,payload;const char *ram,*trace,*pixels;',
       ' unsigned frames;const SceneRuntimeFrame *references;} SceneRuntimeCase;']
rows=[row for row in report['cases'] if row['input']['service']=='scene']
assert rows
if args.report==Path('tmp/map-bitmap-runtime-native/report.json'):assert len(rows)==13
def validate(row,sizes):
    for label,size in sizes:
        b=Path(row[label]).read_bytes()
        assert len(b)==size and hashlib.sha256(b).hexdigest()==row[label+'_sha256']
for i,row in enumerate(rows):
    assert row['passed'] and row['stack_restored'] and not row['heap_headers_seeded'] and not row['draw_stubs']
    validate(row,[('ram',131072),('trace',10),('pixels',28800),('packed',14400)])
    if row['frames']:
        lines.append('static const SceneRuntimeFrame scene_runtime_frames_%d[]={'%i)
        for frame in row['frames']:
            validate(frame,[('ram',131072),('trace',8),('pixels',28800)])
            lines.append(' {%s,%s,%s},'%tuple(json.dumps(frame[l]) for l in ['ram','trace','pixels']))
        lines.append('};')
lines.append('static const SceneRuntimeCase scene_runtime_cases[]={')
import struct
b=args.npe.read_bytes()
directory=struct.unpack_from('>I',b,0x3a+13*8)[0];payload=struct.unpack_from('>I',b,0x3a+14*8)[0]
for i,row in enumerate(rows):
    c=row['input'];lines.append(' {%s,%d,%d,%d,%d,UINT32_C(%d),UINT32_C(%d),%s,%s,%s,%d,%s},'%(
        json.dumps(c['name']),c['number'],c['x'],c['y'],c['kind'],c.get('scene_directory',directory),c.get('scene_payload',payload),
        json.dumps(row['ram']),json.dumps(row['trace']),json.dumps(row['pixels']),len(row['frames']),
        'scene_runtime_frames_%d'%i if row['frames'] else 'NULL'))
lines.append('};')
args.output.parent.mkdir(parents=True,exist_ok=True)
args.output.write_text('\n'.join(lines)+'\n',encoding='utf8')
print('Validated',len(rows),'scene cases with',sum(len(row['frames']) for row in rows),'independent original frame buffers')
