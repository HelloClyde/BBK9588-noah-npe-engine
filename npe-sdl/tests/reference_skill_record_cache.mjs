import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const cases=[{name:'fresh1',id:1,cached:0},{name:'same2',id:2,cached:2},
 {name:'change2',id:2,cached:1},{name:'clear0',id:0,cached:2},
 {name:'fresh41',id:41,cached:1},{name:'invalid3',id:2,event:3,cached:2},
 ...['new','empty','system','title','teleport'].map(route=>({name:route,route,cached:2}))];
const name=process.argv[2],output='tmp/native-skill-record-cache.json';
if(!name){fs.writeFileSync(output,'[]\n');for(const c of cases){
 const p=spawnSync(process.execPath,[process.argv[1],c.name],{stdio:'inherit'});
 if(p.error)throw p.error;if(p.status)throw Error(c.name+' failed '+p.status);}process.exit(0);}
const c=cases.find(c=>c.name===name);if(!c)throw Error('unknown case');
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
 const state=()=>({mode:w(0x7934),event:w(0x7936),map:w(0x7a58),progress:w(0x7a66),init:w(0x7ab4),
  sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),loaded:b(0x7a74),
  bytes:Array.from({length:144},(_,i)=>b(0x730e+i)),skills:Array.from({length:18},(_,i)=>b(0x69df+i)),
  npc:Array.from({length:20},(_,i)=>({id:w(0x6ca2+i*14),x:b(0x6cab+i*14),y:b(0x6cac+i*14),direction:b(0x6caa+i*14)}))});
 if(['new','empty','title'].includes(c.route)){putW(0x7934,7);putW(0x7ab4,1);for(let t=0;t<32;++t)tick(0);}
 for(const mask of c.route==='system'?[64,2,2,2,16,16]:c.route==='title'?[2,16]:c.route==='empty'?[2,16,2]:[])key(mask);
 putB(0x7a74,c.cached);for(let i=0;i<144;++i)putB(0x730e+i,0xa5);
 const title=Buffer.from('bbbac4e6cfc9caf5','hex');for(let i=0;i<12;++i)putB(0x730e+i,title[i]??0);putB(0x731a,0);
 const desc=Buffer.from('bec9cbb5c3f700','hex');for(let i=0;i<desc.length;++i)putB(0x731b+i,desc[i]);
 const commits=[];
 if(!c.route){putW(0x790a,789);putB(0x790c,c.id);putB(0x790d,1);putB(0x790e,1);putW(0x7934,9);putW(0x7936,c.event??1);}
 const before=state();fs.writeFileSync(`tmp/native-skill-record-cache-${name}-before.idx`,rt.framePixelIndices());
 if(c.route){
  if(c.route==='teleport'){
   putW(0x788e,2);putB(0x7892,1);putB(0x7893,1);putB(0x7894,0);putB(0x7895,1);putW(0x7896,789);
   putW(0x7934,16);putW(0x7936,1);let finish=-1;
   for(let t=0;t<100;++t){tick(t%13===3?16:0);if(w(0x7934)===1&&finish<0)finish=t;if(finish>=0&&t>=finish+4)break;}
   if(finish<0||w(0x7a58)!==2)throw Error('teleport failed');
  }else{key(16);for(let t=0;t<40;++t)tick(0);if(w(0x7934)!==1||w(0x7a58)!==1)throw Error('warm route failed');}
 }else{
  let finish=-1;for(let t=0;t<100;++t){tick(t%13===3?16:t%13===7?64:0);
   for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){
    const index=commits.length;fs.writeFileSync(`tmp/native-skill-record-cache-${name}-${index}.idx`,p);commits.push({index,tick:t,state:state()});}}
   if(w(0x7934)===1&&finish<0)finish=t;if(finish>=0&&w(0x7ab4)===0&&commits.length)break;}
  if(finish<0)throw Error('no finish');
 }
 const after=state(),rows=JSON.parse(fs.readFileSync(output)).filter(r=>r.name!==name);
 rows.push({...c,before,commits,after});fs.writeFileSync(output,JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name,commits:commits.length,loaded:after.loaded,preserved:JSON.stringify(before.bytes)===JSON.stringify(after.bytes),padding:after.bytes[119],description:after.bytes.slice(13,24)}));
}finally{rt.free();}
