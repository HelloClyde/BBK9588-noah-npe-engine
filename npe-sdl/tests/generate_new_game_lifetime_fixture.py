import json
from pathlib import Path

rows = (json.loads(Path('tmp/native-new-game-lifetime.json').read_text(encoding='utf-8'))
        + json.loads(Path('tmp/native-new-game-lifetime-event-11.json').read_text(encoding='utf-8')))
state_fields = ['mode', 'event', 'map', 'progress', 'sx', 'sy', 'vx', 'vy', 'direction', 'money']
resident_fields = ['context', 'npcContext', 'kind', 'selected', 'first', 'last', 'row', 'ui',
                   'quantity', 'choice', 'descStart', 'descEnd', 'menu', 'member', 'equipmentSlot',
                   'gearSelected', 'descLength', 'menuPartyCount', 'exitChoice', 'exitSlot', 'music', 'musicSlot']


def resident(s):
    cache = ','.join('{' + ','.join(str(c[k]) for k in ['add', 'character', 'mode', 'next']) + '}' for c in s['partyCache'])
    return '{' + ','.join(str(s[k]) for k in resident_fields) + ',{' + cache + '}}'


def state(s):
    npc = ','.join('{' + ','.join(str(n[k]) for k in ['id', 'x', 'y', 'direction']) + '}' for n in s['npc'])
    actors = []
    for a in s['actors']:
        keys = ['id', 'active', 'level', 'maxHp', 'hp', 'maxQi', 'qi', 'attack', 'defense', 'experience', 'experienceToNext']
        actors.append('{' + ','.join([str(a[k]) for k in keys] + [
            '{' + ','.join(map(str, a[k])) + '}' for k in ['otherStats', 'gear', 'guards', 'skills']]) + '}')
    return '{' + ','.join(str(s[k]) for k in state_fields) + ',{' + npc + '},{' + ','.join(actors) + '}}'


out = ['/* Unmodified original constructor/UI; diagnostic resident inputs. */', 'static const RestartCase restart_cases[]={']
for r in rows:
    assert r['rngBefore'] == r['rngAfter'] and len(r['commits']) == 3
    expected_audio = [{'type': 'audio', 'command': 'stop', 'track_id': 20},
                      {'type': 'audio', 'command': 'play', 'track_id': 22, 'looping': True}] if r['music'] else []
    assert r['audio'] == expected_audio
    out.append('{' + ','.join(['"' + r['route'] + '"', str(r['music']), str(r['before']['event']), str(len(r['commits'])),
                              str(r['rngBefore']) + 'u', str(r['rngAfter']) + 'u',
                              resident(r['beforeResident']), resident(r['afterResident']), state(r['after'])]) + '},')
out.append('};')
Path('npe-sdl/tests/new_game_lifetime_native.h').write_text('\n'.join(out) + '\n', encoding='utf-8')
print(len(rows), 'original new/empty cases,', sum(len(r['commits']) for r in rows), 'commits')
