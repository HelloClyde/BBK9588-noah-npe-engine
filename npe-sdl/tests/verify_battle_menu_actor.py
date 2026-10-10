"""Re-read native menu actor samples, persisted C outputs and regression logs."""
import hashlib,json,re
from pathlib import Path

def read(p):return Path(p).read_bytes()
def sha(p):return hashlib.sha256(read(p)).hexdigest()
def js(p):return json.loads(read(p))
prior=js('tmp/battle-execution-report.json')
for section in ('immutable_sha256','evidence_sha256'):
    for p,digest in prior[section].items():assert sha(p)==digest,p
evidence={'tmp/battle-menu-actor-controller-source.asm','tmp/battle-menu-actor-check-targets.json',
    'tmp/battle-menu-actor-world-before-flee.c','tmp/battle-menu-actor-flee-before-fix.log',
    'tmp/battle-menu-actor-flee-before-fix.exit','tmp/battle-menu-actor-flee-menu-observer-before-fix.log',
    'tmp/battle-menu-actor-flee-menu-observer-before-fix.exit'}
paths=frames=direct=0
for stem,count in [('group-animation',6),('attack-sequence',2),('combine-animation',6),('combine-repeat',2)]:
    p='tmp/native-'+stem+'-menu-actor.json';rows=js(p);evidence.add(p);assert len(rows)==count
    for row in rows:
        name=row['config']['name'];old=name.removesuffix('-menu-actor')
        key=old.removesuffix('-animation') if stem.startswith('combine') else old
        p='tmp/native-'+name+'-commits.json';commits=js(p);evidence.add(p)
        previous=js('tmp/native-'+old+'-commits.json');assert len(previous)==len(commits)
        for i,(c,prev) in enumerate(zip(commits,previous)):
            assert {k:v for k,v in c.items() if k not in ('menuActor','actionReady')}==prev
            assert c['menuActor']==0
            p=f'tmp/native-{name}-commit-{i}.idx';evidence.add(p)
            assert read(p)==read(f'tmp/native-{old}-commit-{i}.idx') and len(read(p))==28800
            p=f'tmp/battle-menu-actor-c-{key}-main-commit-{i}.bin';evidence.add(p)
            assert read(p)==bytes((c['menuActor'],)),p
            if not stem.startswith('combine'):
                p=p.replace('-main-','-direct-');evidence.add(p)
                assert read(p)==bytes((c['menuActor'],)),p;direct+=1
            frames+=1
        for i in range(len(row['events'])):
            p=f'tmp/native-{name}-{i}.idx';evidence.add(p)
            assert read(p)==read(f'tmp/native-{old}-{i}.idx'),p
        paths+=1
assert (paths,frames,direct)==(16,490,210)
# The supplemental repair changes only the failed-escape entry. The81
# already-checked normal/resource routes retain their exact source bodies.
before=read('tmp/battle-menu-actor-world-before-flee.c').decode('utf8').replace('\r\n','\n')
current=read('npe-sdl/world.c').decode('utf8').replace('\r\n','\n')
anchor='    world->battle_action_ready=1; /*2F:BDA6/C624, after the failure prompt. */\n'
addition=('    /*2F:C63C and CA70 reset/scanning also apply to failed escape. */\n'
    '    int first=world_command_party_from(world,0);\n'
    '    world->battle_actor=(uint8_t)(first>=0?first:(int)world_record_party_count(world));\n')
assert before.count(anchor)==1 and current==before.replace(anchor,anchor+addition,1)
p='tmp/native-failed-flee-2-1-random-retry-display.json';native_flee=js(p);evidence.add(p)
assert len(native_flee['commits'])==41 and all(c['actor']==0 for c in native_flee['commits'])
p='tmp/battle-menu-actor-failed-flee-c.bin';flee=read(p);evidence.add(p)
assert len(flee)==33 and flee==bytes(33)
p='tmp/native-failed-flee-2-1-random-retry-attempt2-commit0.idx';evidence.add(p)
assert len(read(p))==28800
asm=read('tmp/battle-menu-actor-controller-source.asm').decode('utf8')
scheduler=[line for line in asm.splitlines() if line.startswith('39:')]
assert scheduler and not any('7A9E' in line for line in scheduler)
prg=read('tmp/NP560.PRG')
for address in (0x2fca70,0x2fca88,0x2fc63c,0x2fc6d4,0x38fefc,0x38ff14):
    raw=prg[address-0x200000:address-0x200000+4]
    assert raw[1]==0xa9 and int.from_bytes(raw[2:],'little')==0x7a9e,hex(address)
targets=js('tmp/battle-menu-actor-check-targets.json');assert len(targets)==len(set(targets))==82
for label in ['generator','configure','build','flee-build',*targets]:
    prefix='tmp/battle-menu-actor-final-'+label
    assert read(prefix+'.exit').strip()==b'0',label
    evidence.update((prefix+'.log',prefix+'.exit'))
warnings=sum(len(re.findall(r'warning:',read('tmp/battle-menu-actor-final-'+label+'.log').decode('utf8',errors='replace'),re.I)) for label in ('build','flee-build'))
assert not warnings
for label in ('group','attack','combine','repeat'):
    prefix='tmp/battle-menu-actor-'+label+'-native'
    assert read(prefix+'.exit').strip()==b'0',label
    evidence.update((prefix+'.log',prefix+'.exit'))
sources=set(prior['source_sha256'])|{
    'npe-sdl/tests/generate_battle_menu_actor_fixture.py','npe-sdl/tests/battle_menu_actor_native.h',
    'npe-sdl/tests/battle_menu_actor_check.h','npe-sdl/tests/verify_battle_menu_actor.py',
    'npe-sdl/tests/sdl_attack_sequence_probe.c','npe-sdl/tests/sdl_failed_flee_display_probe.c',
    'tmp/run_battle_menu_actor_checks.py','tmp/run_battle_menu_actor_flee_check.py'}
report={'passed':True,'scope':'Independent menu role7A9E and physical action role7ACC on normal public routes; full alias/exception controllers remain incomplete',
    'public_paths':paths,'reference_lcd_commits':frames,'main_menu_actor_snapshots':frames,
    'direct_menu_actor_snapshots':direct,'world_bound_bytes':3545,'own_save_format':'NPESAV22',
    'failed_flee_native_commits':41,'failed_flee_live_menu_snapshots':len(flee),
    'failed_flee_prefix_pixel_checks':2,
    'short_checks':len(targets),'build_targets':len(targets)+1,'build_warnings':warnings,
    'prior_evidence_preserved':len(prior['evidence_sha256']),
    'immutable_sha256':prior['immutable_sha256'],'source_sha256':{p:sha(p) for p in sorted(sources)},
    'evidence_sha256':{p:sha(p) for p in sorted(evidence)},
    'pending':['Original menu actor RAM is sampled at advance end; inspected action scheduler does not write7A9E.',
       'All other menu writers, abnormal menu indices/physical slots and failed-flee callers remain incompletely verified.',
       'Disabled-dispatch original full rounds, remaining record reuse and abnormal counts remain incomplete.',
       'Original audio/physical timing and complete other-NPE operation remain incomplete.',
       'Native save interoperation and two full playthrough acceptance runs remain canceled.']}
Path('tmp/battle-menu-actor-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps({k:v for k,v in report.items() if not k.endswith('sha256')},indent=2))
