import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot'),rows=[];
const cases=[['fixed-correct-on',13,1,0,[0x83,0x86]],['fixed-wrong-on',13,1,2,[0x86]],
 ['fixed-correct-off',13,0,1,[0x83,0x86]],['fixed-wrong-off',13,0,2,[0x86]],
 ['category-exit-on',0,1,1,[0x8e]],['category-third-wrong-on',0,1,2,[0x86]]];
for(const [name,event,music,slot,keys] of cases){
 rt.restoreBytes(snapshot);let record=false;const audio=[];
 const tick=mask=>{const out=rt.advance(mask);out.free();const events=JSON.parse(new TextDecoder().decode(rt.drainEventsBytes()));
  if(record)for(const e of events)if(e.type==='audio')audio.push({...e});};
 const idle=n=>{for(let i=0;i<n;++i)tick(0);};tick(64);idle(15);
 const mem=()=>new Uint8Array(exp.memory.buffer),bases=[];
 for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===6&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)bases.push(i);
 if(bases.length!==1)throw Error('quiz audio RAM '+bases.length);
 const base=bases[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 tick(64);idle(15);putW(0x7a50,music);putB(0x7a04,slot);putW(0x7ab4,1);idle(32);
 putW(0x7934,8);putW(0x7936,event);putW(0x7ab4,1);putW(0x7ad0,5);putB(0x7ada,name.includes('third')?2:0);
 const stages=[];record=true;
 const run=(key,n)=>{const begin=audio.length;
  for(let i=0;i<n;++i){if(key!==null)putW(0x7a76,i===3?key:0);tick(0);}
  stages.push({key,mode:w(0x7934),event:w(0x7936),slot:b(0x7a04),audio:audio.slice(begin)});};
 run(null,32);if(name.includes('third')){const answer=b(0x7a96);putW(0x7938,(answer+1)%4);}
 for(const key of keys)run(key,event<10?240:32);
 rows.push({name,event,music,slot,stages,audio});console.log(JSON.stringify(rows.at(-1)));
}
fs.writeFileSync('tmp/native-quiz-audio.json',JSON.stringify(rows,null,2)+'\n');rt.free();
