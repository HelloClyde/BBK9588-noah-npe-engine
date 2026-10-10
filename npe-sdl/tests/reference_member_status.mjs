import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
async function main(){
 const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
 const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
 const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot'),mem=()=>new Uint8Array(exp.memory.buffer);
 const stateAt=(base,m=mem())=>{const a=o=>base+o-(o<0x7000?8:0),b=o=>m[a(o)-1],w=o=>b(o)*256+m[a(o)];return{
  mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),money:(w(0x799e)*65536+w(0x79a0))>>>0,enabled:w(0x7a50),slot:b(0x7a04),periodCounter:b(0x7a32),menu:b(0x7a54),propertyChoice:b(0x7a18),member:b(0x7adc),equipmentSlot:b(0x7a2a),ui:b(0x7a4a),guards:Array.from({length:3},(_,member)=>Array.from({length:4},(_,i)=>w(0x6a04+148*member+i*2))),
  npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})};};
 const tick=mask=>{const out=rt.advance(mask);out.free();};
 const rows=[];
 const cases=Array.from({length:16},(_,mask)=>({name:`negative-mask-${mask}`,guards:[Array.from({length:4},(_,i)=>mask&(1<<i)?65535:0)],keys:[64,16,16,64]}));
 for(let i=0;i<4;++i)cases.push({name:`positive-${i}`,guards:[Array.from({length:4},(_,s)=>s===i?1:0)],keys:[64,16,16,64]});
 cases.push({name:'all-positive',guards:[[1,2,32767,1]],keys:[64,16,16,64]},
  {name:'signed-extremes',guards:[[32768,32767,65535,0]],keys:[64,16,16,64]},
  {name:'three-member-mixed',partyCount:3,guards:[[65535,1,32768,32767],[0,65535,1,0],[1,1,1,1]],keys:[64,16,16,2,2,2,1,1,1,4,8,16,32,64]});
 for(const test of cases){  rt.restoreBytes(snapshot);rt.drainEventsBytes();const rawBefore=mem().slice();const m=mem(),candidates=[];
  for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push({base:i,before:stateAt(i,m)});
  const beforePixels=rt.framePixelIndices(),initial=[];
  for(let t=0;t<16;++t){tick(t===0?64:0);for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p)initial.push({tick:t,pixels:p,states:candidates.map(c=>stateAt(c.base))});}}
  const live=candidates.filter(c=>stateAt(c.base).mode===6);if(live.length!==1)throw Error('property RAM '+live.length);
  const base=live[0].base,index=candidates.indexOf(live[0]),before=live[0].before,stages=[];
  if(test.partyCount){
   const a=o=>base+o-(o<0x7000?8:0),putB=(o,v)=>{mem()[a(o)-1]=v;};
   const hero=mem().slice(a(0x69c6)-1,a(0x69c6+148)-1),hp=hero[0x50]*256+hero[0x51];
   for(let i=1;i<test.partyCount;++i){mem().set(hero,a(0x69c6+i*148)-1);const value=hp-i;
    putB(0x6a16+i*148,value>>>8);mem()[a(0x6a16+i*148)]=value&255;}
   putB(0x7afa,test.partyCount);putB(0x7ab0,test.partyCount);
  }
  if(test.guards){const a=o=>base+o-(o<0x7000?8:0);for(let member=0;member<test.guards.length;++member)for(let s=0;s<4;++s){const o=a(0x6a04+148*member+s*2);mem()[o-1]=test.guards[member][s]>>>8;mem()[o]=test.guards[member][s]&255;}}
  fs.writeFileSync(`tmp/native-member-status-${test.name}-before.idx`,beforePixels);
  const save=(mask,commits)=>{const phase=stages.length;stages.push({mask,commits:commits.map((c,i)=>{fs.writeFileSync(`tmp/native-member-status-${test.name}-${phase}-commit-${i}.idx`,c.pixels);return{tick:c.tick,state:c.state};}),after:stateAt(base)});};
  save(64,initial.map(c=>({tick:c.tick,pixels:c.pixels,state:c.states[index]})));
  for(const mask of test.keys.slice(1)){const commits=[];for(let t=0;t<16;++t){tick(t===0?mask:0);for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p)commits.push({tick:t,pixels:p,state:stateAt(base)});}}save(mask,commits);}
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
  rows.push(row);fs.writeFileSync('tmp/native-member-status-menu.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name:test.name,stages:stages.map(s=>({mask:s.mask,commits:s.commits.length,mode:s.after.mode,event:s.after.event,context:s.after.context,menu:s.after.menu,member:s.after.member,ui:s.after.ui}))}));
 }
 rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
