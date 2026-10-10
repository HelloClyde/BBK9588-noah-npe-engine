import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
async function main(){
 const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
 const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
 const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot');
 const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>{rt.advance(mask).free();};
 const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
 const invalid=process.argv.includes('--invalid-followup');
 const prefix=invalid?'native-dialogue-invalid':'native-dialogue-return';
 const cases=[{name:'plain-npc',map:1,progress:3,x:33,y:13,vx:26,vy:9,direction:3,npc:1},
  {name:'full-story20',map:18,progress:25,x:13,y:5,vx:6,vy:1,direction:3,npc:24},
  {name:'window-story28',map:20,progress:30,x:7,y:11,vx:0,vy:7,direction:3,npc:25}];
 const rows=process.argv.includes('--resume')?JSON.parse(fs.readFileSync(`tmp/${prefix}.json`)):[];
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
   map:w(0x7a58),mapInit:w(0x7ab4),direction:b(0x69d4),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),
   npcState:Array.from({length:250},(_,i)=>b(0x6fba+i)),
   npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})});
  putW(0x7a58,c.map);putW(0x7a66,c.progress);putW(0x7a50,music);putB(0x7a04,2);
  putB(0x7a2e,c.vx);putB(0x7a2c,c.vy);putB(0x69d5,c.x-c.vx);putB(0x69d6,c.y-c.vy);putB(0x69d4,c.direction);
  // The original system UI reloads the unchanged target NPE map/cache.
  for(const mask of [64,2,2,2,16,2,16,16,64,2,2,2,16,16,16])key(mask);
  if(w(0x7934)!==1||w(0x7a58)!==c.map)throw Error('original map load');
  putB(0x69d4,c.direction);putB(0x7a32,0);putW(0x7a5c,0);
  for(let i=0;i<20;++i)putB(0x6ca2+i*14+11,0);
  // Directional input resets the original map's periodic deadline. Return
  // to the entry cell before capturing, so an overdue UI timer is not
  // mixed into the synchronous dialogue's map initialization.
  key(8);key(4);
  if(w(0x7934)!==1||b(0x7a2e)+b(0x69d5)!==c.x||b(0x7a2c)+b(0x69d6)!==c.y)throw Error('timer reset route');
  putB(0x7a32,0);
  putB(0x6fba+c.npc,c.npc===25?1:0);
  const before=state();rt.drainEventsBytes();const commits=[];let entered=false,finish=-1,patches=0;
  const patchedMode=music?255:0,patchedEvent=65535,targetMode=c.npc===1?2:5;
  fs.writeFileSync(`tmp/${prefix}-${rows.length}-before.idx`,rt.framePixelIndices());
  for(let t=0;t<600;++t){tick(t===0?16:t%24===0&&(w(0x7934)===2||w(0x7934)===5)?16:0);
   if(w(0x7934)===2||w(0x7934)===5)entered=true;
   // Diagnostic resident continuation only: keep the NPE and its text intact.
   // Patch after the original loader has entered the waiting text service.
   if(invalid&&w(0x7934)===targetMode){putB(0x7a0a,patchedMode);putW(0x7a82,patchedEvent);++patches;}
   for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){
    fs.writeFileSync(`tmp/${prefix}-${rows.length}-${commits.length}.idx`,p);
    commits.push({tick:t,state:state()});}}
   if(entered&&w(0x7934)===1&&w(0x7ab4)===0&&commits.some(x=>x.state.mode===1)){finish=t;break;}
  }
  if(finish<0){fs.writeFileSync('tmp/dialogue-return-failed.json',JSON.stringify({...c,music,before,commits,after:state()},null,2));throw Error('return did not finish '+c.name);}
  const audio=JSON.parse(new TextDecoder().decode(rt.drainEventsBytes())).filter(e=>e.type==='audio');
  if(invalid&&(!patches||w(0x7936)!==patchedEvent))throw Error('continuation patch not exercised');
  rows.push({...c,music,before,commits,after:state(),audio,finish,...(invalid?{patchedMode,patchedEvent,patches,targetMode}:{} )});
  fs.writeFileSync(`tmp/${prefix}.json`,JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name:c.name,music,commits:commits.map(x=>({tick:x.tick,mode:x.state.mode,event:x.state.event,progress:x.state.progress,mapInit:x.state.mapInit})),audio,after:{mode:w(0x7934),event:w(0x7936),progress:w(0x7a66),npcState:b(0x6fba+c.npc)}}));
 }
 rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
