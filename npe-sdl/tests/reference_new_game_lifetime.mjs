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
 const eventSeed=process.argv.includes('--event-word')?11:0,suffix=eventSeed?`-event-${eventSeed}`:'';
 for(const route of ['new','empty'])for(const music of [1,0]){
  rt.restoreBytes(snapshot);rt.drainEventsBytes();const initial=mem(),candidates=[];
  for(let i=0;i+0x8000<initial.length;++i)if(initial[i+0x7934]===1&&initial[i+0x7935]===0&&initial[i+0x7a58]===1&&initial[i+0x7a66]===1&&initial[i+0x7aec]===0xa9&&initial[i+0x7aeb]===6)candidates.push(i);
  key(64);const live=candidates.filter(i=>stateAt(i).mode===6);if(live.length!==1)throw Error('restart RAM '+live.length);
  const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)],putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};
  key(64);putW(0x7a50,0);
  // Diagnostic warm title entry avoids replaying the completed ending.
  // The constructor and empty-slot branch execute the original UI/service.
  putW(0x7934,7);putW(0x7ab4,1);for(let t=0;t<32;++t)tick(0);
  if(w(0x7934)!==7)throw Error('warm title '+w(0x7934));
  if(route==='empty')for(const mask of [2,16,2])key(mask);
  const resident=()=>({context:w(0x7938),npcContext:b(0x7a68),kind:b(0x7a0e),selected:w(0x7a4c),first:w(0x7ac6),last:w(0x7ab6),row:b(0x7a46),ui:b(0x7a4a),quantity:b(0x7a48),choice:b(0x7a0c),descStart:w(0x7abe),descEnd:w(0x7abc),menu:b(0x7a54),member:b(0x7adc),equipmentSlot:b(0x7a2a),gearSelected:b(0x7ab2),descLength:w(0x7aba),menuPartyCount:b(0x7ab0),exitChoice:b(0x7ac4),exitSlot:b(0x7a84),music:w(0x7a50),musicSlot:b(0x7a04),partyCache:Array.from({length:3},(_,i)=>{const o=0x78a2+i*6;return{add:b(o),character:b(o+1),mode:b(o+2),next:w(o+4)};})});
  putW(0x7938,345);putB(0x7a68,17);putB(0x7a0e,2);putW(0x7a4c,46);putW(0x7ac6,45);putW(0x7ab6,54);
  putB(0x7a46,6);putB(0x7a4a,2);putB(0x7a48,99);putB(0x7a0c,1);putW(0x7abe,67);putW(0x7abc,123);
  putB(0x7a54,10);putB(0x7adc,2);putB(0x7a2a,5);putB(0x7ab2,4);putW(0x7aba,222);putB(0x7ab0,3);
  putB(0x7ac4,1);putB(0x7a84,9);putW(0x7a50,music);putB(0x7a04,2);putW(0x7a66,13);putW(0x799e,0);putW(0x79a0,3456);
  putW(0x7936,eventSeed);
  for(let i=0;i<3;++i){const o=0x78a2+i*6;putB(o,1);putB(o+1,i===1?3:2);putB(o+2,1);putW(o+4,11+i);}
  const before=stateAt(base),beforeResident=resident(),rawBefore=mem().slice(),commits=[];
  rt.drainEventsBytes();
  fs.writeFileSync(`tmp/native-restart-${route}-${music}${suffix}-before.idx`,rt.framePixelIndices());
  for(let t=0;t<20;++t){tick(t===0?16:0);for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(pixels){const index=commits.length;fs.writeFileSync(`tmp/native-restart-${route}-${music}${suffix}-${index}.idx`,pixels);commits.push({tick:t,state:stateAt(base),resident:resident()});}}}
  const after=stateAt(base),afterResident=resident(),row={route,music,before,beforeResident,after,afterResident,commits,audio:JSON.parse(new TextDecoder().decode(rt.drainEventsBytes())).filter(e=>e.type==='audio')};
  if(after.mode!==1||after.map!==1||after.progress!==1)throw Error('restart outcome '+JSON.stringify(after));
  // Resolve the live RNG using the next actual NPC period. An unchanged
  // constructor seed cannot be located by scanning only changed words.
  const rawAfter=mem().slice(),counter=after.periodCounter;
  // The seeded resident exit flag suppresses patrol service. Clear it only
  // after freezing the constructor evidence to locate the live RNG.
  putB(0x7ac4,0);
  const followingAdvanceCalls=300;
  for(let t=0;t<followingAdvanceCalls;++t)tick(0);
  if(stateAt(base).periodCounter===counter)throw Error('no following NPC period');
  const postPeriod=mem(),changes=new Set(),rng=[],u32=(m,i)=>(m[i]*16777216+m[i+1]*65536+m[i+2]*256+m[i+3])>>>0;
  for(let j=0;j<Math.min(rawAfter.length,postPeriod.length);++j)if(rawAfter[j]!==postPeriod[j])for(let k=Math.max(0,j-3);k<=j&&k+4<rawAfter.length;++k)changes.add(k);
  for(const j of changes){const old=u32(rawAfter,j),next=u32(postPeriod,j);if(!old||next<=1||old===next)continue;let v=old;for(let calls=1;calls<=64;++calls){v=(Math.imul(v,0x514f111f)+1)>>>0;if(v===next){rng.push({offset:j,before:old,after:next,calls});break;}}}
  if(rng.length!==1)throw Error('restart following RNG '+JSON.stringify({rng,after,post:stateAt(base)}));
  row.followingPeriodRng={...rng[0],followingAdvanceCalls};row.rngBefore=u32(rawBefore,rng[0].offset);row.rngAfter=u32(rawAfter,rng[0].offset);
  if(row.rngBefore!==row.rngAfter)throw Error('constructor consumed RNG');
  rows.push(row);fs.writeFileSync(`tmp/native-new-game-lifetime${suffix}.json`,JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({route,music,eventBefore:before.event,eventAfter:after.event,before:beforeResident,after:afterResident,commits:commits.length,rngBefore:row.rngBefore,rngAfter:row.rngAfter,followingPeriodRng:row.followingPeriodRng,audio:row.audio}));
 }
 rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
