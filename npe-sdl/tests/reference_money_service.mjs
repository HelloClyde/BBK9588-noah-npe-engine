import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot'),mem=()=>new Uint8Array(exp.memory.buffer);
const tick=mask=>{const out=rt.advance(mask);out.free();};
const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
const tests=[
 ['add',100,50,1,1],['subtract',100,50,0,1],['subtract-short',100,500,0,1],
 ['cap',999980,50,1,1],['add-signed-max',100,32767,1,1],['add-signed-min',100,32768,1,1],
 ['add-minus-one',100,65535,1,1],['subtract-signed-min',100,32768,0,1],
 ['subtract-minus-one',100,65535,0,1],['flag-two',100,50,2,1],
 ['negative-subtract',0xffffffff,50,0,1],['negative-add',0xffffffff,50,1,1],
 ['zero',100,0,1,1],['last-slot',100,50,1,5],['low-byte',100,50,1,257],
 ['invalid-zero',100,50,1,0],['invalid-six',100,50,1,6],['invalid-256',100,50,1,256],
];
const rows=[];
try{
 for(const [name,money,amount,adding,event] of tests){
  rt.restoreBytes(snapshot);rt.drainEventsBytes();const m=mem(),candidates=[];
  for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push(i);
  key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('money base '+live.length);
  const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)],putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};
  const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),money:(w(0x799e)*65536+w(0x79a0))>>>0,init:w(0x7ab4),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),enabled:w(0x7a50),slot:b(0x7a04),npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})});
  key(64);const pixels=rt.framePixelIndices();
  for(let i=0;i<5;++i){const o=0x76da+i*8;putW(o,amount);putW(o+2,789);putB(o+4,adding);putB(o+5,1);}
  putW(0x799e,money>>>16);putW(0x79a0,money&65535);putW(0x7934,11);putW(0x7936,event);
  const before=state(),commits=[];fs.writeFileSync(`tmp/native-money-service-${name}-before.idx`,pixels);
  let finish=-1;
  for(let t=0;t<250;++t){tick(t%13===3?16:t%13===7?64:0);
   for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){const index=commits.length;fs.writeFileSync(`tmp/native-money-service-${name}-${index}.idx`,p);commits.push({tick:t,state:state()});}}
   if(state().mode===1&&finish<0)finish=t;
   if(finish>=0&&t>=finish+3)break;
  }
  if(finish<0)throw Error('money no finish '+name);
  rows.push({name,amount,adding,before,commits,after:state(),finish});
  fs.writeFileSync('tmp/native-money-service.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name,amount,adding,commits:commits.map(c=>({tick:c.tick,mode:c.state.mode,money:c.state.money})),after:state()}));
 }
}finally{rt.free();}
