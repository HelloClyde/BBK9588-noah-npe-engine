import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';

const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const resources=process.argv.includes('--resources');
const output=resources?'tmp/native-battle-status-resources':'tmp/native-battle-status-view';
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const cases=[{name:'one',party:1,actor:0,member:0},{name:'two',party:2,actor:0,member:0},
 {name:'three',party:3,actor:0,member:0},{name:'three-retained',party:3,actor:1,member:2}];
const rows=[];
try{
 for(const c of cases){
  rt.restoreBytes(fs.readFileSync('tmp/battle-fresh-start.bin'));rt.drainEventsBytes();
  const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
  for(let i=0;i+0x8000<mem().length;++i)
   if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
  // Restoring another snapshot can leave an inactive copy in WASM memory.
  // Identify the active controller by an actual left-arrow transition.
  rt.advance(4).free();for(let i=0;i<15;++i)rt.advance(0).free();
  const live=found.filter(i=>mem()[i+0x7ac8-1]===20);
  if(live.length!==1)throw Error('RAM discovery '+live.length);
  const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
  const putB=(o,v)=>{mem()[a(o)-1]=v&255;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
  const tick=k=>rt.advance(k).free(),idle=n=>{for(let i=0;i<n;++i)tick(0);};
  tick(8);idle(15);
  const state=()=>({phase:b(0x7ac8),actor:b(0x7a9e),member:b(0x7aac),row:b(0x7a9c),
   selectorPlane:b(0x7aa4),resourceFlags:w(0x7aa4),mode:w(0x7934),money:(w(0x799e)*65536+w(0x79a0))>>>0,
   map:Array.from({length:3},(_,i)=>Array.from({length:148},(_,j)=>b(0x69c6+148*i+j))),
   battle:Array.from({length:6},(_,i)=>Array.from({length:148},(_,j)=>b(0x6450+148*i+j)))});
  const hero=mem().slice(a(0x69c6)-1,a(0x69c6+148)-1);
  for(let i=1;i<c.party;++i)mem().set(hero,a(0x69c6+i*148)-1);
  for(let i=0;i<3;++i)putB(0x69f1+i*148,i<c.party?1:0);
  putB(0x7afa,c.party);putW(0x7ab4,1);idle(30);
  const entry=state();
  // Distinguish the live combat values from the map's original values.
  for(let i=0;i<c.party;++i){const o=148*i;
   putB(0x6468+o,12+i);putW(0x649a+o,321+i);putW(0x649c+o,123+i);
   putW(0x649e+o,200+20*i);putW(0x64a0+o,i===2?0:50+10*i);
   putW(0x64a2+o,120+10*i);putW(0x64a4+o,30+i);
   putW(0x64a6+o,14+i);putW(0x64a8+o,24+i);putW(0x64aa+o,34+i);
   putW(0x64ac+o,44+i);putW(0x64ae+o,54+i);putW(0x64b0+o,144+i);putW(0x64b2+o,154+i);
   for(let j=0;j<4;++j)putW(0x648e+o+2*j,(i+j)%2?-5-j:7+j);
  }
  putB(0x7a9e,c.actor);putB(0x7aac,c.member);
  if(resources)putB(0x7aa5,166);
  for(const key of [4,2,16,1]){tick(key);idle(31);}
  if(b(0x7ac8)!==31||b(0x7a9c)!==4)throw Error('status flag gate');
  const before=state(),steps=[],prefix=output+'-'+c.name;
  fs.writeFileSync(prefix+'-before.idx',rt.framePixelIndices());
  // F4 restores the flag list. Reopening must retain the selector byte;
  // single-member up/down and unrelated inputs must have no LCD commits.
  for(const key of [16,2,1,1,2,8,4,16,64,16,2,64]){
   const frames=[];
   for(let t=0;t<32;++t){tick(t===0?key:0);
    for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
     fs.writeFileSync(prefix+'-'+steps.length+'-'+frames.length+'.idx',pixels);frames.push({tick:t});}
   }
   steps.push({key,frames,after:state()});
  }
  rows.push({config:c,entry,before,steps});
  fs.writeFileSync(output+'.json',JSON.stringify({rows},null,2)+'\n');
  console.log(c.name,steps.map(s=>[s.key,s.frames.length,s.after.phase,s.after.member]));
 }
 fs.writeFileSync(output+'.json',JSON.stringify({rows},null,2)+'\n');
}finally{rt.free();}
