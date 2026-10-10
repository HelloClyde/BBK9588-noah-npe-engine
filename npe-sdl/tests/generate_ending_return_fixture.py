import json
from pathlib import Path

rows=json.loads(Path('tmp/ending-return/original/report.json').read_text())
keys=['mode','event','init','map','progress','sx','sy','vx','vy','direction',
      'titleSlot','titleSelection','titleStage','mapHandle','battleHandle','battleFlags']
out=['/* Original live mode21 -> title -> arrows -> new-game captures. */',
     'typedef struct {'+','.join('unsigned '+k for k in keys).replace(',unsigned ',';unsigned ')+';} EndingState;',
     'typedef struct {const char *label;unsigned frames;EndingState after;} EndingPhase;',
     'typedef struct {unsigned music;int load;const char *kind;EndingState before;uint8_t npc[280];uint16_t viewport[105];EndingPhase phases[4];} EndingCase;',
     'static const EndingCase ending_cases[]={']
state=lambda s:'{'+','.join(str(s[k]) for k in keys)+'}'
for row in rows:
    assert len(row['phases'])==4
    for p in row['phases']:
        for i in range(len(p['commits'])):
            assert len(Path(f"tmp/ending-return/original/{row['music']}-{row['kind']}-{p['label']}-{i}.idx").read_bytes())==28800
    phases='{'+','.join('{'+json.dumps(p['label'])+','+str(len(p['commits']))+','+state(p['after'])+'}' for p in row['phases'])+'}'
    before=row['before']
    out.append('{'+str(row['music'])+','+str(int(row['kind']=='load'))+','+json.dumps(row['kind'])+','+state(before)+',{'+','.join(map(str,before['npc']))+'},{'+','.join(map(str,before['viewport']))+'},'+phases+'},')
out.append('};')
Path('npe-sdl/tests/ending_return_native.h').write_text('\n'.join(out)+'\n')
print('Generated four original ending/title/new-game or occupied-slot paths,24 complete frames')
