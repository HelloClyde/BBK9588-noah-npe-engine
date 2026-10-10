import fs from 'node:fs';
import {GameRuntime, initWasm} from '../../tmp/opennoah-headless.mjs';

// Isolated mode-5 dispatches read the unchanged current NPE. No save is edited.
const exp=await initWasm({module_or_path:fs.readFileSync(new URL('../../tmp/opennoah_wasm_bg.wasm',import.meta.url))});
const rt=new GameRuntime(fs.readFileSync(new URL('../../tmp/tianjiang-online.opennoah',import.meta.url)),'certified_original_v1');
const cases=[1,3,6,11,51,87,149,195];
const rows=[];
for(const id of cases){
 rt.restoreBytes(fs.readFileSync(new URL('../../tmp/battle-fresh-start.bin',import.meta.url)));
 const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
 for(let i=0;i+0x8000<mem().length;i++)if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
 if(found.length!==1)throw Error('RAM '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0);
 const w=o=>mem()[a(o)-1]<<8|mem()[a(o)];
 const putW=(o,v)=>{mem()[a(o)-1]=(v&65535)>>>8;mem()[a(o)]=v&255;};
 const tick=mask=>{const out=rt.advance(mask);out.free();};
 const idle=n=>{for(let i=0;i<n;++i)tick(0);};
 fs.writeFileSync(new URL(`../../tmp/native-window-story-${id}-before.idx`,import.meta.url),rt.framePixelIndices());
 putW(0x7934,5);putW(0x7936,id);putW(0x7ab4,1);idle(35);
 const row={id,pages:[]};
 for(let page=0;page<20&&w(0x7934)===5&&w(0x7936)===id;++page){
  fs.writeFileSync(new URL(`../../tmp/native-window-story-${id}-page-${page}.idx`,import.meta.url),rt.framePixelIndices());
  row.pages.push({rect:[0x791e,0x7920,0x7922,0x7924].map(w),offset:w(0x7a1a),style:mem()[a(0x7ace)-1],more:mem()[a(0x7a1c)-1]});
  tick(16);idle(15);
 }
 if(!row.pages.length||row.pages.length===20)throw Error('story dispatch '+id+' '+JSON.stringify(row));
 row.after={mode:w(0x7934),event:w(0x7936)};
 rows.push(row);console.log(JSON.stringify(row));
 putW(0x7934,0); // Avoid a retained battle-mode signature on the next restore.
}
fs.writeFileSync(new URL('../../tmp/native-window-story-pages.json',import.meta.url),JSON.stringify(rows,null,2));
rt.free();
