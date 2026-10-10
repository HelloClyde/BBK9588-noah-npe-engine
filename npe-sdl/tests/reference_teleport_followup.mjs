import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot'),mem=()=>new Uint8Array(exp.memory.buffer);
const tick=mask=>{const out=rt.advance(mask);out.free();};
const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
// Real map8/18 group5 records are loaded by the original save/read UI.
// Only source location, music and event entry are diagnostic state inputs.
const cases=[
 ['map18-on',1,10,1,17,0,14,8,18,1],
 ['map18-off',1,10,1,17,0,14,8,18,0],
 ['map8-on',1,37,25,24,0,5,72,8,1],
 ['map8-off',1,37,25,24,0,5,72,8,0],
 ['map18-overwrite-on',1,10,1,17,0,14,1,18,1,true],
 ['map18-overwrite-off',1,10,1,17,0,14,1,18,0,true],
];
const rows=[];
let rngDelta;
const u32=(m,j)=>(m[j]*16777216+m[j+1]*65536+m[j+2]*256+m[j+3])>>>0;
try{
 for(const [name,event,targetMap,x,y,marker,nextMode,nextId,sourceMap,music,diagnostic=false] of cases){
  rt.restoreBytes(snapshot);rt.drainEventsBytes();const m=mem(),candidates=[];
  for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push(i);
  key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('teleport base '+live.length);
  const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)],putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};
  const cache=()=>Array.from({length:2},(_,i)=>{const o=0x788e+i*10;return{map:w(o),x:b(o+4),y:b(o+5),marker:b(o+6),mode:b(o+7),nextId:w(o+8),reserved:[b(o+2),b(o+3)]};});
  const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),init:w(0x7ab4),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),enabled:w(0x7a50),slot:b(0x7a04),visited:b(0x7186+targetMap),cache:cache(),storyCache:Array.from({length:15},(_,i)=>{const o=0x74f4+i*6;return{next:w(o),progress:w(o+2),mode:b(o+4)};}),npcState:Array.from({length:200},(_,i)=>b(0x6fba+i)),npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})});
  key(64);
  putW(0x7a58,sourceMap);putW(0x7a50,music);putB(0x69d5,1);putB(0x69d6,1);putB(0x7a2e,0);putB(0x7a2c,0);
  for(const mask of [64,2,2,2,16,2,16,16,64,2,2,2,16,16,16])key(mask);
  if(w(0x7934)!==1||w(0x7a58)!==sourceMap)throw Error('followup source load');
  const record=cache()[0];
  if(record.map!==targetMap||record.x!==x||record.y!==y||record.marker!==marker||record.mode!==nextMode||record.nextId!==8&&sourceMap===18||record.nextId!==72&&sourceMap===8)throw Error('original teleport record changed');
  if(diagnostic)putW(0x7896,nextId);
  putB(0x7a32,0);const pixels=rt.framePixelIndices();rt.drainEventsBytes();
  putW(0x7934,16);putW(0x7936,event);
  const before=state(),commits=[],old=mem().slice();fs.writeFileSync(`tmp/native-teleport-followup-${name}-before.idx`,pixels);
  let finish=-1;
  for(let t=0;t<500;++t){tick(t%13===3?16:t%13===7?64:0);
   for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){const index=commits.length;fs.writeFileSync(`tmp/native-teleport-followup-${name}-${index}.idx`,p);commits.push({tick:t,state:state()});}}
   if(state().mode===1&&finish<0)finish=t;
   if(finish>=0&&w(0x7ab4)===0&&commits.length>=4)break;
  }
  if(finish<0)throw Error('teleport no finish '+name);
  let rngAddress=rngDelta===undefined?undefined:base+rngDelta;
  if(rngAddress===undefined){
   const post=mem(),offsets=new Set(),matches=[];
   for(let j=0;j<Math.min(old.length,post.length);++j)if(old[j]!==post[j])for(let k=Math.max(0,j-3);k<=j&&k+4<old.length;++k)offsets.add(k);
   for(const j of offsets){const v=u32(old,j);if(v>1&&((Math.imul(v,0x514f111f)+1)>>>0)===u32(post,j))matches.push(j);}
   if(matches.length!==1)throw Error('teleport RNG '+JSON.stringify(matches));rngAddress=matches[0];rngDelta=rngAddress-base;
  }
  before.rng=u32(old,rngAddress);const after={...state(),rng:u32(mem(),rngAddress)};
  const audio=JSON.parse(new TextDecoder().decode(rt.drainEventsBytes())).filter(e=>e.type==='audio');
  rows.push({name,event,targetMap,x,y,marker,nextMode,nextId,sourceMap,music,diagnostic,before,commits,after,finish,audio});
  fs.writeFileSync('tmp/native-teleport-followup.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name,commits:commits.length,after:{mode:after.mode,event:after.event,map:after.map,progress:after.progress},rngBefore:before.rng,rngAfter:after.rng,audio}));
 }
}finally{rt.free();}
