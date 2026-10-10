import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const prefixMatrix=process.argv[2]==='--inventory-prefix';
const routes=prefixMatrix?['new','system','title']:['new','empty','system','title','teleport','equipment'];
const output=prefixMatrix?'tmp/native-shop-prefix-lifetime.json':'tmp/native-item-record-lifetime.json';
const route=process.argv[prefixMatrix?3:2];
if(!route){
 fs.writeFileSync(output,'[]\n');
 for(const name of routes){const p=spawnSync(process.execPath,[process.argv[1],...(prefixMatrix?['--inventory-prefix']:[]),name],{stdio:'inherit'});if(p.error)throw p.error;if(p.status)throw Error(name+' failed '+p.status);}process.exit(0);
}
if(!routes.includes(route))throw Error('unknown route');
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>rt.advance(mask).free();
const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
try{
 rt.restoreBytes(fs.readFileSync('tmp/native-title-load-saved.snapshot'));rt.drainEventsBytes();
 const candidates=[];for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===1&&mem()[i+0x7935]===0&&mem()[i+0x7a58]===1&&mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)candidates.push(i);
 key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('RAM discovery');
 const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};key(64);
 putW(0x7a50,0);putB(0x7a32,0);
 const state=()=>({mode:w(0x7934),map:w(0x7a58),progress:w(0x7a66),selected:b(0x7ab2),
  loaded:b(0x7a72),inventoryPrefix:w(0x67c6),bytes:Array.from({length:142},(_,i)=>b(0x739e+i)),length:w(0x7aba),start:w(0x7abe),end:w(0x7abc)});
 const poison=()=>{putB(0x7a72,1);for(let i=0;i<142;++i)putB(0x739e+i,0xa5);
  const name=Buffer.from('bbbac4e6b5c0bedf','hex');for(let i=0;i<12;++i)putB(0x739e+i,name[i]??0);putB(0x73aa,0);
  const desc=Buffer.from('bec9cbb5c3f700','hex');for(let i=0;i<desc.length;++i)putB(0x73ab+i,desc[i]);};
 if(route==='new'||route==='empty'||route==='title'){
  // Diagnostic warm title entry; constructor and load branches are original.
  putW(0x7934,7);putW(0x7ab4,1);for(let t=0;t<32;++t)tick(0);
 }
 const keys=route==='system'?[64,2,2,2,16,16]:route==='title'?[2,16]:route==='empty'?[2,16,2]:[];
 for(const mask of keys)key(mask);
 poison();if(prefixMatrix)putW(0x67c6,321);const before=state(),stages=[];
 if(route==='teleport'){
  putW(0x788e,2);putB(0x7892,1);putB(0x7893,1);putB(0x7894,0);putB(0x7895,1);putW(0x7896,789);
  putW(0x7934,16);putW(0x7936,1);
  let finish=-1;for(let t=0;t<100;++t){tick(t%13===3?16:0);if(w(0x7934)===1&&finish<0)finish=t;if(finish>=0&&t>=finish+4)break;}
  if(finish<0||w(0x7a58)!==2)throw Error('teleport failed');
 }else if(route==='equipment'){
  // Only the actor's diagnostic gear and resident cache are seeded.
  // Public F4/Space/Down/Right enter and navigate the unmodified menu.
  for(let i=0;i<6;++i)putB(0x69d8+i,i===0?1:0);
  for(const mask of [64,16,2,16,8,4]){
   let commits=0;for(let t=0;t<16;++t){tick(t===0?mask:0);
    for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p)fs.writeFileSync(`tmp/native-item-record-equipment-${stages.length}-${commits++}.idx`,p);}}
   stages.push({mask,commits,...state()});
  }
  fs.writeFileSync('tmp/native-item-record-equipment-attempt.json',JSON.stringify({route,before,stages},null,2)+'\n');
  const same=stages[3],empty=stages[4],reload=stages[5];
  if(same.loaded!==1||JSON.stringify(same.bytes)!==JSON.stringify(before.bytes))throw Error('equipment same-ID reload');
  if(empty.selected!==0||empty.loaded!==1||JSON.stringify(empty.bytes)!==JSON.stringify(before.bytes))throw Error('empty selection overwrote shared record');
  if(reload.selected!==1||reload.loaded!==1||JSON.stringify(reload.bytes)!==JSON.stringify(before.bytes))throw Error('equipment return reloaded same-ID');
 }else{
  key(16);for(let t=0;t<40;++t)tick(0);if(w(0x7934)!==1||w(0x7a58)!==1)throw Error('warm route failed');
 }
 const after=state();
 if(prefixMatrix&&after.inventoryPrefix!==321)throw Error('prefix lifetime changed '+route);
 const rows=JSON.parse(fs.readFileSync(output)).filter(r=>r.route!==route);rows.push({route,before,stages,after});
 fs.writeFileSync(output,JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({route,loaded:after.loaded,inventoryPrefix:after.inventoryPrefix,preserved:JSON.stringify(before.bytes)===JSON.stringify(after.bytes),equipment:stages.map(s=>({mask:s.mask,loaded:s.loaded,selected:s.selected,length:s.length,start:s.start,end:s.end}))}));
}finally{rt.free();}
