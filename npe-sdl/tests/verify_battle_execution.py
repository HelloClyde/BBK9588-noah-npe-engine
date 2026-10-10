"""Re-read original samples, persisted C states, build logs and prior evidence.

Native per-commit RAM is advance-end RAM, not a synchronous LCD callback.
Keep it separate from the cursor sequence implied by the original scheduler.
"""
import hashlib,json,re
from pathlib import Path

def read(name):return Path(name).read_bytes()
def sha(name):return hashlib.sha256(read(name)).hexdigest()
def js(name):return json.loads(read(name))

prior=js('tmp/battle-type-words-report.json')
for section in ('immutable_sha256','preserved_sha256','evidence_sha256'):
    for name,digest in prior[section].items():assert sha(name)==digest,name

report={'passed':False,'scope':'Independent7AA8/7ACC owners, warm retention, normal public scheduler and original service ordering; exception/full-RAM equivalence remains incomplete',
        'public_paths':0,'controller_snapshots':0,'lcd_commits':0,
        'advance_end_ram_agreements':0,'advance_end_ram_transitions':0,
        'world_bound_bytes':3545,'own_save_format':'NPESAV22'}
evidence={'tmp/battle-execution-controller-source.asm','tmp/battle-execution-check-targets.json'}
for stem,expected in [('group-animation',6),('combine-animation',6),('combine-repeat',2),('enemy-animation',2)]:
    path='tmp/native-'+stem+'-execution-state.json';evidence.add(path)
    rows=js(path);assert len(rows)==expected
    for row in rows:
        c=row['config'];native=c['name'];old=native.removesuffix('-execution-state')
        name=old.removesuffix('-animation') if stem.startswith('combine') else old
        frames_path='tmp/native-'+native+'-commits.json';frames=js(frames_path);evidence.add(frames_path)
        old_frames=js('tmp/native-'+old+'-commits.json');assert len(frames)==len(old_frames)
        for f,prev in zip(frames,old_frames):
            assert {k:v for k,v in f.items() if k!='actionReady'}==prev
        events=row['events']
        if stem=='group-animation':events=[events[0],events[-2],events[-1]]
        for route in ('direct','main'):
            for i,e in enumerate(events):
                p=f'tmp/battle-execution-c-{name}-{route}-state-{i}.bin';evidence.add(p)
                assert read(p)==bytes((e['actionReady'],e['execution'])),p
                report['controller_snapshots']+=1
        party=c['party'];total=party+3;slots=[]
        if stem=='group-animation':
            speeds=[1,999,2] if c.get('secondFast') else [999,998,997]
            order=sorted([i for i in range(party) if c['hp'][i]>0 and i!=c.get('sleepSlot')],key=lambda i:-speeds[i])
            for i,actor in enumerate(order):
                if i:slots.append(order[i-1]+1)
                slots.extend([actor+1]*8)
            slots.extend([order[-1]+1]*2+[total+1]*5)
        elif stem.startswith('combine'):
            slots=([1]*14+[party]*9+[total+1]*5)*c.get('rounds',1)
        else:
            order=[('enemy',c['enemyActor']),('party',0)] if c.get('enemyFast') else [('party',0),('enemy',c['enemyActor'])]
            if party==2:order.append(('party',1))
            previous=0
            for i,(side,actor) in enumerate(order):
                if i:slots.append(previous)
                previous=party+actor+1 if side=='enemy' else actor+1
                slots.extend([previous]*(7 if side=='enemy' else 1))
            slots.extend([previous]*2+[total+1]*5)
        assert len(slots)==len(frames)
        for i,(f,slot) in enumerate(zip(frames,slots)):
            p=f'tmp/battle-execution-c-{name}-action-{i}.bin';evidence.add(p)
            state=read(p);assert state==bytes((1,slot)),p
            same=state==bytes((f['actionReady'],f['execution']))
            report['advance_end_ram_agreements' if same else 'advance_end_ram_transitions']+=1
            p=f'tmp/native-{native}-commit-{i}.idx';evidence.add(p)
            assert read(p)==read(f'tmp/native-{old}-commit-{i}.idx') and len(read(p))==28800,p
        for i in range(len(row['events'])):
            p=f'tmp/native-{native}-{i}.idx';evidence.add(p)
            assert read(p)==read(f'tmp/native-{old}-{i}.idx'),p
        report['public_paths']+=1;report['lcd_commits']+=len(frames)
assert report['public_paths']==16 and report['controller_snapshots']==108 and report['lcd_commits']==467

load_path='tmp/native-battle-execution-load.json';evidence.add(load_path)
rows=js(load_path);assert [r['entry'] for r in rows]==['system','title','new','empty']
for r in rows:
    assert r['beforeBytes']==r['afterBytes']==[129,166]
    a='tmp/battle-execution-own-'+r['entry']+'-base.dat'
    b=a.replace('-base.dat','-altered.dat');evidence.update((a,b))
    assert read(a)==read(b) and read(a)[:8]==b'NPESAV22'
