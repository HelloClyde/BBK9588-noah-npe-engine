"""Run actual title/new-game and a reachable opening trigger or NPC in each NPE.

This verifies execution and C/SDL output consistency, not original pixels,
original audio, natural completion, or full compatibility of each game.
No character or plot-state injection. Build sdl-cross-npe-opening-probe first. SDL2 must be on the executable PATH.
"""
from pathlib import Path
import argparse
import json
import os
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--directory', type=Path, required=True)
parser.add_argument('--binary', type=Path, default=root/'npe-sdl/build/sdl-cross-npe-opening-probe.exe')
parser.add_argument('--output', type=Path, default=root/'tmp/cross-npe-opening')
parser.add_argument('--filter', default='')
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
                                  str(prefix.with_suffix('.jsonl'))], cwd=root,
                                 env=os.environ.copy(), stdout=log,
                                 stderr=subprocess.STDOUT, timeout=25)
            status = run.returncode
        except subprocess.TimeoutExpired:
            status = 'timeout'
    trace = prefix.with_suffix('.jsonl')
    events = [json.loads(line) for line in trace.read_text(encoding='utf-8').splitlines()] if trace.exists() else []
    log_text = prefix.with_suffix('.log').read_text(encoding='utf-8')
    complete = bool(events and events[-1]['phase'] == 'complete')
    errors = [line for line in log_text.splitlines()
              if any(word in line.lower() for word in ('cannot', 'unsupported', 'failed', 'assertion'))]
    passed = status == 0 and complete and not errors
    rows.append({'source': str(source.resolve()), 'exit': status, 'passed': passed,
                 'log': str(prefix.with_suffix('.log')), 'errors': errors, 'events': events})
    report = {'scope': 'actual title/new-game, reachable walking trigger, NPC dialogue or first exit, followed by actual SDL dispatch',
              'original_pixel_reference': False, 'original_audio_checked': False, 'audio_device_enabled': False,
              'diagnostic_attributes': False, 'plot_state_injection': False, 'games': rows,
              'passed': sum(row['passed'] for row in rows),
              'frames': sum(row['events'][-1].get('frames', 0) if row['events'] else 0 for row in rows)}
    (out/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(source.name, 'passed' if passed else 'failed', status,
          'frames', events[-1].get('frames', 0) if events else 0, flush=True)
raise SystemExit(0 if all(row['passed'] for row in rows) else 1)
