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
 const u32=(m,j)=>(m[j]*16777216+m[j+1]*65536+m[j+2]*256+m[j+3])>>>0;
 let tests=[{name:'three',order:[0,1,2],flags:[1,1,1],music:0},{name:'two-second',order:[0,1,2],flags:[1,1,0],music:0},
 {name:'two-third',order:[0,1,2],flags:[1,0,1],music:0},{name:'one',order:[0,1,2],flags:[1,0,0],music:0},
 {name:'reverse',order:[0,2,1],flags:[1,1,1],music:0},{name:'non-one',order:[0,1,2],flags:[1,2,1],music:0},
 {name:'three-music',order:[0,1,2],flags:[1,1,1],music:1}];
 const timingOnly=process.argv.includes('--timing-only');
 if(timingOnly)tests=[1,2,4,8,16,32,64].map(mask=>({name:'key-'+mask,order:[0,1,2],flags:[1,1,0],music:0,mask}));
 const timingSteps=process.argv.includes('--timing-steps');
 if(timingSteps)tests=[0,16,64].map(mask=>({name:'steps-'+mask,order:[0,1,2],flags:[1,1,0],music:0,mask}));
 const chainInput=process.argv.includes('--chain-input');
 if(chainInput)tests=[16,64].map(mask=>({name:'chain-key-'+mask,order:[0,1,2],flags:[1,1,1],music:0,mask}));
 const output=chainInput?'tmp/native-leave-notice-chain-input.json':timingSteps?'tmp/native-leave-notice-steps.json':timingOnly?'tmp/native-leave-notice-input.json':'tmp/native-leave-battle.json';
 const extraOnly=process.argv.includes('--music-only');
 const rows=extraOnly?JSON.parse(fs.readFileSync('tmp/native-leave-battle.json')).filter(r=>!r.music):[];
 for(const test of extraOnly?tests.filter(t=>t.music):tests){
  rt.restoreBytes(snapshot);rt.drainEventsBytes();const m=mem(),candidates=[];
  for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push(i);
  const initialNpcContext=new Map(candidates.map(i=>[i,m[i+0x7a68-1]]));
  key(64);const live=candidates.filter(i=>stateAt(i).mode===6);if(live.length!==1)throw Error('leave RAM '+live.length);
  const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)],putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};
  key(64);
  if(process.argv.includes('--cold-context')){const row={source:'true boot/new game/UI-saved snapshot, before F4 and after return',mode:w(0x7934),map:w(0x7a58),npcContextBeforeF4:initialNpcContext.get(base),npcContext:b(0x7a68)};
   fs.writeFileSync('tmp/native-notice-cold-context.json',JSON.stringify(row,null,2)+'\n');console.log(JSON.stringify(row));rt.free();return;}
  putW(0x7a50,0);putW(0x7a58,117);putB(0x69d5,1);putB(0x69d6,1);putB(0x7a2e,0);putB(0x7a2c,0);
  for(const mask of [64,2,2,2,16,2,16,16,64,2,2,2,16,16,16])key(mask);
  if(w(0x7934)!==1||w(0x7a58)!==117)throw Error('real map117 load');
  const original=stateAt(base).actors.map(v=>v.raw);
  for(let i=0;i<3;++i){for(let j=0;j<148;++j)putB(0x69c6+148*i+j,original[test.order[i]][j]);putB(0x69f1+148*i,test.flags[i]);}
  putW(0x7a50,test.music);putW(0x7934,10);putW(0x7936,1);
  const before=stateAt(base),rawBefore=mem().slice(),commits=[];fs.writeFileSync(`tmp/native-leave-battle-${test.name}-before.idx`,rt.framePixelIndices());
  const battle=()=>({party:b(0x7afa),actor:b(0x7a9e),target:b(0x7ac0),enemies:[w(0x792e),w(0x7930),w(0x7932)],
   records:Array.from({length:b(0x7afa)+3},(_,i)=>{const o=148*i;return {hp:w(0x64a0+o),maxHp:w(0x649e+o),qi:w(0x64a4+o),maxQi:w(0x64a2+o),attack:w(0x64b0+o),defense:w(0x64b2+o),speed:w(0x64a8+o),status:Array.from({length:4},(_,s)=>w(0x648e+o+s*2))};})});
  let totalSteps=0n;
  for(let t=0;t<(timingOnly||timingSteps?170:400);++t){tick((timingOnly||timingSteps||chainInput)&&t===20?test.mask:0);for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){const index=commits.length;fs.writeFileSync(`tmp/native-leave-battle-${test.name}-${index}.idx`,p);commits.push({tick:t,step:String(totalSteps+rt.displayCommitStepOffset(j)),stepsPerSecond:rt.displayCommitStepsPerSecond(),state:stateAt(base),battle:battle()});}}totalSteps+=rt.displayCommitStepSpan();}
  const row={...test,before,after:stateAt(base),battle:battle(),commits};if(row.after.mode!==3)throw Error('incomplete leave chain '+test.name+' mode'+row.after.mode);
  const rawAfter=mem().slice();if(test.music){const changes=new Set(),rng=[];
   for(let j=0;j<Math.min(rawBefore.length,rawAfter.length);++j)if(rawBefore[j]!==rawAfter[j])for(let k=Math.max(0,j-3);k<=j&&k+4<rawBefore.length;++k)changes.add(k);
   for(const j of changes){const old=u32(rawBefore,j),next=u32(rawAfter,j);if(old===0||next<=1||old===next)continue;let v=old;for(let calls=1;calls<=64;++calls){v=(Math.imul(v,0x514f111f)+1)>>>0;if(v===next){rng.push({offset:j,before:old,after:next,calls});break;}}}
   if(rng.length!==1){fs.writeFileSync('tmp/leave-music-before.mem',rawBefore);fs.writeFileSync('tmp/leave-music-after.mem',rawAfter);throw Error('leave RNG '+JSON.stringify(rng));}row.rng=rng[0];
  }
  rows.push(row);fs.writeFileSync(output,JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name:test.name,commits:commits.map(c=>({tick:c.tick,mode:c.state.mode,event:c.state.event})),afterActors:row.after.actors.map(a=>[a.id,a.active]),battle:row.battle,rng:row.rng}));
 }
 rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
