"""Compare original instruction captures, modal snapshots and public rounds.

Full-record comparisons cover six physical records per preparation/capture;
public action probes separately compare specified numeric attributes and LCDs.
No claim of whole boot RAM or all battle lifecycle paths is made.
"""
import hashlib,json
from pathlib import Path

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
prior=json.loads(Path('tmp/battle-group-target-report.json').read_text(encoding='utf8'))
preserved={p:d for p,d in prior['preserved_sha256'].items() if p!='npe-sdl/save.c'}
for p in ('npe-sdl/tests/battle_group_prepare_native.h','npe-sdl/tests/group_animation_native.h',
          'npe-sdl/tests/battle_combine_targets_native.h'):
    preserved[p]=prior['source_sha256'][p]
for p,d in {**prior['immutable_sha256'],**preserved}.items():assert sha(p)==d,p
targets=json.loads(Path('tmp/battle-type-words-check-targets.json').read_text(encoding='utf8'))
assert len(targets)==len(set(targets))==56
prefix='tmp/battle-type-words-final-';stamp=Path(prefix+'build.exit').stat().st_mtime_ns
codes={};evidence={}
def keep(path):
    path=str(path).replace('\\','/');evidence[path]=sha(path)
def compare(path,expected):
    assert Path(path).stat().st_mtime_ns>=stamp,path
    assert Path(path).read_bytes()==expected,path;keep(path)
for label in ['prepare-generator','capture-generator','load-generator','skill-generator','item-generator',
              'group-generator','combine-generator','configure','build',*targets]:
    path=prefix+label+'.exit';assert Path(path).read_text(encoding='utf-8-sig').strip()=='0',path
    if label in targets:assert Path(path).stat().st_mtime_ns>=stamp,path
    codes[path]=0;keep(path);keep(prefix+label+'.log')
build=Path(prefix+'build.log').read_text(encoding='utf-8-sig')
assert 'warning:' not in build and 'error:' not in build
for t in ['npe-sdl',*targets]:assert 'Built target '+t in build,t
for label in ['skill','item','load','group','capture','skill-seeded','item-seeded','skill-next','item-next',
              'group-public','combine-public','repeat-public']:
    path='tmp/battle-type-words-'+label+'-native.exit'
    assert Path(path).read_text(encoding='utf-8-sig').strip()=='0',path
    codes[path]=0;keep(path);keep(path.removesuffix('.exit')+'.log')
original=Path('tmp/NP560.PRG').read_bytes();preparations=captures=full_records=type_bytes=0
for capture,count in [(False,18),(True,6)]:
    directory=Path('tmp/battle-selector-capture-native' if capture else 'tmp/battle-group-type-words-native')
    actual='tmp/battle-selector-capture-c-' if capture else 'tmp/battle-group-type-words-c-'
    report=json.loads((directory/'report.json').read_text(encoding='utf8'))
    assert report['passed'] and len(report['cases'])==count
    assert report['original_prg_sha256']==prior['immutable_sha256']['tmp/NP560.PRG'];keep(directory/'report.json')
    for row in report['cases']:
        name=row['input']['name'];ram=(directory/(name+'-after-ram.bin')).read_bytes()
        assert len(ram)==131072
        after=(directory/(name+'-after-records.bin')).read_bytes();assert len(after)==888 and ram[0x16450:0x167c8]==after
        firmware=(directory/(name+'.prg')).read_bytes()
        regions=((0x12c914,0x130),(0x3307b0,0x90)) if capture else ((0x278b94,0x342),(0x2c8178,0x1e0),(0x2c835c,12),(0x3307b0,0x90))
        for address,size in regions:assert firmware[address-0x30000:address-0x30000+size]==original[address-0x30000:address-0x30000+size]
        if capture:assert firmware[0x12ca44-0x30000:0x12ca46-0x30000]==b'\x20\xc6'
        else:assert firmware[0x2c8358-0x30000:0x2c835c-0x30000]==b'\x10\x1a\x10\x1a'
        cpu=(directory/(name+'.cpu.txt')).read_text(encoding='utf8')
        assert row['stack_restored'] and all(s in cpu for s in ['R12=600d','R15=6000','ROM_MISSING=none','UNSUPPORTED=none'])
        controls=bytes(ram[a-0x510000] for a in (0x527a9e,0x527aae,0x527ac0))
        types=b''.join(ram[a-0x510000:a-0x510000+2] for a in (0x527aa0,0x527aa6,0x527aaa))
        assert list(controls)==row['after_controls']
        assert types==b''.join(v.to_bytes(2,'big') for v in row['after_type_words'])
        assert row['before_type_words']==[0x81a6,0xcbf0,0x153a]
        if capture:
            assert after==(directory/(name+'-before-records.bin')).read_bytes()
            actor=row['input']['actor'];assert row['after_type_words']==[after[148*actor+i] for i in (129,128,130)]
        for suffix in ['-before-records.bin','-after-records.bin','-after-ram.bin','.prg','.cpu.txt']:keep(directory/(name+suffix))
        compare(actual+name+'-records.bin',after);compare(actual+name+'-controls.bin',controls);compare(actual+name+'-types.bin',types)
        full_records+=6;type_bytes+=6
    if capture:captures=count
    else:preparations=count
