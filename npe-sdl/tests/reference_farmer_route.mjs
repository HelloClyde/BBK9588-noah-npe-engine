import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Short original-firmware route. Only the entry exit destination and
// progression are diagnostic; the target map/cache/NPC data load from NPE.
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
rt.restoreBytes(fs.readFileSync('tmp/empty-title-first-map-snapshot.bin'));
const tick=mask=>{const out=rt.advance(mask);out.free();};for(let i=0;i<15;++i)tick(0);
const mem=()=>new Uint8Array(exp.memory.buffer),bases=[];
for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===1&&mem()[i+0x7a58]===1&&
 mem()[i+0x7aec]===0xa9&&mem()[i+0x69d5-9]===7&&mem()[i+0x69d6-9]===1)bases.push(i);
if(bases.length!==1)throw Error('farmer RAM '+bases.length);
const base=bases[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
const state=()=>({mode:w(0x7934),map:w(0x7a58),progress:w(0x7a66),sx:b(0x69d5),sy:b(0x69d6),
 vx:b(0x7a2e),vy:b(0x7a2c),x:b(0x69d5)+b(0x7a2e),y:b(0x69d6)+b(0x7a2c),direction:b(0x69d4),kind:w(0x7a86),
 npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),moving:b(o+11)};})});
putW(0x7a50,0);putW(0x7a66,3);putW(0x7a5c,0);putB(0x7ac4,0);
for(let i=0;i<20;++i){putW(0x6ca2+i*14,0);putB(0x6ca2+i*14+11,0);}
const e=0x6b82+10*24;
if(b(e)!==43||b(e+1)!==1)throw Error('entry exit not found');
putB(e+2,0);putW(e+4,81);putW(e+6,3);putB(e+8,12);putB(e+9,13);putB(e+11,1);putW(e+12,0);
let entered=false;
for(let i=0;i<512;++i){tick(i===0?1:0);if(w(0x7a58)===81){entered=true;
 // Isolate legacy numerical walking from the independent patrol timer.
 for(let n=0;n<20;++n)putB(0x6ca2+n*14+11,0);
}}
if(!entered||w(0x7934)!==1)throw Error('target map failed to load');
const entry=state(),steps=[];fs.writeFileSync('tmp/native-farmer-entry.idx',rt.framePixelIndices());
for(const [i,key] of [...'RRURRUUUU'].entries()){
 const mask=key==='R'?8:1,before=state(),commits=[];
 for(let t=0;t<32;++t){tick(t===0?mask:0);
  for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){
   fs.writeFileSync(`tmp/native-farmer-${i}-${commits.length}.idx`,p);commits.push({tick:t});}}}
 const after=state();steps.push({key,mask,before,after,commits});
 console.log(JSON.stringify({step:i,key,x:after.x,y:after.y,kind:after.kind,commits:commits.length}));
}
fs.writeFileSync('tmp/native-farmer-route.json',JSON.stringify({entry,steps},null,2)+'\n');rt.free();
