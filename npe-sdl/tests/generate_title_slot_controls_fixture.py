import json
from pathlib import Path

root=Path('tmp/title-slot-controls/original')
rows=json.loads((root/'report.json').read_text(encoding='utf-8'))
keys=['mode','event','init','slot','selection','stage','map','progress']
state=lambda s:'{'+','.join(str(s[k]) for k in keys)+'}'
out=['/* Ordinary inputs in the unmodified original title and slot chooser. */',
     'typedef struct {'+';'.join('unsigned '+k for k in keys)+';} TitleSlotState;',
     'typedef struct {const char *label;unsigned mask,frames;TitleSlotState after;} TitleSlotPhase;']
for row in rows:
    out.append('static const TitleSlotPhase title_slot_'+row['name']+'[]={')
    for p in row['phases']:
        for i in range(len(p['commits'])):
            assert len((root/f"{row['name']}-{p['label']}-{i}.idx").read_bytes())==28800
        out.append('{'+json.dumps(p['label'])+','+str(p['mask'])+','+str(len(p['commits']))+','+state(p['after'])+'},')
    out.append('};')
out.extend(['typedef struct {const char *name;unsigned count;const TitleSlotPhase *phases;} TitleSlotCase;',
            'static const TitleSlotCase title_slot_cases[]={'])
for row in rows:
    out.append('{'+json.dumps(row['name'])+','+str(len(row['phases']))+',title_slot_'+row['name']+'},')
out.append('};')
Path('npe-sdl/tests/title_slot_controls_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
print('Generated title slot controls:',sum(len(p['commits']) for r in rows for p in r['phases']),'original frames')
