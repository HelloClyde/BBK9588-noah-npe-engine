import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Original program/resources remain unchanged. Only two live viewport words
// differ from their grid cells, so erasure and refresh cannot be conflated.
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const rows=[],mem=()=>new Uint8Array(exp.memory.buffer);
for(const [name,mask,lower,upper] of [['right-cache',8,10,196],['left-cache',4,53,0],['down-cache',2,196,53]]){
 rt.restoreBytes(fs.readFileSync('tmp/empty-title-first-map-snapshot.bin'));
 for(let t=0;t<15;++t)rt.advance(0).free();
 const found=[];
 for(let i=0;i+0x8000<mem().length;++i){const w=o=>mem()[i+o-1]*256+mem()[i+o];
  if(w(0x7934)===1&&w(0x7a58)===1&&w(0x7a66)===1&&
     mem()[i+0x69d2-9]===1&&mem()[i+0x69d5-9]===7&&mem()[i+0x69d6-9]===1&&
     mem()[i+0x6ca2-9]===0&&mem()[i+0x6ca2-8]===1&&mem()[i+0x6ca4-8]===7)found.push(i);}
 if(found.length!==1)throw Error('RAM discovery '+found.length);
 const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v&255;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const state=()=>({mode:w(0x7934),map:w(0x7a58),progress:w(0x7a66),direction:b(0x69d4),
  view:[b(0x7a2e),b(0x7a2c),b(0x69d5),b(0x69d6)],cache:Array.from({length:105},(_,i)=>w(0x70b4+i*2))});
 putW(0x7a50,0);putW(0x7a5c,0);
 for(let j=0;j<20;++j){putW(0x6ca2+j*14,0);putB(0x6ca2+j*14+11,0);}
 const original=state();putW(0x70b4+2*22,lower);putW(0x70b4+2*7,upper);
 const before=state(),prefix=`tmp/walk-live-cache-${rows.length}`,frames=[];
 fs.writeFileSync(prefix+'-before.idx',rt.framePixelIndices());
 for(let t=0;t<32;++t){rt.advance(t===0?mask:0).free();
  for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
   fs.writeFileSync(`${prefix}-${frames.length}.idx`,pixels);frames.push({tick:t,state:state()});}}
 const after=state();
 if(after.mode!==1||after.map!==1||![4,5].includes(frames.length))throw Error(name+' unexpected result');
 rows.push({name,mask,lower,upper,prefix,original,before,after,frames});
 console.log(JSON.stringify({name,frames:frames.length,before:before.view,after:after.view}));
}
rt.free();fs.writeFileSync('tmp/walk-live-cache-native.json',JSON.stringify({rows},null,2)+'\n');
