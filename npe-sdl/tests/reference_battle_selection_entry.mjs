import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const triggers=process.argv.includes('--triggers');
const resourceCycle=process.argv.includes('--resource-cycle');
const reentryGroups=process.argv.includes('--reentry-groups');
const reentryMixed=process.argv.includes('--reentry-mixed')||reentryGroups;
const mixedItem=reentryGroups?64:46;
const reentryTwoAttacks=process.argv.includes('--reentry-two-attacks');
const reentryAttack=process.argv.includes('--reentry-attack')||reentryTwoAttacks||reentryMixed;
const reentryOneEnemy=process.argv.includes('--reentry-one-enemy');
const phaseResident=process.argv.includes('--phase-resident')||reentryAttack;
const commandResident=process.argv.includes('--command-resident')||phaseResident;
const menuResident=process.argv.includes('--menu-resident')||commandResident;
const reentryActions=process.argv.includes('--reentry-actions')||menuResident;
const recordReentry=process.argv.includes('--record-reentry')||reentryActions;
const exitCommands=process.argv.includes('--exit-command-records')||recordReentry;
const exitRecords=process.argv.includes('--exit-records')||exitCommands;
const coordinateTargets=process.argv.includes('--coordinate-targets');
const liveCoordinates=process.argv.includes('--live-coordinates')||coordinateTargets;
const partyDescriptors=process.argv.includes('--party-descriptors')||liveCoordinates||exitRecords;
const zeroFlee=process.argv.includes('--zero-flee')||partyDescriptors;
const zeroMenu=process.argv.includes('--zero-menu')||zeroFlee;
const zeroCount=process.argv.includes('--zero-count')||zeroMenu;
const enemyRecords=process.argv.includes('--enemy-records')||zeroCount;
const baseOutputName=phaseResident?'native-battle-phase-resident':commandResident?'native-battle-command-resident':menuResident?'native-battle-menu-resident':reentryActions?'native-battle-reentry-actions':recordReentry?'native-battle-record-reentry':exitCommands?'native-battle-exit-command-records':exitRecords?'native-battle-exit-records':coordinateTargets?'native-battle-coordinate-targets':liveCoordinates?'native-battle-live-coordinates':partyDescriptors?'native-party-battle-records':zeroFlee?'native-battle-zero-flee':zeroMenu?'native-battle-zero-menu':zeroCount?'native-battle-zero-entry':enemyRecords?'native-enemy-record-entry':'native-battle-selection-entry';
const selectedOutputName=reentryGroups?'native-battle-reentry-groups':reentryMixed?'native-battle-reentry-mixed':reentryTwoAttacks?'native-battle-reentry-two-attacks':reentryAttack?'native-battle-reentry-attack'+(reentryOneEnemy?'-one-enemy':''):baseOutputName;
const outputName=resourceCycle?selectedOutputName.replace('native-battle-','native-battle-resources-'):selectedOutputName;
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/empty-title-first-map-snapshot.bin');
const configs=zeroCount?[
 {name:'zero-one-party',hp:100,steps:1000,encounters:1,enemyIds:[],ids:[1,0x8001,0xffff],num:0},
 {name:'zero-two-party',hp:100,party:2,hps:[100,80,0],steps:1000,encounters:1,enemyIds:[],ids:[0,0,0],num:0},
 {name:'zero-three-party',hp:100,party:3,hps:[100,80,120],steps:1000,encounters:1,enemyIds:[],ids:[0xffff,32768,16385],num:0},
]:enemyRecords?[
 {name:'wrapped-first-enemy',hp:100,steps:1000,encounters:1,enemyIds:[],ids:[16385,0x8001,0xffff],num:1},
 {name:'wrapped-pair',hp:100,steps:1000,encounters:1,enemyIds:[],ids:[16385,16386,0x1234],num:2},
 {name:'wrapped-enemy-51',hp:100,steps:1000,encounters:1,enemyIds:[],ids:[16435,0x8001,0xffff],num:1},
 {name:'negative-records-with-middle',hp:100,steps:1000,encounters:1,enemyIds:[],ids:[0xffff,16386,0x8000],num:3},
]:[
 {name:'empty-retain-one',hp:100,steps:1000,encounters:1,enemyIds:[],ids:[1,0x8001,0xffff],num:1},
 {name:'empty-retain-two',hp:100,steps:1000,encounters:1,enemyIds:[],ids:[1,2,0x1234],num:2},
 {name:'single-retain-words',hp:100,steps:1000,encounters:1,enemyIds:[1],ids:[0x1234,0x8001,0xffff],num:3},
 {name:'double-retain-word',hp:100,steps:1000,encounters:1,enemyIds:[1,2],ids:[0x1234,0x8001,0xffff],num:3,seed:1},
];
if(zeroMenu)configs.push(
 {name:'one-enemy-menu',hp:100,steps:1000,encounters:1,enemyIds:[],ids:[1,0x8001,0xffff],num:1},
 ...(!zeroFlee?[
  {name:'two-enemy-menu',hp:100,steps:1000,encounters:1,enemyIds:[],ids:[1,2,0xffff],num:2},
  {name:'three-enemy-menu',hp:100,steps:1000,encounters:1,enemyIds:[],ids:[1,2,3],num:3},
 ]:[])
);
const rows=[];
if(partyDescriptors)configs.splice(0,configs.length,
 {name:'sex-zero-retains',party:1,sex:[0]},
 {name:'two-custom-voices',party:2,sex:[2,1],voices:[0x12345678,0x9abcdef0,0x10203040,0x50607080]},
 {name:'three-nonstandard-sex',party:3,sex:[255,2,1],voices:[0x01020304,0x05060708,0x11223344,0x55667788]},
 {name:'sex-one-zero-voice',party:1,sex:[1],voices:[0,0,0,0]});
