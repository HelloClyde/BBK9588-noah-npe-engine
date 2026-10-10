import json
from pathlib import Path

def array(values):return '{'+','.join(map(str,values))+'}'
rows=json.loads(Path('tmp/native-party-teleport-load.json').read_text());assert len(rows)==16
out=['/* Original adjacent group5/group6 cache bytes after public UI loads. */',
     'static const PartyTeleportLoadCase load_cases[]={']
for r in rows:
    b,a=r['beforeBytes'],r['afterBytes'];assert len(a)==len(b)==38
    for i in (2,3,12,13,23,29,35):assert b[i]==a[i]
    out.append('{'+json.dumps(r['entry'])+','+str(r['targetMap'])+','+array(b)+','+array(a)+'},')
out.append('};')
Path('npe-sdl/tests/party_teleport_load_native.h').write_text('\n'.join(out)+'\n')

fields=['mode','event','map','progress','sx','sy','vx','vy','direction','money','enabled','slot','context','menu','member','equipmentSlot','gearSelected','descLength','descStart','descEnd','menuPartyCount']
def actor(a):
    keys=['id','active','portrait','level','maxHp','hp','maxQi','qi','attack','defense','experience','experienceToNext']
    return '{'+','.join([str(a[k]) for k in keys]+[array(a[k]) for k in ['otherStats','gear','guards','skills']])+'}'
def party_state(s):
    npc='{'+','.join(array(n[k] for k in ('id','x','y','direction')) for n in s['npc'])+'}'
    return '{'+','.join(str(s[k]) for k in fields)+','+npc+',{'+','.join(actor(a) for a in s['actors'])+'}}'
rows=json.loads(Path('tmp/native-party-resident.json').read_text());assert len(rows)==50
out=['/* Public party dispatcher, complete adjacent resident bytes. */','static const PartyCase party_cases[]={']
for r in rows:
    e=r['event'];b,a=r['before'],r['after']
    out.append('{'+json.dumps(r['name'])+','+','.join(map(str,(e['map'],e['event'],len(r['commits']))))+','+
               party_state(b)+','+party_state(a)+','+array(b['residentBytes'])+','+array(a['residentBytes'])+'},')
out.append('};')
Path('npe-sdl/tests/party_resident_native.h').write_text('\n'.join(out)+'\n')
print('Party',len(rows),'cases',sum(len(r['commits']) for r in rows),'commits')

rows=json.loads(Path('tmp/native-teleport-resident.json').read_text());assert len(rows)==12
keys=['mode','event','context','map','progress','init','sx','sy','vx','vy','direction','enabled','slot','visited','rng']
def cache(c):return array(c[k] for k in ['map','x','y','marker','mode','nextId'])
def tele_state(s):return '{'+','.join(str(s[k]) for k in keys)+',{'+','.join(cache(c) for c in s['cache'])+'}}'
out=['/* Original public teleport dispatcher and full adjacent caches. */','static const TeleCase tele_cases[]={']
for r in rows:
    b,a=r['before'],r['after']
    out.append('{'+json.dumps(r['name'])+','+','.join(str(r[k]) for k in ['targetMap','x','y','marker','nextMode','nextId'])+','+
               str(len(r['commits']))+','+tele_state(b)+','+tele_state(a)+','+array(b['residentBytes'])+','+array(a['residentBytes'])+'},')
out.append('};')
Path('npe-sdl/tests/teleport_resident_native.h').write_text('\n'.join(out)+'\n')
print('Teleport',len(rows),'paths',sum(len(r['commits']) for r in rows),'commits')
