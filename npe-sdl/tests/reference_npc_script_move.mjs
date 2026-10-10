import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Original mode18 service with diagnostic cached move coordinates. Map,
// tiles, NPC graphics and other NPC records remain from the current NPE.
async function main(){
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/empty-title-first-map-snapshot.bin');
const npe=fs.readFileSync('tmp/tjfy-world-probe.npe'),section=npe.readUInt32BE(0x3a+8*8);
const record=section+npe.readUInt32BE(section),width=npe[record+2];
const cases=[
 ['right',6,9,9,9],['left',9,9,6,9],['down',6,8,6,11],['up',6,11,6,8],
 ['diag-right',6,8,9,10],['diag-left',9,11,6,9],['diag-down',6,8,8,11],
 ['top',3,5,3,7],['bottom',3,11,3,13],['offscreen',20,9,23,9],
 ['zero',6,9,6,9],['diag-equal-right',6,8,9,11],['diag-equal-left',9,11,6,8],
 ['right-edge',14,9,16,9],['left-edge',0,9,2,9],
];
const extra=process.argv.includes('--extra');
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>{const o=rt.advance(mask);o.free();};
const rows=extra?JSON.parse(fs.readFileSync('tmp/native-npc-script-move.json')).slice(0,10):[];
// Earlier sampling read an unverified pair of RAM words as RNG. Remove
// that field; native RNG equivalence is not inferred from an unknown zero.
for(const row of rows){delete row.before.rng;delete row.after.rng;for(const c of row.commits)delete c.state.rng;}
for(const [name,x,y,targetX,targetY] of cases.slice(extra?10:0)){
 rt.restoreBytes(snapshot);for(let t=0;t<15;++t)tick(0);rt.drainEventsBytes();
 const bases=[];
 for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===1&&mem()[i+0x7a58]===1&&
  mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x69d5-9]===7&&
  mem()[i+0x69d6-9]===1&&mem()[i+0x6ca2-9]===0&&mem()[i+0x6ca2-8]===1)bases.push(i);
 if(bases.length!==1)throw Error('script move RAM '+bases.length);
 const base=bases[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const state=()=>({mode:w(0x7934),mapInit:w(0x7ab4),event:w(0x7936),progress:w(0x7a66),context:w(0x7938),
  sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),
  npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),graphic:w(o+2),first:w(o+4),
   last:w(o+6),direction:b(o+8),x:b(o+9),y:b(o+10),moving:b(o+11)};})});
 putW(0x7a50,0);putW(0x7a5c,0);putB(0x7a2e,0);putB(0x7a2c,5);putB(0x69d5,7);putB(0x69d6,4);
 putB(0x6cab,x);putB(0x6cac,y);
 for(let i=0;i<20;++i)putB(0x6ca2+i*14+11,0);
 for(let py=0;py<7;++py)for(let px=0;px<15;++px)
  putW(0x70b4+(py*15+px)*2,npe.readUInt16BE(record+17+((py+5)*width+px)*2));
 putW(0x7652,1);putB(0x7654,1);putW(0x7656,0);putW(0x7658,targetX);putW(0x765a,targetY);
 putW(0x7934,18);putW(0x7936,1);
 const before=state(),commits=[];let finish=-1,failed=false;
 try { for(let t=0;t<180;++t){tick(0);
  for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){
   fs.writeFileSync(`tmp/native-npc-script-move-${rows.length}-${commits.length}.idx`,p);
   commits.push({tick:t,state:state()});}}
  if(w(0x7934)===1&&finish<0)finish=t;
  // Wait for the original7AB4 initialization commit; later idle refreshes are separate.
   if(finish>=0&&w(0x7ab4)===0&&commits.some(c=>c.state.mode===1))break;
 }}catch(error){
  if(name!=='zero'||error.code!=='execution_fault')throw error;
  fs.writeFileSync('tmp/native-npc-script-move-zero-fault.json',JSON.stringify({name,before,code:error.code,
   explanation:'Original service divides by its zero step count; reference runtime faults before a followup.'},null,2)+'\n');
  console.log(JSON.stringify({name,code:error.code}));failed=true;
 }
 if(failed)continue;
 if(finish<0)throw Error('move did not return '+name);
 rows.push({name,x,y,targetX,targetY,before,commits,after:state(),finish});
 fs.writeFileSync('tmp/native-npc-script-move.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name,commits:commits.map(c=>({tick:c.tick,mode:c.state.mode,x:c.state.npc[0].x,
  y:c.state.npc[0].y,direction:c.state.npc[0].direction})),after:state().npc[0],finish}));
}
rt.free();

}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
