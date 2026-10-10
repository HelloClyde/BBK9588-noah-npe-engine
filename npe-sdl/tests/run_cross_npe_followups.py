"""Run actual title/new-game and several distinct reachable followups in each NPE.

This verifies execution and C/SDL output consistency, not original pixels,
original audio, natural completion, or full compatibility of each game.
Optional user-authorized character-stat boost; no plot-state injection. Build sdl-cross-npe-followup-probe first. SDL2 must be on the executable PATH.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--directory', type=Path, required=True)
parser.add_argument('--binary', type=Path, default=root/'npe-sdl/build/sdl-cross-npe-followup-probe.exe')
parser.add_argument('--output', type=Path, default=root/'tmp/cross-npe-followup')
parser.add_argument('--filter', default='')
parser.add_argument('--limit', type=int, default=12)
parser.add_argument('--boost', action='store_true')
args = parser.parse_args()
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
    with prefix.with_suffix('.log').open('w', encoding='utf-8') as log:
        try:
            run = subprocess.run([str(args.binary.resolve()), str(manifest),
                                  str(prefix.with_suffix('.jsonl')), str(args.limit), str(int(args.boost))], cwd=root,
                                 env=os.environ.copy(), stdout=log,
                                 stderr=subprocess.STDOUT, timeout=40)
            status = run.returncode
        except subprocess.TimeoutExpired:
            status = 'timeout'
    trace = prefix.with_suffix('.jsonl')
    events = [json.loads(line) for line in trace.read_text(encoding='utf-8').splitlines()] if trace.exists() else []
    log_text = prefix.with_suffix('.log').read_text(encoding='utf-8')
    complete = bool(events and events[-1]['phase'] in ('complete', 'exhausted'))
    errors = [line for line in log_text.splitlines()
              if any(word in line.lower() for word in ('cannot', 'unsupported', 'failed', 'assertion'))
              and not re.fullmatch(r'escape failed; mode \d+, HP \d+', line)]
    passed = status == 0 and complete and not errors
    rows.append({'source': str(source.resolve()), 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                 'probe_sha256': hashlib.sha256(args.binary.read_bytes()).hexdigest(), 'exit': status, 'passed': passed,
                 'log': str(prefix.with_suffix('.log')), 'errors': errors, 'events': events,
                 'frames': max((e.get('frames', 0) for e in events), default=0),
                 'checkpoints': max((e.get('checkpoint', 0) for e in events), default=0),
                 'attribute_edits': max((e.get('attribute_edits', 0) for e in events), default=0),
                 'stop_reason': events[-1]['phase'] if events else 'no-trace'})
    report = {'scope': 'actual title/new-game and distinct reachable walking events, NPCs and exits with live followups',
              'original_pixel_reference': False, 'original_audio_checked': False, 'audio_device_enabled': False,
              'diagnostic_attributes': args.boost, 'plot_state_injection': False, 'games': rows,
              'passed': sum(row['passed'] for row in rows),
              'limit': args.limit,
              'checkpoints': sum(row['checkpoints'] for row in rows),
              'frames': sum(row['frames'] for row in rows)}
    (out/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(source.name, 'passed' if passed else 'failed', status,
          'frames', rows[-1]['frames'], flush=True)
raise SystemExit(0 if all(row['passed'] for row in rows) else 1)
