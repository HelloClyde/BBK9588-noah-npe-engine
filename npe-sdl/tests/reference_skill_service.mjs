import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const cases=[
 {name:'first',character:1},{name:'second',character:2},{name:'third',character:3},
 {name:'missing',character:255},{name:'zero-character',character:0},
 {name:'duplicate-id',character:1,ids:[1,1,3]},
 {name:'zero-skill',skill:0},{name:'full-list',list:Array.from({length:18},(_,i)=>i+1)},
 {name:'gapped-list',list:[1,0,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18]},
 {name:'last-slot',event:2},{name:'low-byte1',event:257},{name:'low-byte2',event:258},
 {name:'invalid0',event:0},{name:'invalid3',event:3},{name:'invalid256',event:256},
 {name:'next-zero',mode:0},{name:'next25',mode:25},{name:'next14-invalid',mode:14,next:0},
 {name:'real-map1',map:1,real:true},
 {name:'real-map64-slot1',map:64,real:true,event:1},
 {name:'real-map64-slot2',map:64,real:true,event:2},
];
const selected=process.argv[2];
if(selected&&!cases.some(c=>c.name===selected))throw Error('unknown skill case');
if(!selected){
 // Isolate the reference runtime per short flow: restore can reallocate the
 // lower RAM bank away from its former adjacent host allocation.
 fs.writeFileSync('tmp/native-skill-service.json','[]\n');
 for(const c of cases){const child=spawnSync(process.execPath,[process.argv[1],c.name],{stdio:'inherit'});
  if(child.error)throw child.error;if(child.status!==0)throw Error('skill child '+c.name+' status '+child.status);}
 process.exit(0);
}
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot'),mem=()=>new Uint8Array(exp.memory.buffer);
const tick=mask=>{rt.advance(mask).free();},key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
const rows=selected?JSON.parse(fs.readFileSync('tmp/native-skill-service.json')).filter(r=>r.name!==selected):[];
try{
 for(const c of cases){
  if(selected&&c.name!==selected)continue;
  rt.restoreBytes(snapshot);rt.drainEventsBytes();const m=mem(),candidates=[];
  for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push(i);
  key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('skill base '+live.length);
  const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)],putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};
  const cache=()=>Array.from({length:2},(_,i)=>{const o=0x790a+i*6;return{nextId:w(o),skill:b(o+2),character:b(o+3),mode:b(o+4),padding:b(o+5)};});
  const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),init:w(0x7ab4),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),enabled:w(0x7a50),slot:b(0x7a04),cache:cache(),actors:Array.from({length:3},(_,i)=>({id:b(0x69d2+i*148),active:b(0x69f1+i*148),name:Array.from({length:12},(_,j)=>b(0x69c6+i*148+j)),skills:Array.from({length:18},(_,j)=>b(0x69df+i*148+j))})),loadedSkill:b(0x7a74),skillName:Array.from({length:12},(_,i)=>b(0x730e+i)),npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})});
  key(64);
  if([0,1,2].some(i=>b(0x69d2+i*148)!==i+1))throw Error('skill character RAM mapping '+c.name+' base '+base);
  if(c.map&&c.map!==1){putW(0x7a58,c.map);putB(0x69d5,1);putB(0x69d6,1);putB(0x7a2e,0);putB(0x7a2c,0);
   for(const mask of [64,2,2,2,16,2,16,16,64,2,2,2,16,16,16])key(mask);
   if(w(0x7934)!==1||w(0x7a58)!==c.map)throw Error('skill source map');}
  if([0,1,2].some(i=>b(0x69d2+i*148)!==i+1))throw Error('skill source character RAM mapping '+c.name);
  const event=c.event??1;
  if(!c.real)for(let i=0;i<2;++i){const o=0x790a+i*6;putW(o,c.next??789);putB(o+2,c.skill??2);putB(o+3,c.character??1);putB(o+4,c.mode??1);}
  if(c.ids)for(let i=0;i<3;++i)putB(0x69d2+i*148,c.ids[i]);
  if(c.list)for(let i=0;i<18;++i)putB(0x69df+i,c.list[i]);
  putB(0x7a32,0);const pixels=rt.framePixelIndices();putW(0x7934,9);putW(0x7936,event);
  const before=state(),commits=[];fs.writeFileSync(`tmp/native-skill-service-${c.name}-before.idx`,pixels);let finish=-1;
  for(let t=0;t<250;++t){tick(t%13===3?16:t%13===7?64:0);
   for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){const index=commits.length;fs.writeFileSync(`tmp/native-skill-service-${c.name}-${index}.idx`,p);commits.push({tick:t,state:state()});}}
   if(w(0x7934)===1&&finish<0)finish=t;
   if(finish>=0&&w(0x7ab4)===0&&commits.length)break;
  }
  if(finish<0)throw Error('skill no finish '+c.name);
  rows.push({...c,event,before,commits,after:state(),finish});rows.sort((a,b)=>cases.findIndex(c=>c.name===a.name)-cases.findIndex(c=>c.name===b.name));fs.writeFileSync('tmp/native-skill-service.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name:c.name,commits:commits.length,after:{mode:w(0x7934),event:w(0x7936)},actors:state().actors,loadedSkill:b(0x7a74),skillName:state().skillName}));
 }
}finally{rt.free();}
