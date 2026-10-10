import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Isolated original RAM fixtures; no game/resource/snapshot file is changed.
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const zeroEvent=process.argv.includes('--zero-event');
const prefix=zeroEvent?'native-battle-zero-event':'native-battle-fallback-entry';
const configs=(zeroEvent?[
 {name:'zero-random',event:0,enemies:[1,2],seed:12},
 {name:'zero-retained',event:0,enemies:[],seed:1},
 {name:'zero-party-two',event:0,enemies:[1],seed:1,hps:[130,226]},
 {name:'zero-chase',event:0,enemies:[],seed:1,context:1,chaseEnemy:2},
]:[
 {name:'empty',enemies:[],seed:1},
 {name:'one',enemies:[1],seed:1},
 {name:'hole-stops',enemies:[1,0,2],seed:1},
 {name:'two-double',enemies:[1,2],seed:1},
 {name:'two-single',enemies:[1,2],seed:14},
 {name:'two-triple',enemies:[1,2],seed:12},
 {name:'two-repeat-threshold',enemies:[1,2],seed:537},
 {name:'eight-triple',enemies:[1,2,3,4,5,6,7,8],seed:12},
 {name:'eight-repeat-threshold',enemies:[1,2,3,4,5,6,7,8],seed:5945},
 {name:'negative-context',enemies:[1,2],seed:12,context:65535},
 {name:'chase-first',enemies:[1,2],seed:12,context:1,chaseEnemy:2},
 {name:'chase-last',enemies:[1,2],seed:12,context:2,chaseEnemy:3},
 {name:'chase-alias',enemies:[1,2],seed:12,context:3,chaseEnemy:4},
]).map(c=>({hps:[130],winMode:1,loseMode:1,winEvent:0x1234,loseEvent:0x4321,event:1,formation:254,context:0,...c}));
const rows=[];
for(const config of configs){
 rt.restoreBytes(fs.readFileSync('tmp/battle-fresh-start.bin'));
 const mem=()=>new Uint8Array(exp.memory.buffer),bank=Buffer.from(exp.memory.buffer),found=[];
 const signature=Buffer.from([0,3,0,1]);
 for(let p=bank.indexOf(signature);p>=0;p=bank.indexOf(signature,p+1)){
  const i=p-0x7934+1;if(i<0||i+0x8000>=bank.length)continue;
  if(bank[i+0x7a58]===9&&bank[i+0x7a66]===6&&bank[i+0x7a67]===0)found.push(i);
 }
 if(found.length!==1)throw Error('unique original RAM page '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 // Leave the previous battle via a real mode1 dispatch before entering
 // mode3 again. A mode3 redraw alone has a different presentation order.
 putW(0x7934,1);putW(0x7936,0);putW(0x7a50,0);putW(0x7ab4,1);
 for(let tick=0;tick<30;++tick){const out=rt.advance(0);out.free();}
 if(w(0x7934)!==1||w(0x7a58)!==9)throw Error('map dispatch before entry');
 for(let i=0;i<3;++i){putB(0x69f1+148*i,i<config.hps.length?1:0);
  if(i<config.hps.length)putW(0x6a16+148*i,config.hps[i]);}
 putB(0x7afa,config.hps.length);
 for(let i=0;i<config.hps.length;++i)for(let s=0;s<4;++s)putW(0x648e+148*i+2*s,[-2,3,-4,5][s]&65535);
 // Discover the original global generator through an actual map period,
 // then seed it once. This does not claim device cold-start entropy.
 const u32=(m,j)=>((m[j]*16777216+m[j+1]*65536+m[j+2]*256+m[j+3])>>>0);
 const oldNpc=Array.from({length:14},(_,i)=>b(0x6ca2+i));
 putW(0x6ca2,1);putW(0x6ca6,0);putW(0x6ca8,65535);putB(0x6cad,1);
 const rawBefore=mem().slice(),counter=b(0x7a32);
 for(let t=0;t<300;++t){const out=rt.advance(0);out.free();if(b(0x7a32)!==counter)break;if(t===299)throw Error('period');}
 const rawAfter=mem(),changes=new Set(),rng=[];
 for(let j=0;j<Math.min(rawBefore.length,rawAfter.length);++j)if(rawBefore[j]!==rawAfter[j])
  for(let k=Math.max(0,j-3);k<=j&&k+4<rawBefore.length;++k)changes.add(k);
 for(const j of changes){const old=u32(rawBefore,j),next=u32(rawAfter,j);if(old<=1||next<=1||old===next)continue;
  let v=old;for(let calls=1;calls<=128;++calls){v=(Math.imul(v,0x514f111f)+1)>>>0;if(v===next){rng.push(j);break;}}}
 if(rng.length!==1)throw Error('RNG discovery '+rng.length);
 for(let i=0;i<14;++i)putB(0x6ca2+i,oldNpc[i]);
 for(let t=0;t<160;++t){const out=rt.advance(0);out.free();}
 const rngAddress=rng[0];for(let j=0;j<4;++j)mem()[rngAddress+j]=(config.seed>>>(24-j*8))&255;
 for(let i=0;i<8;++i)putB(0x7916+i,config.enemies[i]??0);
 putW(0x7938,config.context);putW(0x792e,1);putW(0x7930,51);putW(0x7932,2);
 if(config.context>0&&config.context<32768)putW(0x7840+(config.context-1)*14,config.chaseEnemy);

 for(let i=0;i<32;++i)putB(0x77fc+i,(i*37+0x81)&255);
 const rawIndex=(config.event-1)&65535,index=rawIndex<32768?rawIndex:rawIndex-65536,address=0x77fc+index*8;
 putB(address,config.formation);putB(address+1,config.winMode);putB(address+2,config.loseMode);
 putB(address+3,0xd3);putW(address+4,config.winEvent);putW(address+6,config.loseEvent);
 for(let i=0;i<4;++i)putB(0x761a+i*14+11,255);
 const tables=()=>({battle:Array.from({length:32},(_,i)=>b(0x77fc+i)),monster:Array.from({length:56},(_,i)=>b(0x761a+i)),alias:Array.from({length:8},(_,i)=>b(address+i)),random:Array.from({length:8},(_,i)=>b(0x7916+i)),chase:Array.from({length:28},(_,i)=>b(0x783a+i))});
 const beforeTables=tables();
 putW(0x7934,3);putW(0x7936,config.event);putW(0x7a50,0);putW(0x7ab4,1);
 const state=()=>({mode:w(0x7934),event:w(0x7936),map:w(0x7a58),progress:w(0x7a66),
  count:b(0x7af0),context:w(0x7938),rng:u32(mem(),rngAddress),party:b(0x7afa),actor:b(0x7a9e),target:b(0x7ac0),ids:[w(0x792e),w(0x7930),w(0x7932)],
  mapHp:Array.from({length:3},(_,i)=>w(0x6a16+148*i)),
  view:{x:b(0x7a2e),y:b(0x7a2c),sx:b(0x69d5),sy:b(0x69d6),direction:b(0x69d4)},
  records:Array.from({length:config.hps.length+3},(_,i)=>{const o=148*i;return {
   hp:w(0x64a0+o),maxHp:w(0x649e+o),qi:w(0x64a4+o),maxQi:w(0x64a2+o),
   attack:w(0x64b0+o),defense:w(0x64b2+o),speed:w(0x64a8+o),
   status:Array.from({length:4},(_,s)=>w(0x648e+o+2*s))};})});
 const before=state(),commits=[];
 for(let tick=0;tick<160;++tick){const out=rt.advance(0);out.free();
  for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
   fs.writeFileSync(`tmp/${prefix}-${rows.length}-commit-${commits.length}.idx`,pixels);commits.push({tick});}}
 const after=state();if(commits.length!==4)throw Error('entry commits '+commits.length+' '+JSON.stringify(after));
 rows.push({config,address,beforeTables,afterTables:tables(),before,after,commits});console.log(JSON.stringify(rows.at(-1)));
 // Retired RAM allocations can survive restore; invalidate only that old
 // fixture's discovery signature before restoring the next snapshot.
 putW(0x7934,0);
}
fs.writeFileSync(`tmp/${prefix}.json`,JSON.stringify(rows,null,2)+'\n');rt.free();
