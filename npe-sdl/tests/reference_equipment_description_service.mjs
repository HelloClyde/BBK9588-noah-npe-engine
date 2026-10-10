import fs from 'node:fs';
import {GameRuntime,initWasm} from '../../tmp/opennoah-headless.mjs';
const exp=await initWasm({module_or_path:fs.readFileSync('tmp/opennoah_wasm_bg.wasm')});
const rt=new GameRuntime(fs.readFileSync('tmp/tianjiang-online.opennoah'),'certified_original_v1');
const mem=()=>new Uint8Array(exp.memory.buffer),tick=mask=>rt.advance(mask).free(),rows=[];
try{for(const name of ['pages','odd-forward','controls','right-word-wrap','right-past-string','left-past-string','left-signed-stop','empty-entry','empty-right','empty-left']){
 rt.restoreBytes(fs.readFileSync('tmp/native-title-load-saved.snapshot'));rt.drainEventsBytes();
 const candidates=[];for(let i=0;i+0x8000<mem().length;++i)
  if(mem()[i+0x7934]===1&&mem()[i+0x7935]===0&&mem()[i+0x7a58]===1&&mem()[i+0x7a66]===1&&mem()[i+0x7aec]===0xa9&&mem()[i+0x7aeb]===6)candidates.push(i);
 for(let t=0;t<128;++t)tick(t===0?64:0);
 const live=candidates.filter(i=>mem()[i+0x7934]===6);if(live.length!==1)throw Error('RAM discovery');
 const base=live[0],a=o=>base+o-(o<0x7000?8:0),b=o=>mem()[a(o)-1],w=o=>b(o)*256+mem()[a(o)];
 const putB=(o,v)=>{mem()[a(o)-1]=v&255;},putW=(o,v)=>{putB(o,v>>>8);mem()[a(o)]=v&255;};
 for(let t=0;t<128;++t)tick(t===0?64:0);
 putW(0x7a50,0);putB(0x7a32,0);
 for(let i=0;i<3;++i)putB(0x69f1+148*i,i===0?1:0);putB(0x7afa,1);putB(0x7ab0,1);
 const gear=[[name==='empty-entry'?0:91,0,0,0,0,0]];
 for(let i=0;i<6;++i)putB(0x69d8+i,gear[0][i]);
 putB(0x7a72,91);for(let i=0;i<142;++i)putB(0x739e+i,0);
 const title=Buffer.from('b5c0bedf','hex');for(let i=0;i<title.length;++i)putB(0x739e+i,title[i]);
 const description=Array.from({length:100},(_,i)=>65+i%26);
 if(name==='odd-forward'){description[65]=0xb5;description[66]=0xc0;}
 if(name==='controls'){description[4]=13;description[5]=10;}
 for(let i=0;i<description.length;++i)putB(0x73ab+i,description[i]);
 putW(0x7aba,77);putW(0x7abe,22);putW(0x7abc,37);
 const state=()=>({mode:w(0x7934),event:w(0x7936),context:w(0x7938),map:w(0x7a58),progress:w(0x7a66),sx:b(0x69d5),sy:b(0x69d6),vx:b(0x7a2e),vy:b(0x7a2c),direction:b(0x69d4),money:(w(0x799e)*65536+w(0x79a0))>>>0,enabled:w(0x7a50),slot:b(0x7a04),menu:b(0x7a54),member:b(0x7adc),equipmentSlot:b(0x7a2a),gearSelected:b(0x7ab2),descLength:w(0x7aba),descStart:w(0x7abe),descEnd:w(0x7abc),itemLoaded:b(0x7a72),skillLoaded:b(0x7a74),
  itemBytes:Array.from({length:142},(_,i)=>b(0x739e+i)),skillBytes:Array.from({length:144},(_,i)=>b(0x730e+i)),
  npc:Array.from({length:20},(_,i)=>{const o=0x6ca2+i*14;return{id:w(o),x:b(o+9),y:b(o+10),direction:b(o+8)};})});
 const before=state();fs.writeFileSync(`tmp/native-equipment-description-${name}-before.idx`,rt.framePixelIndices());
 const phases=[];
 const tail=name==='pages'||name==='odd-forward'||name==='controls'?[{mask:256},{mask:256},{mask:512},{mask:512}]:
  name==='empty-entry'?[{mask:256},{mask:512}]:name==='empty-right'?[{mask:8},{mask:256},{mask:256},{mask:512}]:
  name==='empty-left'?[{mask:8},{mask:512,patch:{start:10,end:30}}]:
  [{mask:name.startsWith('right')?256:512,patch:name==='right-word-wrap'?{end:65535}:name==='right-past-string'?{length:65535,end:150}:name==='left-past-string'?{start:101}:{start:32768}}];
 for(const phase of [{mask:64},{mask:16},{mask:2},{mask:16},...tail]){
  for(const [field,value] of Object.entries(phase.patch??{}))putW({length:0x7aba,start:0x7abe,end:0x7abc}[field],value);
  const prior=state(),commits=[],index=phases.length;
  for(let t=0;t<128;++t){if(phase.mask>=256)putW(0x7a76,t===16?(phase.mask===256?0x85:0x84):0);tick(t===0&&phase.mask<256?phase.mask:0);
   for(let j=0;j<rt.displayCommitCount();++j){const pixels=rt.displayCommitPixelIndices(j);if(pixels){
    fs.writeFileSync(`tmp/native-equipment-description-${name}-${index}-commit-${commits.length}.idx`,pixels);commits.push({state:state()});
   }}
  }
  phases.push({...phase,before:prior,commits,after:state()});
 }
 rows.push({name,gear,before,phases});fs.writeFileSync('tmp/native-equipment-description-service.json',JSON.stringify(rows,null,2)+'\n');
 console.log(JSON.stringify({name,phases:phases.map(p=>({mask:p.mask,commits:p.commits.length,menu:p.after.menu,length:p.after.descLength,start:p.after.descStart,end:p.after.descEnd,loaded:p.after.itemLoaded}))}));
 putW(0x7934,0);
}}finally{rt.free();}