def clean(value):
    if isinstance(value,dict):return {k:clean(v) for k,v in value.items() if k not in ('typeWords','seedTypes','nextTypes')}
    if isinstance(value,list):return [clean(v) for v in value]
    if isinstance(value,str):return value.removesuffix('-type-words')
    return value
modal_totals={};preserved_menu_frames=0
for kind,menus,size,count in [('skill',(21,27),144,15),('item',(33,37),142,16)]:
    native='tmp/native-battle-'+kind+'-type-words';actual='tmp/battle-type-words-c-'+kind+'-'
    rows=json.loads(Path(native+'.json').read_text(encoding='utf8'))
    old=json.loads(Path('tmp/native-battle-'+kind+'-selector-target.json').read_text(encoding='utf8'))
    assert len(rows)==count and len({r['name'] for r in rows})==count
    assert clean([r for r in rows if not r['name'].startswith('seeded-')])==old
    keep(native+'.json');frames_total=states_total=outer_total=0
    for row in rows:
        name=row['name'];frames=[{'index':65535,'state':row['before']}];states=[row['before']]
        for phase in row['phases']:
            modal=[c for c in phase['commits'] if c['state']['menu'] in menus and c['state']['actor']==0]
            frames+=modal;states += [c['state'] for c in modal]+[phase['after']]
            outer_total+=len(phase['commits'])-len(modal)
        for n,f in enumerate(frames):
            suffix='-'+name+('-before' if f['index']==65535 else '-'+str(f['index']))+'.idx'
            path=native+suffix;pixels=Path(path).read_bytes();assert len(pixels)==28800;keep(path)
            if not name.startswith('seeded-'):
                assert pixels==Path('tmp/native-battle-'+kind+'-selector-target'+suffix).read_bytes();preserved_menu_frames+=1
            compare(actual+name+'-frame-'+str(n)+'.idx',pixels)
        for n,s in enumerate(states):
            cache=bytes([s['menu'],s['actor']])+s['resourceFlags'].to_bytes(2,'big')+bytes(s['bytes'])
            assert len(cache)==size+4;compare(actual+name+'-state-'+str(n)+'.bin',cache)
            compare(actual+name+'-target-'+str(n)+'.bin',bytes([s['selectorTarget']]))
            compare(actual+name+'-types-'+str(n)+'.bin',b''.join(v.to_bytes(2,'big') for v in s['typeWords']))
        if name=='seeded-next-actor':assert row['after']['actor']==1 and row['after']['typeWords']==[145,131,173]
        if name=='seeded-party-target':
            assert row['before']['typeWords']==[0x81a6,0xcbf0,0x153a]
            assert row['after']['typeWords']==([0x81a6,0xcbf0,0x153a] if kind=='skill' else [1,0xcbf0,0x153a])
        frames_total+=len(frames);states_total+=len(states)
    modal_totals[kind]=dict(paths=count,lcd_commits=frames_total,type_word_snapshots=states_total,outer_lcds_excluded=outer_total)
public_totals={};public_action_pixels=public_input_pixels=0
for stem,count,group in [('group-animation',6,True),('combine-animation',6,False),('combine-repeat',2,False)]:
    path='tmp/native-'+stem+'-type-words.json';rows=json.loads(Path(path).read_text(encoding='utf8'));keep(path)
    previous_path='tmp/native-'+stem+('' if group else '-selector-target')+'.json'
    old=json.loads(Path(previous_path).read_text(encoding='utf8'))
    # The prior combine reference uses a different diagnostic output suffix.
    def strip_suffix(value):
        if isinstance(value,dict):return {k:strip_suffix(v) for k,v in value.items() if k!='typeWords'}
        if isinstance(value,list):return [strip_suffix(v) for v in value]
        if isinstance(value,str):return value.removesuffix('-type-words').removesuffix('-selector-target')
        return value
    assert len(rows)==count and strip_suffix(rows)==strip_suffix(old)
    snapshots=0
    for row,previous in zip(rows,old):
        name=row['config']['name'];old_name=previous['config']['name']
        c_name=name.removesuffix('-type-words') if group else name.removesuffix('-type-words').removesuffix('-animation')
        states=[row['events'][i] for i in (0,-2,-1)] if group else row['events']
        for phase,s in enumerate(states):
            for route in ['direct','main']:
                compare('tmp/battle-'+('group' if group else 'combine')+'-type-public-c-'+c_name+'-'+route+'-phase-'+str(phase)+'.bin',b''.join(v.to_bytes(2,'big') for v in s['typeWords']))
                snapshots+=1
        for n in range(len(row['events'])):
            a='tmp/native-'+name+'-'+str(n)+'.idx';b='tmp/native-'+old_name+'-'+str(n)+'.idx'
            assert len(Path(a).read_bytes())==28800 and Path(a).read_bytes()==Path(b).read_bytes();keep(a);public_input_pixels+=1
        commits=json.loads(Path('tmp/native-'+name+'-commits.json').read_text(encoding='utf8'))
        old_commits=json.loads(Path('tmp/native-'+old_name+'-commits.json').read_text(encoding='utf8'))
        assert commits==old_commits;keep('tmp/native-'+name+'-commits.json')
        for c in commits:
            n=c['index'];a='tmp/native-'+name+'-commit-'+str(n)+'.idx';b='tmp/native-'+old_name+'-commit-'+str(n)+'.idx'
            assert Path(a).read_bytes()==Path(b).read_bytes();keep(a);public_action_pixels+=1
    public_totals[stem]=dict(paths=count,type_word_snapshots=snapshots)
