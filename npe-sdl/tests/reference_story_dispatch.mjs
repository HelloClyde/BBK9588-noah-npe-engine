import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Original mode14 service: low-byte event indexing and retained/empty slots.
// NPE resources are preserved; diagnostic service fields isolate dispatch.
async function main(){
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/empty-title-first-map-snapshot.bin');
const npe=fs.readFileSync('tmp/tjfy-world-probe.npe'),section=npe.readUInt32BE(0x3a+8*8);
const record=section+npe.readUInt32BE(section),width=npe[record+2],height=npe[record+3];
const cases=[
 ['retained-fifteenth',15,1,71,33,1],['retained-low-byte',271,1,71,33,0],
 ['invalid-zero',0,1,71,33,0],['invalid-16',16,1,71,33,0],['invalid-low-zero',256,1,71,33,0],
 ['empty-fifteenth',15,0,0,0,0],['next-invalid',15,25,77,33,0]
];
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>{const o=rt.advance(mask);o.free();};
const resume=process.argv.includes('--resume');
const rows=resume?JSON.parse(fs.readFileSync('tmp/native-story-dispatch.json')):[];
for(const [name,inputEvent,nextMode,nextId,targetProgress,music] of cases.filter(c=>!rows.some(r=>r.name===c[0]))){
 rt.restoreBytes(snapshot);for(let t=0;t<15;++t)tick(0);rt.drainEventsBytes();
 const bases=[];
 for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===1&&mem()[i+0x7a58]===1&&
  mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x69d5-9]===7&&
  mem()[i+0x69d6-9]===1&&mem()[i+0x6ca2-9]===0&&mem()[i+0x6ca2-8]===1)bases.push(i);
 for(let t=0;t<16;++t)tick(t===0?64:0);
 const live=bases.filter(i=>mem()[i+0x7934]===6);
 if(live.length!==1)throw Error('live story RAM '+live.length);
 for(let t=0;t<16;++t)tick(t===0?64:0);
 rt.drainEventsBytes();
 const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const state=()=>({mode:w(0x7934),event:w(0x7936),progress:w(0x7a66),context:w(0x7938),
  direction:b(0x69d4),mapInit:w(0x7ab4),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),
  npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),graphic:w(o+2),first:w(o+4),
   last:w(o+6),direction:b(o+8),x:b(o+9),y:b(o+10),moving:b(o+11)};})});
 putW(0x7a50,music);putB(0x7a04,2);putB(0x7a32,0);putW(0x7a5c,0);
 for(let i=0;i<20;++i)putB(0x6ca2+i*14+11,0);
 putW(0x74f4+14*6,nextId);putW(0x74f6+14*6,targetProgress);putB(0x74f8+14*6,nextMode);
 putW(0x7934,14);putW(0x7936,inputEvent);
 const before=state(),commits=[];let finish=-1;
 for(let t=0;t<180;++t){tick(0);
  for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){
   fs.writeFileSync(`tmp/native-story-dispatch-${rows.length}-${commits.length}.idx`,p);
   commits.push({tick:t,state:state()});}}
  if(w(0x7934)===1&&finish<0)finish=t;
  if(finish>=0&&w(0x7ab4)===0&&commits.some(c=>c.state.mode===1))break;
 }
 if(finish<0)throw Error('move did not return '+name);
 rows.push({name,music,targetProgress,audio:JSON.parse(new TextDecoder().decode(rt.drainEventsBytes())).filter(e=>e.type==='audio'),inputEvent,nextMode,nextId,before,commits,after:state(),finish});
 fs.writeFileSync('tmp/native-story-dispatch.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name,commits:commits.map(c=>({tick:c.tick,mode:c.state.mode,x:c.state.npc[0].x,
  y:c.state.npc[0].y,direction:c.state.npc[0].direction})),after:state(),finish}));
}
rt.free();

}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
