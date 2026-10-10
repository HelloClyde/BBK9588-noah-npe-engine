import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
async function main(){
 const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
 const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
 const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>{rt.advance(mask).free();};
 const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
 rt.restoreBytes(fs.readFileSync('tmp/native-title-load-saved.snapshot'));rt.drainEventsBytes();
 const candidates=[];
 for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===1&&mem()[i+0x7a58]===1&&mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)candidates.push(i);
 key(64);const active=candidates.filter(i=>mem()[i+0x7934]===6);
 if(active.length!==1)throw Error('active RAM');const base=active[0];key(64);
 const a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const state=()=>({mode:w(0x7934),event:w(0x7936),progress:w(0x7a66),map:w(0x7a58),ready:w(0x7ab4),
  sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),
  npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return {id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};}),
  npcState:Array.from({length:250},(_,i)=>b(0x6fba+i))});
 putW(0x7a50,1);putB(0x7a04,2);putB(0x7a32,0);putW(0x7a5c,0);
 for(let i=0;i<20;++i)putB(0x6ca2+i*14+11,0);
 putW(0x74f4+14*6,15);putW(0x74f6+14*6,1);putB(0x74f8+14*6,14);
 putW(0x7934,14);putW(0x7936,15);rt.drainEventsBytes();
 const before=state(),commits=[];let stopped=-1;
 for(let t=0;t<2400;++t){tick(0);
  for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){
   fs.writeFileSync(`tmp/native-event-chain-${commits.length}.idx`,p);commits.push({tick:t,state:state()});}}
  if(w(0x7934)!==14||w(0x7936)!==15||w(0x7a66)!==1)throw Error('cycle unexpectedly left mode14');
  if(commits.length>=70){stopped=t;break;}
 }
 if(stopped<0)throw Error('cycle prefix too short');
 const prefixState=state(),prefixCommits=commits.length;
 const audio=JSON.parse(new TextDecoder().decode(rt.drainEventsBytes())).filter(e=>e.type==='audio');
 const row={before,commits,prefixState,prefixCommits,stopped,audio,prefixOnly:true,music:1};
 fs.writeFileSync('tmp/native-event-chain.json',JSON.stringify(row,null,2)+'\n');
 console.log(JSON.stringify({prefixCommits,stopped,after:prefixState,audio}));rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
