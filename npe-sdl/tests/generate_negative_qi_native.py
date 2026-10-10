import json
from pathlib import Path

names = ['minus-one', 'minimum', 'zero', 'below', 'exact', 'negative-cost',
         'minimum-cost', 'both-negative', 'queued-drain']
samples = [json.loads(Path(f'tmp/native-negative-qi-{name}.json').read_text(encoding='utf-8'))
           for name in names]
lines = ['/* Generated from independent original runtime RAM/resource fixtures. */',
         '#ifndef NEGATIVE_QI_NATIVE_H', '#define NEGATIVE_QI_NATIVE_H',
         'typedef struct { uint16_t hp, qi; int16_t status[4]; } QiRecord;',
         'typedef struct { const char *name; unsigned cost, party, rejected; QiRecord before[5], after[5]; } QiCase;',
         'static const QiCase qi_cases[] = {']
def records(event):
    return ','.join('{%u,%u,{%s}}' % (r['hp'], r['qi'],
                   ','.join(str(v if v < 32768 else v - 65536) for v in r['status']))
                   for r in event['records'])
for s in samples:
    c = s['config']
    qi, cost = (v if v < 32768 else v - 65536 for v in [c['qi'], c['cost']])
    rejected = qi < cost
    assert s['events'][0]['mode'] == s['events'][-1]['mode'] == 3
    lines.append('{"%s",%u,%u,%u,{%s},{%s}},' %
                 (s['variant'], c['cost'], 2 if c.get('queued') else 1, rejected,
                  records(s['events'][0]), records(s['events'][-1])))
lines += ['};', '#endif', '']
Path('npe-sdl/tests/negative_qi_native.h').write_text('\n'.join(lines), encoding='utf-8', newline='\n')
print(f'{len(samples)} original signed QI/fee sequences')
