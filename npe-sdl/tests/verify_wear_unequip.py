"""Audit existing original fixtures and completed runs; never run or generate oracles."""
import hashlib,json,re
from pathlib import Path

def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
rows=json.loads(Path('tmp/native-wear-unequip.json').read_text(encoding='utf-8'))
expected=[(r['name'],p,i) for r in rows for p,phase in enumerate(r['phases'])
          for i in range(len(phase['commits']))]
ignored=[(r['name'],p,phase['mask']) for r in rows for p,phase in enumerate(r['phases']) if not phase['commits']]
cold_names={r['name'] for r in rows if r['name']!='two-members'}
assert len(rows)==9 and len(expected)==101 and len(ignored)==18
frames=[]
for r in rows:
    paths=[f"tmp/native-wear-unequip-{r['name']}-before.idx"]
    paths += [f"tmp/native-wear-unequip-{r['name']}-{p}-commit-{i}.idx"
              for p,phase in enumerate(r['phases']) for i in range(len(phase['commits']))]
    for path in paths:
        raw=Path(path).read_bytes();assert len(raw)==240*120 and max(raw)<=15
        frames.append({'path':path,'sha256':digest(path)})
runs=[]
for variant in ['default','shift','caps']:
    path=f'tmp/wear-unequip-final-{variant}.log';log=Path(path).read_text(encoding='utf-8')
    assert not re.search('Assertion failed|Missing original LCD|mismatch|Extra C/SDL',log)
    actual=re.findall(r'wear unequip (\S+) phase(\d+) commit(\d+) C=(\d+) SDL=(\d+)',log)
    assert [(n,int(p),int(i)) for n,p,i,c,s in actual]==expected
    assert all(c=='0' and s=='0' for n,p,i,c,s in actual)
    assert [(n,int(p),int(k)) for n,p,k in re.findall(r'Original ignored input: (\S+) phase(\d+) mask(\d+)',log)]==ignored
    assert set(re.findall(r'Wear unequip (\S+): cold save/load and resumed map render passed',log))==cold_names
    assert set(re.findall(r'Wear unequip (\S+) complete; full roles/inventory/item cache/NPC/settings matched',log))=={r['name'] for r in rows}
    assert 'Wear unequip return:101 original C/SDL commits' in log
    runs.append({'variant':variant,'lcd_comparisons':len(actual),'ignored_inputs':len(ignored),
                 'cold_save_load_paths':len(cold_names),'log':path,'sha256':digest(path)})
regressions=[]
for name,count,marker in [('view',71,'Equipment view return:71'),('description',57,'Equipment description return:57'),('equip',36,'9 actual SDL paths / 36 LCD commits')]:
    path=f'tmp/wear-unequip-regression-{name}.log';log=Path(path).read_text(encoding='utf-8')
    assert marker in log and 'Assertion failed' not in log
    matches=re.findall(r' C=(\d+) SDL=(\d+)',log)
    assert len(matches)==count and all(c=='0' and s=='0' for c,s in matches)
    regressions.append({'name':name,'lcd_comparisons':count,'log':path,'sha256':digest(path)})
before='tmp/wear-unequip-before-controller.log'
assert 'Missing original LCD: six-slots phase4 mask1024 C=0 native=1' in Path(before).read_text(encoding='utf-8')
sources=['npe-sdl/main.c','npe-sdl/world.c','npe-sdl/text.c','npe-sdl/npe.h','npe-sdl/text.h',
         'npe-sdl/tests/reference_wear_unequip.mjs','npe-sdl/tests/generate_wear_unequip_fixture.py',
         'npe-sdl/tests/wear_unequip_native.h','npe-sdl/tests/sdl_wear_unequip_probe.c','npe-sdl/build/npe-sdl.exe']
refs=['tmp/NP560.PRG','tmp/opennoah-headless.mjs','tmp/opennoah_wasm_bg.wasm',
      'tmp/tianjiang-online.opennoah','tmp/native-title-load-saved.snapshot',
      'tmp/tjfy-world-probe.npe','tmp/native-wear-unequip.json']
report={'passed':True,'original_cases':len(rows),'original_lcd_commits':len(expected),
        'wear_lcd_comparisons':sum(r['lcd_comparisons'] for r in runs),
        'regression_lcd_comparisons':sum(r['lcd_comparisons'] for r in regressions),
        'ignored_inputs':sum(r['ignored_inputs'] for r in runs),
        'cold_save_load_paths':len(cold_names),'cold_save_load_runs':sum(r['cold_save_load_paths'] for r in runs),
        'cold_save_scope':'actual F4 return, persistent inventory/equipment/attributes/status, cold load and resumed map render; extra close frame unmeasured; cloned duplicate-role diagnostic excluded',
        'before_controller':{'log':before,'sha256':digest(before)},'runs':runs,'regressions':regressions,
        'sources':{p:digest(p) for p in sources},'original_references':{p:digest(p) for p in refs},
        'frames':frames,'whole_goal_complete':False,'overall_estimate_percent':80}
Path('tmp/wear-unequip-report.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
print('PASS: 9 original paths / 101 commits; 303 wear C/SDL + 164 regression comparisons; 54 ignored inputs; 8 cold-save paths / 24 runs. Goal remains active.')
