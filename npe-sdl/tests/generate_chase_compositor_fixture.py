import json
import argparse
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('--margin-matrix',action='store_true')
args=parser.parse_args()
stem='chase-margin' if args.margin_matrix else 'chase-compositor'
rows=json.loads(Path(f'tmp/native-{stem}.json').read_text())['rows']
out=['/* Generated from original firmware retained LCD periodic events. */',
     'static const ChaseCase chase_cases[] = {']
for row in rows:
    b,a=row['before'],row['after']
    out.append('    {"%s",%d,%d,%d,%d, {' % (row['name'],b['heroY'],a['mode'],b['rng'],a['rng']))
    for which,fields in [('before',('id','graphic','first','last','direction','x','y','moving')),
                         ('after',('id','graphic','first','last','direction','x','y','moving')),
                         ('before',('graphic','first','last','enemy','x','y','cooldown')),
                         ('after',('graphic','first','last','enemy','x','y','cooldown'))]:
        key='npc' if len(fields)==8 else 'chase'
        for item in row[which][key]:
            out.append('        {'+','.join(str(item[f]) for f in fields)+'},')
        out.append('    }, {')
    out.append('        '+','.join(str(v) for v in b['cacheTail']))
    out.append('    }},')
out.append('};')
header='chase_margin_native.h' if args.margin_matrix else 'chase_compositor_native.h'
Path('npe-sdl/tests',header).write_text('\n'.join(out)+'\n',newline='\n')
