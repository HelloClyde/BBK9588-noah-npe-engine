import json
import sys
from pathlib import Path
type_words='--type-words' in sys.argv
capture='--capture' in sys.argv
directory='tmp/battle-selector-capture-native' if capture else 'tmp/battle-group-type-words-native' if type_words else 'tmp/battle-group-prepare-native'
report=json.loads(Path(directory+'/report.json').read_text(encoding='utf8'))
assert report['passed'] and len(report['cases'])==(6 if capture else 18)
lines=['/* Original group/combine record preparation; auxiliary call excluded. */',
       'static const GroupPrepareCase group_prepare_cases[]={']
for row in report['cases']:
    c=row['input'];calls=[2 if s=='capture' else int(s=='combine') for s in c['calls']]
    assert row['stack_restored'] and len(calls) in (1,2)
    for phase in ('before','after'):
        raw=Path(directory+'/'+c['name']+'-'+phase+'-records.bin').read_bytes()
        assert len(raw)==888
    extra=','+','.join('{'+','.join(map(str,row[k]))+'}' for k in ('before_type_words','after_type_words')) if type_words else ''
    lines.append('{"%s",%u,%u,%u,{%s},{%s},{%s}%s},'%(c['name'],c['party'],c['count'],len(calls),
                 ','.join(map(str,calls)),','.join(map(str,row['before_controls'])),','.join(map(str,row['after_controls'])),extra))
lines.append('};')
Path('npe-sdl/tests/'+('battle_selector_capture' if capture else 'battle_group_type_words' if type_words else 'battle_group_prepare')+'_native.h').write_text('\n'.join(lines)+'\n',encoding='utf8')
print(str(len(report['cases']))+' original preparations/captures, six complete physical records each')
