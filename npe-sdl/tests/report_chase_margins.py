"""Describe native margin changes without treating them as a renderer rule."""
import json
from collections import Counter
from pathlib import Path

root = Path(__file__).resolve().parents[2]
directory = root / 'tmp'
rows = json.loads((directory / 'native-chase-margin.json').read_text())['rows']
report = []
for index in range(1, len(rows), 2):
    row = rows[index]
    assert rows[index - 1]['name'] == 'dirty-' + row['name']
    assert len(row['commits']) == 1
    before = (directory / f'native-chase-margin-{index}-before.idx').read_bytes()
    after = (directory / f'native-chase-margin-{index}-commit-0.idx').read_bytes()
    assert len(before) == len(after) == 240 * 120
    assert before == (directory / f'native-chase-margin-{index-1}-after.idx').read_bytes()
    positions = [i for i in range(116 * 240, 120 * 240) if before[i] != after[i]]
    bounds = ([min(i % 240 for i in positions), min(i // 240 for i in positions),
               max(i % 240 for i in positions), max(i // 240 for i in positions)]
              if positions else None)
    item = {'name': row['name'], 'changed_bottom_pixels': len(positions),
            'bounds': bounds,
            'before_colors': dict(Counter(before[i] for i in positions)),
            'after_colors': dict(Counter(after[i] for i in positions)),
            'view': {key: row['before'][key] for key in
                     ('heroX', 'heroY', 'screenX', 'screenY', 'viewX', 'viewY', 'direction')},
            'mode': row['after']['mode'], 'rng_after': row['after']['rng']}
    report.append(item)
    print(row['name'], len(positions), bounds, item['before_colors'], item['after_colors'])
(directory / 'chase-margin-report.json').write_text(
    json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
