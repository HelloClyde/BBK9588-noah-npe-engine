from pathlib import Path
import json
rows=json.loads(Path('tmp/native-character-escape.json').read_text(encoding='utf-8'))
assert len(rows)==2
lines=['#ifndef CHARACTER_ESCAPE_NATIVE_H','#define CHARACTER_ESCAPE_NATIVE_H','#include "negative_hp_native.h"',
 'typedef struct { const char *name; unsigned party,character,frames; NegativeHpRecord before[6],after[6]; } FleeAnimationCase;',
 'static const FleeAnimationCase flee_animation_cases[]={']
def records(rs):
    out=[]
    for r in rs:
        values=[r[k] for k in ['hp','maximum','qi','mental','attack','defense','level','experience','attackTimer','defenseTimer']]
        status=[v if v<32768 else v-65536 for v in r['status']]
        out.append('{'+','.join(map(str,values))+',{'+','.join(map(str,status))+'}}')
    return ','.join(out)

for row in rows:
 c,ev=row['config'],row['events']; assert ev[-1]['mode']==1
 submitted=json.loads(Path('tmp/native-'+c['name']+'-commits.json').read_text(encoding='utf-8'))
 assert ev[0]['records']==ev[-1]['records']
 assert len(submitted)==7*sum(0<h<32768 for h in c['hp'][:c['party']])+3
 lines.append('{"%s",%u,%u,%u,{%s},{%s}},'%(c['name'],c['party'],c['character'],len(submitted),records(ev[0]['records']),records(ev[-1]['records'])))
lines+=['};','#endif','']
Path('npe-sdl/tests/character_escape_native.h').write_text('\n'.join(lines),encoding='utf-8',newline='\n')
