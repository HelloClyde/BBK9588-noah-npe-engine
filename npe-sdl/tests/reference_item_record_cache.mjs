import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const tests=[{name:'load1',id:1},{name:'same1',id:1,poison:1},{name:'change2',id:2,poison:1},{name:'clear0',id:0,poison:1}];
const selected=process.argv[2];
if(!selected){fs.writeFileSync('tmp/native-item-record-cache.json','[]\n');
 for(const test of tests){const c=spawnSync(process.execPath,[process.argv[1],test.name],{stdio:'inherit'});if(c.error)throw c.error;if(c.status)throw Error(test.name+' failed '+c.status);}process.exit(0);}
const test=tests.find(t=>t.name===selected);if(!test)throw Error('unknown case');
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
 if(test.poison){putB(0x7a72,test.poison);for(let i=0;i<142;++i)putB(0x739e+i,0xa5);
  const name=Buffer.from('bbbac4e6b5c0bedf','hex');for(let i=0;i<12;++i)putB(0x739e+i,name[i]??0);putB(0x73aa,0);
  const desc=Buffer.from('bec9cbb5c3f700','hex');for(let i=0;i<desc.length;++i)putB(0x73ab+i,desc[i]);}
 const state=()=>({mode:w(0x7934),event:w(0x7936),map:w(0x7a58),progress:w(0x7a66),init:w(0x7ab4),
  sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),loaded:b(0x7a72),
  bytes:Array.from({length:142},(_,i)=>b(0x739e+i)),quantity:w(0x67c8+test.id*2),
  npc:Array.from({length:20},(_,i)=>({id:w(0x6ca2+i*14),x:b(0x6cab+i*14),y:b(0x6cac+i*14),direction:b(0x6caa+i*14)}))});
 putW(0x76b0,1);putW(0x76b2,test.id);putW(0x76b4,789);putB(0x76b6,1);putB(0x76b7,1);
 putW(0x7934,12);putW(0x7936,1);const before=state(),commits=[];
 fs.writeFileSync(`tmp/native-item-record-cache-${test.name}-before.idx`,rt.framePixelIndices());
 let finish=-1;for(let t=0;t<100;++t){tick(t%13===3?16:t%13===7?64:0);
  for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){fs.writeFileSync(`tmp/native-item-record-cache-${test.name}-${commits.length}.idx`,p);commits.push({tick:t,state:state()});}}
  if(w(0x7934)===1&&finish<0)finish=t;if(finish>=0&&t>=finish+4)break;}
 if(finish<0)throw Error('mode12 failed');const after=state();
 if(test.name==='same1'&&JSON.stringify(after.bytes)!==JSON.stringify(before.bytes))throw Error('same ID reloaded');
 if(test.name==='clear0'&&(after.loaded!==0||after.bytes.some(v=>v)))throw Error('clear0 failed');
 const rows=JSON.parse(fs.readFileSync('tmp/native-item-record-cache.json'));rows.push({...test,before,commits,after});
 fs.writeFileSync('tmp/native-item-record-cache.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name:test.name,id:test.id,loaded:after.loaded,commits:commits.length,bytes:after.bytes}));
}finally{rt.free();}
