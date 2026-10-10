import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
async function main(){
 const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
 const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
 const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot'),mem=()=>new Uint8Array(exp.memory.buffer);
 const stateAt=(base,m=mem())=>{const a=o=>base+o-(o<0x7000?8:0),b=o=>m[a(o)-1],w=o=>b(o)*256+m[a(o)];return{
  mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),money:(w(0x799e)*65536+w(0x79a0))>>>0,enabled:w(0x7a50),slot:b(0x7a04),periodCounter:b(0x7a32),menu:b(0x7a54),propertyChoice:b(0x7a18),member:b(0x7adc),equipmentSlot:b(0x7a2a),ui:b(0x7a4a),partyCount:b(0x7afa),menuPartyCount:b(0x7ab0),actors:Array.from({length:3},(_,i)=>{const o=148*i;return{id:b(0x69d2+o),active:b(0x69f1+o),portrait:w(0x6a02+o),level:b(0x69de+o),maxHp:w(0x6a14+o),hp:w(0x6a16+o),maxQi:w(0x6a18+o),qi:w(0x6a1a+o),attack:w(0x6a22+o),defense:w(0x6a24+o),experience:w(0x6a12+o),experienceToNext:w(0x6a10+o),otherStats:Array.from({length:3},(_,s)=>w(0x6a1c+o+s*2)),gear:Array.from({length:6},(_,s)=>b(0x69d8+o+s)),guards:Array.from({length:4},(_,s)=>w(0x6a04+o+s*2)),skills:Array.from({length:18},(_,s)=>b(0x69df+o+s)),raw:Array.from({length:148},(_,s)=>b(0x69c6+o+s))};}),gearSelected:b(0x7ab2),descLength:w(0x7aba),descStart:w(0x7abe),descEnd:w(0x7abc),
  npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})};};


 const tick=mask=>{const out=rt.advance(mask);out.free();};
 const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
 const rows=[];
 for(const music of [0,1]){
  const targetMap=126;
  rt.restoreBytes(snapshot);rt.drainEventsBytes();const m=mem(),candidates=[];
  for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push(i);
  key(64);const live=candidates.filter(i=>stateAt(i).mode===6);if(live.length!==1)throw Error('cache base '+live.length);
  const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)],putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};
  const cache=()=>Array.from({length:5},(_,i)=>{const o=0x7652+i*10;return{npc:w(o),nextMode:b(o+2),padding:b(o+3),nextId:w(o+4),x:w(o+6),y:w(o+8)};});
  key(64);
  putW(0x7a58,targetMap);putW(0x7a66,168);putW(0x7a50,music);putB(0x7a04,2);
  putB(0x69d5,7);putB(0x69d6,4);putB(0x69d4,1);putB(0x7a2e,13);putB(0x7a2c,5);
  for(const mask of [64,2,2,2,16,2,16,16,64,2,2,2,16,16,16])key(mask);
  if(w(0x7934)!==1||w(0x7a58)!==targetMap)throw Error('chain original map load');
  // The move records, NPCs and tiles are loaded by the original UI from
  // the unmodified NPE. Only select the existing service entry event1.
  putB(0x7a32,0);putW(0x7a5c,0);
  const before=stateAt(base),beforeCache=cache(),setupAudio=JSON.parse(new TextDecoder().decode(rt.drainEventsBytes())).filter(e=>e.type==='audio');
  putW(0x7934,18);putW(0x7936,1);
  const commits=[];let finish=-1;
  for(let t=0;t<360;++t){tick(0);
   for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){
    fs.writeFileSync(`tmp/native-npc-move-chain-${rows.length}-${commits.length}.idx`,p);
    commits.push({tick:t,state:stateAt(base)});}}
   if(w(0x7934)===1&&w(0x7a66)===169&&finish<0)finish=t;
   // Follow mode1 long enough for its deferred map/audio initialization.
   if(finish>=0&&t>=finish+32)break;
  }
  if(finish<0)throw Error('chain did not return');
  const after=stateAt(base),audio=JSON.parse(new TextDecoder().decode(rt.drainEventsBytes())).filter(e=>e.type==='audio');
  rows.push({music,targetMap,before,beforeCache,commits,after,audio,setupAudio,finish});
  fs.writeFileSync('tmp/native-npc-move-chain.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({music,commits:commits.length,finish,mode:after.mode,event:after.event,progress:after.progress,audio,npc:after.npc.slice(0,6)}));

 }
 rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
