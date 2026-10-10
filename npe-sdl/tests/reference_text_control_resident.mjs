import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
async function main(){
 const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
 const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
 const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot'),rows=[];
 const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>{rt.advance(mask).free();};
 const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
 for(const route of ['system','title','new','empty','cycle']){
  rt.restoreBytes(snapshot);rt.drainEventsBytes();const candidates=[];
  for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===1&&mem()[i+0x7a58]===1&&mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)candidates.push(i);
  key(64);const active=candidates.filter(i=>mem()[i+0x7934]===6);
  if(active.length!==1)throw Error('active RAM');const base=active[0];key(64);
  const a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
  const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
  const state=()=>({mode:w(0x7934),event:w(0x7936),progress:w(0x7a66),map:w(0x7a58),control:b(0x7a0a),next:w(0x7a82),ready:w(0x7ab4)});
  putW(0x7a50,0);
  if(route==='cycle'){
   putW(0x7a58,18);putW(0x7a66,25);putB(0x7a2e,6);putB(0x7a2c,1);putB(0x69d5,7);putB(0x69d6,4);putB(0x69d4,3);
   for(const mask of [64,2,2,2,16,2,16,16,64,2,2,2,16,16,16])key(mask);
   for(let i=0;i<20;++i)putB(0x6ca2+i*14+11,0);putW(0x7a5c,0);key(8);key(4);putB(0x7a32,0);putB(0x6fba+24,0);
   const commits=[];let patched=0;
   for(let t=0;t<240;++t){tick(t===0?16:t%24===0&&(w(0x7934)===2||w(0x7934)===5)?16:0);
    if(w(0x7934)===5){putB(0x7a0a,5);putW(0x7a82,20);++patched;}
    for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){fs.writeFileSync(`tmp/native-text-cycle-${commits.length}.idx`,p);commits.push({tick:t,...state()});}}
   }
   if(!patched||w(0x7934)!==5||w(0x7936)!==20||commits.filter(c=>c.mode===5).length<4)throw Error('cycle prefix');
   rows.push({route,commits,after:state(),patched,prefixOnly:true});
  }else{
   if(route!=='system'){
    putW(0x7934,7);putW(0x7ab4,1);for(let t=0;t<32;++t)tick(0);
    if(w(0x7934)!==7)throw Error('warm title');
    if(route==='empty')for(const mask of [2,16,2])key(mask);
   }
   putB(0x7a0a,25);putW(0x7a82,65535);const before=state();
   if(route==='system')for(const mask of [64,2,2,2,16,2,16,16,64,2,2,2,16,16,16])key(mask);
   else if(route==='title')for(const mask of [2,16,16])key(mask);
   else key(16);
   const after=state();if(after.mode!==1||after.control!==25||after.next!==65535)throw Error('resident changed '+route+' '+JSON.stringify(after));
   rows.push({route,before,after});
  }
  fs.writeFileSync('tmp/native-text-control-resident.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify(rows.at(-1)));
 }
 rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
