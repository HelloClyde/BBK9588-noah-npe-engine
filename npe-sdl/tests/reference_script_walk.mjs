import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Original mode19 service; diagnostic resident cache and player origin.
// Current NPE terrain, graphic resources and other records are preserved.
async function main(){
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/empty-title-first-map-snapshot.bin');
const npe=fs.readFileSync('tmp/tjfy-world-probe.npe'),section=npe.readUInt32BE(0x3a+8*8);
const record=section+npe.readUInt32BE(section),width=npe[record+2],height=npe[record+3];
const cases=[
 ['right',1,43,2,46,1,-1],['left',1,43,2,40,1,-1],
 ['down',1,43,2,43,4,-1],['up',1,43,4,43,0,-1],
 ['tie-vertical',1,43,2,46,4,-1],['zero',1,43,2,43,1,-1],
 ['blocked-npc',1,43,2,46,1,0],['invalid',0,43,2,46,1,-1],
 ['retained-fourth',260,43,2,46,1,-1]
];
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>{const o=rt.advance(mask);o.free();};
const rows=[];
for(const [name,inputEvent,x,y,targetX,targetY,blockedNpc] of cases){
 const music=0,nextMode=1,nextId=0;
 rt.restoreBytes(snapshot);for(let t=0;t<15;++t)tick(0);rt.drainEventsBytes();
 const bases=[];
 for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===1&&mem()[i+0x7a58]===1&&
  mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x69d5-9]===7&&
  mem()[i+0x69d6-9]===1&&mem()[i+0x6ca2-9]===0&&mem()[i+0x6ca2-8]===1)bases.push(i);
 if(bases.length!==1)throw Error('script move RAM '+bases.length);
 const base=bases[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const state=()=>({mode:w(0x7934),event:w(0x7936),progress:w(0x7a66),context:w(0x7938),
  direction:b(0x69d4),mapInit:w(0x7ab4),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),
  npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),graphic:w(o+2),first:w(o+4),
   last:w(o+6),direction:b(o+8),x:b(o+9),y:b(o+10),moving:b(o+11)};})});
 putW(0x7a50,music);putB(0x7a04,2);putW(0x7a5c,0);putB(0x7a32,0);
 putB(0x7a2e,36);putB(0x7a2c,0);putB(0x69d5,x-36);putB(0x69d6,y);putB(0x69d4,1);
 for(let i=0;i<20;++i)putB(0x6ca2+i*14+11,0);
 if(blockedNpc>=0){putB(0x6cab,44);putB(0x6cac,2);}
 for(let py=0;py<7;++py)for(let px=0;px<15;++px)
  putW(0x70b4+(py*15+px)*2,npe.readUInt16BE(record+17+(py*width+px+36)*2));
 const cached=0x7728+(inputEvent===260?3:0)*10;
 putB(cached,1);putB(cached+2,nextMode);putW(cached+4,nextId);putW(cached+6,targetX);putW(cached+8,targetY);
 putW(0x7934,19);putW(0x7936,inputEvent);
 const before=state(),commits=[];let finish=-1;
 for(let t=0;t<180;++t){tick(0);
  for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){
   fs.writeFileSync(`tmp/native-script-walk-${rows.length}-${commits.length}.idx`,p);
   commits.push({tick:t,state:state()});}}
  if(w(0x7934)===1&&finish<0)finish=t;
  if(finish>=0&&w(0x7ab4)===0&&commits.some(c=>c.state.mode===1))break;
 }
 if(finish<0)throw Error('move did not return '+name);
 rows.push({name,music,blockedNpc,audio:JSON.parse(new TextDecoder().decode(rt.drainEventsBytes())).filter(e=>e.type==='audio'),inputEvent,nextMode,nextId,x,y,targetX,targetY,before,commits,after:state(),finish});
 fs.writeFileSync('tmp/native-script-walk.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name,commits:commits.map(c=>({tick:c.tick,mode:c.state.mode,x:c.state.vx+c.state.sx,y:c.state.vy+c.state.sy,direction:c.state.direction})),after:state(),finish}));
}
rt.free();

}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
