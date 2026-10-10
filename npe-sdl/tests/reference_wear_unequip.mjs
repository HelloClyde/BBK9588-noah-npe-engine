// Diagnostic inputs to the original interpreter. No C-engine-derived oracle.
import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const snapshot=fs.readFileSync('tmp/native-title-load-saved.snapshot');
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>rt.advance(mask).free();
const U=1024,upperU=2048;
const cases=[
 {name:'six-slots',gear:[[9,73,17,27,1,76]],keys:[64,16,2,16,U,U,8,upperU,8,U,8,upperU,8,U,8,upperU,U]},
 {name:'duplicate-ids',gear:[[85,85,85,85,85,85]],keys:[64,16,2,16,U,U,8,U,8,8,8,8,U]},
 {name:'cached-selected-id',gear:[[85,0,0,0,0,0]],keys:[64,16,2,16,{mask:U,selected:91},U,8,4,U]},
 {name:'wrong-slot-type',gear:[[26,0,0,0,0,0]],keys:[64,16,2,16,U,U,256,512,8,4]},
 {name:'empty-slot',gear:[[0,0,0,0,0,0]],keys:[64,16,2,16,U,upperU,256,512,8,U]},
 {name:'two-members',partyCount:2,gear:[[9,0,17,0,0,0],[0,73,0,27,1,76]],keys:[64,16,2,16,2,8,8,U,8,U,8,U,8,U,1,8,U]},
 {name:'two-native-members',partyCount:2,nativeParty:true,gear:[[9,0,17,0,0,0],[0,73,0,27,1,76]],keys:[64,16,2,16,2,8,8,U,8,U,8,U,8,U,1,8,U]},
 {name:'word-wrap',gear:[[85,0,0,0,0,0]],stocks:[[85,65535]],signedFields:true,keys:[64,16,2,16,U,256,512,U,16,2,16]},
 {name:'description-retained',gear:[[91,0,0,0,0,85]],keys:[64,16,2,16,256,U,256,512,U,8,4,4,U,256,512]},
];
const requested=process.argv[2];
if(requested&&!cases.some(c=>c.name===requested))throw Error('unknown case '+requested);
const rows=requested?JSON.parse(fs.readFileSync('tmp/native-wear-unequip.json')).filter(c=>c.name!==requested):[];
try{
 let base;
 const a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v&255;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),money:(w(0x799e)*65536+w(0x79a0))>>>0,enabled:w(0x7a50),slot:b(0x7a04),menu:b(0x7a54),member:b(0x7adc),equipmentSlot:b(0x7a2a),gearSelected:b(0x7ab2),descLength:w(0x7aba),descStart:w(0x7abe),descEnd:w(0x7abc),itemLoaded:b(0x7a72),skillLoaded:b(0x7a74),
  itemBytes:Array.from({length:142},(_,i)=>b(0x739e+i)),skillBytes:Array.from({length:144},(_,i)=>b(0x730e+i)),roles:Array.from({length:444},(_,i)=>b(0x69c6+i)),inventory:Array.from({length:255},(_,i)=>w(0x67c8+2*i)),
  npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+14*i;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})});
 for(const c of cases.filter(c=>!requested||c.name===requested)){
  rt.restoreBytes(snapshot);rt.drainEventsBytes();
  const candidates=[];for(let i=0;i+0x8000<mem().length;++i)if(mem()[i+0x7934]===1&&mem()[i+0x7935]===0&&mem()[i+0x7a58]===1&&mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)candidates.push(i);
  for(let t=0;t<16;++t)tick(t===0?64:0);
  const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('RAM discovery '+live.length);base=live[0];
  for(let t=0;t<16;++t)tick(t===0?64:0);
  if(w(0x7934)!==1)throw Error('map return');
  const count=c.partyCount??1,hero=Array.from({length:148},(_,i)=>b(0x69c6+i));
  if(!c.nativeParty)for(let member=1;member<count;++member){for(let i=0;i<148;++i)putB(0x69c6+148*member+i,hero[i]);putW(0x6a16+148*member,w(0x6a16)-member);}
  for(let i=0;i<3;++i)putB(0x69f1+148*i,i<count?1:0);putB(0x7afa,count);putB(0x7ab0,count);
  for(let member=0;member<c.gear.length;++member)for(let slot=0;slot<6;++slot)putB(0x69d8+148*member+slot,c.gear[member][slot]);
  for(const [id,n] of c.stocks??[])putW(0x67c8+2*id,n);
  if(c.signedFields){for(const [o,v] of [[0x6a16,65535],[0x6a14,32767],[0x6a1a,32768],[0x6a18,65535],[0x6a22,1],[0x6a24,0],[0x6a1c,65535],[0x6a1e,32768],[0x6a20,32767],[0x6a04,65535],[0x6a06,1],[0x6a08,65534],[0x6a0a,2]])putW(o,v);}
  const before=state(),phases=[];fs.writeFileSync(`tmp/native-wear-unequip-${c.name}-before.idx`,rt.framePixelIndices());
  for(const input of c.keys){const p=typeof input==='number'?{mask:input}:input;
   if(p.selected!==undefined)putB(0x7ab2,p.selected);
   const prior=state(),commits=[],phase=phases.length;
   for(let t=0;t<16;++t){if(p.mask>=128){const key=p.mask===U?0x75:p.mask===upperU?0x55:p.mask===256?0x85:0x84;putW(0x7a76,t===3?key:0);}tick(t===0&&p.mask<128?p.mask:0);
    for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(pixels){fs.writeFileSync(`tmp/native-wear-unequip-${c.name}-${phase}-commit-${commits.length}.idx`,pixels);commits.push({tick:t,state:state()});}}
   }
   phases.push({...p,before:prior,commits,after:state()});
  }
  rows.push({...c,before,phases});
  fs.writeFileSync('tmp/native-wear-unequip.json',JSON.stringify(rows,null,2)+'\n');
  console.log(JSON.stringify({name:c.name,phases:phases.map(p=>({mask:p.mask,commits:p.commits.length,menu:p.after.menu,selected:p.after.gearSelected,loaded:p.after.itemLoaded,desc:[p.after.descLength,p.after.descStart,p.after.descEnd],gear:p.after.roles.slice(18,24)}))}));
 }
}finally{rt.free();}
