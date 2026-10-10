import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const partyCount=Number(process.argv[2]||1), fleeActor=Number(process.argv[3]||0);
const variant=process.argv[4]||'scripted';
const exp=await initWasm({module_or_path:fs.readFileSync(new URL('../../tmp/opennoah_wasm_bg.wasm',import.meta.url))});
const rt=new GameRuntime(fs.readFileSync(new URL('../../tmp/tianjiang-online.opennoah',import.meta.url)),'certified_original_v1');
rt.restoreBytes(fs.readFileSync(new URL('../../tmp/battle-fresh-start.bin',import.meta.url)));
let candidates=[],m=new Uint8Array(exp.memory.buffer);
for(let i=0;i+0x8000<m.length;i++)if(m[i+0x7934]===3&&m[i+0x7935]===0&&m[i+0x7936]===1&&m[i+0x7a58]===9&&m[i+0x7a66]===6&&m[i+0x7a67]===0)candidates.push(i);
if(candidates.length!==1)throw Error('page not unique');
const base=candidates[0],a=o=>base+o-(o<0x7000?8:0),mem=()=>new Uint8Array(exp.memory.buffer);
const w=o=>mem()[a(o)-1]<<8|mem()[a(o)],b=o=>mem()[a(o)-1];
const putW=(o,v)=>{mem()[a(o)-1]=v>>>8;mem()[a(o)]=v&255;},putB=(o,v)=>mem()[a(o)-1]=v;
// Separate formation/attribute fixture. NPE and PRG bytes remain intact.
const hero=mem().slice(a(0x69c6)-1,a(0x69c6+0x94)-1);
for(let i=1;i<partyCount;i++)mem().set(hero,a(0x69c6+i*0x94)-1);
putB(0x7afa,partyCount);putB(0x7626,51);putW(0x7ab4,1);
const advance=mask=>{const out=rt.advance(mask);out.free();};
for(let i=0;i<30;i++)advance(0);
for(let i=0;i<partyCount;i++){
 putW(0x64a0+148*i,10000);putW(0x649e+148*i,10000);
 putW(0x64a8+148*i,999);
}
putW(0x67c8+159*2,fleeActor);
const events=[];let last='';
const state=label=>{
 const entry={mode:w(0x7934),menu:b(0x7ac8),actor:b(0x7a9e),executing:b(0x7acc),quantity:w(0x67c8+159*2),
 party:Array.from({length:partyCount},(_,i)=>({hp:w(0x64a0+148*i),kind:b(0x64cf+148*i),target:b(0x64ce+148*i),item:b(0x64d5+148*i),parameter2:b(0x64d2+148*i),acted:b(0x64e2+148*i)})),
 enemy:[0,1,2].map(i=>w(0x64a0+148*(partyCount+i)))};
 const key=JSON.stringify(entry);if(label||key!==last){events.push({label:label||'transition',tick:Number(rt.logicalTick()),...entry});last=key;}
};
let recording=false,commits=[],attempt=1;
const tick=mask=>{advance(mask);state();if(recording)for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){const index=commits.length;fs.writeFileSync(new URL(`../../tmp/native-failed-flee-${partyCount}-${fleeActor}-${variant}-attempt${attempt}-commit${index}.idx`,import.meta.url),p);commits.push({index,tick:Number(rt.logicalTick()),menu:b(0x7ac8),executing:b(0x7acc),actor:b(0x7a9e),mode:w(0x7934),kind:b(0x64cf),stepOffset:String(rt.displayCommitStepOffset(j))});}}};
const key=(mask,label)=>{tick(mask);for(let i=0;i<10;i++)tick(0);state(label);};
const choose=selection=>{
 if(b(0x7ac8)===10){key(4,'magic');key(2,'flag');}
 key(16,'commands');
 for(let i=0;i<6&&b(0x7a9c)!==selection;i++)key(b(0x7a9c)<selection?2:1,'command-select');
 if(b(0x7a9c)!==selection)throw Error('command selection');
};
state('entry');
for(let i=0;i<fleeActor;i++){
 choose(1);key(16,'item-menu');if(b(0x7aa2)===0)key(2,'throw');key(16,'list');key(16,'target');key(16,'queued');
}
if(variant==='fallen')putW(0x64a0,0);
if(variant==='asleep')putW(0x6490,0xfffe);
choose(3);
if(variant.startsWith('random')){
 putB(0x7a9a,0);
 const delay=/^random(\d+)$/.exec(variant);
 for(let i=0;i<(delay?Number(delay[1]):0);i++)tick(0);
}
state('before-flee');fs.writeFileSync(new URL(`../../tmp/native-failed-flee-${partyCount}-${fleeActor}-${variant}-attempt${attempt}-before.idx`,import.meta.url),rt.framePixelIndices());recording=true;key(16,'flee-confirm');
for(let i=0;i<650;i++)tick(0);
recording=false;state('settled');
if(variant==='random-retry'){
 for(attempt=2;attempt<=20&&w(0x7934)===1;attempt++){
  commits=[];
  // Independent repeated battle-entry fixture to observe random failure.
  putW(0x7934,3);putW(0x7ab4,1);
  for(let i=0;i<30;i++)tick(0);
  for(let i=0;i<partyCount;i++){
   putW(0x64a0+148*i,10000);putW(0x649e+148*i,10000);putW(0x64a8+148*i,999);
  }
  putW(0x67c8+159*2,fleeActor);state(`entry-attempt-${attempt}`);
  for(let i=0;i<fleeActor;i++){
   choose(1);key(16,'item-menu');if(b(0x7aa2)===0)key(2,'throw');key(16,'list');key(16,'target');key(16,'queued');
  }
  choose(3);putB(0x7a9a,0);state(`before-flee-attempt-${attempt}`);fs.writeFileSync(new URL(`../../tmp/native-failed-flee-${partyCount}-${fleeActor}-${variant}-attempt${attempt}-before.idx`,import.meta.url),rt.framePixelIndices());recording=true;key(16,'flee-confirm');
  for(let i=0;i<650;i++)tick(0);
  recording=false;state(`settled-attempt-${attempt}`);
 }
 if(w(0x7934)!==3)throw Error('no random failure observed');--attempt;
}
console.log(JSON.stringify({partyCount,fleeActor,variant,events:events.length,final:events.at(-1),attackSetup:events.find(e=>e.party.every(p=>p.kind===0&&p.parameter2===1))}));
fs.writeFileSync(new URL(`../../tmp/native-failed-flee-${partyCount}-${fleeActor}-${variant}-display.json`,import.meta.url),JSON.stringify({party:partyCount,fleeActor,variant,attempt,commits,events},null,2));
console.log(JSON.stringify({commits:commits.length,attempt}));rt.free();
