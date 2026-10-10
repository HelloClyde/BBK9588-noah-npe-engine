import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';

async function main(){
 const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
 const loopOnly=process.argv.includes('--save-loop-only');
 const rows=loopOnly?JSON.parse(fs.readFileSync('tmp/native-system-slot-return.json','utf8')).filter(r=>r.name!=='save'):[];
 for(const name of loopOnly?['save']:['save','load','empty-load']){
  const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
  rt.installPersistentDataBytes(fs.readFileSync('tmp/native-title-load-persistent.bin'));
  const tick=mask=>{const o=rt.advance(mask);o.free();};
  const idle=n=>{for(let i=0;i<n;++i)tick(0);};
  rt.setBootTarget('title');const boot=rt.boot();boot.free();idle(1000);
  const mem=()=>new Uint8Array(exp.memory.buffer),found=[];
  for(let i=0;i+0x8000<mem().length;++i)
   if(mem()[i+0x7934]===7&&mem()[i+0x7935]===0&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)found.push(i);
  if(found.length!==1)throw Error('cold RAM '+JSON.stringify(found));
  const base=found[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
  const state=()=>({mode:w(0x7934),event:w(0x7936),map:w(0x7a58),progress:w(0x7a66),
   sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),
   money:(w(0x799e)*65536+w(0x79a0))>>>0,redraw:w(0x7ab4),
   npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;
    return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8),moving:b(o+11)};})});
  tick(16);idle(283);
  if(w(0x7934)!==1)throw Error('new game '+JSON.stringify(state()));
  const before=state();fs.writeFileSync(`tmp/native-system-slot-${name}-before.idx`,rt.framePixelIndices());
  const stages=[],keys=name==='save'?[64,2,2,2,16,2,2,2,2,1,1,1,1,2,16,16]:name==='load'?[64,2,2,2,16,16,16]:[64,2,2,2,16,16,2,16];
  for(let phase=0;phase<keys.length;++phase){
   const commits=[],ticks=phase===keys.length-1?(name==='empty-load'?112:8):16;
   for(let t=0;t<ticks;++t){tick(t===0?keys[phase]:0);
    for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(!p)continue;
     const index=commits.length;fs.writeFileSync(`tmp/native-system-slot-${name}-${phase}-commit-${index}.idx`,p);
     commits.push({index,tick:t,state:state()});
    }
   }
   stages.push({phase,mask:keys[phase],commits,after:state()});
  }
  const row={name,before,stages,after:state()};rows.push(row);rows.sort((a,b)=>['save','load','empty-load'].indexOf(a.name)-['save','load','empty-load'].indexOf(b.name));
  fs.writeFileSync('tmp/native-system-slot-return.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name,before:before.npc,after:row.after.npc,
    stages:stages.map(s=>({phase:s.phase,commits:s.commits.map(c=>({tick:c.tick,mode:c.state.mode,npc:c.state.npc}))}))}));
  rt.free();
 }
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