if(partyDescriptors)for(const c of configs)Object.assign(c,{hp:100,hps:[100,80,120],steps:1000,encounters:1,enemyIds:[],ids:[0,0,0],num:0});
if(liveCoordinates)configs.splice(0,configs.length,
 {name:'live-one-enemy',party:1,num:1,ids:[1,0,0],partyCoords:[[155,45]],enemyCoords:[[38,32],[0,0],[0,0]]},
 {name:'live-two-enemies',party:2,num:2,ids:[1,2,0],partyCoords:[[128,69],[172,44]],enemyCoords:[[74,22],[46,43],[0,0]]},
 {name:'live-three-enemies',party:3,num:3,ids:[1,2,3],partyCoords:[[128,69],[172,44],[195,24]],enemyCoords:[[74,22],[46,43],[6,58]]},
 {name:'live-empty-enemies',party:3,num:0,ids:[0,0,0],partyCoords:[[128,69],[172,44],[195,24]],enemyCoords:[[210,92],[17,14],[99,61]]});
if(liveCoordinates)for(const c of configs)Object.assign(c,{hp:100,hps:[100,80,120],steps:1000,encounters:1,enemyIds:[],sex:[1,1,1]});
if(coordinateTargets)configs.splice(0,configs.length,...configs.filter(c=>c.num>1));
if(exitRecords){
 configs.splice(0,configs.length,
  {name:'exit-win-one',party:1,outcome:'win'},
  {name:'exit-win-three-fallen',party:3,outcome:'win'},
  {name:'exit-win-level',party:1,outcome:'win',upgrade:true},
  {name:'exit-lose-one',party:1,outcome:'lose'},
  {name:'exit-lose-three',party:3,outcome:'lose'},
  {name:'exit-scripted-win',party:2,outcome:'win',scripted:true},
  {name:'exit-scripted-lose-map',party:2,outcome:'lose',scripted:true});
 for(const c of configs)Object.assign(c,{hp:100,hps:[100,80,120],steps:1000,encounters:1,enemyIds:[],ids:[1,0,0],num:1,seed:11,sex:[1,2,255]});
}
if(exitCommands){
 configs.splice(0,configs.length,
  {name:'exit-items-loss-one',party:1,outcome:'lose'},
  {name:'exit-items-loss-three',party:3,outcome:'lose'},
  {name:'exit-items-win-three',party:3,outcome:'win'},
  {name:'exit-items-flee-one',party:1,outcome:'flee'},
  {name:'exit-items-flee-three',party:3,outcome:'flee'});
 for(const c of configs)Object.assign(c,{hp:100,hps:[100,80,120],steps:1000,encounters:1,enemyIds:[],ids:[1,0,0],num:1,seed:11,sex:[1,2,255]});
}
if(recordReentry){
 configs.splice(0,configs.length,...configs.filter(c=>c.outcome!=='lose'));
 configs.forEach((c,i)=>Object.assign(c,{reentryParty:[1,3,2][i],reentryCount:2,reentryIds:[1,2,0]}));
 if(reentryOneEnemy)configs.forEach(c=>Object.assign(c,{reentryCount:1,reentryIds:[1,0,0]}));
 if(reentryTwoAttacks||reentryMixed)configs.splice(0,configs.length,...configs.filter(c=>c.reentryParty===3));
 if(reentryMixed)configs.forEach(c=>c.mixedItem=mixedItem);
}
for(const config of configs){
 rt.restoreBytes(snapshot);for(let t=0;t<15;++t){const out=rt.advance(0);out.free();}
const mem=()=>new Uint8Array(exp.memory.buffer),bases=[];
const bank=Buffer.from(exp.memory.buffer),signature=Buffer.from([0,1,0,7]);
for(let p=bank.indexOf(signature);p>=0;p=bank.indexOf(signature,p+1)){
 const i=p-0x6ca2+9;if(i<0||i+0x8000>=mem().length)continue;
 const w=o=>mem()[i+o-1]*256+mem()[i+o];
 if(w(0x7934)===1&&w(0x7a58)===1&&w(0x7a66)===1&&mem()[i+0x69d2-9]===1&&
    mem()[i+0x69d5-9]===7&&mem()[i+0x69d6-9]===1&&mem()[i+0x7a2e-1]===36&&
    mem()[i+0x7a2c-1]===1&&mem()[i+0x6cab-9]===32&&mem()[i+0x6cac-9]===13)bases.push(i);}
if(bases.length!==1)throw Error('RAM discovery '+bases.length);
const base=bases[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>mem()[a(o)-1]*256+mem()[a(o)];
const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
const npe=fs.readFileSync('tmp/tjfy-world-probe.npe');
const section=npe.readUInt32BE(0x3a+8*8),record=section+npe.readUInt32BE(section);
const width=npe[record+2];
const tile=(x,y)=>npe.readUInt16BE(record+17+(y*width+x)*2);
for(let y=0;y<7;++y)for(let x=0;x<15;++x)
 if(w(0x70b4+(y*15+x)*2)!==tile(36+x,1+y))throw Error('initial cache / NPE mismatch');
let rngAddress;
const u32=(m,j)=>((m[j]*16777216+m[j+1]*65536+m[j+2]*256+m[j+3])>>>0);
const state=()=>({mixedInventory:reentryMixed?w(0x67c8+mixedItem*2):undefined,
 skillCache:reentryMixed?{id:b(0x7a74),bytes:Array.from({length:144},(_,j)=>b(0x730e+j))}:undefined,
 itemCache:reentryMixed?{id:b(0x7a72),bytes:Array.from({length:142},(_,j)=>b(0x739e+j))}:undefined,
 commandInventory:exitCommands?[1,8,159].map(id=>w(0x67c8+id*2)):undefined,hp:w(0x6a16),hpMap:Array.from({length:3},(_,i)=>w(0x6a16+i*148)),
 allMapRecords:recordReentry?Array.from({length:3},(_,i)=>Array.from({length:148},(_,j)=>b(0x69c6+148*i+j))):undefined,
 allBattleRecords:recordReentry?Array.from({length:6},(_,i)=>Array.from({length:148},(_,j)=>b(0x6450+148*i+j))):undefined,
 voiceDescriptors:partyDescriptors?[0x7946,0x7942,0x7966,0x7962].map(o=>(w(o)*65536+w(o+2))>>>0):undefined,
 enemyDirectory:enemyRecords?(w(0x79b6)*65536+w(0x79b8))>>>0:undefined,
 residentEnemyRecords:enemyRecords?Array.from({length:3},(_,i)=>Array.from({length:148},(_,j)=>b(0x6450+148*(b(0x7afa)+i)+j))):undefined,
 residentMapRecords:zeroFlee?Array.from({length:config.party??1},(_,i)=>Array.from({length:148},(_,j)=>b(0x69c6+148*i+j))):undefined,
 residentBattlePartyRecords:zeroFlee?Array.from({length:config.party??1},(_,i)=>Array.from({length:148},(_,j)=>b(0x6450+148*i+j))):undefined,
 partyInput:config.party??1,encounterIds:Array.from({length:8},(_,i)=>b(0x7916+i)),
 partyCount:b(0x7afa),event:w(0x7936),chaseSlot:w(0x7938),money:(w(0x799e)*65536+w(0x79a0))>>>0,
 menuSelection:b(0x7ae2),battleMenu:b(0x7ac8),battleRow:b(0x7a9c),battleItemMode:b(0x7aa2),battleActor:b(0x7a9e),
 resourceFlags:resourceCycle?w(0x7aa4):undefined,
 battlePreviousKind:commandResident?b(0x7aae):undefined,
 displayTarget:b(0x7ac0),selectedIds:[w(0x792e),w(0x7930),w(0x7932)],num:b(0x7af0),flag:b(0x7a9a),
 battleRecords:Array.from({length:6},(_,i)=>{const o=i*148;return {hp:w(0x64a0+o),maxHp:w(0x649e+o),qi:w(0x64a4+o),maxQi:w(0x64a2+o),
 attack:w(0x64b0+o),defense:w(0x64b2+o),speed:w(0x64a8+o),level:b(0x6468+o),id:b(0x6474+o),type:b(0x64ce+o),
 statuses:Array.from({length:4},(_,j)=>w(0x648e+o+2*j))};}),map:w(0x7a58),encounters:w(0x7a5c),encounterSteps:w(0x7a02),counter:b(0x7a32),mode:w(0x7934),progress:w(0x7a66),heroY:b(0x69d6)+b(0x7a2c),
 heroX:b(0x69d5)+b(0x7a2e),screenX:b(0x69d5),screenY:b(0x69d6),
 viewX:b(0x7a2e),viewY:b(0x7a2c),direction:b(0x69d4),
 redraw:w(0x7ab4),moveKind:w(0x7a86),exitChoice:b(0x7ac4),
 trigger:Array.from({length:10},(_,i)=>{const o=0x7490+10*i;return {event:w(o),first:w(o+2),last:w(o+4),tile:w(o+6),mode:b(o+8)};}),
 cacheTail:Array.from({length:15},(_,i)=>w(0x70b4+(105+i)*2)),
 rng:rngAddress===undefined?null:u32(mem(),rngAddress),
 npc:Array.from({length:20},(_,j)=>{const o=0x6ca2+j*14;return{id:w(o),graphic:w(o+2),first:w(o+4),last:w(o+6),direction:b(o+8),x:b(o+9),y:b(o+10),moving:b(o+11)};}),
 chase:Array.from({length:2},(_,j)=>{const o=0x783a+j*14;return{graphic:w(o),first:w(o+2),last:w(o+4),enemy:w(o+6),x:w(o+8),y:w(o+10),cooldown:b(o+12)};})});
const period=prefix=>{const counter=b(0x7a32),commits=[];for(let ticks=0;ticks<300;++ticks){
 const out=rt.advance(0);out.free();
 if(prefix)for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
  fs.writeFileSync(`${prefix}-commit-${commits.length}.idx`,pixels);commits.push({tick:ticks});}
 if(b(0x7a32)!==counter)return {ticks,commits};}throw Error('period timeout');};
const initial=state(),rawBefore=mem().slice();period();const rawAfter=mem(),changes=new Set(),rng=[];
for(let j=0;j<Math.min(rawBefore.length,rawAfter.length);++j)if(rawBefore[j]!==rawAfter[j])
 for(let k=Math.max(0,j-3);k<=j&&k+4<rawBefore.length;++k)changes.add(k);
for(const j of changes){const old=u32(rawBefore,j),next=u32(rawAfter,j);if(old<=1||next<=1||old===next)continue;
 let v=old;for(let calls=1;calls<=128;++calls){v=(Math.imul(v,0x514f111f)+1)>>>0;if(v===next){rng.push({offset:j,before:old,after:next,calls});break;}}}
if(rng.length!==1)throw Error('RNG discovery '+JSON.stringify(rng));rngAddress=rng[0].offset;
putW(0x7a50,0);putW(0x7a5c,0);putB(0x7ac4,0);
for(let i=0;i<20;++i){putW(0x6ca2+i*14,0);putB(0x6ca2+i*14+11,0);}
for(let i=0;i<2;++i)putW(0x783a+i*14+6,0);
for(let i=0;i<4;++i){putW(0x761a+i*14+6,0);putB(0x761a+i*14+11,0);}
putB(0x69d5,6);putB(0x69d6,1);putB(0x7a2e,36);putB(0x7a2c,1);
for(let t=0;t<32;++t){const out=rt.advance(t===0?8:0);out.free();}
for(let i=0;i<3;++i){putB(0x69f1+i*148,i<(config.party??1)?1:0);
 if(i<(config.party??1))putW(0x6a16+i*148,config.hps?.[i]??config.hp);}
for(let i=0;i<3;++i)putW(0x792e+2*i,config.ids[i]);putB(0x7af0,config.num);
putW(0x7a02,config.steps);putW(0x7a5c,config.encounters??1);
for(let i=0;i<8;++i)putB(0x7916+i,config.enemyIds===undefined?(i===0?89:0):(config.enemyIds[i]??0));
if(config.chase){const o=0x783a;putW(o,1);putW(o+2,0);putW(o+4,0);putW(o+6,89);
 putW(o+8,config.chase[0]);putW(o+10,config.chase[1]);putB(o+12,config.cooldown??0);}
if(triggers){
 if(config.money!==undefined){putW(0x799e,config.money>>>16);putW(0x79a0,config.money&65535);}
 for(let i=0;i<10;++i)putW(0x7490+i*10+6,65535);
 const o=0x7490;putW(o,config.triggerEvent??1);putW(o+2,config.triggerFirst??1);
 putW(o+4,config.triggerLast??1);putW(o+6,3*width+43);putB(o+8,config.triggerMode??6);
}
mem().set([0,0,0,config.seed??11],rngAddress);
if(partyDescriptors){
 for(let m=0;m<config.party;++m){const o=0x69c6+148*m;putB(o+17,config.sex[m]);
  for(let j=114;j<126;++j)putB(o+j,(37*m+11*j+5)&255);
 }
 if(config.voices)for(let i=0;i<4;++i){const o=[0x7946,0x7942,0x7966,0x7962][i];putW(o,config.voices[i]>>>16);putW(o+2,config.voices[i]&65535);}
}
if(resourceCycle)putB(0x7aa5,166);
const before=state(),index=rows.length,prefix=`tmp/${outputName}-${index}`,commits=[],states=[];
fs.writeFileSync(`${prefix}-before.idx`,rt.framePixelIndices());
for(let t=0;t<160;++t){const out=rt.advance(t===0?2:0);out.free();
 for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
  fs.writeFileSync(`${prefix}-commit-${commits.length}.idx`,pixels);commits.push({tick:t});}
 const now=state();if(!states.length||JSON.stringify(now)!==JSON.stringify(states.at(-1).state))states.push({tick:t,state:now});}
