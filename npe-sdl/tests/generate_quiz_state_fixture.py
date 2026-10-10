from pathlib import Path
import json
rows=json.loads(Path('tmp/native-quiz-state.json').read_text())
out=['/* Original UI state snapshots; offline fixture. Input uses32 resident bytes. */','static const QuizStateCase quiz_state_cases[]={']
def state(v):
 return '{%s,{%s},{%s}}'%(','.join(str(v[k]) for k in ['mode','event','context','count','wrong','previous','category','type','length']),','.join(map(str,v['stats'])),','.join(map(str,v['input'][:32])))
for r in rows:
 out.append('{"%s",%d,%s,%s,%s},'%(r['route'],len(r['commits']),state(r.get('saved',r['before'])),state(r['before']),state(r['after'])))
out.append('};');Path('npe-sdl/tests/quiz_state_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
