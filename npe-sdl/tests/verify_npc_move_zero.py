"""Check the DIV specification, original numeric samples, C fix and live xab continuation.

Existing nonzero-movement pixel references stay unchanged. Zero samples
compare original numeric state; their C entry pixels are self-consistency,
not original LCD. The old WASM execution_fault is retained as host evidence.
"""
import hashlib
import json
import re
from pathlib import Path

root = Path('tmp/npc-zero-div')
read = lambda p: json.loads(Path(p).read_text(encoding='utf-8'))
native = read(root/'native/report.json')
before = read(root/'before-checks.json')
after = read(root/'after-checks.json')
assert native['passed'] and len(native['cases']) == len(before['cases']) == len(after['cases']) == 3
assert not native['physical_hardware_checked'] and not native['original_lcd_checked']
prg = Path('tmp/NP560.PRG').read_bytes()
image = bytes(0x30000)+prg
assert hashlib.sha256(prg).hexdigest() == native['original_prg_sha256']
old = (root/'world-before.c').read_bytes()
current = Path('npe-sdl/world.c').read_bytes()
removed = b'    /* Original3C:8DF4 divides by this count; the reference runtime\r\n       faults for zero. Reject that service without applying a followup. */\r\n    if(!move->steps)return -1;\r\n'
added = b'    /* DIV by zero sets V (CN1916884A p28); the original does not trap.\r\n       With zero steps the loop at3C:90B6 skips all quotient uses, but\r\n       still writes the direction and publishes the cached followup. */\r\n'
assert old.count(removed) == 1 and old.replace(removed, added) == current
assert hashlib.sha256(old).hexdigest() == before['world_sha256']
assert hashlib.sha256(current).hexdigest() == after['world_sha256']
for reference, was, now in zip(native['cases'], before['cases'], after['cases']):
    assert reference['input']['name'] == was['name'] == now['name']
    assert was['exit'] == 1 and not was['state']['passed']
    assert now['exit'] == 0 and now['state']['passed']
    for region in reference['code_regions']:
        addr, size = region['address'], region['size']
        assert hashlib.sha256(image[addr:addr+size]).hexdigest() == region['sha256']
    expected, actual = reference['after'], now['state']
    for key in ['event', 'map_init', 'direction', 'x', 'y']:
        assert expected[key] == actual[key], (key, expected, actual)
    # The public C interface includes existing outer-controller mode0->1.
    assert actual['mode'] == (expected['mode'] or 1)
    assert actual['commits'] == 1 and actual['waits'] == actual['entry_pixel_differences'] == 0
previous = read('tmp/cross-npe-followup/final2/report.json')
old_xab = next(g for g in previous['games'] if 'xab' in g['source'])
new = read(root/'xab-followup/report.json')
xab = new['games'][0]
assert not old_xab['passed'] and old_xab['checkpoints'] == 9 and old_xab['frames'] == 532
assert xab['passed'] and xab['exit'] == 0 and xab['checkpoints'] == 12 and xab['frames'] == 743
assert new['diagnostic_attributes'] and not new['plot_state_injection']
assert not new['original_pixel_reference'] and not new['original_audio_checked']
assert Path(xab['source']).read_bytes() and hashlib.sha256(Path(xab['source']).read_bytes()).hexdigest() == native['xab_sha256'] == xab['source_sha256']
binary = Path('npe-sdl/build/sdl-cross-npe-followup-probe.exe')
assert hashlib.sha256(binary.read_bytes()).hexdigest() == xab['probe_sha256']
old_followups = [e for e in old_xab['events'] if e['phase'] == 'followup']
followups = [e for e in xab['events'] if e['phase'] == 'followup']
assert followups[:9] == old_followups
continued = followups[9]
assert (continued['map'], continued['progress'], continued['mode'], continued['event'], continued['frames']) == (1, 0, 1, 0, 532)
assert followups[-1]['map'] == 13 and followups[-1]['map_init'] == 0
assert xab['attribute_edits'] == old_xab['attribute_edits'] == 1
assert xab['events'][-1]['phase'] == 'complete'
regressions = read(root/'regressions.json')
assert len(regressions) == 4 and all(r['exit'] == 0 for r in regressions)
count = 0
for r in regressions:
    log = Path(r['log']).read_text(encoding='utf-8')
    assert 'Assertion failed' not in log
    commits = re.findall(r'commit\d+ C=(\d+) SDL=(\d+)', log)
    assert commits and all(pair == ('0', '0') for pair in commits)
    count += len(commits)
assert count == 425
diagnostic = read(root/'chain-diagnostic.json')
assert diagnostic[0]['exit'] and diagnostic[1]['exit']
assert diagnostic[0]['checks'] == diagnostic[1]['checks'] and len(diagnostic[0]['checks']) == 2
assert 'plays=0 expected_plays=1 cursor=0 expected_cursor=0' in diagnostic[0]['checks'][1]
for name in ['after-build.log', 'observer-build.log']:
    log = (root/name).read_text(encoding='utf-8')
    assert not re.search(r'\b(?:warning|error):', log, re.I)
wasm_fault = read('tmp/native-npc-move-resident-zero-fault.json')
assert wasm_fault['code'] == 'execution_fault'
result = dict(passed=True, original_numeric_cases=3, old_rejections=3, corrected_c_cases=3,
              xab_live_checkpoints=12, xab_sdl_submissions=743, old_first_nine_followups_unchanged=True,
              actual_zero_followup_frame=532, final_map=13, diagnostic_attribute_edits=1,
              regressions=regressions, unchanged_original_nonzero_lcd_submissions=count,
              zero_original_lcd_checked=False, physical_hardware_checked=False,
              old_wasm_fault_is_hardware_proof=False, observer_failure_reproduced_before_fix=True,
              world_sha256=after['world_sha256'], overall_estimate_percent=80, goal_complete=False)
(root/'verified-report.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print(json.dumps(result, ensure_ascii=False, indent=2))
