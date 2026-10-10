import json
import sys
from pathlib import Path
def a(v):return '{'+','.join(map(str,v))+'}'
resource_cycle='--resource-cycle' in sys.argv
reentry_groups='--reentry-groups' in sys.argv
reentry_mixed='--reentry-mixed' in sys.argv or reentry_groups
reentry_two_attacks='--reentry-two-attacks' in sys.argv
reentry_attack='--reentry-attack' in sys.argv or reentry_two_attacks or reentry_mixed
phase_resident='--phase-resident' in sys.argv or reentry_attack
command_resident='--command-resident' in sys.argv or phase_resident
menu_resident='--menu-resident' in sys.argv or command_resident
reentry_actions='--reentry-actions' in sys.argv or menu_resident
record_reentry='--record-reentry' in sys.argv or reentry_actions
exit_commands='--exit-command-records' in sys.argv or record_reentry
exit_records='--exit-records' in sys.argv or exit_commands
coordinate_targets='--coordinate-targets' in sys.argv
live_coordinates='--live-coordinates' in sys.argv or coordinate_targets
party_descriptors='--party-descriptors' in sys.argv or live_coordinates or exit_records
zero_flee='--zero-flee' in sys.argv or party_descriptors
zero_menu='--zero-menu' in sys.argv or zero_flee
zero_count='--zero-count' in sys.argv or zero_menu
stem='battle-phase-resident' if phase_resident else 'battle-command-resident' if command_resident else 'battle-menu-resident' if menu_resident else 'battle-reentry-actions' if reentry_actions else 'battle-record-reentry' if record_reentry else 'battle-exit-command-records' if exit_commands else 'battle-exit-records' if exit_records else 'battle-coordinate-targets' if coordinate_targets else 'battle-live-coordinates' if live_coordinates else 'party-battle-records' if party_descriptors else 'battle-zero-flee' if zero_flee else 'battle-zero-menu' if zero_menu else 'battle-zero-entry' if zero_count else 'enemy-record-entry'
if reentry_attack:stem='battle-reentry-attack'
if reentry_attack and '--reentry-one-enemy' in sys.argv:stem+='-one-enemy'
if reentry_two_attacks:stem='battle-reentry-two-attacks'
if reentry_mixed:stem='battle-reentry-mixed'
if reentry_groups:stem='battle-reentry-groups'
if resource_cycle:stem=stem.replace('battle-','battle-resources-',1)
rows=json.loads(Path('tmp/native-'+stem+'.json').read_text(encoding='utf8'))['rows'];assert len(rows)==(1 if reentry_two_attacks or reentry_mixed else 3 if record_reentry else 5 if exit_commands else 7 if exit_records else 2 if coordinate_targets else 4 if zero_flee else 6 if zero_menu else 3 if zero_count else 4)
out=['/* Original public battle entry, generated from recorded firmware states. */','static const EntryCase entry_cases[]={']
for r in rows:
 b,z=r['before'],r['after'];vals=[len(r['commits']),b['hp'],b['encounters'],b['encounterSteps'],z['hp'],z['mode'],z['encounterSteps'],z['rng'],z['chaseSlot'],b['rng'],b['partyInput']]
 s='{'+json.dumps(r['name'])+','+','.join(map(str,vals))+','+a(b['hpMap'])+','+a(b['encounterIds'])+','
 s+='{'+','.join(a([n[k] for k in ('id','graphic','first','last','direction','x','y','moving')]) for n in b['npc'])+'},'
 for n in (b['chase'],z['chase']):s+='{'+','.join(a([c[k] for k in ('graphic','first','last','enemy','x','y','cooldown')]) for c in n)+'},'
 s+=a(z['selectedIds'])+','+'{'+','.join('{'+','.join(str(n[k]) for k in ('hp','maxHp','qi','maxQi','attack','defense','speed'))+','+a(n['statuses'])+'}' for n in z['battleRecords'])+'},'
 s+=a(b['selectedIds'])+','+str(b['num'])+','+str(z['num'])+','+str(z['flag'])+'},';out.append(s)
out.append('};');out.append('static const uint8_t enemy_entry_records[][3][148]={')
out.extend('{'+','.join(a(record) for record in r['after']['residentEnemyRecords'])+'},' for r in rows)
out.append('};');out.append('static const uint32_t enemy_entry_directory[]='+a(r['before']['enemyDirectory'] for r in rows)+';')
if zero_menu:
 out.append('static const EntryNavigation entry_navigation[]['+str(len(rows[0]['menuNavigation']) if coordinate_targets or exit_records else 16 if zero_flee else 14)+']={')
 for r in rows:
  nav=[]
  for n in r['menuNavigation']:
   z=n['after'];menu=z['battleMenu'];icon={10:1,20:0,30:2,40:3}.get(menu,2)
   level={31:1,32:2}.get(menu,0)
   nav.append(a([n['key'],len(n['frames']),icon,level,z['battleRow'],z['battleItemMode'],z['battleActor'],z['mode']]+([z['displayTarget']-r['before']['partyInput']-1] if coordinate_targets else [])))
  out.append('{'+','.join(nav)+'},')
 out.append('};')
