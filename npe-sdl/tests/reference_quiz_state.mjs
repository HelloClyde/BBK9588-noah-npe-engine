import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
async function main(){
 const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
 const pack=fs.readFileSync('tmp/tianjiang-online.opennoah');let rt=new GameRuntime(pack,'certified_original_v1');
 const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot');
 const mem=()=>new Uint8Array(exp.memory.buffer);let base;
 const tick=mask=>{const o=rt.advance(mask);o.free();};
 const idle=n=>{for(let t=0;t<n;++t)tick(0);};
 const key=mask=>{tick(mask);idle(15);};
 const locate=mode=>{const m=mem(),found=[];for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===mode&&m[i+0x7935]===0&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)found.push(i);if(found.length!==1)throw Error('quiz lifetime RAM '+found.length);base=found[0];};
 const a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),stats:Array.from({length:22},(_,i)=>w(0x7684+2*i)),count:w(0x7ad0),wrong:b(0x7ada),previous:w(0x7ade),category:w(0x7a52),type:b(0x7ae0),length:b(0x7ad8),input:Array.from({length:33},(_,i)=>b(0x77dc+i))});
 const seed=(kind,boundary=false)=>{
  for(let i=0;i<22;++i)putW(0x7684+2*i,kind==='saved'?(boundary?65535-i:100+7*i):60000+i);
  putW(0x7ad0,kind==='saved'?321:65535);putB(0x7ada,kind==='saved'?2:255);
  putW(0x7ade,kind==='saved'?1780:65535);putW(0x7a52,kind==='saved'?6:13);putB(0x7ae0,1);
  putB(0x7ad8,kind==='saved'?8:32);for(let i=0;i<33;++i)putB(0x77dc+i,i===32?0:65+(i%26));
 };
 const rows=[];
 for(const route of ['system','title','new','empty','cold','cold-boundary']){
  rt.restoreBytes(snapshot);key(64);locate(6);key(64);putW(0x7a50,0);putB(0x7ac4,0);putW(0x7a66,1);
  let saved,persistent;
  if(!['new','empty'].includes(route)){
   seed('saved',route==='cold-boundary');
   for(const mask of [64,2,2,2,16,2,16,16])key(mask);
   idle(80);if(w(0x7934)!==1)throw Error('quiz native save '+route);
   saved=state();persistent=rt.persistentDataBytes();
   fs.writeFileSync(`tmp/native-quiz-state-${route}-persistent.bin`,persistent);
  }
  if(route.startsWith('cold')){
   rt.free();rt=new GameRuntime(pack,'certified_original_v1');rt.installPersistentDataBytes(persistent);rt.setBootTarget('title');const o=rt.boot();o.free();idle(1000);locate(7);
  } else if(route!=='system'){
   putW(0x7934,7);putW(0x7ab4,1);idle(32);
  }
  if(route==='empty')for(const mask of [2,16,2])key(mask);
  else if(route==='system')for(const mask of [64,2,2,2,16,16])key(mask);
  else if(['title','cold','cold-boundary'].includes(route))for(const mask of [2,16])key(mask);
  if(!route.startsWith('cold'))seed('live');
  const before=state(),commits=[];
  for(let t=0;t<192;++t){tick(t===0?16:0);for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){fs.writeFileSync(`tmp/native-quiz-state-${route}-${commits.length}.idx`,p);commits.push({tick:t,state:state()});}}if(t>32&&w(0x7934)===1)break;}
  const after=state();if(after.mode!==1||after.map!==1||after.progress!==1)throw Error('quiz lifetime result '+route);
  const row={route,saved,before,after,commits};rows.push(row);fs.writeFileSync('tmp/native-quiz-state.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({route,saved,before,after,commits:commits.length}));
 }
 rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
