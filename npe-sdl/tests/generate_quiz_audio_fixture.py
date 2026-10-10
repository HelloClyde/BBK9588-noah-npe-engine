import json
from pathlib import Path
rows=json.loads(Path('tmp/native-quiz-audio.json').read_text(encoding='utf-8'))
out=['/* Original UI audio requests; one retained-slot restart maps stop+play. */','static const QuizAudioCase quiz_audio_cases[]={']
for r in rows:
    assert r['stages'][-1]['mode']==1
    expected=[len([a for a in s['audio'] if a['command']=='play']) for s in r['stages']]
    for a in r['audio']:
        assert a['track_id']==20+r['slot'] and a['command'] in ['stop','play']
        if a['command']=='play':assert a['looping']
    assert expected==[0]*(len(expected)-1)+[r['music']]
    out.append('{"%s",%d,%d,%d,%d,%d,{%s},{%s}},'%(r['name'],r['event'],r['music'],r['slot'],len(r['stages'])-1,
        2 if 'third' in r['name'] else 0,','.join(str(s['key']) for s in r['stages'][1:]),','.join(map(str,expected))))
out.append('};');Path('npe-sdl/tests/quiz_audio_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
