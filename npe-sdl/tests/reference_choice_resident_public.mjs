import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot'),mem=()=>new Uint8Array(exp.memory.buffer);
const tick=mask=>rt.advance(mask).free(),key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
const cases=[
 {name:'default',keys:[16]}, {name:'toggle-right',keys:[8,16]},
 {name:'toggle-left',keys:[4,16]}, {name:'toggle-twice',keys:[8,4,16]},
 {name:'ignored',keys:[1,2,32,64,16]}, {name:'last-slot',event:5,keys:[8,16]},
 {name:'word-257',event:257,keys:[16]}, {name:'zero-alias',event:0,keys:[8,16]},
 {name:'six-chase',event:6,keys:[16]}, {name:'negative-two',event:65535,keys:[8,16]},
];
const rows=[];
try{for(const c of cases){
 rt.restoreBytes(snapshot);rt.drainEventsBytes();const m=mem(),candidates=[];
 for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push(i);
 key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('choice base '+live.length);
 const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)],putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};
 key(64);
 for(let i=0;i<5;++i){const o=0x781c+i*6;putB(o,1);putB(o+1,1);putW(o+2,0x1234+i);putW(o+4,0x4321+i);}
 const event=c.event??1,rawIndex=(event-1)&65535,index=rawIndex<32768?rawIndex:rawIndex-65536,address=0x781c+index*6;
 if(event===0||event===6||event===257||event===65535){putB(address,1);putB(address+1,1);putW(address+2,0xabcd);putW(address+4,0xcdef);}
 const table=()=>Array.from({length:30},(_,i)=>b(0x781c+i));
 const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),init:w(0x7ab4)});
 putW(0x7934,17);putW(0x7936,event);putW(0x7938,9);putW(0x7ab4,1);putB(0x7a32,0);
 const before=state(),beforeBytes=table(),alias=Array.from({length:6},(_,i)=>b(address+i)),commits=[],phases=[];
 fs.writeFileSync(`tmp/native-choice-resident-public-${c.name}-before.idx`,rt.framePixelIndices());
 for(const mask of [0,...c.keys]){const begin=commits.length;
  for(let t=0;t<(phases.length?16:64);++t){tick(t===0?mask:0);
   for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){const n=commits.length;fs.writeFileSync(`tmp/native-choice-resident-public-${c.name}-${n}.idx`,p);commits.push({phase:phases.length,tick:t,state:state()});}}
   if(mask===16&&w(0x7934)===1&&w(0x7ab4)===0)break;
  }
  phases.push({key:mask,commits:commits.length-begin,after:state()});
 }
 rows.push({...c,address,before,beforeBytes,alias,commits,phases,after:state(),afterBytes:table()});
 fs.writeFileSync('tmp/native-choice-resident-public.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name:c.name,phases,after:state()}));
}}finally{rt.free();}