if zero_flee and not coordinate_targets and not exit_records:
 out.append('static const uint32_t flee_rng[]='+a(r['menuNavigation'][-1]['after']['rng'] for r in rows)+';')
 out.append('static const unsigned flee_hp[][3]={' + ','.join(a(r['menuNavigation'][-1]['after']['hpMap']) for r in rows)+'};')
 for name,key in [('flee_map_records','residentMapRecords'),('flee_battle_records','residentBattlePartyRecords')]:
  out.append('static const uint8_t '+name+'[][3][148]={')
  out.extend('{'+','.join(a(record) for record in r['menuNavigation'][-1]['after'][key])+'},' for r in rows)
  out.append('};')
if party_descriptors:
 out.append('static const uint32_t entry_voices[][4]={' + ','.join(a(r['before']['voiceDescriptors']) for r in rows)+'};')
 for name,key,state in [('entry_map_before','residentMapRecords','before'),('entry_map_after','residentMapRecords','after'),('entry_party_after','residentBattlePartyRecords','after')]:
  out.append('static const uint8_t '+name+'[][3][148]={')
  out.extend('{'+','.join(a(record) for record in r[state][key])+'},' for r in rows)
  out.append('};')
if live_coordinates:
 out.append('static const uint8_t entry_coordinates[][6][2]={')
 out.extend('{'+','.join(a(xy) for xy in r['config']['partyCoords']+r['config']['enemyCoords'])+'},' for r in rows)
 out.append('};')
if exit_records:
 if phase_resident:
  out.append('static const uint8_t exit_phase[]='+a(r['menuNavigation'][-1]['after']['battleMenu'] for r in rows)+';')
 out.append('static const uint8_t exit_scripted[]='+a(int(r['config'].get('scripted',False)) for r in rows)+';')
 out.append('static const unsigned exit_mode[]='+a(r['menuNavigation'][-1]['after']['mode'] for r in rows)+';')
 out.append('static const uint32_t exit_rng[]='+a(r['menuNavigation'][-1]['after']['rng'] for r in rows)+';')
 out.append('static const unsigned exit_result[]='+a(1 if r['config']['outcome']=='win' else 3 if r['config']['outcome']=='flee' else 2 for r in rows)+';')
 for name,key,state in [('exit_input_party','residentBattlePartyRecords','exitInput'),('exit_input_enemy','residentEnemyRecords','exitInput'),('exit_map_records','residentMapRecords',None),('exit_battle_records','residentBattlePartyRecords',None)]:
  out.append('static const uint8_t '+name+'[][3][148]={')
  out.extend('{'+','.join(a(record) for record in (r[state] if state else r['menuNavigation'][-1]['after'])[key])+'},' for r in rows)
  out.append('};')
if exit_commands:
 for name,state in [('exit_inventory_before','exitInput'),('exit_inventory_after',None)]:
  out.append('static const uint16_t '+name+'[][3]={'+','.join(a((r[state] if state else r['menuNavigation'][-1]['after'])['commandInventory']) for r in rows)+'};')
if record_reentry:
 out.append('static const unsigned reentry_party[]='+a(r['config']['reentryParty'] for r in rows)+';')
 if reentry_attack:
  out.append('static const uint8_t reentry_enemy_count[]='+a(r['config']['reentryCount'] for r in rows)+';')
  out.append('static const uint16_t reentry_enemy_ids[][3]={'+','.join(a(r['config']['reentryIds']) for r in rows)+'};')
 out.append('static const unsigned reentry_commits[]='+a(len(r['reentry']['frames']) for r in rows)+';')
 out.append('static const uint32_t reentry_rng[]='+a(r['reentry']['after']['rng'] for r in rows)+';')
 for phase in ['retained','input','after']:
  for label,key,count in [('map','allMapRecords',3),('battle','allBattleRecords',6)]:
   out.append('static const uint8_t reentry_'+phase+'_'+label+'[]['+str(count)+'][148]={')
   out.extend('{'+','.join(a(record) for record in r['reentry'][phase][key])+'},' for r in rows)
   out.append('};')
