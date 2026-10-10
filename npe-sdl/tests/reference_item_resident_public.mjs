import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot'),mem=()=>new Uint8Array(exp.memory.buffer);
const tick=mask=>{const out=rt.advance(mask);out.free();};
const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
const tests=[
 ['normal-add',5,2,1,1,1],['normal-subtract',1,2,0,5,3],
 ['wide-map-add',5,2,1,1,256],['wide-map-wrap',65535,2,1,5,256],
 ['wide-map-subtract',1,2,0,257,256],
 ['wide-exit-add',5,2,1,1,512],['wide-exit-subtract',32768,1,0,1,512],
 ['signed-battle-add',5,2,1,1,65280],['signed-battle-subtract',1,2,0,1,65280],
 ['self-record-add',5,2,1,1,1908],['self-record-subtract',5,2,0,1,1908],
 ['self-next-add',5,2,1,1,1910],['self-next-subtract',5,2,0,1,1910],
 ['zero-name-cache-add',5,2,1,1,1536],
 ['raw-ram-add',5,2,1,1,2560],['raw-ram-subtract',1,2,0,1,2560],
];
const realOnly=process.argv.includes('--real-only');
const rows=realOnly?JSON.parse(fs.readFileSync('tmp/native-item-resident-public.json')).filter(r=>!r.real_npe_record):[];
try{
 for(const [name,current,quantity,adding,event,item] of tests.filter(t=>!realOnly||t[0].startsWith('real-'))){
  rt.restoreBytes(snapshot);rt.drainEventsBytes();const m=mem(),candidates=[];
  for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push(i);
  key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('item base '+live.length);
  const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)],putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};
  const tables=()=>[...Array.from({length:40},(_,i)=>b(0x76b0+i)),...Array.from({length:40},(_,i)=>b(0x76da+i))];
  const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),quantity:w(0x67c8+(item<32768?item:item-65536)*2),loadedItem:b(0x7a72),itemName:Array.from({length:12},(_,i)=>b(0x739e+i)),init:w(0x7ab4),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),enabled:w(0x7a50),slot:b(0x7a04),npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})});
  key(64);const pixels=rt.framePixelIndices();
  const real_npe_record=name.startsWith('real-');
  if(!real_npe_record)for(let i=0;i<5;++i){const o=0x76b0+i*8;putW(o,quantity);putW(o+2,item);putW(o+4,789);putB(o+6,adding);putB(o+7,1);}
  const o=0x76b0+((event&255)-1)*8,record=real_npe_record?{quantity:w(o),item:w(o+2),nextId:w(o+4),adding:b(o+6),mode:b(o+7)}:{quantity,item,nextId:789,adding,mode:1};
  if(real_npe_record&&(record.quantity!==quantity||record.item!==item||record.adding!==adding||record.mode!==1))throw Error('original record changed');
  putW(0x67c8+(item<32768?item:item-65536)*2,current);putW(0x7934,12);putW(0x7936,event);
  const before=state(),beforeBytes=tables(),commits=[];fs.writeFileSync(`tmp/native-item-resident-public-${name}-before.idx`,pixels);
  let finish=-1;
  for(let t=0;t<250;++t){tick(t%13===3?16:t%13===7?64:0);
   for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){const index=commits.length;fs.writeFileSync(`tmp/native-item-resident-public-${name}-${index}.idx`,p);commits.push({tick:t,state:state()});}}
   if(state().mode===1&&finish<0)finish=t;
   if(finish>=0&&t>=finish+3)break;
  }
  if(finish<0)throw Error('item no finish '+name);
  rows.push({name,item,quantity,adding,record,real_npe_record,before,beforeBytes,commits,after:state(),afterBytes:tables(),finish});
  fs.writeFileSync('tmp/native-item-resident-public.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name,item,quantity,adding,commits:commits.map(c=>({tick:c.tick,mode:c.state.mode,quantity:c.state.quantity})),after:state()}));
 }
}finally{rt.free();}
