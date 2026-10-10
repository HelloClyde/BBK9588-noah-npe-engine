import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Original mode18 service with diagnostic cached move coordinates. Map,
// tiles, NPC graphics and other NPC records remain from the current NPE.
async function main(){
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/empty-title-first-map-snapshot.bin');
const npe=fs.readFileSync('tmp/tjfy-world-probe.npe'),section=npe.readUInt32BE(0x3a+8*8);
const record=section+npe.readUInt32BE(section),width=npe[record+2],height=npe[record+3];
const cases=[
 ['word-left-wrap',1,1,0,2,9,65535,9],
 ['word-up-wrap',1,1,0,6,2,6,65535],
 ['target-255',1,1,0,253,9,255,9],
 ['start-255',1,1,0,255,9,254,9],
 ['counter-wrap',1,1,0,0,9,255,9],
 ['negative-count',1,1,0,6,9,32774,32777]
];
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>{const o=rt.advance(mask);o.free();};
const edgeOnly=process.argv.includes('--edge-only');
const counterOnly=process.argv.includes('--counter-only');
const rows=edgeOnly||counterOnly?JSON.parse(fs.readFileSync('tmp/native-npc-move-byte.json')):[];
for(const {params,index} of cases.map((params,index)=>({params,index})).filter(c=>edgeOnly?c.index>=5:counterOnly?c.index===4:true)){
 const [name,inputEvent,nextMode,nextId,x,y,targetX,targetY]=params;
 const nonterminal=name==='counter-wrap';
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
 const cached=0x7652+(inputEvent===261?4:0)*10;
 putW(cached,1);putB(cached+2,nextMode);putW(cached+4,nextId);putW(cached+6,targetX);putW(cached+8,targetY);
 putW(0x7934,18);putW(0x7936,inputEvent);
 const before=state(),commits=[];let finish=-1,saw255=false;
 for(let t=0;t<(nonterminal?3000:180);++t){tick(0);
  for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){
   fs.writeFileSync(`tmp/native-npc-move-byte-${index}-${commits.length}.idx`,p);
   commits.push({tick:t,state:state()});}}
  if(nonterminal){
   if(w(0x7934)!==18)throw Error('counter path unexpectedly returned');
   if(b(0x6cab)===255)saw255=true;
   if(saw255&&b(0x6cab)===1&&finish<0)finish=t;
   // The first commit at this coordinate can be the preceding erase.
   // Observe its draw too, stopping during the following wait, before step2.
   if(finish>=0&&t>=finish+2)break;
  }else{
   if(w(0x7934)===1&&finish<0)finish=t;
   // Wait for the original7AB4 initialization commit; later idle refreshes are separate.
   if(finish>=0&&w(0x7ab4)===0&&commits.some(c=>c.state.mode===1))break;
  }
 }
 if(finish<0)throw Error('move did not return '+name);
 rows[index]={name,nonterminal,inputEvent,nextMode,nextId,x,y,targetX,targetY,before,commits,after:state(),finish};
 fs.writeFileSync('tmp/native-npc-move-byte.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name,commits:commits.map(c=>({tick:c.tick,mode:c.state.mode,x:c.state.npc[0].x,
  y:c.state.npc[0].y,direction:c.state.npc[0].direction})),after:state().npc[0],finish}));
}
rt.free();

}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