const after=state();fs.writeFileSync(`${prefix}-after.idx`,rt.framePixelIndices());
// Mutate only live RAM after the complete original entry. Retain the
// already committed scene so native redraw/erasure behavior is observable.
if(liveCoordinates){
 for(let m=0;m<config.party;++m){putB(0x64cc+148*m,config.partyCoords[m][0]);putB(0x64cd+148*m,config.partyCoords[m][1]);}
 for(let m=0;m<3;++m){putB(0x64cc+148*(config.party+m),config.enemyCoords[m][0]);putB(0x64cd+148*(config.party+m),config.enemyCoords[m][1]);}
}
if(exitRecords){
 for(let m=0;m<config.party;++m){const o=148*m;
  putW(0x64a0+o,config.outcome==='flee'?[100,80,120][m]:m===0?(config.outcome==='win'?100:1):0);
  putB(0x64ca+o,0xa0+m);putB(0x64cb+o,0xb0+index);
  putB(0x64e3+o,0xc0+index*3+m);
 }
 if(config.upgrade)putW(0x649a,1);
 for(let m=0;m<3;++m)putW(0x64a0+148*(config.party+m),config.outcome==='win'||m?0:35);
 if(config.outcome==='lose'){const o=148*config.party;putW(0x64ac+o,600);putW(0x64b0+o,600);}
 if(config.scripted){putW(0x7936,1);putB(0x7a9a,1);
  mem().set([0,1,0,0,1,0,0,0],a(0x77fc)-1);
 }
}
if(exitCommands){
 for(let m=0;m<config.party;++m){const o=148*m;
  putB(0x64d5+o,[1,8,159][m]);putB(0x64d7+o,0x70+m);
  putW(0x6496+o,7+m);putW(0x6498+o,9+m);
  putW(0x64b0+o,w(0x64ac+o)+17+m);putW(0x64b2+o,w(0x64ae+o)+25+m);
  for(let j=0;j<4;++j)putW(0x648e+o+2*j,100+10*m+j);
 }
 for(const [j,id] of [1,8,159].entries())putW(0x67c8+id*2,[17,30,99][j]);
}
const exitInput=exitRecords?state():null;
const menuNavigation=[];
if(zeroMenu||(triggers&&index===0))for(const key of exitCommands?[4,2,16,2,2,config.outcome==='flee'?2:8,16]:exitRecords?[4,2,16,2,2,16]:coordinateTargets?[16,2,1,2,64,16,2,64]:zeroMenu?[4,2,16,2,16,2,64,1,2,2,2,64,8,4,...(zeroFlee?[16,16]:[])]:[2,2,2,2,1,1,1,1]){
 const frames=[];
 const fleeing=zeroFlee&&!exitRecords&&menuNavigation.length===15;
 const exiting=exitRecords&&menuNavigation.length===(exitCommands?6:5);let returnedAt=-1;
 for(let t=0;t<(fleeing||exiting?1400:32);++t){const out=rt.advance(t===0?key:0);out.free();
  for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
   fs.writeFileSync(`${prefix}-menu-${menuNavigation.length}-${frames.length}.idx`,pixels);frames.push({tick:t});}
  if((fleeing&&state().mode===1||exiting&&[1,7].includes(state().mode))&&returnedAt<0)returnedAt=t;
  // Capture the first map return, ending before an unrelated queued map
  // timer can produce another map later in the one-member scenario.
  if(returnedAt>=0&&t>=returnedAt+4)break;
 }
 if(fleeing&&state().mode!==1)throw Error('zero-enemy flee did not return to map');
 if(exiting&&state().mode!==(config.outcome!=='lose'||config.scripted?1:7))throw Error('battle exit mode '+JSON.stringify(state()));
 menuNavigation.push({key,frames,after:state()});
}
let reentry;
if(recordReentry){
 const retained=state();
 for(let m=0;m<3;++m){putB(0x69f1+148*m,m<config.reentryParty?1:0);
  if(m<config.reentryParty)putW(0x6a16+148*m,[200,180,220][m]);}
 for(let i=0;i<3;++i)putW(0x792e+2*i,config.reentryIds[i]);
 putB(0x7af0,config.reentryCount);putW(0x7a02,1000);putW(0x7a5c,1);
 for(let i=0;i<8;++i)putB(0x7916+i,0);
 mem().set([0,0,0,11],rngAddress);
 const input=state(),frames=[];
 for(let t=0;t<160;++t){const out=rt.advance(t===0?2:0);out.free();
  for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
   fs.writeFileSync(`${prefix}-reentry-${frames.length}.idx`,pixels);frames.push({tick:t});}}
 const end=state();
 if(end.mode!==3||end.partyCount!==config.reentryParty||end.num!==config.reentryCount)
  throw Error('record reentry '+JSON.stringify(end));
 reentry={retained,input,frames,after:end};
 if(reentryActions){
   // Diagnostic input only: the next actor initially points at another
   // living enemy. Ordinary selector initialization must reset it too.
   if(reentryTwoAttacks)putB(0x64ce+148,b(0x7afa)+2);
   if(reentryMixed){
    // Explicit diagnostic input before menu actions. Do not patch outcomes.
    for(let i=0;i<3;++i){const d=148*i;putB(0x69de+d,99);
     for(let j=0;j<18;++j)putB(0x69df+d+j,j===0?(reentryGroups?9:[6,9,4][i]):0);}
    for(let i=0;i<6;++i){const d=148*i;
     putW(0x649e+d,i<3?200:30000);putW(0x64a0+d,i<3?[50,60,70][i]:30000);
     putW(0x64a2+d,120);putW(0x64a4+d,100);putB(0x6468+d,99);
     putW(0x64a6+d,1);putW(0x64a8+d,i<3?999-i:1);putW(0x64aa+d,1);
     putW(0x64ac+d,i<3?90:1);putW(0x64b0+d,i<3?90:1);
     putW(0x64ae+d,0);putW(0x64b2+d,0);
     for(let j=0;j<4;++j)putW(0x648e+d+j*2,0);
     if(i>=3)for(let j=0;j<3;++j)putB(0x6469+d+j,0);
    }
    for(let id=0;id<255;++id)putW(0x67c8+id*2,id===mixedItem?2:0);
    putB(0x7a74,0);putB(0x7a72,0);
    for(let i=0;i<144;++i)putB(0x730e+i,0);
    for(let i=0;i<142;++i)putB(0x739e+i,0);
   }

  if(menuResident){if(!reentryAttack){putB(0x7a9c,[4,0,1][rows.length]);putB(0x7aa2,1);
   if(commandResident)putB(0x7aae,[129,2,255][rows.length]);
   if(phaseResident)putB(0x7ac8,30);
   }
   reentry.menuInput=state();}
  const navigation=[];
  const commandKey=(key,execute=false)=>{
   const before=state(),frames=[];
   for(let t=0;t<(execute?900:32);++t){const out=rt.advance(t===0?key:0);out.free();
    for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
     fs.writeFileSync(`${prefix}-action-${navigation.length}-${frames.length}.idx`,pixels);frames.push({tick:t,execution:state()});}}
   const after=state();navigation.push({key,execute,before,frames,after});
   console.log(config.name,'action',navigation.length-1,'key',key,'commits',frames.length,'actor',after.battleActor,'menu',after.battleMenu,'RNG',after.rng);
  };
  if(reentryMixed){
   const flags=()=>{
    const phase=b(0x7ac8);
    if(phase===10){commandKey(4);commandKey(2);}
    else if(phase===20)commandKey(2);
    else if(phase===40)commandKey(4);
    if(b(0x7ac8)!==30)throw Error('mixed flags phase '+b(0x7ac8));
    commandKey(16);
   };
   const flagRow=row=>{
    for(let n=0;n<6&&b(0x7a9c)!==row;++n)commandKey(b(0x7a9c)<row?2:1);
    if(b(0x7a9c)!==row)throw Error('mixed flags row');
   };
   const spell=(actor,id)=>{
    const phase=b(0x7ac8);
    if(phase===10)commandKey(4);
    else if(phase===30)commandKey(1);
    else if(phase===40){commandKey(1);commandKey(4);}
    if(b(0x7ac8)!==20)throw Error('mixed magic phase '+b(0x7ac8));
    commandKey(16);commandKey(16,reentryGroups&&actor===2);
    if(b(0x7a9e)===actor){
     if(id===4)commandKey(2); // Second living enemy, physical slot4.
     commandKey(16,actor===2);
    }
   };
   const medicine=actor=>{
    flags();flagRow(1);commandKey(16);
    if(b(0x7aa2)!==0)commandKey(1);
    commandKey(16);
    if(b(0x7ac8)!==33)throw Error('mixed item list '+b(0x7ac8));
    commandKey(16,reentryGroups&&actor===2);
    if(b(0x7a9e)===actor){
     if(b(0x7ac8)!==37)throw Error('mixed item target '+b(0x7ac8));
     commandKey(2);commandKey(16,actor===2);
    }
   };
   reentry.roundStates=[];
   for(let round=0;round<2;++round){
    for(let actor=0;actor<3;++actor){
     if(b(0x7a9e)!==actor)throw Error('mixed actor '+b(0x7a9e));
     if(reentryGroups){
      if(round===0&&actor===0||round===1&&actor===2)medicine(actor);
      else if(actor===1){flags();flagRow(2);commandKey(16);}
      else spell(actor,9);
     }else if(round===0&&actor===0)spell(actor,6);
     else if(round===0&&actor===1||round===1&&actor===0)medicine(actor);
     else if(round===1)spell(actor,actor===1?9:4);
     else{flags();flagRow(2);commandKey(16,true);}
    }
    if(w(0x7934)!==3||b(0x7a9e)!==0)throw Error('mixed round did not continue');
    reentry.roundStates.push(state());
   }
  }else{
  for(let round=0;round<(reentryTwoAttacks?2:1);++round){
   if(round){
    if(b(0x7ac8)===30){commandKey(1);commandKey(8);}
    else if(b(0x7ac8)===20)commandKey(8);
    else if(b(0x7ac8)===40)commandKey(1);
   }
  for(let actor=0;actor<config.reentryParty;++actor){
   if(b(0x7a9e)!==actor)throw Error('reentry command actor '+b(0x7a9e));
   if(reentryAttack&&(actor===0 || reentryTwoAttacks&&round===0&&actor===1)){
    if(b(0x7ac8)!==10)throw Error('reentry attack phase '+b(0x7ac8));
    let living=0;for(let e=0;e<config.reentryCount;++e){
     const hp=w(0x64a0+148*(config.reentryParty+e));living+=hp>0&&hp<32768;}
    commandKey(16,living===1);
    if(b(0x7ac8)===17)commandKey(16,true);
    else if(living!==1)throw Error('reentry attack target phase '+b(0x7ac8));
    continue;
   }
   if(b(0x7ac8)===10){commandKey(4);commandKey(2);}else if(b(0x7ac8)===20||b(0x7ac8)===40)commandKey(2);
   if(b(0x7ac8)!==30)throw Error('reentry flag '+b(0x7ac8));
   commandKey(16);
   for(let i=0;i<5&&b(0x7a9c)!==2;++i)commandKey(b(0x7a9c)<2?2:1);
   if(b(0x7a9c)!==2)throw Error('reentry defend row');
   commandKey(16,true);
  }
  }
  }
  if(w(0x7934)!==3||b(0x7a9e)!==0)throw Error('reentry round did not continue');
  if(menuResident&&!reentryAttack){
   for(const key of [16,1,16,2,1,64,64])commandKey(key);
  }
  reentry.actions=navigation;
 }
 console.log(config.name,'reentry',frames.length,'party',end.partyCount,'RNG',end.rng);
}
rows.push({name:config.name,config,before,after,exitInput,commits,states,menuNavigation,reentry,discovery:rng[0]});
fs.writeFileSync(`tmp/${outputName}.json`,JSON.stringify({rows},null,2)+'\n');
console.log(config.name,JSON.stringify({commits:commits.length,mode:after.mode,hp:after.hp,records:after.battleRecords.slice(0,4),
 event:after.event,ids:after.selectedIds,rng:after.rng,steps:after.encounterSteps,chase:after.chaseSlot,positions:after.chase}));
}
rt.free();