report['warm_constructors']=report['own_save_pairs']=4

# Independently inspect the actual original writes/branch: MOVB to these
# addresses, and CMP1/NZ branch immediately following the7AA8 load.
prg=read('tmp/NP560.PRG')
def code(address,size):return prg[address-0x200000:address-0x200000+size]
assert code(0x395d22,14)==bytes.fromhex('1068a400f299a87a215602ac1e60')
for a,address in [(0x498e10,0x7acc),(0x36deb8,0x7acc),(0x2fcb3e,0x7acc),
                  (0x395d1e,0x7acc),(0x395fee,0x7acc),(0x395ab4,0x7acc),
                  (0x33bc7e,0x7acc),(0x33c4f2,0x7acc),(0x33c850,0x7acc),
                  (0x395ad6,0x7aa8),(0x2fcb94,0x7aa8),(0x2fc6bc,0x7aa8),
                  (0x2fc13e,0x7aa8),(0x33bcf6,0x7aa8),(0x33c4fe,0x7aa8)]:
    raw=code(a,4);assert raw[1]==0xa9 and int.from_bytes(raw[2:],'little')==address,hex(a)
report['original_instruction_writes']=15

targets=js('tmp/battle-execution-check-targets.json');assert len(targets)==len(set(targets))==78
exits={}
for label in ['generator','configure','build','repair-build','last-repair-build',*targets]:
    prefix='tmp/battle-execution-final-'+label
    assert read(prefix+'.exit').strip()==b'0',label
    exits[label]=0;evidence.update((prefix+'.log',prefix+'.exit'))
warnings=sum(len(re.findall(r'warning:',read('tmp/battle-execution-final-'+label+'.log').decode('utf8',errors='replace'),re.I)) for label in ('build','repair-build','last-repair-build'))
assert warnings==0
report.update(short_checks=len(targets),build_targets=len(targets)+1,build_warnings=warnings,
              immutable_sha256=prior['immutable_sha256'],prior_evidence_preserved=len(prior['evidence_sha256']),
              exit_codes=exits)
for label in ('group','combine','repeat','load','enemy'):
    prefix='tmp/battle-execution-'+label+'-native';assert read(prefix+'.exit').strip()==b'0',label
    evidence.update((prefix+'.log',prefix+'.exit'))

sources=['npe-sdl/npe.h','npe-sdl/main.c','npe-sdl/world.c','npe-sdl/save.c','npe-sdl/resident_resources.inc','npe-sdl/CMakeLists.txt',
         'npe-sdl/tests/reference_combine_animation.mjs','npe-sdl/tests/reference_battle_selection_load.mjs',
         'npe-sdl/tests/generate_battle_execution_fixture.py','npe-sdl/tests/battle_execution_native.h',
         'npe-sdl/tests/battle_execution_check.h','npe-sdl/tests/battle_execution_load_native.h',
         'npe-sdl/tests/verify_battle_execution.py','tmp/run_battle_execution_checks.py',
         'tmp/run_battle_execution_last_check.py']
sources+=sorted(set(prior['source_sha256'])-set(sources))
sources+=['npe-sdl/tests/battle_animation_entry_fixture.h']
sources+=['npe-sdl/tests/'+s for s in ('sdl_battle_execution_skill_record_probe.c',
    'sdl_battle_execution_item_record_probe.c','sdl_battle_command_retention_probe.c',
    'sdl_battle_skill_resident_probe.c','sdl_battle_item_resident_probe.c',
    'sdl_attack_animation_probe.c','sdl_walk_battle_entry_probe.c')]
sources+=['npe-sdl/tests/'+s for s in ('sdl_attack_defend_probe.c','sdl_attack_sequence_probe.c','sdl_defeat_animation_probe.c',
    'sdl_enemy_animation_probe.c','sdl_enemy_spell_animation_probe.c','sdl_enemy_spell_damage_probe.c','sdl_enemy_spell_status_probe.c',
    'sdl_item_animation_probe.c','sdl_party_spell_animation_probe.c','sdl_postwin_animation_probe.c','sdl_revival_animation_probe.c',
    'sdl_throw_animation_probe.c','sdl_victory_animation_probe.c')]
report['source_sha256']={p:sha(p) for p in sorted(set(sources))}
report['evidence_sha256']={p:sha(p) for p in sorted(evidence)}
report['pending']=['Advance-end RAM is not synchronous LCD RAM; transition states use original instruction/service ordering.',
    'Disabled-dispatch full original rounds and malformed execution indices/alias accesses remain unverified.',
    'All remaining scheduler callers, enemy spell/reentry record reuse and abnormal counts are incomplete.',
    'Original PCM/DAC/IRQ, physical clock/input and complete other-NPE operation remain incomplete.',
    'Native save interoperation and the two long playthrough acceptance runs remain canceled.']
report['passed']=True
Path('tmp/battle-execution-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps({k:v for k,v in report.items() if not k.endswith('sha256') and k!='exit_codes'},indent=2))
