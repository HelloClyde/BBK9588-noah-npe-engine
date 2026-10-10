import json
from pathlib import Path
r=json.loads(Path('tmp/native-event-chain.json').read_text())
b=r['before'];f=Path('tmp/native-event-chain-0.idx').read_bytes()
assert r['prefixCommits']==70 and r['prefixState']['mode']==14 and not r['audio']
assert all(Path(f'tmp/native-event-chain-{i}.idx').read_bytes()==f for i in range(70))
values=[b[k] for k in ['sx','sy','vx','vy','direction']]+[f[0],70]
npcs=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in b['npc'])
Path('npe-sdl/tests/event_chain_native.h').write_text('/* Original resident mode14 self-cycle, 70-commit prefix. */\nstatic const ChainFixture chain={'+','.join(map(str,values))+',{'+npcs+'},{'+','.join(map(str,b['npcState']))+'}};\n')
print('70 original mode14 cycle commits; all frame bytes equal')