if reentry_actions:
 if reentry_two_attacks:
  out.append('static const uint8_t reentry_target_input[][3]={'+','.join(a(rec[126] for rec in r['reentry']['menuInput']['allBattleRecords'][:3]) for r in rows)+'};')
 if menu_resident:
  out.append('static const uint8_t reentry_menu_input[][2]={'+','.join(a([r['reentry']['menuInput']['battleRow'],r['reentry']['menuInput']['battleItemMode']]) for r in rows)+'};')
 if command_resident:
  out.append('static const uint8_t reentry_command_input[][2]={'+','.join(a([r['reentry']['menuInput']['battleActor'],r['reentry']['menuInput']['battlePreviousKind']]) for r in rows)+'};')
 if phase_resident:
  out.append('static const uint8_t reentry_phase_input[]='+a(r['reentry']['menuInput']['battleMenu'] for r in rows)+';')
  out.append('static const uint8_t reentry_phase_after[]='+a(r['reentry']['after']['battleMenu'] for r in rows)+';')
 actions=[r['reentry']['actions'] for r in rows]
 limit=max(map(len,actions))
 out.append('static const unsigned reentry_action_steps[]='+a(map(len,actions))+';')
 out.append('static const ReentryAction reentry_actions[]['+str(limit)+']={')
 for steps in actions:
  parts=[]
  for n in steps:
   z=n['after'];menu=z['battleMenu'];icon={10:1,20:0,30:2,40:3}.get(menu,2);level={31:1,32:2}.get(menu,0)
   ui=a([n['key'],len(n['frames']),icon,level,z['battleRow'],z['battleItemMode'],z['battleActor'],z['mode']])
   parts.append('{'+ui+','+str(int(n['execute']))+','+str(z['rng'])+(','+str(z['battlePreviousKind']) if command_resident else '')+(','+str(z['battleMenu']) if phase_resident else '')+'}')
  out.append('{'+','.join(parts)+'},')
 out.append('};')
 for label,key,count in [('map','allMapRecords',3),('battle','allBattleRecords',6)]:
  out.append('static const uint8_t reentry_action_'+label+'[]['+str(limit)+']['+str(count)+'][148]={')
  out.extend('{'+','.join('{'+','.join(a(record) for record in n['after'][key])+'}' for n in steps)+'},' for steps in actions)
  out.append('};')
 if reentry_mixed:
  out.append('static const uint8_t mixed_inventory_item[]='+a(r['config'].get('mixedItem',46) for r in rows)+';')
  for label,key,count in [('map','allMapRecords',3),('battle','allBattleRecords',6)]:
   out.append('static const uint8_t mixed_input_'+label+'[]['+str(count)+'][148]={')
   out.extend('{'+','.join(a(record) for record in r['reentry']['menuInput'][key])+'},' for r in rows)
   out.append('};')
  out.append('static const uint16_t mixed_stock[]['+str(limit)+']={'+','.join(a(n['after']['mixedInventory'] for n in steps) for steps in actions)+'};')
  for label,key,size in [('skill','skillCache',144),('item','itemCache',142)]:
   out.append('static const uint8_t mixed_'+label+'_ids[]['+str(limit)+']={'+','.join(a(n['after'][key]['id'] for n in steps) for steps in actions)+'};')
   out.append('static const uint8_t mixed_'+label+'_bytes[]['+str(limit)+']['+str(size)+']={')
   out.extend('{'+','.join(a(n['after'][key]['bytes']) for n in steps)+'},' for steps in actions)
   out.append('};')
if resource_cycle:
 for name,key in [('entry_display_before','before'),('entry_display_after','after')]:
  out.append('static const uint8_t '+name+'[]='+a(r[key]['displayTarget'] for r in rows)+';')
 out.append('static const uint8_t exit_display_target[]='+a(r['menuNavigation'][-1]['after']['displayTarget'] for r in rows)+';')
 if record_reentry:
  out.append('static const uint8_t reentry_display_target[][3]={'+','.join(a(r['reentry'][key]['displayTarget'] for key in ['retained','input','after']) for r in rows)+'};')
 if reentry_actions:
  out.append('static const uint8_t action_display_target[]['+str(limit)+']={'+','.join(a(n['after']['displayTarget'] for n in r['reentry']['actions']) for r in rows)+'};')
 for name,key in [('entry_resource_before','before'),('entry_resource_after','after')]:
  out.append('static const uint16_t '+name+'[]='+a(r[key]['resourceFlags'] for r in rows)+';')
 out.append('static const uint16_t exit_resource_flags[]='+a(r['menuNavigation'][-1]['after']['resourceFlags'] for r in rows)+';')
 if record_reentry:
  out.append('static const uint16_t reentry_resource_flags[][3]={'+','.join(a(r['reentry'][key]['resourceFlags'] for key in ['retained','input','after']) for r in rows)+'};')
 if reentry_actions:
  out.append('static const uint16_t action_resource_flags[]['+str(limit)+']={'+','.join(a(n['after']['resourceFlags'] for n in r['reentry']['actions']) for r in rows)+'};')
Path('npe-sdl/tests/'+stem.replace('-','_')+'_native.h').write_text('\n'.join(out)+'\n',encoding='utf8')
print(len(rows),'original public entries;',sum(len(r['commits']) for r in rows),'LCD frames')
if menu_resident and not reentry_mixed:
 load_stem='battle-phase-load' if phase_resident else 'battle-command-rows-load' if command_resident else 'battle-menu-rows-load'
 loads=json.loads(Path('tmp/native-'+load_stem+'.json').read_text(encoding='utf8'))
 assert [r['entry'] for r in loads]==['system','title','new','empty']
 assert all(len(r['beforeBytes'])==len(r['afterBytes'])==2 for r in loads)
 text=['/* Original four warm constructors; retained full list-selection bytes. */',
       'static const MenuRowsLoad menu_rows_load[]={']
 text += ['{'+json.dumps(r['entry'])+','+a(r['beforeBytes'])+','+a(r['afterBytes'])+'},' for r in loads]
 text += ['};']
 Path('npe-sdl/tests/'+load_stem.replace('-','_')+'_native.h').write_text('\n'.join(text)+'\n',encoding='utf8')
