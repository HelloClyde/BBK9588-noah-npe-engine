import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot'),mem=()=>new Uint8Array(exp.memory.buffer);
const tick=mask=>rt.advance(mask).free(),key=mask=>{for(let t=0;t<16;++t)tick(t===0?mask:0);};
const cases=[
 {name:'local'}, {name:'last-slot',event:5},{name:'low-byte',event:257},
 {name:'erase',erase:1},{name:'draw-one',draw:1},{name:'draw-two',draw:2},
 {name:'negative-x',x:65535},{name:'word-next',next:0x1234},
 {name:'invalid-zero',event:0},{name:'invalid-six',event:6},{name:'invalid-256',event:256},
 {name:'zero-group',group:0},
];
const rows=[];
try{for(const c of cases){
 rt.restoreBytes(snapshot);rt.drainEventsBytes();const m=mem(),candidates=[];
 for(let i=0;i+0x8000<m.length;++i)if(m[i+0x7934]===1&&m[i+0x7935]===0&&m[i+0x7a58]===1&&m[i+0x7a66]===1&&m[i+0x7aec]===0xa9&&m[i+0x7aeb]===6)candidates.push(i);
 key(64);const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('scene base '+live.length);
 const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)],putB=(o,v)=>{mem()[a(o)-1]=v;},putW=(o,v)=>{putB(o,v>>8);mem()[a(o)]=v&255;};
 if(b(0x69d2)!==1||b(0x6a66)!==2)throw Error('scene actor RAM mapping');
 key(64);const pixels=rt.framePixelIndices();
 for(let i=0;i<5;++i){const o=0x754e+i*16;putW(o,c.next??789);putB(o+2,c.group??2);putB(o+3,0xcd);
  putB(o+4,c.draw??0);putB(o+5,c.erase??0);putB(o+6,1);putB(o+7,0x81+i);
  putW(o+8,c.x??60);putW(o+10,0);putW(o+12,0x8123);putW(o+14,0xcdef);
 }
 const table=()=>Array.from({length:80},(_,i)=>b(0x754e+i));
 const state=()=>({mode:w(0x7934),event:w(0x7936),map:w(0x7a58),progress:w(0x7a66),init:w(0x7ab4),
  sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),
  npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})});
 putB(0x7a32,0);putW(0x7934,15);putW(0x7936,c.event??1);
 const before=state(),beforeBytes=table(),commits=[];fs.writeFileSync(`tmp/native-scene-resident-public-${c.name}-before.idx`,pixels);
 let finish=-1;
 for(let t=0;t<1800;++t){tick(0);
  for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){const index=commits.length;fs.writeFileSync(`tmp/native-scene-resident-public-${c.name}-${index}.idx`,p);commits.push({tick:t,state:state()});}}
  if(w(0x7934)===1&&w(0x7ab4)===0&&finish<0)finish=t;
  if(finish>=0&&t>=finish+3)break;
 }
 if(finish<0)throw Error('scene no finish '+c.name+' '+JSON.stringify(state()));
 const serviceCommits=commits.length,after=state(),afterBytes=table(),menu=[];
 for(const mask of [64,64]){const begin=commits.length;
  for(let t=0;t<16;++t){tick(t===0?mask:0);
   for(let j=0;j<rt.displayCommitCount();++j){const p=rt.displayCommitPixelIndices(j);if(p){const index=commits.length;fs.writeFileSync(`tmp/native-scene-resident-public-${c.name}-${index}.idx`,p);commits.push({tick:t,phase:'menu',state:state()});}}
  }
  menu.push({commits:commits.length-begin,after:state()});
 }
 if(menu[0].after.mode!==6||menu[1].after.mode!==1)throw Error('scene menu states');
 rows.push({...c,before,beforeBytes,commits,serviceCommits,after,afterBytes,menu,finish});
 fs.writeFileSync('tmp/native-scene-resident-public.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name:c.name,commits:commits.length,finish,after:state()}));
}}finally{rt.free();}
