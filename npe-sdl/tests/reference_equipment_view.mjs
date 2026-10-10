import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
async function main(){
 const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
 const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
 const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot'),mem=()=>new Uint8Array(exp.memory.buffer);
 const stateAt=(base,m=mem())=>{const a=o=>base+o-(o<0x7000?8:0),b=o=>m[a(o)-1],w=o=>b(o)*256+m[a(o)];return{
  mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),money:(w(0x799e)*65536+w(0x79a0))>>>0,enabled:w(0x7a50),slot:b(0x7a04),periodCounter:b(0x7a32),menu:b(0x7a54),propertyChoice:b(0x7a18),member:b(0x7adc),equipmentSlot:b(0x7a2a),ui:b(0x7a4a),gearSelected:b(0x7ab2),descLength:w(0x7aba),descStart:w(0x7abe),descEnd:w(0x7abc),
  npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})};};
 const tick=mask=>{const out=rt.advance(mask);out.free();};
 const rows=[];
 for(const test of [
  {name:'six-gear-cycle',gear:[[24,26,85,91,45,1]],keys:[64,16,2,16,...Array(6).fill(8),...Array(6).fill(4),16,32,64]},
  {name:'mixed-two-members',partyCount:2,gear:[[85,0,26,0,91,0],[0,24,0,45,0,1]],keys:[64,16,2,16,8,8,2,8,8,4,1,4,4,16,32,64]},
  {name:'description-keys',gear:[[91,0,0,0,0,0]],keys:[64,16,2,16,256,256,512,512,128,64]},
  {name:'empty-description-keys',gear:[[91,0,0,0,0,0]],keys:[64,16,2,16,8,256,256,512,512,4,256,64]},
  {name:'odd-byte-description',gear:[[26,0,0,0,0,0]],keys:[64,16,2,16,256,512,256,512,64]},
  {name:'empty-after-full',partyCount:2,gear:[[1,2,3,4,5,6],[0,0,0,0,0,0]],keys:[64,16,2,16,2,8,1,4,2,1,64]},
 ]){
  rt.restoreBytes(snapshot);rt.drainEventsBytes();const rawBefore=mem().slice();const m=mem(),candidates=[];
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
  if(test.gear){const a=o=>base+o-(o<0x7000?8:0);for(let member=0;member<test.gear.length;++member)for(let slot=0;slot<6;++slot)mem()[a(0x69d8+148*member+slot)-1]=test.gear[member][slot];}
  fs.writeFileSync(`tmp/native-equipment-view-${test.name}-before.idx`,beforePixels);
  const save=(mask,commits)=>{const phase=stages.length;stages.push({mask,commits:commits.map((c,i)=>{fs.writeFileSync(`tmp/native-equipment-view-${test.name}-${phase}-commit-${i}.idx`,c.pixels);return{tick:c.tick,state:c.state};}),after:stateAt(base)});};
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
  rows.push(row);fs.writeFileSync('tmp/native-equipment-view-menu.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name:test.name,stages:stages.map(s=>({mask:s.mask,commits:s.commits.length,mode:s.after.mode,event:s.after.event,context:s.after.context,menu:s.after.menu,member:s.after.member,ui:s.after.ui}))}));
 }
 rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
