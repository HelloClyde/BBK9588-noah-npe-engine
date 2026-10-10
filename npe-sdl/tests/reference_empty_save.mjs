import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync(new URL('../../tmp/opennoah_wasm_bg.wasm',import.meta.url))});
const rows=[];
const noticeKeys=process.argv.includes('--notice-keys');
const tests=noticeKeys?[{enabled:0,mask:16},{enabled:0,mask:64}]:[{enabled:0,mask:0},{enabled:1,mask:0}];
for(const {enabled,mask} of tests){const entry='system';
 const rt=new GameRuntime(fs.readFileSync(new URL('../../tmp/tianjiang-online.opennoah',import.meta.url)),'certified_original_v1');
 rt.restoreBytes(fs.readFileSync(new URL('../../tmp/battle-fresh-start.bin',import.meta.url)));
 const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
 for(let i=0;i+0x8000<mem().length;++i)
  if(mem()[i+0x7934]===3&&mem()[i+0x7935]===0&&mem()[i+0x7936]===1&&mem()[i+0x7a58]===9&&mem()[i+0x7a66]===6)found.push(i);
 if(found.length!==1)throw Error('RAM '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),w=o=>mem()[a(o)-1]*256+mem()[a(o)];
 const putW=(o,v)=>{mem()[a(o)-1]=v>>>8;mem()[a(o)]=v&255;};
 const tick=mask=>{const out=rt.advance(mask);out.free();},idle=n=>{for(let i=0;i<n;++i)tick(0);},key=mask=>{tick(mask);idle(15);};
 const state=()=>({mode:w(0x7934),map:w(0x7a58),progress:w(0x7a66),enabled:w(0x7a50)});
 putW(0x7934,1);putW(0x7ab4,1);putW(0x7a66,1);putW(0x7a50,enabled);idle(35);
 for(const mask of [64,2,2,2,16,16])key(mask);
 const name=noticeKeys?`notice-key-${mask}`:`${entry}-${enabled}`,before=state();
 fs.writeFileSync(new URL(`../../tmp/native-empty-${name}-before.idx`,import.meta.url),rt.framePixelIndices());
 rt.drainEventsBytes();
 const commits=[],audio=[];
 for(let i=0;i<180;++i){
  tick(i===0?16:i===20?mask:0);
  for(let j=0;j<rt.displayCommitCount();++j){
   const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
   const index=commits.length;
   fs.writeFileSync(new URL(`../../tmp/native-empty-${name}-commit-${index}.idx`,import.meta.url),pixels);
   commits.push({index,tick:i,state:state()});
  }
  for(const e of JSON.parse(new TextDecoder().decode(rt.drainEventsBytes())))if(e.type==='audio')audio.push({tick:i,...e});
  if(noticeKeys && state().mode===1 && commits.length>=3)break;
 }
 const after=state();
 if(after.mode!==1||after.map!==before.map||after.progress!==before.progress||after.enabled!==enabled||commits.length!==3)
  throw Error('empty load outcome '+JSON.stringify({before,after,commits}));
 const restored=fs.readFileSync(new URL(`../../tmp/native-empty-${name}-commit-1.idx`,import.meta.url));
 if(!restored.equals(fs.readFileSync(new URL(`../../tmp/native-empty-${name}-before.idx`,import.meta.url))))throw Error('popup background restoration');
 const row={entry,enabled,mask,before,after,commits,audio};rows.push(row);console.log(JSON.stringify(row));
 putW(0x7934,0);rt.free();
}
fs.writeFileSync(new URL(noticeKeys?'../../tmp/native-empty-save-notice-keys.json':'../../tmp/native-empty-save.json',import.meta.url),JSON.stringify(rows,null,2)+'\n');
