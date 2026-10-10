import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';

async function main(){
 const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
 const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
 const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot');
 const mem=()=>new Uint8Array(exp.memory.buffer);
 const u32=(m,j)=>(m[j]*16777216+m[j+1]*65536+m[j+2]*256+m[j+3])>>>0;
 const tick=mask=>{const out=rt.advance(mask);out.free();};
 const stateAt=(base,m=mem())=>{
  const a=o=>base+o-(o<0x7000?8:0),b=o=>m[a(o)-1],w=o=>b(o)*256+m[a(o)];
  return{mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),money:(w(0x799e)*65536+w(0x79a0))>>>0,enabled:w(0x7a50),slot:b(0x7a04),periodCounter:b(0x7a32),ui:b(0x7a4a),selected:w(0x7a4c),first:w(0x7ac6),last:w(0x7ab6),row:b(0x7a46),quantity:b(0x7a48),choice:b(0x7a0c),descStart:w(0x7abe),descEnd:w(0x7abc),
   stocks:Array.from({length:256},(_,i)=>w(0x67c8+i*2)),
   npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})};
 };
 const rows=[];
 for(const test of [{name:'buy-list',sell:false,quantity:false},{name:'buy-quantity-cancel',sell:false,quantity:true},{name:'sell-list',sell:true,quantity:false},{name:'sell-quantity-cancel',sell:true,quantity:true,diagnosticStock:[[45,2]]}]){
  rt.restoreBytes(snapshot);rt.drainEventsBytes();
  const original=mem(),candidates=[];
  for(let i=0;i+0x8000<original.length;++i)if(original[i+0x7934]===1&&original[i+0x7935]===0&&original[i+0x7a58]===1&&original[i+0x7a66]===1&&original[i+0x7aec]===0xa9&&original[i+0x7aeb]===6)candidates.push({base:i,counter:stateAt(i).periodCounter});
  const old=mem().slice();let live,elapsed;
  for(let t=0;t<300;++t){tick(0);const changed=candidates.filter(c=>stateAt(c.base).periodCounter!==c.counter);if(changed.length){if(changed.length!==1)throw Error('ambiguous RPG RAM '+changed.length);live=changed[0];elapsed=t;break;}}
  if(!live)throw Error('RPG period timeout');
  const post=mem(),changes=new Set(),rng=[];
  for(let j=0;j<Math.min(old.length,post.length);++j)if(old[j]!==post[j])for(let k=Math.max(0,j-3);k<=j&&k+4<old.length;++k)changes.add(k);
  for(const j of changes){const before=u32(old,j),after=u32(post,j);if(before<=1||after<=1||before===after)continue;
   let v=before;for(let calls=1;calls<=32;++calls){v=(Math.imul(v,0x514f111f)+1)>>>0;if(v===after){rng.push({offset:j,before,after,calls});break;}}}
  if(rng.length!==1)throw Error('resident RNG '+JSON.stringify(rng));
  const base=live.base,rngAddress=rng[0].offset,a=o=>base+o-(o<0x7000?8:0);
  const putW=(o,v)=>{mem()[a(o)-1]=v>>>8;mem()[a(o)]=v&255;};
  const state=()=>({...stateAt(base),rng:u32(mem(),rngAddress)});
  for(const [id,quantity] of test.diagnosticStock??[])putW(0x67c8+id*2,quantity);
  const prefix=`tmp/native-shop-return-${test.name}`;
  const before=state();fs.writeFileSync(`${prefix}-before.idx`,rt.framePixelIndices());
  // Controlled service entry: only mode/event/context/redraw/input are set.
  // The final sell case also seeds one item for a nonempty quantity popup.
  // No original NPE, NPC, graphics color or RNG data is changed.
  putW(0x7934,4);putW(0x7936,1);putW(0x7938,test.sell?10:13);putW(0x7ab4,1);putW(0x7a76,0);
  const stages=[];
  const collect=(phase,t,commits)=>{for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(pixels){const n=commits.length;fs.writeFileSync(`${prefix}-${phase}-commit-${n}.idx`,pixels);commits.push({tick:t,state:state()});}}};
  const phase=(key,ticks)=>{const n=stages.length,commits=[];
   for(let t=0;t<ticks;++t){if(key===128)putW(0x7a76,t===3?0x86:0);tick(t===0&&key!==128?key:0);collect(n,t,commits);}
   stages.push({mask:key,commits,after:state()});
  };
  phase(0,16);
  if(state().mode!==4||state().ui!==1)throw Error('shop entry '+JSON.stringify(state()));
  if(test.quantity){phase(128,16);if(state().ui!==2)throw Error('quantity entry');phase(64,16);}
  phase(64,16);
  if(state().mode!==1)throw Error('shop return');
  const counter=state().periodCounter,n=stages.length,commits=[];let periodTicks;
  for(let t=0;t<300;++t){tick(0);collect(n,t,commits);if(state().periodCounter!==counter){periodTicks=t;break;}}
  if(periodTicks===undefined)throw Error('return period timeout');
  stages.push({mask:0,period:true,commits,after:state()});
  const row={...test,before,discovery:{...rng[0],elapsed},stages,after:state()};rows.push(row);
  fs.writeFileSync('tmp/native-shop-return-periodic.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name:test.name,phases:stages.map(s=>({mask:s.mask,period:s.period??false,commits:s.commits.length,mode:s.after.mode,ui:s.after.ui,rng:s.after.rng})),npc:before.npc.filter(n=>n.id).length}));
 }
 rt.free();
}
main().catch(e=>{console.error(e.name,e.code,e.message);process.exitCode=1;});
