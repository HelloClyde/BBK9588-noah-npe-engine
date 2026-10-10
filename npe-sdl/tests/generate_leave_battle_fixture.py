import json
from pathlib import Path

rows = (json.loads(Path('tmp/native-leave-battle.json').read_text(encoding='utf-8'))
        + json.loads(Path('tmp/native-leave-notice-input.json').read_text(encoding='utf-8'))
        + json.loads(Path('tmp/native-leave-notice-chain-input.json').read_text(encoding='utf-8')))
fields = ['mode', 'event', 'map', 'progress', 'sx', 'sy', 'vx', 'vy', 'direction', 'money',
          'enabled', 'slot', 'context', 'menu', 'member', 'equipmentSlot', 'gearSelected',
          'descLength', 'descStart', 'descEnd', 'menuPartyCount']


def actor(a):
    keys = ['id', 'active', 'portrait', 'level', 'maxHp', 'hp', 'maxQi', 'qi', 'attack',
            'defense', 'experience', 'experienceToNext']
    return '{' + ','.join([str(a[k]) for k in keys] + [
        '{' + ','.join(map(str, a[k])) + '}' for k in ['otherStats', 'gear', 'guards', 'skills']]) + '}'


def state(s):
    npc = ','.join('{' + ','.join(str(n[k]) for k in ['id', 'x', 'y', 'direction']) + '}' for n in s['npc'])
    return '{' + ','.join(str(s[k]) for k in fields) + ',{' + npc + '},{' + ','.join(actor(a) for a in s['actors']) + '}}'


def battle(b):
    records = ','.join('{' + ','.join(str(r[k]) for k in ['hp', 'maxHp', 'qi', 'maxQi', 'attack', 'defense', 'speed'])
                      + ',{' + ','.join(map(str, r['status'])) + '}}' for r in b['records'])
    return '{' + ','.join(str(b[k]) for k in ['party', 'actor', 'target']) + ',{' + ','.join(map(str, b['enemies'])) + '},{' + records + '}}'


out = ['/* Original UI-loaded map, unmodified NPE group6 -> group8. */', 'static const LeaveCase leave_cases[]={']
for r in rows:
    rng = r.get('rng', {'before': 11, 'after': 11, 'calls': 0})
    commits = ','.join(state(c['state']) for c in r['commits'])
    out.append('{"' + r['name'] + '",' + str(len(r['commits'])) + ',' + str(r.get('mask', 0)) + ',' + state(r['before']) + ','
               + state(r['after']) + ',{' + commits + '},' + battle(r['battle']) + ','
               + ','.join(str(rng[k]) + ('u' if k != 'calls' else '') for k in ['before', 'after', 'calls']) + '},')
out.append('};')
Path('npe-sdl/tests/leave_battle_native.h').write_text('\n'.join(out) + '\n', encoding='utf-8')
print(len(rows), 'original cases,', sum(len(r['commits']) for r in rows), 'commits')
