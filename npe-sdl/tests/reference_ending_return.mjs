// Original program and NPE unchanged; isolate mode21 in live map RAM.
import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const dir='tmp/ending-return/original';fs.mkdirSync(dir,{recursive:true});
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot');
const tick=mask=>rt.advance(mask).free(),key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
const mem=()=>new Uint8Array(exp.memory.buffer),rows=[];
try{
 for(const music of [1,0])for(const kind of ['new','load']){
  rt.restoreBytes(snapshot);rt.drainEventsBytes();const m=mem(),candidates=[];
  for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push(i);
  key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('ending base '+live.length);
  const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
  const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};
  key(64);for(let t=0;t<64;++t)tick(0);
  const state=()=>({mode:w(0x7934),event:w(0x7936),init:w(0x7ab4),map:w(0x7a58),progress:w(0x7a66),
   sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),
   titleSlot:b(0x7a7a),titleSelection:b(0x7a7c),titleStage:b(0x7a7e),mapHandle:w(0x7a6a),battleHandle:w(0x7a88),battleFlags:w(0x7aa4),
   npc:Array.from({length:280},(_,i)=>b(0x6ca2+i)),viewport:Array.from({length:105},(_,i)=>w(0x70b4+i*2))});
  putW(0x7a50,music);putW(0x7934,21);putW(0x7936,0xbeef);putW(0x7ab4,0);
  putB(0x7a7a,3);putB(0x7a7c,3);putB(0x7a7e,2);
  const row={music,kind,before:state(),phases:[]};rt.drainEventsBytes();
  for(const [label,mask] of (kind==='new'?[['title',0],['down',2],['up',1],['new',16]]:[['title',0],['down',2],['slots',16],['load',16]])){
   const phase={label,commits:[]};
   for(let t=0;t<32;++t){tick(t===0?mask:0);for(let j=0;j<rt.displayCommitCount();++j){
    const p=rt.displayCommitPixelIndices(j);if(p){const index=phase.commits.length;
     fs.writeFileSync(`${dir}/${music}-${kind}-${label}-${index}.idx`,p);phase.commits.push({tick:t,state:state()});
    }
   }}
   phase.after=state();phase.events=JSON.parse(new TextDecoder().decode(rt.drainEventsBytes()));row.phases.push(phase);
   console.log(music,kind,label,phase.commits.length,JSON.stringify(phase.after));
  }
  rows.push(row);
 }
}finally{rt.free();fs.writeFileSync(`${dir}/report.json`,JSON.stringify(rows,null,2)+'\n');}
