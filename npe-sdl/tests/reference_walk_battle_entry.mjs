import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const triggers=process.argv.includes('--triggers');
const outputName=triggers?'native-walk-trigger-continuation':'native-walk-battle-entry';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/empty-title-first-map-snapshot.bin');
const configs=triggers?[
 {name:'menu-trigger-chase',hp:100,steps:0,encounters:0,chase:[38,4]},
 {name:'menu-trigger-roll-chase',hp:100,steps:0,chase:[38,4],money:0},
 {name:'menu-trigger-random',hp:100,steps:1000},
 {name:'random-overrides-trigger-contact',hp:100,steps:1000,chase:[44,5]},
 {name:'contact-overrides-trigger',hp:100,steps:0,encounters:0,chase:[44,5]},
 {name:'map-trigger-roll-chase',hp:100,steps:0,chase:[38,4],triggerMode:1},
 {name:'map-trigger-random',hp:100,steps:1000,triggerMode:1},
 {name:'zero-event-menu-trigger',hp:100,steps:0,encounters:0,chase:[38,4],triggerEvent:0,money:999999},
 {name:'trigger-range-denied',hp:100,steps:0,chase:[38,4],triggerFirst:2,triggerLast:2},
 {name:'trigger-chase-cooldown',hp:100,steps:0,encounters:0,chase:[44,5],cooldown:2,money:65535},
 {name:'zero-mode-trigger',hp:100,steps:0,encounters:0,chase:[38,4],triggerMode:0},
 {name:'invalid-mode-trigger',hp:100,steps:0,encounters:0,chase:[38,4],triggerMode:255},
]:[
 {name:'zero-hp-roll',hp:0,steps:0},
 {name:'zero-hp-random',hp:0,steps:1000},
 {name:'positive-hp-random',hp:100,steps:1000},
 {name:'last-count-random',hp:100,steps:999},
 {name:'random-before-chase',hp:0,steps:1000,chase:[44,5]},
 {name:'zero-hp-chase',hp:0,steps:0,encounters:0,chase:[44,5]},
 {name:'roll-before-chase',hp:0,steps:0,chase:[44,5]},
 {name:'two-members-first-fallen',hp:0,hps:[0,100],party:2,steps:1000},
 {name:'three-members-mixed',hp:0,hps:[0,100,0],party:3,steps:1000},
 {name:'two-enemies',hp:100,steps:1000,enemyIds:[88,89],seed:1},
 {name:'three-enemies',hp:100,steps:1000,enemyIds:[88,89,90],seed:12},
];
const rows=[];
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
const state=()=>({hp:w(0x6a16),hpMap:Array.from({length:3},(_,i)=>w(0x6a16+i*148)),
 partyInput:config.party??1,encounterIds:Array.from({length:8},(_,i)=>b(0x7916+i)),
 partyCount:b(0x7afa),event:w(0x7936),chaseSlot:w(0x7938),money:(w(0x799e)*65536+w(0x79a0))>>>0,
 menuSelection:b(0x7ae2),
 selectedIds:[w(0x792e),w(0x7930),w(0x7932)],
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
putW(0x7a02,config.steps);putW(0x7a5c,config.encounters??1);
for(let i=0;i<8;++i)putB(0x7916+i,config.enemyIds?.[i]??(i===0?89:0));
if(config.chase){const o=0x783a;putW(o,1);putW(o+2,0);putW(o+4,0);putW(o+6,89);
 putW(o+8,config.chase[0]);putW(o+10,config.chase[1]);putB(o+12,config.cooldown??0);}
if(triggers){
 if(config.money!==undefined){putW(0x799e,config.money>>>16);putW(0x79a0,config.money&65535);}
 for(let i=0;i<10;++i)putW(0x7490+i*10+6,65535);
 const o=0x7490;putW(o,config.triggerEvent??1);putW(o+2,config.triggerFirst??1);
 putW(o+4,config.triggerLast??1);putW(o+6,3*width+43);putB(o+8,config.triggerMode??6);
}
mem().set([0,0,0,config.seed??11],rngAddress);
const before=state(),index=rows.length,prefix=`tmp/${outputName}-${index}`,commits=[],states=[];
fs.writeFileSync(`${prefix}-before.idx`,rt.framePixelIndices());
for(let t=0;t<160;++t){const out=rt.advance(t===0?2:0);out.free();
 for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
  fs.writeFileSync(`${prefix}-commit-${commits.length}.idx`,pixels);commits.push({tick:t});}
 const now=state();if(!states.length||JSON.stringify(now)!==JSON.stringify(states.at(-1).state))states.push({tick:t,state:now});}
const after=state();fs.writeFileSync(`${prefix}-after.idx`,rt.framePixelIndices());
const menuNavigation=[];
if(triggers&&index===0)for(const key of [2,2,2,2,1,1,1,1]){
 const frames=[];
 for(let t=0;t<32;++t){const out=rt.advance(t===0?key:0);out.free();
  for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
   fs.writeFileSync(`${prefix}-menu-${menuNavigation.length}-${frames.length}.idx`,pixels);frames.push({tick:t});}}
 menuNavigation.push({key,frames,after:state()});
}
rows.push({name:config.name,config,before,after,commits,states,menuNavigation,discovery:rng[0]});
fs.writeFileSync(`tmp/${outputName}.json`,JSON.stringify({rows},null,2)+'\n');
console.log(config.name,JSON.stringify({commits:commits.length,mode:after.mode,hp:after.hp,records:after.battleRecords.slice(0,4),
 event:after.event,ids:after.selectedIds,rng:after.rng,steps:after.encounterSteps,chase:after.chaseSlot,positions:after.chase}));
}
rt.free();
