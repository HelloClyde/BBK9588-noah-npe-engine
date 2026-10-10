import json
from pathlib import Path
rows=json.loads(Path('tmp/native-quiz-flow.json').read_text(encoding='utf-8'))
out=['/* Original-engine LCD submissions, offline only. */','static const SdlQuizCase sdl_quiz_cases[]={']
for r in rows:
 frames=[(j,k) for j,s in enumerate(r['stages']) for k,c in enumerate(s['commits'])]
 v=r['stages'][-1]['after'];state=[v[k] for k in ['mode','event','context','count','wrong','previous','money']]
 out.append('{"%s",%d,%d,%d,%d,%d,%d,%d,%d,{%s},{%s},{%s},{%s},%s,{%s}},'%(r['name'],r['event'],r.get('count',0),r.get('wrong',0),len(r['stages'])-1,len(frames),r.get('correctSeed',0),r.get('wrongSeed',0),r.get('fixedSeed',0),','.join(str(s.get('code',0)) for s in r['stages'][1:]),','.join('{%d,%d}'%f for f in frames),','.join(str(len(s['commits'])) for s in r['stages']),','.join(map(str,state)),json.dumps(bytes(v['answer']).decode('ascii')),','.join(map(str,v['stats']))))
 # Preserve the original entry statistics independently of the final result.
 out[-1]=out[-1][:-2]+',{'+','.join(map(str,r['stages'][0]['after']['stats']))+'}},'
out.append('};');Path('npe-sdl/tests/sdl_quiz_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
# Refresh fixed state fixture after excluding unrelated post-service periods.
out=['/* Original-engine fixed quiz service, offline only. */','static const QuizCase quiz_cases[]={']
for r in rows:
 if r['event']<10:continue
 out.append('{"%s",%d,%d,%d,%d,{'%(r['name'],r['event'],r.get('count',0),r.get('fixedSeed',0),len(r['stages'])))
 for st in r['stages']:
  v=st['after'];fields=[v[k] for k in ['mode','event','context','count','wrong','previous','type']]
  out.append('{%d,%d,{%s},%s,{%s}},'%(st.get('code',0),len(st['commits']),','.join(map(str,fields)),json.dumps(bytes(v['answer']).decode('ascii')),','.join(map(str,v['stats']))))
 out.append('}},')
out.append('};');Path('npe-sdl/tests/quiz_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