warm=json.loads(Path('tmp/native-battle-type-words-load.json').read_text(encoding='utf8'));keep('tmp/native-battle-type-words-load.json')
assert [r['entry'] for r in warm]==['system','title','new','empty']
for r in warm:
    assert r['beforeBytes']==r['afterBytes']==[129,166,203,240,21,58]
    base='tmp/battle-type-words-own-'+r['entry']+'-'
    a=Path(base+'base.dat').read_bytes();b=Path(base+'altered.dat').read_bytes();assert a==b and a[:8]==b'NPESAV22'
    compare(base+'base.dat',a);compare(base+'altered.dat',a)
regression_outputs=0
for path,digest in prior['evidence_sha256'].items():
    assert sha(path)==digest,path
    if '-c-' in path or path.startswith(('tmp/rpg-shutdown-runtime-','tmp/rpg-shutdown-close-',
                                        'tmp/rpg-shutdown-main-','tmp/battle-selector-target-own-')):
        assert Path(path).stat().st_mtime_ns>=stamp,path;regression_outputs+=1
    evidence[path]=digest
sources=['npe-sdl/npe.h','npe-sdl/world.c','npe-sdl/save.c','npe-sdl/resident_resources.inc','npe-sdl/CMakeLists.txt',
         'npe-sdl/tests/reference_battle_selection_load.mjs','npe-sdl/tests/generate_battle_type_words_load_fixture.py',
         'npe-sdl/tests/battle_type_words_load_native.h','npe-sdl/tests/battle_menu_rows_load_probe.c',
         'npe-sdl/tests/reference_battle_group_prepare.py','npe-sdl/tests/generate_battle_group_prepare_fixture.py',
         'npe-sdl/tests/battle_group_type_words_native.h','npe-sdl/tests/battle_selector_capture_native.h',
         'npe-sdl/tests/battle_group_prepare_probe.c','npe-sdl/tests/reference_combine_animation.mjs',
         'npe-sdl/tests/generate_group_animation_native.py','npe-sdl/tests/group_animation_type_words_native.h',
         'npe-sdl/tests/sdl_group_animation_probe.c','npe-sdl/tests/generate_battle_combine_target_fixture.py',
         'npe-sdl/tests/battle_combine_type_words_native.h','npe-sdl/tests/sdl_combine_attack_probe.c',
         'npe-sdl/tests/verify_battle_type_words.py','tmp/run_battle_type_words_checks.py']
sources += [str(p).replace('\\','/') for p in Path('npe-sdl/tests').glob('*.c') if '3543' in p.read_text(encoding='utf8') and str(p).replace('\\','/') not in sources]
for k in ('skill','item'):
    sources += ['npe-sdl/tests/'+p for p in ['reference_battle_'+k+'_record.mjs','generate_battle_'+k+'_record_fixture.py',
                'sdl_battle_'+k+'_record_probe.c','battle_'+k+'_type_words_native.h']]
report=dict(passed=True,scope='Three independent selector words: live bridge, group preparation, controller capture, item write, modal retain/next-actor, group/combine round return and warm constructors',
            preparation_cases=preparations,capture_cases=captures,full_physical_records=full_records,original_type_bytes=type_bytes,
            modal=modal_totals,public_rounds=public_totals,preserved_menu_lcds=preserved_menu_frames,
            public_action_lcds_rechecked=public_action_pixels,public_input_lcds_rechecked=public_input_pixels,
            warm_constructors=4,own_save_pairs=4,world_bound_bytes=3543,own_save_format='NPESAV22',
            short_checks=56,build_targets=57,build_warnings=0,regression_persisted_outputs=regression_outputs,
            immutable_sha256=prior['immutable_sha256'],preserved_sha256=preserved,
            source_sha256={p:sha(p) for p in sources},evidence_sha256=evidence,exit_codes=codes,
            pending=['All remaining selector/execution fields and control call ordering','Other continuous actions, complete physical record reuse and abnormal counts',
                     'Complete boot/shutdown/file lifecycle','Original PCM and physical timing','Other NPE full dynamic compatibility'])
Path('tmp/battle-type-words-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps({k:report[k] for k in ('passed','modal','public_rounds','full_physical_records','short_checks','world_bound_bytes')}))
