import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';

const cases=[
 {name:'second',target:2,flags:[1,1,0]},
 {name:'third',target:3,flags:[1,0,1]},
 {name:'second-inactive',target:2,flags:[1,0,1]},
 {name:'second-flag2',target:2,flags:[1,2,0]},
 {name:'third-flag2',target:3,flags:[1,0,2]},
 {name:'first-only',target:1,flags:[1,1,1]},
 {name:'missing',target:255,flags:[1,1,1]},
 {name:'second-direction',target:2,flags:[1,1,1],directions:[1,3,2]},
 {name:'third-direction',target:3,flags:[1,1,1],directions:[3,0,2]},
 {name:'first-inactive',target:2,flags:[0,1,0]},
 {name:'duplicate',target:2,flags:[1,1,1],ids:[1,2,2]},
 {name:'low-byte1',target:2,flags:[1,1,0],event:257},
 {name:'invalid0',target:2,flags:[1,1,0],event:0},
 {name:'invalid2',target:2,flags:[1,1,0],event:2},
 {name:'invalid256',target:2,flags:[1,1,0],event:256},
 {name:'invalid258',target:2,flags:[1,1,0],event:258},
 {name:'next-zero',target:2,flags:[1,1,0],mode:0},
 {name:'next25',target:3,flags:[1,0,1],mode:25},
 {name:'next14-invalid',target:2,flags:[1,1,0],mode:14,next:0},
 {name:'map2-resident',target:2,flags:[1,1,0],map:2},
 {name:'map97-empty',map:97,originalCache:true},
];
const selected=process.argv[2];
if(selected&&!cases.some(c=>c.name===selected))throw Error('unknown party adjustment case');
if(!selected){
 fs.writeFileSync('tmp/native-party-adjust.json','[]\n');
 for(const c of cases){const child=spawnSync(process.execPath,[process.argv[1],c.name],{stdio:'inherit'});
  if(child.error)throw child.error;if(child.status!==0)throw Error('party adjustment child '+c.name+' status '+child.status);}
 process.exit(0);
}
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot'),mem=()=>new Uint8Array(exp.memory.buffer);
const tick=mask=>{rt.advance(mask).free();},key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
const rows=JSON.parse(fs.readFileSync('tmp/native-party-adjust.json')).filter(r=>r.name!==selected);
try{
 const c=cases.find(c=>c.name===selected);
 rt.restoreBytes(snapshot);rt.drainEventsBytes();const m=mem(),candidates=[];
 for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push(i);
 key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('party adjustment RAM '+live.length);
 const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)],putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};
 const cache=()=>({character:b(0x79a2),mode:b(0x79a3),next:w(0x79a4)});
 const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),init:w(0x7ab4),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),enabled:w(0x7a50),slot:b(0x7a04),menuCount:b(0x7ab0),period:b(0x7a32),cache:cache(),actors:Array.from({length:3},(_,i)=>({id:b(0x69d2+i*148),active:b(0x69f1+i*148),raw:Array.from({length:148},(_,j)=>b(0x69c6+i*148+j))})),npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})});
 key(64);
 if(c.map&&c.map!==1){putW(0x7a58,c.map);putB(0x69d5,1);putB(0x69d6,1);putB(0x7a2e,0);putB(0x7a2c,0);
  for(const mask of [64,2,2,2,16,2,16,16,64,2,2,2,16,16,16])key(mask);
  if(w(0x7934)!==1||w(0x7a58)!==c.map)throw Error('party adjustment map');}
 if([0,1,2].some(i=>b(0x69d2+i*148)!==i+1))throw Error('party adjustment actor RAM');
 if(!c.originalCache){putB(0x79a2,c.target);putB(0x79a3,c.mode??1);putW(0x79a4,c.next??789);}
 if(c.flags)for(let i=0;i<3;++i)putB(0x69f1+i*148,c.flags[i]);
 if(c.ids)for(let i=0;i<3;++i)putB(0x69d2+i*148,c.ids[i]);
 if(c.directions)for(let i=0;i<3;++i)putB(0x69d4+i*148,c.directions[i]);
 putB(0x7a32,0);const pixels=rt.framePixelIndices();putW(0x7934,13);putW(0x7936,c.event??1);
 const before=state(),commits=[];fs.writeFileSync(`tmp/native-party-adjust-${c.name}-before.idx`,pixels);let finish=-1;
 for(let t=0;t<64;++t){tick(0);
  for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){const index=commits.length;fs.writeFileSync(`tmp/native-party-adjust-${c.name}-${index}.idx`,p);commits.push({tick:t,state:state()});}}
  if(w(0x7934)===1&&w(0x7ab4)===0&&commits.length&&finish<0)finish=t;
  //7AB4 is cleared before the map renderer finishes. Keep advancing until
  // that pending drawing work and its LCD submission have completed.
  if(finish>=0&&t>=finish+8&&w(0x7934)===1&&w(0x7ab4)===0)break;
 }
 if(finish<0)throw Error('party adjustment no finish '+c.name);
 const after=state();rows.push({...c,before,after,commits,finish});rows.sort((a,b)=>cases.findIndex(c=>c.name===a.name)-cases.findIndex(c=>c.name===b.name));
 fs.writeFileSync('tmp/native-party-adjust.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name:c.name,cache:before.cache,commits:commits.length,finish,before:before.actors.map(a=>[a.id,a.active,a.raw[14]]),after:after.actors.map(a=>[a.id,a.active,a.raw[14]]),end:{mode:after.mode,event:after.event,direction:after.direction,init:after.init}}));
}finally{rt.free();}
