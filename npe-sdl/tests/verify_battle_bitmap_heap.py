from pathlib import Path
import hashlib
import json

def digest(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

rows = []
source_hashes={
    'tmp/tjfy-world-probe.npe':'df4b52df6b69e1105a5ea6100e43f35f832ed859c10cb8714879e44ad4d8cb40',
    'tmp/NP560.PRG':'d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5',
    'tmp/opennoah-headless.mjs':'2fec1a7c970516be40bba4a301a1a6ccf2ad7295ee64d0ee3ad15417fd72d32d',
    'tmp/opennoah_wasm_bg.wasm':'af98dd54ebaa6073cfc3809a7387a0f7af087605c79056449d88e63075ef4d86',
    'tmp/tianjiang-online.opennoah':'c59772070ca7beec488a86a61bfec3ac1e60cd70c248ab36239ff81e2b862148',
}
assert all(digest(p)==h for p,h in source_hashes.items())
for name in ('extended-31', 'short-header-34'):
    paths = [Path(f'tmp/native-battle-bitmap-heap-{tag}-{name}.json') for tag in ('plain','trace')]
    assert all(p.exists() for p in paths), paths
    plain, trace = [json.loads(p.read_text(encoding='utf8')) for p in paths]
    assert plain['before']['fields'] == trace['before']['fields'], name
    assert len(plain['samples']) == len(trace['samples']) == 4
    commits = 0
    for a,b in zip(plain['samples'], trace['samples']):
        assert a['tick'] == b['tick']
        assert a['commits'] == b['commits']
        assert a['fields'] == b['fields'], (name,a['tick'])
        assert len(a['frames']) == len(b['frames']) == a['commits']
        for p,q in zip(a['frames'],b['frames']):
            assert Path(p).read_bytes() == Path(q).read_bytes(), (name,p,q)
            assert len(Path(p).read_bytes()) == 240*120
            commits += 1
    assert commits == 6
    assert [sample['commits'] for sample in plain['samples']] == [6,0,0,0]
    high, low, length, calls = trace['samples'][0]['trace']
    pointer = high*65536+low
    assert calls == 1
    assert length == (35328 if name == 'extended-31' else 2)
    assert pointer == 0x51e9c2
    assert trace['samples'][0]['heapHandle'] == 0
    copied = []
    window = None
    if name == 'extended-31':
        npe = Path('tmp/tjfy-world-probe.npe').read_bytes()
        state = plain['samples'][0]['fields']
        for field,start,count in [('battleRecords',0x526450,6),('mapRecords',0x5269c6,3)]:
            for i in range(count):
                address = start+148*i
                source = 15098+address-pointer
                assert bytes(state[field][i]) == npe[source:source+148]
                copied.append({'guest_address':hex(address),'file_offset':source,'length':148})
        # Only this previously verified named-field RAM window is flat here.
        # The captured host slice below it is not a guest-memory map.
        import base64
        raw=base64.b64decode(plain['samples'][0]['rawRAM'])
        start,end=0x6000,0x73c2
        actual=bytes(raw[0x2000+o-(9 if o<0x7000 else 1)] for o in range(start,end))
        source=15098+0x526000-pointer
        assert actual == npe[source:source+len(actual)]
        window={'guest_start':'0x526000','guest_end_exclusive':'0x5273c2',
                'file_offset':source,'length':len(actual),'all_bytes_equal':True}
    rows.append({'name':name,'pointer':hex(pointer),'length':length,'handle':0,
                 'calls_first_update':calls,'lcd_commits_equal':commits,
                 'before_and_four_update_field_samples_equal':True,
                 'copied_records':copied,
                 'copied_window':window,
                 'json_sha256':{str(p):digest(p) for p in paths}})

experiments=[]
baseline=json.loads(Path('tmp/native-battle-bitmap-heap-plain-short-header-34.json').read_text(encoding='utf8'))
for path in [
    'tmp/native-battle-heap-request-35329-short-header-34.json',
    'tmp/native-battle-heap-request-65535-short-header-34.json',
    'tmp/native-battle-heap-second-3-197-short-header-34.json',
    'tmp/native-battle-heap-second-35329-197-short-header-34.json',
    'tmp/native-battle-heap-header-3-short-header-34.json',
    'tmp/native-battle-heap-header-35329-short-header-34.json',
]:
    trial=json.loads(Path(path).read_text(encoding='utf8'))
    assert trial['before']['fields']==baseline['before']['fields'],path
    commits=0
    for a,b in zip(baseline['samples'],trial['samples']):
        assert a['fields']==b['fields'],(path,a['tick'])
        assert a['commits']==b['commits']
        for p,q in zip(a['frames'],b['frames']):
            assert Path(p).read_bytes()==Path(q).read_bytes(),(path,p,q)
            commits+=1
    assert commits==6
    trace=trial['samples'][0]['trace']
    assert trace[:4]==[0x51,0xe9c2,2,1]
    assert trace[4]==trial['request']
    if 'second' in trial:
        assert trace[5:]==[0,0x51,0xe9c2]
    experiments.append({'path':path,'sha256':digest(path),'request':trial['request'],
                        'second_request':trial.get('second'),'trace':trace,
                        'headers':trial['samples'][0].get('headers'),
                        'incorrect_far_address_diagnostic':'heap-header-' in path,
                        'lcd_commits_equal':commits,
                        'before_and_four_update_field_samples_equal':True})

report={'module':'Battle bitmap file-read buffer pointer trace',
        'status':'Diagnostic pointer evidence; production RAM aliases not implemented',
        'scope':'Trace modifies isolated guest instruction memory. Plain reference is uninstrumented. Disk PRG, NPE and WASM are unchanged.',
        'source_sha256':source_hashes,
        'cases':rows,
        'isolated_allocation_experiments':experiments,
        'limits':[
            'No production heap implementation or RAM-write binding in this change',
            'Current prepared Tianjiang state only; not a global allocator rule',
            'Low RAM outside the verified copied window is not a flat host mapping',
            'Allocation experiments alter guest requests and calls; not original game traces'
            ,'Old heap-header diagnostic used an incorrect upper-half far-pointer encoding; its header values are not allocator evidence'
        ]}
Path('tmp/battle-bitmap-heap-pointer-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps({'cases':len(rows),'lcd_commits':sum(r['lcd_commits_equal'] for r in rows),
                  'allocation_experiments':len(experiments),'experiment_lcd_commits':sum(r['lcd_commits_equal'] for r in experiments),
                  'pointers':[r['pointer'] for r in rows]}))
