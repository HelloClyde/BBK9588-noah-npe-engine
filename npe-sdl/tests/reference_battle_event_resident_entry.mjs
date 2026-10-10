import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Isolated original RAM fixtures; no game/resource/snapshot file is changed.
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const configs=[
 {name:'first',event:1,formation:1,left:0,right:0},
 {name:'last',event:4,formation:51,left:51,right:51,monsterSlot:3},
 {name:'word-256',event:256,formation:1,left:0,right:0},
 {name:'five-choice',event:5,formation:1,left:0,right:0},
 {name:'signed-negative',event:65535,formation:1,left:0,right:0},
 {name:'high-mode',event:1,formation:1,left:0,right:0,winMode:255,loseMode:254},
 {name:'zero-formation',event:2,formation:0,left:0,right:0,monsterSlot:1},
].map(c=>({hps:[130],winMode:1,loseMode:1,winEvent:0x1234,loseEvent:0x4321,...c}));
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
 for(let i=0;i<32;++i)putB(0x77fc+i,(i*37+0x81)&255);
 const rawIndex=(config.event-1)&65535,index=rawIndex<32768?rawIndex:rawIndex-65536,address=0x77fc+index*8;
 putB(address,config.formation);putB(address+1,config.winMode);putB(address+2,config.loseMode);
 putB(address+3,0xd3);putW(address+4,config.winEvent);putW(address+6,config.loseEvent);
 for(let i=0;i<4;++i)putB(0x761a+i*14+11,255);
 const slot=config.monsterSlot??0,monster=0x761a+slot*14;
 putB(monster+11,config.formation);putB(monster+12,config.left);putB(monster+13,config.right);
 const tables=()=>({battle:Array.from({length:32},(_,i)=>b(0x77fc+i)),monster:Array.from({length:56},(_,i)=>b(0x761a+i)),alias:Array.from({length:8},(_,i)=>b(address+i))});
 const beforeTables=tables();
 putW(0x7934,3);putW(0x7936,config.event);putW(0x7a50,0);putW(0x7ab4,1);
 const state=()=>({mode:w(0x7934),event:w(0x7936),map:w(0x7a58),progress:w(0x7a66),
  party:b(0x7afa),actor:b(0x7a9e),target:b(0x7ac0),ids:[w(0x792e),w(0x7930),w(0x7932)],
  mapHp:Array.from({length:3},(_,i)=>w(0x6a16+148*i)),
  view:{x:b(0x7a2e),y:b(0x7a2c),sx:b(0x69d5),sy:b(0x69d6),direction:b(0x69d4)},
  records:Array.from({length:config.hps.length+3},(_,i)=>{const o=148*i;return {
   hp:w(0x64a0+o),maxHp:w(0x649e+o),qi:w(0x64a4+o),maxQi:w(0x64a2+o),
   attack:w(0x64b0+o),defense:w(0x64b2+o),speed:w(0x64a8+o),
   status:Array.from({length:4},(_,s)=>w(0x648e+o+2*s))};})});
 const before=state(),commits=[];
 for(let tick=0;tick<160;++tick){const out=rt.advance(0);out.free();
  for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
   fs.writeFileSync(`tmp/native-battle-event-resident-entry-${rows.length}-commit-${commits.length}.idx`,pixels);commits.push({tick});}}
 const after=state();if(commits.length!==4)throw Error('entry commits '+commits.length+' '+JSON.stringify(after));
 rows.push({config,address,beforeTables,afterTables:tables(),before,after,commits});console.log(JSON.stringify(rows.at(-1)));
 // Retired RAM allocations can survive restore; invalidate only that old
 // fixture's discovery signature before restoring the next snapshot.
 putW(0x7934,0);
}
fs.writeFileSync('tmp/native-battle-event-resident-entry.json',JSON.stringify(rows,null,2)+'\n');rt.free();
