// Original mode22 service: exact UI and signed-fee paths. Diagnostic inputs
// change service entry, amounts and role stats only; original NPE/program stay.
import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot'),mem=()=>new Uint8Array(exp.memory.buffer);
const tick=mask=>{const out=rt.advance(mask);out.free();};
const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
const tests=[
 ['paid',100,10],['short',9,10],['exact',10,10],['free',0,0],
 ['negative-fee',100,65535],['min-fee',100,32768],
 ['negative-money',0xffffffff,10],['money-min',0x80000000,1],
 ['positive-overflow',0x7fffffff,65535],
];
const rows=[];
try{
 for(const [name,money,fee] of tests){
  rt.restoreBytes(snapshot);rt.drainEventsBytes();const m=mem(),candidates=[];
  for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push(i);
  key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('healing base '+live.length);
  const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)],putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};
  const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),money:(w(0x799e)*65536+w(0x79a0))>>>0,init:w(0x7ab4),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),enabled:w(0x7a50),slot:b(0x7a04),maxHp:Array.from({length:3},(_,i)=>w(0x6a14+i*148)),hp:Array.from({length:3},(_,i)=>w(0x6a16+i*148)),maxQi:Array.from({length:3},(_,i)=>w(0x6a18+i*148)),qi:Array.from({length:3},(_,i)=>w(0x6a1a+i*148)),npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})});
  key(64);const pixels=rt.framePixelIndices();
  for(let i=0;i<3;++i){const o=i*148;putW(0x6a14+o,101+i*101);putW(0x6a16+o,7+i);putW(0x6a18+o,81+i*81);putW(0x6a1a+o,3+i);}
  putW(0x799e,money>>>16);putW(0x79a0,money&65535);
  putW(0x7934,22);putW(0x7936,fee);putW(0x7ab4,0);putW(0x7a50,0);
  const before=state(),commits=[];fs.writeFileSync(`tmp/native-healing-service-${name}-before.idx`,pixels);
  let finish=-1;
  for(let t=0;t<250;++t){tick(t%13===3?16:t%13===7?64:0);
   for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){const index=commits.length;fs.writeFileSync(`tmp/native-healing-service-${name}-${index}.idx`,p);commits.push({tick:t,state:state()});}}
   if(state().mode===1&&finish<0)finish=t;
   if(finish>=0&&t>=finish+3)break;
  }
  if(finish<0)throw Error('healing no finish '+name);
  rows.push({name,fee,before,commits,after:state(),finish});
  fs.writeFileSync('tmp/native-healing-service.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name,fee,commits:commits.map(c=>({tick:c.tick,mode:c.state.mode,money:c.state.money})),after:state()}));
 }
}finally{rt.free();}
