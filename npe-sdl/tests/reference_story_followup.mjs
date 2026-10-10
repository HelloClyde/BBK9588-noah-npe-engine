import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
// Short original UI flows. The dialogue/story, followup and map-event records
// come from the unchanged NPE; only entry state and patrol timing are isolated.
async function main(){
 const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
 const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
 const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot');
 const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>{rt.advance(mask).free();};
 const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
 const cases=[{name:'opening-trigger',map:1,progress:1,x:45,y:2,direction:1,mode:5,event:1},
              {name:'npc4-dialogue',map:33,progress:3,x:4,y:6,direction:3,mode:2,event:4}];
 const rows=process.argv.includes('--resume')?JSON.parse(fs.readFileSync('tmp/native-story-followup.json')).filter(r=>cases.some(c=>c.name===r.name)):[];
 for(const c of cases)for(const music of [0,1]){
  if(rows.some(r=>r.name===c.name&&r.music===music))continue;
  rt.restoreBytes(snapshot);rt.drainEventsBytes();const candidates=[];
  for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===1&&mem()[i+0x7a58]===1&&
   mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)candidates.push(i);
  key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);
  if(live.length!==1)throw Error('active RAM '+live.length);const base=live[0];key(64);
  const a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
  const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
  const state=()=>({mode:w(0x7934),event:w(0x7936),progress:w(0x7a66),context:w(0x7938),
   map:w(0x7a58),mapInit:w(0x7ab4),direction:b(0x69d4),sx:b(0x69d5),sy:b(0x69d6),
   vx:b(0x7a2e),vy:b(0x7a2c),npc4:b(0x6fba+4),npc3:b(0x6fba+3),
   npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})});
  putW(0x7a58,c.map);putW(0x7a66,c.progress);putW(0x7a50,music);putB(0x7a04,2);
  const vx=c.map===1?36:0,vy=c.map===1?1:0;
  putB(0x7a2e,vx);putB(0x7a2c,vy);putB(0x69d5,c.x-vx);putB(0x69d6,c.y-vy);putB(0x69d4,c.direction);
  // Save/read through the original system UI rebuilds this map's caches.
  for(const mask of [64,2,2,2,16,2,16,16,64,2,2,2,16,16,16])key(mask);
  if(w(0x7934)!==1||w(0x7a58)!==c.map)throw Error('original map load');
  putB(0x69d4,c.direction);putB(0x7a32,0);putW(0x7a5c,0);
  for(let i=0;i<20;++i)putB(0x6ca2+i*14+11,0);
  putB(0x6fba+3,c.mode===2?1:0);putB(0x6fba+4,0);
  const before=state();rt.drainEventsBytes();const commits=[];let finish=-1;
  fs.writeFileSync(`tmp/native-story-followup-${rows.length}-before.idx`,rt.framePixelIndices());
  for(let t=0;t<360;++t){tick(t===0?(c.mode===2?16:8):t>0&&(w(0x7934)===5||w(0x7934)===2)?t%24===0?16:0:0);
   for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){
    fs.writeFileSync(`tmp/native-story-followup-${rows.length}-${commits.length}.idx`,p);
    commits.push({tick:t,state:state()});}}
   if(w(0x7934)===1&&w(0x7a66)!==c.progress&&finish<0)finish=t;
   if(finish>=0&&w(0x7ab4)===0&&commits.some(x=>x.state.mode===1))break;
  }
  if(finish<0){fs.writeFileSync('tmp/story-followup-failed.json',JSON.stringify({...c,music,before,commits,after:state()},null,2));throw Error('followup did not finish '+c.name);}
  const audio=JSON.parse(new TextDecoder().decode(rt.drainEventsBytes())).filter(e=>e.type==='audio');
  rows.push({...c,music,before,commits,after:state(),audio,finish});
  fs.writeFileSync('tmp/native-story-followup.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name:c.name,music,commits:commits.map(x=>({tick:x.tick,mode:x.state.mode,event:x.state.event,progress:x.state.progress,mapInit:x.state.mapInit})),after:state(),audio}));
 }
 rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
