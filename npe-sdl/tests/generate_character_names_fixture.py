import json
from pathlib import Path

rows=json.loads(Path('tmp/native-character-names.json').read_text(encoding='utf8'))
def arr(v):return '{'+','.join(map(str,v))+'}'
def names(v):return '{'+','.join(arr(n) for n in v+[[0]*12]*(3-len(v)))+'}'
out=['/* Independent original RAM name copies, including bytes after NUL. */',
 'typedef struct {uint8_t before[3][12],entry[3][12],mutated[3][12],exit[3][12],swapped[3][12],reentry[3][12];unsigned alias;uint8_t reserved[3][12],canceled[3][12],wrap_before[3][12],wrap_reserved[3][12],wrap_canceled[3][12];} NameCase;',
 'static const NameCase name_cases[]={']
assert len(rows)==3
for r in rows:
    n=len(r['namesEntry'])
    assert r['namesEntry']==r['namesBefore'][:n]
    assert r['namesExit']==r['namesMutated']+r['namesBefore'][n:]
    swapped=r['namesRemoved'] or r['namesExit']
    if r['namesRemoved']:assert swapped==[r['namesExit'][i] for i in [0,2,1]]
    assert r['namesReentry']==swapped[:len(r['namesReentry'])]
    alias=r['alias'];zero=[[0]*12]*3
    if alias:
        assert alias['before']['map']==r['namesBefore']
        assert alias['before']['battle']==r['namesMutated']
        assert alias['reserved']['battle']==r['namesMutated']==alias['canceled']['battle']
        assert alias['canceled']['map']==r['namesBefore']
        assert [alias[k]['actor'] for k in ['before','reserved','canceled']]==[0,1,0]
        assert [alias[k]['quantity'] for k in ['wrapBefore','wrapReserved','wrapCanceled']]==[65535,65534,0]
        assert [alias[k]['actor'] for k in ['wrapBefore','wrapReserved','wrapCanceled']]==[0,1,0]
        for k in ['wrapBefore','wrapReserved','wrapCanceled']:assert alias[k]['battle']==r['namesMutated']
    out.append('{'+','.join(names(r[k] or r['namesExit']) for k in ['namesBefore','namesEntry','namesMutated','namesExit','namesRemoved','namesReentry'])+','+
       str(int(bool(alias)))+','+','.join(names(alias[k]['map'] if alias else zero) for k in ['reserved','canceled','wrapBefore','wrapReserved','wrapCanceled'])+'},')
out+=['};','']
Path('npe-sdl/tests/character_names_native.h').write_text('\n'.join(out),encoding='utf8')
print('3 name lifetimes,1 swap,1 real empty throw/cancel alias,12 entry LCD commits')
