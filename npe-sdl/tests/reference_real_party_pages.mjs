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
 const rows=[];
 for(const test of [
  {name:'status-after-menu-flags',active:[1,1,1],keys:[64,16,16,2,2,2,1,1,1,64]},
  {name:'equipment-after-menu-flags',active:[1,1,1],keys:[64,16,2,16,...Array(6).fill(8),2,...Array(6).fill(8),2,...Array(6).fill(8),64]},
  {name:'status-sparse-after-menu',active:[1,0,1],keys:[64,16,16,2,2,1,1,64]},
  {name:'status-real-three',beforeEntry:true,active:[1,1,1],keys:[64,16,16,2,2,2,1,1,1,64]},
  {name:'equipment-real-three',beforeEntry:true,active:[1,1,1],keys:[64,16,2,16,...Array(6).fill(8),2,...Array(6).fill(8),2,...Array(6).fill(8),64]},
  {name:'status-sparse-entry',beforeEntry:true,active:[1,0,1],keys:[64,16,16,2,2,1,1,64]},
  {name:'equipment-sparse-entry',beforeEntry:true,active:[1,0,1],keys:[64,16,2,16,2,8,8,1,4,64]},
  {name:'non-one-flag',beforeEntry:true,active:[1,2,1],keys:[64,16,16,2,2,1,1,64]},
 ]){  rt.restoreBytes(snapshot);rt.drainEventsBytes();const rawBefore=mem().slice();const m=mem(),candidates=[];
  for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push({base:i,before:stateAt(i,m)});
  if(test.beforeEntry){for(const c of candidates){const a=o=>c.base+o-(o<0x7000?8:0);for(let i=0;i<3;++i)mem()[a(0x69f1+148*i)-1]=test.active[i];c.before=stateAt(c.base);}}
  const beforePixels=rt.framePixelIndices(),initial=[];
  for(let t=0;t<16;++t){tick(t===0?64:0);for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p)initial.push({tick:t,pixels:p,states:candidates.map(c=>stateAt(c.base))});}}
  const live=candidates.filter(c=>stateAt(c.base).mode===6);if(live.length!==1)throw Error('property RAM '+live.length);
  const base=live[0].base,index=candidates.indexOf(live[0]),before=live[0].before,stages=[];
  if(test.active && !test.beforeEntry){const a=o=>base+o-(o<0x7000?8:0);for(let i=0;i<3;++i)mem()[a(0x69f1+148*i)-1]=test.active[i];mem()[a(0x7afa)-1]=test.active.reduce((sum,v)=>sum+v,0);}
  fs.writeFileSync(`tmp/native-real-party-${test.name}-before.idx`,beforePixels);
  const save=(mask,commits)=>{const phase=stages.length;stages.push({mask,commits:commits.map((c,i)=>{fs.writeFileSync(`tmp/native-real-party-${test.name}-${phase}-commit-${i}.idx`,c.pixels);return{tick:c.tick,state:c.state};}),after:stateAt(base)});};
  save(64,initial.map(c=>({tick:c.tick,pixels:c.pixels,state:c.states[index]})));
  for(const mask of test.keys.slice(1)){const commits=[];for(let t=0;t<16;++t){if(mask>=128){const a=o=>base+o-(o<0x7000?8:0),key=mask===128?0x86:mask===256?0x85:0x84;mem()[a(0x7a76)-1]=0;mem()[a(0x7a76)]=t===3?key:0;}tick(t===0&&mask<128?mask:0);for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p)commits.push({tick:t,pixels:p,state:stateAt(base)});}}save(mask,commits);}
  const row={...test,before,stages,after:stateAt(base)};
  const rawAfter=mem().slice(),counter=row.after.periodCounter;let elapsed;
  for(let t=0;t<300;++t){tick(0);if(stateAt(base).periodCounter!==counter){elapsed=t;break;}}
  if(elapsed===undefined)throw Error('following property period');
  const u32=(m,j)=>(m[j]*16777216+m[j+1]*65536+m[j+2]*256+m[j+3])>>>0;
  const post=mem(),changes=new Set(),rng=[];
  for(let j=0;j<Math.min(rawAfter.length,post.length);++j)if(rawAfter[j]!==post[j])for(let k=Math.max(0,j-3);k<=j&&k+4<rawAfter.length;++k)changes.add(k);
  for(const j of changes){const old=u32(rawAfter,j),next=u32(post,j);if(old<=1||next<=1||old===next)continue;let v=old;
   for(let calls=1;calls<=32;++calls){v=(Math.imul(v,0x514f111f)+1)>>>0;if(v===next){rng.push({offset:j,before:old,after:next,calls});break;}}}
  if(rng.length!==1)throw Error('property RNG '+JSON.stringify(rng));
  row.followingPeriodRng={...rng[0],elapsed};row.rngBefore=u32(rawBefore,rng[0].offset);row.rngAfter=u32(rawAfter,rng[0].offset);
  rows.push(row);fs.writeFileSync('tmp/native-real-party-menu.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name:test.name,stages:stages.map(s=>({mask:s.mask,commits:s.commits.length,mode:s.after.mode,event:s.after.event,context:s.after.context,menu:s.after.menu,member:s.after.member,ui:s.after.ui}))}));
 }
 rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
