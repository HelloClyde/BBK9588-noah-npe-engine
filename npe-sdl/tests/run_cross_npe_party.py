"""Run current-NPE party services and their actual SDL followups.

Diagnostic active flags and battle attributes are used. This does not check
original pixels, audio, reachability from new game, or whole-game compatibility.
Build sdl-cross-npe-party-probe and put SDL2 on PATH before running.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--directory', type=Path, required=True)
parser.add_argument('--binary', type=Path, default=root/'npe-sdl/build/sdl-cross-npe-party-probe.exe')
parser.add_argument('--output', type=Path, default=root/'tmp/cross-npe-party')
parser.add_argument('--filter', default='')
parser.add_argument('--special-reference', type=Path,
                    help='Original special-party report; applied only to its exact source hash')
args = parser.parse_args()
native_reference = None
if args.special_reference:
    native_reference = json.loads((args.special_reference/'report.json').read_text(encoding='utf-8'))
    if not native_reference.get('passed'):
        parser.error('special original reference did not pass')
files = sorted(p for p in args.directory.glob('*.npe') if args.filter in p.name)
if not files:
    parser.error('no matching NPE files')
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
rows = []
for index, source in enumerate(files):
    prefix = out/f'case-{index:02d}'
    manifest = prefix.with_suffix('.path')
    manifest.write_text(str(source.resolve())+'\n', encoding='utf-8')
    trace = prefix.with_suffix('.jsonl')
    trace.write_text('', encoding='utf-8')
    with prefix.with_suffix('.log').open('w', encoding='utf-8') as log:
        try:
            command = [str(args.binary.resolve()), str(manifest), str(trace)]
            if native_reference and hashlib.sha256(source.read_bytes()).hexdigest() == native_reference['source_sha256']:
                command.append(str(args.special_reference.resolve()))
            run = subprocess.run(command,
                                 cwd=root, env=os.environ.copy(), stdout=log,
                                 stderr=subprocess.STDOUT, timeout=35)
            status = run.returncode
        except subprocess.TimeoutExpired:
            status = 'timeout'
    events = [json.loads(line) for line in trace.read_text(encoding='utf-8').splitlines()]
    log_text = prefix.with_suffix('.log').read_text(encoding='utf-8')
    complete = bool(events and events[-1]['phase'] == 'complete')
    errors = [line for line in log_text.splitlines()
              if any(word in line.lower() for word in ('cannot', 'unsupported', 'failed', 'assertion'))]
    passed = status == 0 and complete and not errors
    covered = sum(event['phase'] == 'service' for event in events) > 0
    rows.append({'source': str(source.resolve()), 'exit': status, 'passed': passed, 'covered': covered,
                 'log': str(prefix.with_suffix('.log')), 'errors': errors, 'events': events})
    totals = {key: sum(row['events'][-1].get(key, 0) for row in rows
                      if row['events'] and row['events'][-1]['phase'] == 'complete')
              for key in ('frames', 'party_services', 'leader_services', 'battle_rounds', 'unresolved_maps')}
    report = {'scope': 'current-NPE party/leader service and actual SDL followups',
              'original_pixel_reference': False, 'original_audio_checked': False,
              'diagnostic_active_flags': True, 'diagnostic_battle_attributes': True,
              'games': rows, 'passed': sum(row['passed'] for row in rows),
              'covered_files': sum(row['covered'] for row in rows),
              'covered_passed_files': sum(row['passed'] and row['covered'] for row in rows),
              'special_original_reference': str(args.special_reference.resolve()) if args.special_reference else None,
              'special_original_services': sum(event['phase'] == 'special-original' for row in rows for event in row['events']), **totals}
    (out/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(source.name, ('passed' if covered else 'no party services') if passed else 'incomplete', status,
          events[-1] if events else 'no stages', flush=True)
raise SystemExit(0 if all(row['passed'] for row in rows) else 1)
