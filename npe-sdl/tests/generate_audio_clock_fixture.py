"""Freeze every original PRG clock checkpoint, rather than recomputing it."""
import json
from pathlib import Path

report=json.loads(Path('tmp/audio-clock-native/report.json').read_text())
assert report['passed'] and not report['codec_executed']
rows=report['cases'];assert len(rows)==22
out=['/* Unchanged PRG clock routines executed on the DMCU reference.',
     '   No codec, IRQ, physical timing or silicon overflow proof. */',
     'static const AudioClockNative audio_clock_native[]={']
for row in rows:
    assert row['passed'] and row['stack_restored'] and not row['host_guest_memory_writes']
    labels=['before']+(['configured'] if row['configure'] else [])
    labels += [f'advance{i}' for i in range(row['advances'])]+['add-ms']
    labels += [f'retract{i}' for i in range(row['retracts'])]
    assert [t['label'] for t in row['trace']]==labels and len(labels)<=8
    trace='{'+','.join('{UINT64_C(%d),UINT32_C(%d)}'%(t['value'],t['increment']) for t in row['trace'])+'}'
    out.append('{'+json.dumps(row['name'])+','+','.join(str(int(row[k])) for k in
        ['configure','rate','words','advances','retracts','add_ms','milliseconds'])+',UINT64_C(%d),UINT32_C(%d),%d,%s},'%(
        row['value'],row['increment'],len(labels),trace))
out.append('};')
Path('npe-sdl/tests/audio_clock_native.h').write_text('\n'.join(out)+'\n')
print(f'{len(rows)} PRG clock paths, {sum(len(r["trace"]) for r in rows)} checkpoints')
