import json
import sys
from pathlib import Path
closing='--shop-close' in sys.argv
dialogue=closing or '--dialogue' in sys.argv
rows=json.loads(Path('tmp/native-shop-close.json' if closing else 'tmp/native-exit-dialogue.json' if dialogue else 'tmp/native-exit-choice.json').read_text(encoding='utf-8'))['rows']
def state(s):
    return '{'+','.join(str(s[k]) for k in ('mode','event','map','progress','direction','screenX','screenY','viewX','viewY','exitChoice','exitSlot','encounterSteps','rng'))+'}'
out=['/* Isolated original exit controller: actual LCD frames and resident state. */','static const ExitCase exit_cases[]={']
for r in rows:
    e=r['before']['exit']
    raw=e[:3]+e[4:10]+[e[10],e[11],e[13]]+e[14:21]+[e[21],e[23]]
    assert len(raw)==21
    out.append('{"'+r['name']+'",'+str(len(r['commits']))+','+state(r['before'])+', {'+','.join(map(str,raw))+'}, {')
    for n in r['before']['npc']:
        out.append('{'+','.join(str(n[k]) for k in ('id','graphic','first','last','direction','x','y','moving'))+'},')
    out.append('},'+str(len(r['phases']))+', {')
    for p in r['phases']:
        out.append('{'+str(p['key'])+','+str(p['commits'])+','+state(p['after'])+'},')
    out.append('}},')
out.append('};')
if closing:
    out.append('static const uint16_t shop_close_map_cache[][105]={')
    for r in rows:
        cache=r['before']['cache']
        assert len(cache)==105
        out.append('{'+','.join(map(str,cache))+'},')
    out.append('};')
if dialogue:
    out.append('static const ExitNpc dialogue_patch[]={')
    for r in rows:
        n=next(p['before']['npc'][0] for p in r['phases'] if p['label']=='key-16')
        out.append('{'+','.join(str(n[k]) for k in ('id','graphic','first','last','direction','x','y','moving'))+'},')
    out.append('};')
    out.append('static const unsigned dialogue_context[][32][2]={')
    for r in rows:
        out.append('{'+','.join('{'+str(p['after']['talkContext'])+','+str(p['after']['context'])+'}' for p in r['phases'])+'},')
    out.append('};')
Path('npe-sdl/tests/shop_close_native.h' if closing else 'npe-sdl/tests/exit_dialogue_native.h' if dialogue else 'npe-sdl/tests/exit_choice_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8',newline='\n')
