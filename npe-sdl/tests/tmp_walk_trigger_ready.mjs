// Unmodified original program/NPE. Diagnostic RAM is seeded only before entry.
import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot');
const tick=mask=>rt.advance(mask).free(),key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
const mem=()=>new Uint8Array(exp.memory.buffer);
function entry(){
 rt.restoreBytes(snapshot);rt.drainEventsBytes();const m=mem(),candidates=[];
 for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push(i);
 key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('trigger base '+live.length);
 const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};key(64);for(let t=0;t<64;++t)tick(0);
 const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),init:w(0x7ab4),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),enabled:w(0x7a50),slot:b(0x7a04),cache:Array.from({length:100},(_,i)=>b(0x7490+i))});
 return {b,w,putB,putW,state};
}
const loads=[],services=[];
try{
 for(const cfg of [].concat([{entry:'system',map:1},{entry:'title',map:1},{entry:'system',map:2},{entry:'title',map:2},{entry:'system',map:97},{entry:'title',map:97},{entry:'new',map:1},{entry:'empty',map:1}]).slice(0,0)){
  const q=entry(),{putB,putW,w,state}=q;
  if(cfg.entry==='system'||cfg.entry==='title'){
   putW(0x7a58,cfg.map);putB(0x69d5,1);putB(0x69d6,1);putB(0x7a2e,0);putB(0x7a2c,0);
   for(const mask of [64,2,2,2,16,2,16,16])key(mask);if(w(0x7934)!==1)throw Error('save');
  }
  if(cfg.entry!=='system'){putW(0x7934,7);putW(0x7ab4,1);for(let t=0;t<32;++t)tick(0);}
  for(const mask of cfg.entry==='system'?[64,2,2,2,16,16]:cfg.entry==='new'?[]:cfg.entry==='empty'?[2,16,2]:[2,16])key(mask);
  for(let i=0;i<100;++i)putB(0x7490+i,(i*37+0x81)&255);
  const before=state();key(16);if(w(0x7934)!==1||w(0x7a58)!==cfg.map)throw Error('load');
  loads.push({...cfg,before:before.cache,after:state().cache});console.log('load',cfg.entry,cfg.map);
 }
 const cases=[{name:'slot1',event:1},{name:'slot10',event:10},{name:'low-byte1',event:257},{name:'low-byte10',event:266},
  {name:'invalid0',event:0},{name:'invalid11',event:11},{name:'invalid256',event:256},{name:'invalid65535',event:65535},
  {name:'next25',event:10,nextMode:25},{name:'next-zero',event:10,nextMode:0}];
 for(const cfg of cases.slice(0,1)){
  const {putB,putW,w,state}=entry();
  for(let i=0;i<100;++i)putB(0x7490+i,(i*29+0x53)&255);
  for(let i=0;i<10;++i){const o=0x7490+i*10;putW(o,701+i);putB(o+8,cfg.nextMode??1);}
  putW(0x7a50,0);putW(0x7ab4,0);putW(0x7934,20);putW(0x7936,cfg.event);
  const before=state();fs.writeFileSync(`tmp/native-walk-trigger-${cfg.name}-before.idx`,rt.framePixelIndices());
  const commits=[];let finish=-1;
  for(let t=0;t<100;++t){tick(0);for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){fs.writeFileSync(`tmp/native-walk-trigger-${cfg.name}-${commits.length}.idx`,p);commits.push({tick:t,state:state()});}}
   if(w(0x7934)===1&&w(0x7ab4)===0&&finish<0)finish=t;if(finish>=0&&t>=finish+32)break;
  }
  if(finish<0)throw Error('mode20 no finish '+cfg.name);
  services.push({...cfg,before,after:state(),commits});console.log('service',cfg.name,commits.length,state().event);
 }
}finally{rt.free();fs.writeFileSync('tmp/native-walk-trigger-ready-diagnostic.json',JSON.stringify({loads,services},null,2)+'\n');}
