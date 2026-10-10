import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';

async function main(){
 const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
 const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
 const on=fs.readFileSync('tmp/native-title-load-saved.snapshot');
 const mem=()=>new Uint8Array(exp.memory.buffer);
 const stateAt=(base,m=mem())=>{
  const a=o=>base+o-(o<0x7000?8:0),b=o=>m[a(o)-1],w=o=>b(o)*256+m[a(o)];
  return{mode:w(0x7934),event:w(0x7936),map:w(0x7a58),progress:w(0x7a66),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),
   money:(w(0x799e)*65536+w(0x79a0))>>>0,enabled:w(0x7a50),slot:b(0x7a04),periodCounter:b(0x7a32),menu:b(0x7a54),parentChoice:b(0x7ae2),systemChoice:b(0x7a1e),musicChoice:b(0x7a8a),
   npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})};
 };
 const tick=mask=>{const out=rt.advance(mask);out.free();};
 const candidates=()=>{const m=mem(),found=[];
  for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x69d5-9]===7&&m[i+0x69d6-9]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)found.push({base:i,before:stateAt(i,m)});
  if(!found.length)throw Error('no original map RAM');return found;
 };
 const activate=()=>{const found=candidates(),captured=[];
  for(let t=0;t<16;++t){tick(t===0?64:0);for(let i=0;i<rt.displayCommitCount();++i){const p=rt.displayCommitPixelIndices(i);if(p)captured.push({tick:t,pixels:p,states:found.map(f=>stateAt(f.base))});}}
  const live=found.filter(f=>stateAt(f.base).mode===6);if(live.length!==1)throw Error('live menu RAM '+live.length);
  const index=found.indexOf(live[0]);return{base:live[0].base,before:live[0].before,commits:captured.map(c=>({tick:c.tick,pixels:c.pixels,state:c.states[index]}))};
 };
 // Derive the off baseline through original menu input, without changing RAM.
 rt.restoreBytes(on);let initial=activate();
 for(const key of [2,2,2,16,2,2,16,16])for(let t=0;t<16;++t)tick(t===0?key:0);
 const offState=stateAt(initial.base);if(offState.mode!==1||offState.enabled!==0)throw Error('off baseline '+JSON.stringify(offState));
 const off=rt.snapshotBytes();fs.writeFileSync('tmp/native-system-menu-off.snapshot',off);
 const parent=[64,2,2,2,16],music=[...parent,2,2,16];
 const cases=[
  {name:'main-ignore',enabled:1,keys:[64,4,8,32,64]},
  {name:'submenu-ignore',enabled:1,keys:[...parent,4,8,32,64]},
  {name:'slots-ignore',enabled:1,keys:[...parent,16,4,8,32,64]},
  {name:'music-on-on',enabled:1,keys:[...music,1,4,8,32,16]},
  {name:'music-on-off',enabled:1,keys:[...music,16]},
  {name:'music-off-on',enabled:0,keys:[...music,16]},
  {name:'music-off-off',enabled:0,keys:[...music,2,16]},
  {name:'music-on-cancel',enabled:1,keys:[...music,1,64]},
  {name:'music-off-cancel',enabled:0,keys:[...music,2,64]},
 ];
 const rows=[];
 const u32=(m,j)=>(m[j]*16777216+m[j+1]*65536+m[j+2]*256+m[j+3])>>>0;
 for(const test of cases){
  rt.restoreBytes(test.enabled?on:off);rt.drainEventsBytes();
  const rawBefore=mem().slice();
  const beforePixels=rt.framePixelIndices();fs.writeFileSync(`tmp/native-system-menu-${test.name}-before.idx`,beforePixels);
  const active=activate(),base=active.base,before=active.before;
  if(before.enabled!==test.enabled)throw Error('initial setting '+test.name);
  const stages=[];
  const save=(phase,mask,commits)=>{const converted=commits.map((c,index)=>{fs.writeFileSync(`tmp/native-system-menu-${test.name}-${phase}-commit-${index}.idx`,c.pixels);return{index,tick:c.tick,state:c.state};});
   stages.push({phase,mask,commits:converted,after:stateAt(base)});};
  save(0,64,active.commits);
  for(let phase=1;phase<test.keys.length;++phase){const commits=[];
   for(let t=0;t<16;++t){tick(t===0?test.keys[phase]:0);for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p)commits.push({tick:t,pixels:p,state:stateAt(base)});}}
   save(phase,test.keys[phase],commits);
  }
  const row={...test,before,stages,after:stateAt(base)};rows.push(row);
  // Snapshot restore relocates the resident heap independently of RPG RAM.
  // Identify the RNG through a following natural NPC period for this case,
  // then read its observed address in the frozen before/after route images.
  const rawAfter=mem().slice(),counter=row.after.periodCounter;let elapsed;
  for(let t=0;t<300;++t){tick(0);if(stateAt(base).periodCounter!==counter){elapsed=t;break;}}
  if(elapsed===undefined)throw Error('following natural period '+test.name);
  const postPeriod=mem(),changes=new Set(),rng=[];
   for(let j=0;j<Math.min(rawAfter.length,postPeriod.length);++j)if(rawAfter[j]!==postPeriod[j])for(let k=Math.max(0,j-3);k<=j&&k+4<rawAfter.length;++k)changes.add(k);
   for(const j of changes){const old=u32(rawAfter,j),next=u32(postPeriod,j);if(old<=1||next<=1||old===next)continue;
    let v=old;for(let calls=1;calls<=32;++calls){v=(Math.imul(v,0x514f111f)+1)>>>0;if(v===next){rng.push({offset:j,before:old,after:next,calls});break;}}
   }
   if(rng.length!==1)throw Error('resident RNG '+JSON.stringify(rng));
   row.followingPeriodRng={...rng[0],elapsed};row.rngBefore=u32(rawBefore,rng[0].offset);row.rngAfter=u32(rawAfter,rng[0].offset);
   const changed=before.npc.some((n,i)=>JSON.stringify(n)!==JSON.stringify(row.after.npc[i]));
   if(!changed&&row.rngBefore!==row.rngAfter)throw Error('unexplained menu RNG '+test.name);
   if(changed){let v=row.rngBefore;for(let calls=1;calls<=32;++calls){v=(Math.imul(v,0x514f111f)+1)>>>0;if(v===row.rngAfter){row.routeRngCalls=calls;break;}}
    if(!row.routeRngCalls)throw Error('route RNG transition '+test.name);}
  if(row.after.mode!==1)throw Error('return mode '+JSON.stringify(row.after));
  fs.writeFileSync('tmp/native-system-menu-return.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name:test.name,beforeEnabled:before.enabled,afterEnabled:row.after.enabled,phases:stages.map(s=>({mask:s.mask,commits:s.commits.length,mode:s.after.mode,menu:s.after.menu,choice:s.after.systemChoice}))}));
 }
 fs.writeFileSync('tmp/native-system-menu-return.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({rows:rows.map(r=>({name:r.name,before:r.rngBefore,after:r.rngAfter,calls:r.routeRngCalls??0}))}));
 rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
