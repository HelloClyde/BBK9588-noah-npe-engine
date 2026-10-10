import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';

const cases=[
 {name:'space',keys:[16]}, {name:'enter',keys:[256]},
 {name:'menu',keys:[64,64]}, {name:'left',keys:[4,16]},
 {name:'down',keys:[2,16]},
 {name:'byte255-space',direction:255,keys:[16]},
 {name:'byte255-enter',direction:255,keys:[256]},
 {name:'object-viewport',map:80,direction:3,keys:[256],scope:'numeric-object'},
];
const selected=process.argv[2];
if(!selected){fs.writeFileSync('tmp/native-leader-actions.json','[]\n');
 for(const c of cases){const child=spawnSync(process.execPath,[process.argv[1],c.name],{stdio:'inherit'});
  if(child.error)throw child.error;if(child.status!==0)throw Error('leader action child '+c.name+' '+child.status);}
 process.exit(0);}
const c=cases.find(c=>c.name===selected);if(!c)throw Error('unknown leader action');
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>rt.advance(mask).free();
const key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
try{
 rt.restoreBytes(fs.readFileSync('tmp/native-title-load-saved.snapshot'));rt.drainEventsBytes();
 const candidates=[];for(let i=0;i+0x8000<mem().length;++i)
  if(mem()[i+0x7934]===1&&mem()[i+0x7935]===0&&mem()[i+0x7a58]===1&&mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)candidates.push(i);
 key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('leader action RAM '+live.length);
 const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};key(64);
 if(c.map){putW(0x7a58,c.map);putB(0x69d5,1);putB(0x69d6,1);putB(0x7a2e,0);putB(0x7a2c,0);
  for(const mask of [64,2,2,2,16,2,16,16,64,2,2,2,16,16,16])key(mask);
  if(w(0x7934)!==1||w(0x7a58)!==c.map)throw Error('leader action map UI');}
 if([0,1,2].some(i=>b(0x69d2+i*148)!==i+1))throw Error('leader action actor RAM');
 putW(0x7a50,0);putB(0x7a32,0);putW(0x7a5c,0);
 // Real directional inputs reset the map deadline before the diagnostic swap.
 // Do not mix an overdue restored-snapshot NPC timer into a synchronous action.
 key(4);key(8);if(w(0x7934)!==1)throw Error('leader timer reset route');
 putB(0x69f1,1);putB(0x69f1+148,1);putB(0x69f1+296,0);
 putB(0x79a2,2);putB(0x79a3,1);putW(0x79a4,789);putW(0x7934,13);putW(0x7936,1);
 for(let t=0;t<16;++t)tick(0);
 if(w(0x7934)!==1||w(0x7ab4)||b(0x69d2)!==2||b(0x69d4)!==4)throw Error('leader action swap');
 if(c.direction!==undefined)putB(0x69d4,c.direction);
 if(c.scope==='numeric-object'){putB(0x69d5,7);putB(0x69d6,4);putB(0x7a2e,20);putB(0x7a2c,16);}
 putB(0x7a32,0);
 const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),init:w(0x7ab4),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),menuCount:b(0x7ab0),steps:w(0x7a02),
  actors:Array.from({length:3},(_,i)=>({id:b(0x69d2+i*148),active:b(0x69f1+i*148),direction:b(0x69d4+i*148),raw:Array.from({length:148},(_,j)=>b(0x69c6+i*148+j))})),
  npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};}),
  inventory:Array.from({length:256},(_,i)=>w(0x67c8+i*2)),objects:Array.from({length:16},(_,i)=>b(0x742c+i))});
 const before=state(),commits=[],phases=[];fs.writeFileSync(`tmp/native-leader-actions-${c.name}-before.idx`,rt.framePixelIndices());
 for(const input of c.keys){const start=commits.length;
  for(let t=0;t<32;++t){
   // Web Confirm emits Space. A resident0x86 write alone does not prove that
   // a new keyboard message was delivered. These attempts are retained as
   // source-backed C Enter fixtures, not original Enter input evidence.
   if(input===256&&t>0)putW(0x7a76,t===3?0x86:0);
   tick(t===0&&input!==256?input:0);
   for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(!p)continue;
    fs.writeFileSync(`tmp/native-leader-actions-${c.name}-${commits.length}.idx`,p);commits.push({key:input,tick:t,state:state()});}
  }
  phases.push({key:input,commits:commits.length-start,after:state()});
 }
 const rows=JSON.parse(fs.readFileSync('tmp/native-leader-actions.json')).filter(r=>r.name!==c.name);
 rows.push({...c,inputDelivery:c.keys.some(k=>k===256)?'not-proven-resident-key-write':'original-public-mask',before,phases,commits,after:state()});rows.sort((a,b)=>cases.findIndex(c=>c.name===a.name)-cases.findIndex(c=>c.name===b.name));
 fs.writeFileSync('tmp/native-leader-actions.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name:c.name,commits:commits.length,phases:phases.map(p=>({key:p.key,commits:p.commits,mode:p.after.mode,event:p.after.event,direction:p.after.direction,sx:p.after.sx,sy:p.after.sy,menuCount:p.after.menuCount}))}));
}finally{rt.free();}
