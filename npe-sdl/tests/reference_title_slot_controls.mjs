import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';

// Unmodified original program, current NPE and existing original slot data.
// All state transitions below are ordinary title/chooser inputs.
const dir='tmp/title-slot-controls/original';
fs.mkdirSync(dir,{recursive:true});
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const paths=[
 ['cancel',[
  ['down',2],['open',16],['slot2',2],['slot3',2],['wrap-down',2],
  ['wrap-up',1],['up2',1],['up1',1],['cancel',64],['exit-row',2],
  ['read-row',1],['reopen',16],['slot3-again',1],['cancel-again',64]]],
 ['empty', [['down',2],['open',16],['slot3',1],['new-from-empty',16]]]
];
const rows=[];
for(const [name,keys] of paths){
 const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
 try{
  rt.installPersistentDataBytes(fs.readFileSync('tmp/native-title-load-persistent.bin'));
  const tick=mask=>rt.advance(mask).free();
  rt.setBootTarget('title');rt.boot().free();for(let i=0;i<1000;++i)tick(0);
  const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
  for(let i=0;i+0x8000<mem().length;++i)
   if(mem()[i+0x7934]===7&&mem()[i+0x7935]===0&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)found.push(i);
  if(found.length!==1)throw Error('live title RAM '+JSON.stringify(found));
  const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
  const state=()=>({mode:w(0x7934),event:w(0x7936),init:w(0x7ab4),
   slot:b(0x7a7a),selection:b(0x7a7c),stage:b(0x7a7e),map:w(0x7a58),progress:w(0x7a66)});
  const row={name,before:state(),phases:[]};rt.drainEventsBytes();
  for(const [label,mask] of keys){
   const phase={label,mask,commits:[]};
   for(let t=0;t<(label==='new-from-empty'?128:32);++t){
    tick(t===0?mask:0);
    for(let j=0;j<rt.displayCommitCount();++j){
     const p=rt.displayCommitPixelIndices(j);if(!p)continue;
     fs.writeFileSync(`${dir}/${name}-${label}-${phase.commits.length}.idx`,p);
     phase.commits.push({tick:t,state:state()});
    }
   }
   phase.after=state();phase.events=JSON.parse(new TextDecoder().decode(rt.drainEventsBytes()));
   row.phases.push(phase);console.log(name,label,phase.commits.length,JSON.stringify(phase.after));
  }
  rows.push(row);
 }finally{rt.free();}
}
fs.writeFileSync(`${dir}/report.json`,JSON.stringify(rows,null,2)+'\n');
