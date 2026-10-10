import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const rows=[];
for(const entry of ['system','title'])for(const choice of [1,2])for(const slot of [3,10]){
 rt.restoreBytes(fs.readFileSync('tmp/battle-fresh-start.bin'));
 const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
 for(let i=0;i+0x8000<mem().length;++i)
  if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
 if(found.length!==1)throw Error('RAM '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putW=(o,v)=>{mem()[a(o)-1]=v>>>8;mem()[a(o)]=v&255;},putB=(o,v)=>{mem()[a(o)-1]=v;};
 const tick=mask=>{const out=rt.advance(mask);out.free();};
 const idle=n=>{for(let i=0;i<n;++i)tick(0);};
 const key=mask=>{tick(mask);idle(15);};
 const state=()=>({mode:w(0x7934),event:w(0x7936),map:w(0x7a58),progress:w(0x7a66),
  choice:b(0x7ac4),exitSlot:w(0x7a84),moveKind:w(0x7a86),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c)});
 putW(0x7934,1);putW(0x7ab4,1);putW(0x7a66,1);putW(0x7a50,0);putB(0x7ac4,0);idle(35);
 for(const mask of [64,2,2,2,16,2,16,16])key(mask);
 if(w(0x7934)!==1)throw Error('save failed '+JSON.stringify(state()));
 const saved=state();putW(0x7a66,4);putB(0x7ac4,choice);putW(0x7a84,slot);
 if(entry==='title'){putW(0x7934,7);putW(0x7ab4,1);idle(35);}
 const before=state();
 for(const mask of entry==='system'?[64,2,2,2,16,16]:[2,16])key(mask);
 const menu=state();key(16);idle(150);
 const after=state();
 if(after.mode!==1||after.progress!==1)throw Error('load failed '+JSON.stringify(after));
 const row={entry,choice,slot,saved,before,menu,after};rows.push(row);
 fs.writeFileSync('tmp/native-exit-state-load.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify(row));putW(0x7934,0);
}
rt.free();
