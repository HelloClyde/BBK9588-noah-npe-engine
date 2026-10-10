import fs from 'node:fs';
import { GameRuntime, initWasm } from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const pack=fs.readFileSync('tmp/tianjiang-online.opennoah');
const snapshot=fs.readFileSync('tmp/empty-title-cold-snapshot.bin');
const report=[];
for(const route of ['new','empty'])for(const priorDirection of [0,3]) {
 const rt=new GameRuntime(pack,'certified_original_v1');rt.restoreBytes(snapshot);
 const mem=()=>new Uint8Array(exp.memory.buffer);
 const tick=mask=>{const out=rt.advance(mask);out.free();};
 const idle=()=>{for(let i=0;i<15;++i)tick(0);};
 const candidates=[];
 for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934-1]===0&&mem()[i+0x7934]===7&&
     mem()[i+0x7a7c-1]===1&&mem()[i+0x7a7e-1]===1)candidates.push(i);
 tick(2);idle();
 const bases=candidates.filter(i=>mem()[i+0x7a7c-1]===2);
 if(bases.length!==1)throw Error('title live RAM discovery '+bases.length);
 const base=bases[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1];
 const w=o=>mem()[a(o)-1]*256+mem()[a(o)];
 if(route==='new'){tick(1);idle();}
 mem()[a(0x69d4)-1]=priorDirection;
 if(route==='empty'){tick(16);idle();}
 const commits=[];
 for(let t=0;t<20;++t){
  tick(t===0?16:0);
  for(let j=0;j<rt.displayCommitCount();++j){
   const pixels=rt.displayCommitPixelIndices(j);if(!pixels)continue;
   const index=commits.length;
   fs.writeFileSync(`tmp/native-new-game-${route}-prior-${priorDirection}-commit-${index}.idx`,pixels);
   commits.push({index,tick:t,mode:w(0x7934),direction:b(0x69d4)});
  }
 }
 const row={route,priorDirection,mode:w(0x7934),map:w(0x7a58),progress:w(0x7a66),
  direction:b(0x69d4),view:[b(0x7a2e),b(0x7a2c)],
  npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return {
   id:w(o),graphic:w(o+2),direction:b(o+8),x:b(o+9),y:b(o+10),moving:b(o+11)};}),commits};
 if(row.mode!==1||row.map!==1||row.progress!==1||row.direction!==1||commits.length!==3)
  throw Error('unexpected new-game constructor state '+JSON.stringify(row));
 report.push(row);console.log(JSON.stringify({route,priorDirection,direction:row.direction,commits}));rt.free();
}
fs.writeFileSync('tmp/native-new-game-display.json',JSON.stringify(report,null,2)+'\n');
